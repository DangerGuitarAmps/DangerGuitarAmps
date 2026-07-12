#pragma once

#include <cstddef>

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
struct ReverbIRTag
{
};

using PreEQStage = PassThroughStage<PreEQTag>;
using CompressorStage = PassThroughStage<CompressorTag>;
using ReverbIRStage = PassThroughStage<ReverbIRTag>;
}; // namespace danger::signal_chain
