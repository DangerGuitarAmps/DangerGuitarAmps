// Unserialization
//
// This plugin is used in important places, so we need to be considerate when
// attempting to unserialize. If the project was last saved with a legacy
// version, then we need it to "update" to the current version is as
// reasonable a way as possible.
//
// In order to handle older versions, the pattern is:
// 1. Implement unserialization for every version into a version-specific
//    struct (Let's use our friend nlohmann::json. Why not?)
// 2. Implement an "update" from each struct to the next one.
// 3. Implement assigning the data contained in the current struct to the
//    current plugin configuration.
//
// This way, a constant amount of effort is required every time the
// serialization changes instead of having to implement a current
// unserialization for each past version.

// Add new unserialization versions to the top, then add logic to the class method at the bottom.

// Boilerplate

void NeuralAmpModeler::_UnserializeApplyConfig(nlohmann::json& config)
{
  auto getParamByName = [&](std::string& name) {
    // Could use a map but eh
    for (int i = 0; i < kNumParams; i++)
    {
      iplug::IParam* param = GetParam(i);
      if (strcmp(param->GetName(), name.c_str()) == 0)
      {
        return param;
      }
    }
    // else
    return (iplug::IParam*)nullptr;
  };
  TRACE
  ENTER_PARAMS_MUTEX
  for (auto it = config.begin(); it != config.end(); ++it)
  {
    std::string name = it.key();
    const bool deprecatedToneStackParam =
      name == "Bass" || name == "Middle" || name == "Treble" || name == "ToneStack";
    const bool deprecatedPostEQParam = name.rfind("PostEQBand", 0) == 0 || name == "PostEQLowCut"
                                       || name == "PostEQHighCut";
    const bool deprecatedReverbParam = name.rfind("ReverbIR", 0) == 0;
    if (deprecatedToneStackParam || deprecatedPostEQParam || deprecatedReverbParam)
    {
      iplug::Trace(TRACELOC, "%s DEPRECATED-IGNORED", name.c_str());
      continue;
    }
    iplug::IParam* pParam = getParamByName(name);
    if (pParam != nullptr)
    {
      pParam->Set(*it);
      iplug::Trace(TRACELOC, "%s %f", pParam->GetName(), pParam->Value());
    }
    else
    {
      iplug::Trace(TRACELOC, "%s NOT-FOUND", name.c_str());
    }
  }
  OnParamReset(iplug::EParamSource::kPresetRecall);
  LEAVE_PARAMS_MUTEX

  mNAMPath.Set(static_cast<std::string>(config["NAMPath"]).c_str());
  mIRPath.Set(static_cast<std::string>(config["IRPath"]).c_str());
  // Consume the legacy path field but never reactivate the removed Reverb IR engine.
  mReverbIRPath.Set("");

  if (mNAMPath.GetLength())
  {
    _StageModel(mNAMPath);
  }
  if (mIRPath.GetLength())
  {
    _StageIR(mIRPath);
  }
}

// Unserialize NAM Path, IR path, then named keys
int _UnserializePathsAndExpectedKeys(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config,
                                     std::vector<std::string>& paramNames)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());

  for (auto it = paramNames.begin(); it != paramNames.end(); ++it)
  {
    double v = 0.0;
    pos = chunk.Get(&v, pos);
    config[*it] = v;
  }
  return pos;
}

void _RenameKeys(nlohmann::json& j, std::unordered_map<std::string, std::string> newNames)
{
  // Assumes no aliasing!
  for (auto it = newNames.begin(); it != newNames.end(); ++it)
  {
    j[it->second] = j[it->first];
    j.erase(it->first);
  }
}

// v0.7.19: four Post-EQ bands.
int _GetConfigFrom_0_7_19(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["ReverbIRPath"] = std::string(path.Get());

  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "ReverbIRBypass",
                                      "ReverbIRMix",
                                      "ReverbIRPreDelay",
                                      "ReverbIRLowCut",
                                      "ReverbIRHighCut",
                                      "ReverbIRWetLevel",
                                      "PreEQBypass",
                                      "PreEQLowCut",
                                      "PreEQLowShelfGain",
                                      "PreEQMidGain",
                                      "PreEQMidFrequency",
                                      "PreEQHighShelfGain",
                                      "PostEQBypass",
                                      "PostEQLowCut",
                                      "PostEQBand1Frequency",
                                      "PostEQBand1Gain",
                                      "PostEQBand1Q",
                                      "PostEQBand2Frequency",
                                      "PostEQBand2Gain",
                                      "PostEQBand2Q",
                                      "PostEQBand3Frequency",
                                      "PostEQBand3Gain",
                                      "PostEQBand3Q",
                                      "PostEQBand4Frequency",
                                      "PostEQBand4Gain",
                                      "PostEQBand4Q",
                                      "PostEQHighCut"};
  for (const auto& name : paramNames)
  {
    double value = 0.0;
    pos = chunk.Get(&value, pos);
    config[name] = value;
  }
  return pos;
}

void _UpdateConfigFrom_0_7_18(nlohmann::json& config)
{
  // v0.7.18 serialized a fifth Post-EQ band before High Cut. The reader has
  // already consumed those values to preserve chunk alignment; they are not
  // mapped into the four-band processor.
  config.erase("PostEQBand5Frequency");
  config.erase("PostEQBand5Gain");
  config.erase("PostEQBand5Q");
}

// v0.7.18: five Post-EQ bands.
int _GetConfigFrom_0_7_18(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["ReverbIRPath"] = std::string(path.Get());

  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "ReverbIRBypass",
                                      "ReverbIRMix",
                                      "ReverbIRPreDelay",
                                      "ReverbIRLowCut",
                                      "ReverbIRHighCut",
                                      "ReverbIRWetLevel",
                                      "PreEQBypass",
                                      "PreEQLowCut",
                                      "PreEQLowShelfGain",
                                      "PreEQMidGain",
                                      "PreEQMidFrequency",
                                      "PreEQHighShelfGain",
                                      "PostEQBypass",
                                      "PostEQLowCut",
                                      "PostEQBand1Frequency",
                                      "PostEQBand1Gain",
                                      "PostEQBand1Q",
                                      "PostEQBand2Frequency",
                                      "PostEQBand2Gain",
                                      "PostEQBand2Q",
                                      "PostEQBand3Frequency",
                                      "PostEQBand3Gain",
                                      "PostEQBand3Q",
                                      "PostEQBand4Frequency",
                                      "PostEQBand4Gain",
                                      "PostEQBand4Q",
                                      "PostEQBand5Frequency",
                                      "PostEQBand5Gain",
                                      "PostEQBand5Q",
                                      "PostEQHighCut"};
  for (const auto& name : paramNames)
  {
    double value = 0.0;
    pos = chunk.Get(&value, pos);
    config[name] = value;
  }
  _UpdateConfigFrom_0_7_18(config);
  return pos;
}

void _UpdateConfigFrom_0_7_17(nlohmann::json& config)
{
  config["PostEQBypass"] = 1.0;
  config["PostEQLowCut"] = 20.0;
  config["PostEQBand1Frequency"] = 100.0;
  config["PostEQBand1Gain"] = 0.0;
  config["PostEQBand1Q"] = 1.0;
  config["PostEQBand2Frequency"] = 400.0;
  config["PostEQBand2Gain"] = 0.0;
  config["PostEQBand2Q"] = 1.0;
  config["PostEQBand3Frequency"] = 2500.0;
  config["PostEQBand3Gain"] = 0.0;
  config["PostEQBand3Q"] = 1.0;
  config["PostEQBand4Frequency"] = 7000.0;
  config["PostEQBand4Gain"] = 0.0;
  config["PostEQBand4Q"] = 1.0;
  config["PostEQHighCut"] = 20000.0;
}

// v0.7.17
int _GetConfigFrom_0_7_17(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["ReverbIRPath"] = std::string(path.Get());

  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "ReverbIRBypass",
                                      "ReverbIRMix",
                                      "ReverbIRPreDelay",
                                      "ReverbIRLowCut",
                                      "ReverbIRHighCut",
                                      "ReverbIRWetLevel",
                                      "PreEQBypass",
                                      "PreEQLowCut",
                                      "PreEQLowShelfGain",
                                      "PreEQMidGain",
                                      "PreEQMidFrequency",
                                      "PreEQHighShelfGain"};
  for (const auto& name : paramNames)
  {
    double value = 0.0;
    pos = chunk.Get(&value, pos);
    config[name] = value;
  }
  _UpdateConfigFrom_0_7_17(config);
  return pos;
}

void _UpdateConfigFrom_0_7_16(nlohmann::json& config)
{
  config["PreEQBypass"] = 1.0;
  config["PreEQLowCut"] = 120.0;
  config["PreEQLowShelfGain"] = 0.0;
  config["PreEQMidGain"] = 0.0;
  config["PreEQMidFrequency"] = 800.0;
  config["PreEQHighShelfGain"] = 0.0;
  _UpdateConfigFrom_0_7_17(config);
}

// v0.7.16: NAM path, Speaker IR path, Reverb IR path, then named parameters.
int _GetConfigFrom_0_7_16(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  int pos = startPos;
  WDL_String path;
  pos = chunk.GetStr(path, pos);
  config["NAMPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["IRPath"] = std::string(path.Get());
  pos = chunk.GetStr(path, pos);
  config["ReverbIRPath"] = std::string(path.Get());

  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim",
                                      "ReverbIRBypass",
                                      "ReverbIRMix",
                                      "ReverbIRPreDelay",
                                      "ReverbIRLowCut",
                                      "ReverbIRHighCut",
                                      "ReverbIRWetLevel"};
  for (const auto& name : paramNames)
  {
    double value = 0.0;
    pos = chunk.Get(&value, pos);
    config[name] = value;
  }
  _UpdateConfigFrom_0_7_16(config);
  return pos;
}

// v0.7.14

void _UpdateConfigFrom_0_7_14(nlohmann::json& config)
{
  // v0.7.15 and earlier have no Reverb IR path or parameters.
  config["ReverbIRPath"] = "";
  config["ReverbIRBypass"] = 1.0;
  config["ReverbIRMix"] = 0.0;
  config["ReverbIRPreDelay"] = 0.0;
  config["ReverbIRLowCut"] = 20.0;
  config["ReverbIRHighCut"] = 20000.0;
  config["ReverbIRWetLevel"] = 0.0;
  _UpdateConfigFrom_0_7_16(config);
}

int _GetConfigFrom_0_7_14(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode",
                                      "Slim"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  _UpdateConfigFrom_0_7_14(config);
  return pos;
}

// v0.7.12

void _UpdateConfigFrom_0_7_12(nlohmann::json& config)
{
  config["Slim"] = 1.0;
  _UpdateConfigFrom_0_7_14(config);
}

int _GetConfigFrom_0_7_12(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{"Input",
                                      "Threshold",
                                      "Bass",
                                      "Middle",
                                      "Treble",
                                      "Output",
                                      "NoiseGateActive",
                                      "ToneStack",
                                      "IRToggle",
                                      "CalibrateInput",
                                      "InputCalibrationLevel",
                                      "OutputMode"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_12(config);
  return pos;
}

// 0.7.10

void _UpdateConfigFrom_0_7_10(nlohmann::json& config)
{
  // Note: "OutNorm" is Bool-like in v0.7.10, but "OutputMode" is enum.
  // This works because 0 is "Raw" (cf OutNorm false) and 1 is "Calibrated" (cf OutNorm true).
  std::unordered_map<std::string, std::string> newNames{{"OutNorm", "OutputMode"}};
  _RenameKeys(config, newNames);
  // There are new parameters. If they're not included, then 0.7.12 is ok, but future ones might not be.
  config[kCalibrateInputParamName] = (double)kDefaultCalibrateInput;
  config[kInputCalibrationLevelParamName] = kDefaultInputCalibrationLevel;
  _UpdateConfigFrom_0_7_12(config);
}

int _GetConfigFrom_0_7_10(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Threshold", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};
  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_0_7_10(config);
  return pos;
}

// Earlier than 0.7.10 (Assumed to be 0.7.3-0.7.9)

void _UpdateConfigFrom_Earlier(nlohmann::json& config)
{
  std::unordered_map<std::string, std::string> newNames{{"Gate", "Threshold"}};
  _RenameKeys(config, newNames);
  _UpdateConfigFrom_0_7_10(config);
}

int _GetConfigFrom_Earlier(const iplug::IByteChunk& chunk, int startPos, nlohmann::json& config)
{
  std::vector<std::string> paramNames{
    "Input", "Gate", "Bass", "Middle", "Treble", "Output", "NoiseGateActive", "ToneStack", "OutNorm", "IRToggle"};

  int pos = _UnserializePathsAndExpectedKeys(chunk, startPos, config, paramNames);
  // Then update:
  _UpdateConfigFrom_Earlier(config);
  return pos;
}

//==============================================================================

class _Version
{
public:
  _Version(const int major, const int minor, const int patch)
  : mMajor(major)
  , mMinor(minor)
  , mPatch(patch) {};
  _Version(const std::string& versionStr)
  {
    std::istringstream stream(versionStr);
    std::string token;
    std::vector<int> parts;

    // Split the string by "."
    while (std::getline(stream, token, '.'))
    {
      std::size_t parsedCharacters = 0;
      const int value = std::stoi(token, &parsedCharacters);
      const bool semanticPrerelease = parts.size() == 2 && parsedCharacters < token.size()
                                      && token[parsedCharacters] == '-';
      if (parsedCharacters != token.size() && !semanticPrerelease)
        throw std::invalid_argument("Invalid version segment");
      parts.push_back(value);
    }

    // Check if we have exactly 3 parts
    if (parts.size() != 3)
    {
      throw std::invalid_argument("Input string does not contain exactly 3 segments separated by '.'");
    }

    // Assign the parts to the provided int variables
    mMajor = parts[0];
    mMinor = parts[1];
    mPatch = parts[2];
  };

  bool operator>=(const _Version& other) const
  {
    // Compare on major version:
    if (GetMajor() > other.GetMajor())
    {
      return true;
    }
    if (GetMajor() < other.GetMajor())
    {
      return false;
    }
    // Compare on minor
    if (GetMinor() > other.GetMinor())
    {
      return true;
    }
    if (GetMinor() < other.GetMinor())
    {
      return false;
    }
    // Compare on patch
    return GetPatch() >= other.GetPatch();
  };

  int GetMajor() const { return mMajor; };
  int GetMinor() const { return mMinor; };
  int GetPatch() const { return mPatch; };

private:
  int mMajor;
  int mMinor;
  int mPatch;
};

int NeuralAmpModeler::_UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  // We already got through the header before calling this.
  int pos = startPos;

  // Get the version
  WDL_String wVersion;
  pos = chunk.GetStr(wVersion, pos);
  std::string versionStr(wVersion.Get());
  _Version version(versionStr);
  // Act accordingly
  nlohmann::json config;
  if (version >= _Version(0, 7, 19))
  {
    pos = _GetConfigFrom_0_7_19(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 18))
  {
    pos = _GetConfigFrom_0_7_18(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 17))
  {
    pos = _GetConfigFrom_0_7_17(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 16))
  {
    pos = _GetConfigFrom_0_7_16(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 14))
  {
    pos = _GetConfigFrom_0_7_14(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 12))
  {
    pos = _GetConfigFrom_0_7_12(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 10))
  {
    pos = _GetConfigFrom_0_7_10(chunk, pos, config);
  }
  else if (version >= _Version(0, 7, 9))
  {
    pos = _GetConfigFrom_Earlier(chunk, pos, config);
  }
  else
  {
    // You shouldn't be here...
    assert(false);
  }
  _UnserializeApplyConfig(config);
  return _UnserializeIRFormatExtension(chunk, pos);
}

int NeuralAmpModeler::_UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos)
{
  nlohmann::json config;
  int pos = _GetConfigFrom_Earlier(chunk, startPos, config);
  _UnserializeApplyConfig(config);
  return _UnserializeIRFormatExtension(chunk, pos);
}

int NeuralAmpModeler::_UnserializeIRFormatExtension(const iplug::IByteChunk& chunk, const int startPos)
{
  // Stereo support appends an optional extension after the historical
  // parameter payload. Old chunks end at startPos and therefore take this
  // safe mono default without any out-of-bounds read.
  mIRChannelFormat = 1;
  if (startPos < 0 || startPos >= chunk.Size())
    return startPos;

  constexpr const char* kMarker = "DangerIRFormatV1";
  const auto readMarker = [&chunk, kMarker](const int markerPos, WDL_String& marker) {
    int length = 0;
    if (chunk.Get(&length, markerPos) < 0 || length != static_cast<int>(strlen(kMarker)))
      return -1;
    const int end = chunk.GetStr(marker, markerPos);
    return end >= 0 && strcmp(marker.Get(), kMarker) == 0 ? end : -1;
  };
  WDL_String marker;
  int pos = readMarker(startPos, marker);
  if (pos < 0)
  {
    // Current states append the nine graphic-EQ gains and Post Level after
    // the historical named payload. Older states have either no suffix or
    // the stereo marker immediately at startPos.
    constexpr std::array<int, 10> params{
      kGraphicEQ62HzGain, kGraphicEQ125HzGain, kGraphicEQ250HzGain, kGraphicEQ500HzGain, kGraphicEQ1kHzGain,
      kGraphicEQ2kHzGain, kGraphicEQ4kHzGain, kGraphicEQ8kHzGain, kGraphicEQ16kHzGain, kGraphicEQPostLevel};
    pos = startPos;
    std::array<double, params.size()> values{};
    for (double& value : values)
    {
      pos = chunk.Get(&value, pos);
      if (pos < 0)
        return startPos;
    }
    const int markerEnd = readMarker(pos, marker);
    if (markerEnd < 0)
      return startPos;
    for (size_t i = 0; i < params.size(); ++i)
      GetParam(params[i])->Set(values[i]);
    _ApplyPostEQParams();
    pos = markerEnd;
  }

  int cabinetChannels = 1;
  int reverbChannels = 1;
  pos = chunk.Get(&cabinetChannels, pos);
  if (pos < 0)
    return startPos;
  pos = chunk.Get(&reverbChannels, pos);
  if (pos < 0)
    return startPos;
  mIRChannelFormat.store(cabinetChannels == 2 ? 2 : 1, std::memory_order_relaxed);
  (void)reverbChannels; // Reserved legacy field; Reverb IR processing was removed.
  return pos;
}
