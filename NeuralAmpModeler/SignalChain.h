#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace danger::signal_chain
{
// Structure-only integration point for a future stage. Processing is deliberately
// an identity operation and owns no buffers, parameters, or state.
template <typename StageTag> class PassThroughStage
{
public:
  template <typename Sample>
  Sample** Process(Sample** inputs, const std::size_t /*numChannels*/, const std::size_t /*numFrames*/) const noexcept
  {
    return inputs;
  }
};

struct PreEQTag
{
};
struct CompressorTag
{
};

using PreEQStage = PassThroughStage<PreEQTag>;
using CompressorStage = PassThroughStage<CompressorTag>;

// An internal-only reverb convolution stage. Host parameters, persistence and
// UI integration are intentionally deferred. File loading and buffer preparation
// happen in StageFile()/Prepare(), never in Process().
class ReverbIRStage
{
public:
  static constexpr double kMaximumPreDelaySeconds = 2.0;

  dsp::wav::LoadReturnCode StageFile(const char* fileName, const double sampleRate, const std::size_t maxBlockSize)
  {
    std::unique_ptr<State> candidate;
    try
    {
      candidate = std::make_unique<State>(fileName, sampleRate, maxBlockSize);
    }
    catch (...)
    {
      return dsp::wav::LoadReturnCode::ERROR_OTHER;
    }

    const auto result = candidate->mConvolution->GetWavState();
    if (result != dsp::wav::LoadReturnCode::SUCCESS)
      return result;

    std::lock_guard<std::mutex> lock(mExchangeMutex);
    mRetired.reset();
    mStaged = std::move(candidate);
    mClearRequested = false;
    return result;
  }

  void RequestClear()
  {
    std::lock_guard<std::mutex> lock(mExchangeMutex);
    mStaged.reset();
    mRetired.reset();
    mClearRequested = true;
  }

  void Prepare(const double sampleRate, const std::size_t maxBlockSize)
  {
    std::lock_guard<std::mutex> lock(mExchangeMutex);
    mSampleRate = sampleRate;
    mMaxBlockSize = std::max<std::size_t>(maxBlockSize, 1);
    if (mClearRequested)
      return;

    // OnReset is an off-audio preparation point. Rebuild loaded convolution
    // state at a new sample rate, retaining the raw IR data.
    State* source = mStaged != nullptr ? mStaged.get() : mActive.get();
    if (source != nullptr && (source->mSampleRate != sampleRate || source->mMaxBlockSize != mMaxBlockSize))
    {
      auto replacement = std::make_unique<State>(source->mConvolution->GetData(), sampleRate, mMaxBlockSize);
      mRetired.reset();
      mStaged = std::move(replacement);
    }
  }

  // Called only at a ProcessBlock boundary. try_lock is deliberately used so
  // that the audio thread never waits for a file-loading/control thread.
  void ApplyStaged() noexcept
  {
    if (!mExchangeMutex.try_lock())
      return;

    if (mClearRequested && mRetired == nullptr)
    {
      mRetired = std::move(mActive);
      mClearRequested = false;
    }
    else if (mStaged != nullptr && mRetired == nullptr)
    {
      mRetired = std::move(mActive);
      mActive = std::move(mStaged);
    }
    mExchangeMutex.unlock();
  }

  double** Process(double** inputs, const std::size_t numChannels, const std::size_t numFrames) noexcept
  {
    State* state = mActive.get();
    const double wetMix = mWetMix.load(std::memory_order_relaxed);
    if (mBypassed.load(std::memory_order_relaxed) || wetMix <= 0.0 || state == nullptr || numChannels == 0
        || numFrames == 0 || numFrames > state->mMaxBlockSize)
      return inputs;

    if (mFilterSettingsChanged.exchange(false, std::memory_order_acq_rel))
    {
      recursive_linear_filter::HighPassParams lowCutParams(
        state->mSampleRate, std::min(mLowCutHz.load(std::memory_order_relaxed), 0.49 * state->mSampleRate));
      recursive_linear_filter::LowPassParams highCutParams(
        state->mSampleRate, std::min(mHighCutHz.load(std::memory_order_relaxed), 0.49 * state->mSampleRate));
      state->mLowCut.SetParams(lowCutParams);
      state->mHighCut.SetParams(highCutParams);
    }

    // The shared convolution implementation is mono. Collapse any future
    // multichannel input to mono without modifying the caller's buffer.
    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
      double mono = 0.0;
      for (std::size_t channel = 0; channel < numChannels; ++channel)
        mono += inputs[channel][frame];
      state->mMonoInput[frame] = mono / static_cast<double>(numChannels);
    }

    const std::size_t delaySamples = std::min(
      state->mDelayLine.size() - 1,
      static_cast<std::size_t>(mPreDelaySeconds.load(std::memory_order_relaxed) * state->mSampleRate));
    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
      state->mDelayLine[state->mDelayWrite] = state->mMonoInput[frame];
      const std::size_t read = (state->mDelayWrite + state->mDelayLine.size() - delaySamples) % state->mDelayLine.size();
      state->mPreDelayed[frame] = state->mDelayLine[read];
      state->mDelayWrite = (state->mDelayWrite + 1) % state->mDelayLine.size();
    }

    double** wet = state->mConvolution->Process(state->mPreDelayPointers.data(), 1, numFrames);
    wet = state->mLowCut.Process(wet, 1, numFrames);
    wet = state->mHighCut.Process(wet, 1, numFrames);

    const double wetGain = mWetOutputGain.load(std::memory_order_relaxed);
    const double dryMix = 1.0 - wetMix;
    for (std::size_t frame = 0; frame < numFrames; ++frame)
      state->mOutput[frame] = dryMix * state->mMonoInput[frame] + wetMix * wetGain * wet[0][frame];

    return state->mOutputPointers.data();
  }

  void SetBypassed(const bool bypassed) noexcept { mBypassed.store(bypassed, std::memory_order_relaxed); }
  void SetWetMix(const double wetMix) noexcept
  {
    mWetMix.store(std::clamp(wetMix, 0.0, 1.0), std::memory_order_relaxed);
  }
  void SetPreDelaySeconds(const double seconds) noexcept
  {
    mPreDelaySeconds.store(std::clamp(seconds, 0.0, kMaximumPreDelaySeconds), std::memory_order_relaxed);
  }
  void SetWetOutputGain(const double gain) noexcept
  {
    mWetOutputGain.store(std::max(gain, 0.0), std::memory_order_relaxed);
  }
  void SetWetFilterFrequencies(const double lowCutHz, const double highCutHz) noexcept
  {
    mLowCutHz.store(std::max(lowCutHz, 1.0), std::memory_order_relaxed);
    mHighCutHz.store(std::max(highCutHz, 1.0), std::memory_order_relaxed);
    mFilterSettingsChanged.store(true, std::memory_order_release);
  }

private:
  struct State
  {
    State(const char* fileName, const double sampleRate, const std::size_t maxBlockSize)
    : mConvolution(std::make_unique<dsp::ImpulseResponse>(fileName, sampleRate))
    , mSampleRate(sampleRate)
    , mMaxBlockSize(std::max<std::size_t>(maxBlockSize, 1))
    {
      if (mConvolution->GetWavState() == dsp::wav::LoadReturnCode::SUCCESS)
        PrepareBuffers();
    }

    State(const dsp::ImpulseResponse::IRData& data, const double sampleRate, const std::size_t maxBlockSize)
    : mConvolution(std::make_unique<dsp::ImpulseResponse>(data, sampleRate))
    , mSampleRate(sampleRate)
    , mMaxBlockSize(std::max<std::size_t>(maxBlockSize, 1))
    {
      PrepareBuffers();
    }

    void PrepareBuffers()
    {
      mMonoInput.resize(mMaxBlockSize);
      mPreDelayed.resize(mMaxBlockSize);
      mOutput.resize(mMaxBlockSize);
      mDelayLine.resize(static_cast<std::size_t>(kMaximumPreDelaySeconds * mSampleRate) + 1);
      mPreDelayPointers = {mPreDelayed.data()};
      mOutputPointers = {mOutput.data()};

      recursive_linear_filter::HighPassParams lowCutParams(mSampleRate, 20.0);
      recursive_linear_filter::LowPassParams highCutParams(mSampleRate, 20000.0);
      mLowCut.SetParams(lowCutParams);
      mHighCut.SetParams(highCutParams);

      // Prime all shared DSP buffers and convolution history off the audio thread.
      double** wet = mConvolution->Process(mPreDelayPointers.data(), 1, mMaxBlockSize);
      wet = mLowCut.Process(wet, 1, mMaxBlockSize);
      mHighCut.Process(wet, 1, mMaxBlockSize);
    }

    std::unique_ptr<dsp::ImpulseResponse> mConvolution;
    double mSampleRate;
    std::size_t mMaxBlockSize;
    std::vector<double> mMonoInput;
    std::vector<double> mPreDelayed;
    std::vector<double> mOutput;
    std::vector<double> mDelayLine;
    std::vector<double*> mPreDelayPointers;
    std::vector<double*> mOutputPointers;
    std::size_t mDelayWrite = 0;
    recursive_linear_filter::HighPass mLowCut;
    recursive_linear_filter::LowPass mHighCut;
  };

  std::unique_ptr<State> mActive;
  std::unique_ptr<State> mStaged;
  // Replaced state is reclaimed by the next off-thread StageFile/Prepare call,
  // preventing a convolution destructor/free from running in ProcessBlock.
  std::unique_ptr<State> mRetired;
  bool mClearRequested = false;
  std::mutex mExchangeMutex;
  double mSampleRate = 48000.0;
  std::size_t mMaxBlockSize = 1;

  std::atomic<bool> mBypassed{true};
  std::atomic<double> mWetMix{0.0};
  std::atomic<double> mPreDelaySeconds{0.0};
  std::atomic<double> mWetOutputGain{1.0};
  std::atomic<double> mLowCutHz{20.0};
  std::atomic<double> mHighCutHz{20000.0};
  std::atomic<bool> mFilterSettingsChanged{false};
};
}; // namespace danger::signal_chain
