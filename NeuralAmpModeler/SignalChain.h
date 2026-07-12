#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"

#include <algorithm>
#include <array>
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

using CompressorStage = PassThroughStage<CompressorTag>;

class PreEQStage
{
public:
  static constexpr std::size_t kMaximumChannels = 2;
  static constexpr double kLowShelfFrequency = 120.0;
  static constexpr double kHighShelfFrequency = 4000.0;
  static constexpr double kMidQ = 0.7071067811865476;

  void Prepare(const double sampleRate, const std::size_t maxBlockSize)
  {
    mSampleRate = std::max(sampleRate, 1.0);
    mMaxBlockSize = std::max<std::size_t>(maxBlockSize, 1);
    for (auto& channel : mOutput)
      channel.resize(mMaxBlockSize);
    for (std::size_t channel = 0; channel < kMaximumChannels; ++channel)
      mOutputPointers[channel] = mOutput[channel].data();
    mSmoothingCoefficient = 1.0 - std::exp(-1.0 / (0.02 * mSampleRate));
    Reset();
    UpdateCoefficientTargets();
    mHighPass.SnapToTarget();
    mLowShelf.SnapToTarget();
    mMid.SnapToTarget();
    mHighShelf.SnapToTarget();
    mProcessedMix = mBypassed.load(std::memory_order_relaxed) ? 0.0 : 1.0;
  }

  void Reset() noexcept
  {
    mHighPass.Reset();
    mLowShelf.Reset();
    mMid.Reset();
    mHighShelf.Reset();
  }

  double** Process(double** inputs, const std::size_t numChannels, const std::size_t numFrames) noexcept
  {
    if (numChannels == 0 || numChannels > kMaximumChannels || numFrames == 0 || numFrames > mMaxBlockSize)
      return inputs;

    const double targetMix = mBypassed.load(std::memory_order_relaxed) ? 0.0 : 1.0;
    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
      mHighPass.AdvanceCoefficients(mSmoothingCoefficient);
      mLowShelf.AdvanceCoefficients(mSmoothingCoefficient);
      mMid.AdvanceCoefficients(mSmoothingCoefficient);
      mHighShelf.AdvanceCoefficients(mSmoothingCoefficient);
      mProcessedMix += mSmoothingCoefficient * (targetMix - mProcessedMix);
      if (std::abs(targetMix - mProcessedMix) < 1.0e-9)
        mProcessedMix = targetMix;

      for (std::size_t channel = 0; channel < numChannels; ++channel)
      {
        const double dry = inputs[channel][frame];
        double processed = mHighPass.ProcessSample(dry, channel);
        processed = mLowShelf.ProcessSample(processed, channel);
        processed = mMid.ProcessSample(processed, channel);
        processed = mHighShelf.ProcessSample(processed, channel);
        mOutput[channel][frame] = dry + mProcessedMix * (processed - dry);
      }
    }
    return mOutputPointers.data();
  }

  void SetBypassed(const bool bypassed) noexcept { mBypassed.store(bypassed, std::memory_order_relaxed); }
  void SetHighPassFrequency(const double frequencyHz) noexcept
  {
    mHighPassFrequency.store(std::clamp(frequencyHz, 20.0, 300.0), std::memory_order_relaxed);
    UpdateCoefficientTargets();
  }
  void SetLowShelfGain(const double gainDB) noexcept
  {
    mLowShelfGain.store(std::clamp(gainDB, -12.0, 12.0), std::memory_order_relaxed);
    UpdateCoefficientTargets();
  }
  void SetMid(const double gainDB, const double frequencyHz) noexcept
  {
    mMidGain.store(std::clamp(gainDB, -12.0, 12.0), std::memory_order_relaxed);
    mMidFrequency.store(std::clamp(frequencyHz, 150.0, 4000.0), std::memory_order_relaxed);
    UpdateCoefficientTargets();
  }
  void SetHighShelfGain(const double gainDB) noexcept
  {
    mHighShelfGain.store(std::clamp(gainDB, -12.0, 12.0), std::memory_order_relaxed);
    UpdateCoefficientTargets();
  }

private:
  struct Biquad
  {
    void SetCoefficients(const double nb0, const double nb1, const double nb2, const double na0, const double na1,
                         const double na2) noexcept
    {
      targetB0.store(nb0 / na0, std::memory_order_relaxed);
      targetB1.store(nb1 / na0, std::memory_order_relaxed);
      targetB2.store(nb2 / na0, std::memory_order_relaxed);
      targetA1.store(na1 / na0, std::memory_order_relaxed);
      targetA2.store(na2 / na0, std::memory_order_relaxed);
    }

    void AdvanceCoefficients(const double smoothingCoefficient) noexcept
    {
      b0 += smoothingCoefficient * (targetB0.load(std::memory_order_relaxed) - b0);
      b1 += smoothingCoefficient * (targetB1.load(std::memory_order_relaxed) - b1);
      b2 += smoothingCoefficient * (targetB2.load(std::memory_order_relaxed) - b2);
      a1 += smoothingCoefficient * (targetA1.load(std::memory_order_relaxed) - a1);
      a2 += smoothingCoefficient * (targetA2.load(std::memory_order_relaxed) - a2);
    }

    void SnapToTarget() noexcept
    {
      b0 = targetB0.load(std::memory_order_relaxed);
      b1 = targetB1.load(std::memory_order_relaxed);
      b2 = targetB2.load(std::memory_order_relaxed);
      a1 = targetA1.load(std::memory_order_relaxed);
      a2 = targetA2.load(std::memory_order_relaxed);
    }

    double ProcessSample(const double input, const std::size_t channel) noexcept
    {
      const double output = b0 * input + z1[channel];
      z1[channel] = b1 * input - a1 * output + z2[channel];
      z2[channel] = b2 * input - a2 * output;
      return output;
    }

    void Reset() noexcept
    {
      z1.fill(0.0);
      z2.fill(0.0);
    }

    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    std::atomic<double> targetB0{1.0};
    std::atomic<double> targetB1{0.0};
    std::atomic<double> targetB2{0.0};
    std::atomic<double> targetA1{0.0};
    std::atomic<double> targetA2{0.0};
    std::array<double, kMaximumChannels> z1{};
    std::array<double, kMaximumChannels> z2{};
  };

  double LimitedFrequency(const double frequency) const noexcept
  {
    return std::min(frequency, 0.49 * mSampleRate);
  }

  void SetHighPassCoefficients(Biquad& filter, const double frequency, const double q) noexcept
  {
    const double omega = 2.0 * MATH_PI * LimitedFrequency(frequency) / mSampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double b0 = 0.5 * (1.0 + cosine);
    const double b1 = -(1.0 + cosine);
    const double b2 = b0;
    const double a0 = 1.0 + alpha;
    const double a1 = -2.0 * cosine;
    const double a2 = 1.0 - alpha;
    filter.SetCoefficients(b0, b1, b2, a0, a1, a2);
  }

  void SetPeakingCoefficients(Biquad& filter, const double frequency, const double q, const double gainDB) noexcept
  {
    const double a = std::pow(10.0, gainDB / 40.0);
    const double omega = 2.0 * MATH_PI * LimitedFrequency(frequency) / mSampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    filter.SetCoefficients(1.0 + alpha * a, -2.0 * cosine, 1.0 - alpha * a, 1.0 + alpha / a,
                           -2.0 * cosine, 1.0 - alpha / a);
  }

  void SetShelfCoefficients(Biquad& filter, const double frequency, const double q, const double gainDB,
                            const bool highShelf) noexcept
  {
    const double a = std::pow(10.0, gainDB / 40.0);
    const double omega = 2.0 * MATH_PI * LimitedFrequency(frequency) / mSampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double ap = a + 1.0;
    const double am = a - 1.0;
    const double rootTerm = 2.0 * std::sqrt(a) * alpha;

    if (highShelf)
    {
      filter.SetCoefficients(a * (ap + am * cosine + rootTerm), -2.0 * a * (am + ap * cosine),
                             a * (ap + am * cosine - rootTerm), ap - am * cosine + rootTerm,
                             2.0 * (am - ap * cosine), ap - am * cosine - rootTerm);
    }
    else
    {
      filter.SetCoefficients(a * (ap - am * cosine + rootTerm), 2.0 * a * (am - ap * cosine),
                             a * (ap - am * cosine - rootTerm), ap + am * cosine + rootTerm,
                             -2.0 * (am + ap * cosine), ap + am * cosine - rootTerm);
    }
  }

  void UpdateCoefficientTargets() noexcept
  {
    SetHighPassCoefficients(mHighPass, mHighPassFrequency.load(std::memory_order_relaxed), kMidQ);
    SetShelfCoefficients(mLowShelf, kLowShelfFrequency, kMidQ, mLowShelfGain.load(std::memory_order_relaxed), false);
    SetPeakingCoefficients(mMid, mMidFrequency.load(std::memory_order_relaxed), kMidQ,
                           mMidGain.load(std::memory_order_relaxed));
    SetShelfCoefficients(mHighShelf, kHighShelfFrequency, kMidQ,
                         mHighShelfGain.load(std::memory_order_relaxed), true);
  }

  std::atomic<bool> mBypassed{true};
  double mSampleRate = 48000.0;
  std::size_t mMaxBlockSize = 1;
  double mSmoothingCoefficient = 1.0;
  double mProcessedMix = 0.0;
  std::atomic<double> mHighPassFrequency{120.0};
  std::atomic<double> mLowShelfGain{0.0};
  std::atomic<double> mMidGain{0.0};
  std::atomic<double> mMidFrequency{800.0};
  std::atomic<double> mHighShelfGain{0.0};
  Biquad mHighPass;
  Biquad mLowShelf;
  Biquad mMid;
  Biquad mHighShelf;
  std::array<std::vector<double>, kMaximumChannels> mOutput;
  std::array<double*, kMaximumChannels> mOutputPointers{};
};

// An internal-only reverb convolution stage. Host parameters, persistence and
// UI integration are intentionally deferred. File loading and buffer preparation
// happen in StageFile()/Prepare(), never in Process().
class ReverbIRStage
{
public:
  static constexpr double kMaximumPreDelaySeconds = 2.0;

  dsp::wav::LoadReturnCode StageFile(const char* fileName, const double sampleRate, const std::size_t maxBlockSize)
  {
    std::size_t preparedBlockSize = std::max<std::size_t>(maxBlockSize, 1);
    {
      std::lock_guard<std::mutex> lock(mExchangeMutex);
      preparedBlockSize = std::max(preparedBlockSize, mMaxBlockSize);
    }

    std::unique_ptr<State> candidate;
    try
    {
      candidate = std::make_unique<State>(fileName, sampleRate, preparedBlockSize);
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
        || numChannels > state->mOutputPointers.size() || numFrames == 0 || numFrames > state->mMaxBlockSize)
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

    // The plug-in is internally mono. If a two-channel caller is introduced,
    // expose the same processed mono return on both channels rather than an
    // invalid one-element pointer array.
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
      mOutputPointers = {mOutput.data(), mOutput.data()};

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
