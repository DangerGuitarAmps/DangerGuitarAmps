#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/wav.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
void WriteU16(std::ofstream& stream, const std::uint16_t value)
{
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteU32(std::ofstream& stream, const std::uint32_t value)
{
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WritePCM16Wav(const std::filesystem::path& path, const int channels,
                   const std::vector<std::int16_t>& interleaved)
{
  std::ofstream stream(path, std::ios::binary);
  const std::uint32_t dataBytes = static_cast<std::uint32_t>(interleaved.size() * sizeof(std::int16_t));
  stream.write("RIFF", 4);
  WriteU32(stream, 36 + dataBytes);
  stream.write("WAVEfmt ", 8);
  WriteU32(stream, 16);
  WriteU16(stream, 1);
  WriteU16(stream, static_cast<std::uint16_t>(channels));
  WriteU32(stream, 48000);
  WriteU32(stream, 48000 * channels * 2);
  WriteU16(stream, static_cast<std::uint16_t>(channels * 2));
  WriteU16(stream, 16);
  stream.write("data", 4);
  WriteU32(stream, dataBytes);
  stream.write(reinterpret_cast<const char*>(interleaved.data()), dataBytes);
}

bool NearZero(const double value) { return std::abs(value) < 1.0e-9; }
}

int main()
{
  const auto directory = std::filesystem::temp_directory_path() / "danger-stereo-ir-test";
  std::filesystem::create_directories(directory);
  const auto stereoPath = directory / "stereo.wav";
  const auto surroundPath = directory / "surround.wav";
  WritePCM16Wav(stereoPath, 2, {32767, 0, 0, 32767, 0, 0, 0, 0});
  WritePCM16Wav(surroundPath, 3, {32767, 0, 0, 0, 32767, 0});

  dsp::wav::AudioData decoded;
  if (dsp::wav::Load(stereoPath.string().c_str(), decoded) != dsp::wav::LoadReturnCode::SUCCESS
      || decoded.channels.size() != 2 || decoded.channels[0][0] < 0.99f || decoded.channels[1][1] < 0.99f)
    return 1;
  if (dsp::wav::Load(surroundPath.string().c_str(), decoded)
      != dsp::wav::LoadReturnCode::ERROR_UNSUPPORTED_CHANNEL_COUNT)
    return 2;

  dsp::ImpulseResponse stereo(stereoPath.string().c_str(), 48000.0);
  stereo.Prepare(4);
  double inputLeft[4]{1.0, 0.0, 0.0, 0.0};
  double inputRight[4]{1.0, 0.0, 0.0, 0.0};
  double* inputs[2]{inputLeft, inputRight};
  double** output = stereo.Process(inputs, 2, 4);
  if (NearZero(output[0][0]) || !NearZero(output[1][0]) || NearZero(output[1][1]))
    return 3;

  // A mono kernel must retain independent histories for an already-stereo
  // input instead of copying or summing the left result into the right.
  dsp::ImpulseResponse::IRData monoData{{{1.0f}}, 48000.0};
  dsp::ImpulseResponse mono(monoData, 48000.0);
  mono.Prepare(4);
  double silentRight[4]{};
  double* asymmetricInputs[2]{inputLeft, silentRight};
  output = mono.Process(asymmetricInputs, 2, 4);
  if (NearZero(output[0][0]) || !NearZero(output[1][0]))
    return 4;

  for (const double sampleRate : {44100.0, 48000.0, 96000.0})
  {
    dsp::ImpulseResponse resampled(monoData, sampleRate);
    resampled.Prepare(4);
    output = resampled.Process(asymmetricInputs, 2, 4);
    if (!std::isfinite(output[0][0]) || NearZero(output[0][0]) || !NearZero(output[1][0]))
      return 5;
  }

  std::filesystem::remove_all(directory);
  std::cout << "stereo IR tests passed\n";
  return 0;
}
