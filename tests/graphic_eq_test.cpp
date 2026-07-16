#include "../NeuralAmpModeler/SignalChain.h"

#include <array>
#include <cmath>
#include <iostream>
#include <vector>

using danger::signal_chain::GraphicEQStage;

namespace
{
double RenderRMS(GraphicEQStage& eq, const double frequency, const double sampleRate, const int blockSize,
                 const int channels = 1)
{
  const int blocks = std::max(400, static_cast<int>(sampleRate * 0.25 / blockSize));
  std::array<std::vector<double>, 2> input;
  std::array<double*, 2> pointers{};
  for (int channel = 0; channel < channels; ++channel)
  {
    input[channel].resize(blockSize);
    pointers[channel] = input[channel].data();
  }
  double phase = 0.0;
  double sum = 0.0;
  int count = 0;
  for (int block = 0; block < blocks; ++block)
  {
    for (int frame = 0; frame < blockSize; ++frame)
    {
      const double value = std::sin(phase);
      phase += 2.0 * MATH_PI * frequency / sampleRate;
      for (int channel = 0; channel < channels; ++channel)
        input[channel][frame] = channel == 0 ? value : 0.0;
    }
    double** output = eq.Process(pointers.data(), channels, blockSize);
    if (block > blocks / 2)
      for (int frame = 0; frame < blockSize; ++frame)
      {
        sum += output[0][frame] * output[0][frame];
        ++count;
        if (channels == 2 && std::abs(output[1][frame]) > 1.0e-12)
          throw std::runtime_error("graphic EQ leaked between stereo channels");
      }
  }
  return std::sqrt(sum / count);
}

void Require(const bool condition, const char* message)
{
  if (!condition)
    throw std::runtime_error(message);
}
} // namespace

int main()
{
  try
  {
    for (const double sampleRate : {44100.0, 48000.0, 96000.0})
      for (const int blockSize : {1, 64, 511})
      {
        GraphicEQStage eq;
        eq.Prepare(sampleRate, blockSize);
        eq.SetBypassed(false);
        const double flat = RenderRMS(eq, 1000.0, sampleRate, blockSize, 2);
        Require(std::abs(20.0 * std::log10(flat * std::sqrt(2.0))) < 0.15, "flat EQ was not neutral");

        eq.SetBandGain(4, 12.0);
        const double boosted = RenderRMS(eq, 1000.0, sampleRate, blockSize);
        Require(20.0 * std::log10(boosted / flat) > 10.0, "1 kHz boost was ineffective");

        eq.SetBandGain(4, -12.0);
        const double cut = RenderRMS(eq, 1000.0, sampleRate, blockSize);
        Require(20.0 * std::log10(cut / flat) < -10.0, "1 kHz cut was ineffective");

        eq.SetBandGain(4, 0.0);
        eq.SetPostLevelDB(-6.0);
        const double level = RenderRMS(eq, 1000.0, sampleRate, blockSize);
        Require(std::abs(20.0 * std::log10(level / flat) + 6.0) < 0.2, "Post Level scaling was incorrect");

        // FLAT sends 0 dB to every band and Post Level through the host
        // parameter path; exercise the resulting DSP state here.
        for (std::size_t band = 0; band < GraphicEQStage::kNumBands; ++band)
          eq.SetBandGain(band, 0.0);
        eq.SetPostLevelDB(0.0);
        const double reset = RenderRMS(eq, 1000.0, sampleRate, blockSize);
        Require(std::abs(20.0 * std::log10(reset / flat)) < 0.15, "FLAT did not restore Post Level to 0 dB");

        eq.SetBypassed(true);
        const double bypassed = RenderRMS(eq, 1000.0, sampleRate, blockSize);
        Require(std::abs(20.0 * std::log10(bypassed / flat)) < 0.15, "bypass did not bypass EQ and Post Level");
      }
    std::cout << "graphic EQ tests passed\n";
    return 0;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
