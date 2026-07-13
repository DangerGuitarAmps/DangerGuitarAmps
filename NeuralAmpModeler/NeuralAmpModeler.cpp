#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

// Styles
const float kControlLabelTextSize = 17.0f;
const float kSectionHeadingTextSize = 15.0f;
const float kSectionHeadingTopPadding = 4.0f;
const float kSectionHeadingHeight = 18.0f;
const float kSectionHeadingBottomPadding = 4.0f;
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {kControlLabelTextSize, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(27, PluginColors::OFF_WHITE, "Michroma-Regular"))
                             .WithDrawFrame(false)
                             .WithShadowOffset(1.f);
const IVStyle shellLegendStyle = DEFAULT_STYLE.WithValueText(IText(10, PluginColors::NAM_3, "Roboto-Regular"))
                                   .WithDrawFrame(false);
const IVStyle sectionHeadingStyle =
  DEFAULT_STYLE.WithValueText(IText(kSectionHeadingTextSize, PluginColors::OFF_WHITE, "Roboto-Regular"))
    .WithDrawFrame(false)
    .WithDrawShadows(false);
const IVStyle sectionControlStyle =
  style.WithLabelText(style.labelText.WithSize(kControlLabelTextSize)).WithValueText(style.valueText.WithSize(13.f));
const float kSectionKnobDiameter = 80.0f;
const float kSectionBypassX = 55.0f;
const float kSectionBypassWidth = 100.0f;
const float kSectionBypassHeight = 40.0f;
const float kCompactControlTextSize = 11.0f;
const IVStyle compactControlStyle = style.WithShowLabel(false)
                                      .WithDrawShadows(false)
                                      .WithValueText(style.valueText.WithSize(kCompactControlTextSize)
                                                       .WithVAlign(EVAlign::Bottom));
const IVStyle postEQTextStyle = DEFAULT_STYLE
                                  .WithValueText(IText(kControlLabelTextSize, PluginColors::NAM_THEMEFONTCOLOR,
                                                       "Roboto-Regular"))
                                  .WithDrawFrame(false)
                                  .WithDrawShadows(false);
const IVStyle postEQCutSliderStyle = compactControlStyle
                                       .WithColor(kFG, PluginColors::NAM_3)
                                       .WithColor(kPR, PluginColors::NAM_THEMECOLOR)
                                       .WithColor(kFR, PluginColors::OFF_WHITE)
                                       .WithFrameThickness(1.0f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;


NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 1, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);
  GetParam(kReverbIRBypass)->InitBool("ReverbIRBypass", true);
  GetParam(kReverbIRMix)->InitDouble("ReverbIRMix", 0.0, 0.0, 100.0, 0.1, "%");
  GetParam(kReverbIRPreDelay)->InitDouble("ReverbIRPreDelay", 0.0, 0.0, 200.0, 0.1, "ms");
  GetParam(kReverbIRLowCut)->InitDouble("ReverbIRLowCut", 20.0, 20.0, 1000.0, 1.0, "Hz");
  GetParam(kReverbIRHighCut)->InitDouble("ReverbIRHighCut", 20000.0, 1000.0, 20000.0, 1.0, "Hz");
  GetParam(kReverbIRWetLevel)->InitGain("ReverbIRWetLevel", 0.0, -24.0, 12.0, 0.1);
  _ApplyReverbIRParams();
  GetParam(kPreEQBypass)->InitBool("PreEQBypass", true);
  GetParam(kPreEQLowCut)->InitDouble("PreEQLowCut", 120.0, 20.0, 300.0, 1.0, "Hz");
  GetParam(kPreEQLowShelfGain)->InitGain("PreEQLowShelfGain", 0.0, -12.0, 12.0, 0.1);
  GetParam(kPreEQMidGain)->InitGain("PreEQMidGain", 0.0, -12.0, 12.0, 0.1);
  GetParam(kPreEQMidFrequency)->InitDouble("PreEQMidFrequency", 800.0, 150.0, 4000.0, 1.0, "Hz");
  GetParam(kPreEQHighShelfGain)->InitGain("PreEQHighShelfGain", 0.0, -12.0, 12.0, 0.1);
  _ApplyPreEQParams();
  GetParam(kPostEQBypass)->InitBool("PostEQBypass", true);
  GetParam(kPostEQLowCut)->InitDouble("PostEQLowCut", 20.0, 20.0, 500.0, 1.0, "Hz");
  GetParam(kPostEQBand1Frequency)->InitDouble("PostEQBand1Frequency", 100.0, 40.0, 400.0, 1.0, "Hz");
  GetParam(kPostEQBand1Gain)->InitGain("PostEQBand1Gain", 0.0, -18.0, 12.0, 0.1);
  GetParam(kPostEQBand1Q)->InitDouble("PostEQBand1Q", 1.0, 0.3, 6.0, 0.01);
  GetParam(kPostEQBand2Frequency)->InitDouble("PostEQBand2Frequency", 400.0, 120.0, 2000.0, 1.0, "Hz");
  GetParam(kPostEQBand2Gain)->InitGain("PostEQBand2Gain", 0.0, -18.0, 12.0, 0.1);
  GetParam(kPostEQBand2Q)->InitDouble("PostEQBand2Q", 1.0, 0.3, 6.0, 0.01);
  GetParam(kPostEQBand3Frequency)->InitDouble("PostEQBand3Frequency", 2500.0, 500.0, 7000.0, 1.0, "Hz");
  GetParam(kPostEQBand3Gain)->InitGain("PostEQBand3Gain", 0.0, -18.0, 12.0, 0.1);
  GetParam(kPostEQBand3Q)->InitDouble("PostEQBand3Q", 1.0, 0.3, 6.0, 0.01);
  GetParam(kPostEQBand4Frequency)->InitDouble("PostEQBand4Frequency", 7000.0, 2000.0, 14000.0, 1.0, "Hz");
  GetParam(kPostEQBand4Gain)->InitGain("PostEQBand4Gain", 0.0, -18.0, 12.0, 0.1);
  GetParam(kPostEQBand4Q)->InitDouble("PostEQBand4Q", 1.0, 0.3, 6.0, 0.01);
  GetParam(kPostEQHighCut)->InitDouble("PostEQHighCut", 20000.0, 3000.0, 20000.0, 1.0, "Hz");
  _ApplyPostEQParams();

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);
    const auto dangerLogoSVG = pGraphics->LoadSVG(DANGER_LOGO_FN);
    const auto dangerBackgroundSVG = pGraphics->LoadSVG(DANGER_BACKGROUND_FN);

    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 62.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);
    const auto logoArea = titleArea.GetFromLeft(44.0f).GetCentredInside(36.0f, 32.0f);
    const auto productTitleArea = titleArea.GetReducedFromLeft(48.0f).GetFromTop(38.0f);
    const auto productLegendArea = titleArea.GetReducedFromLeft(48.0f).GetFromBottom(17.0f);

    // Shared section-title geometry keeps headings clear of panel borders.
    const auto sectionTitleArea = [](const IRECT& section) {
      return IRECT(section.L, section.T + kSectionHeadingTopPadding, section.R,
                   section.T + kSectionHeadingTopPadding + kSectionHeadingHeight);
    };
    const auto topSectionArea = IRECT(45.0f, 82.0f, b.R - 45.0f, 260.0f);
    const auto inputKnobArea = IRECT(110.0f, 111.0f, 190.0f, 231.0f);
    const auto gateSwitchArea = IRECT(225.0f, 121.0f, 275.0f, 221.0f);
    const auto noiseGateArea = IRECT(310.0f, 111.0f, 390.0f, 231.0f);
    const auto outputKnobArea = IRECT(410.0f, 111.0f, 490.0f, 231.0f);
    const auto topSectionTitleArea = sectionTitleArea(topSectionArea);

    // Compact Pre-EQ panel. The logical render and hit rectangles are the
    // same, so scale-mode resizing preserves control alignment.
    const auto preEQSectionArea = IRECT(45.0f, 268.0f, b.R - 45.0f, 412.0f);
    const auto preEQTitleArea = sectionTitleArea(preEQSectionArea);
    const auto sectionBypassArea = [](const float top) {
      return IRECT(kSectionBypassX, top, kSectionBypassX + kSectionBypassWidth, top + kSectionBypassHeight);
    };
    const auto preEQBypassArea = sectionBypassArea(308.0f);
    const auto preEQKnobsArea = IRECT(155.0f, preEQTitleArea.B + kSectionHeadingBottomPadding,
                                      b.R - 45.0f, 412.0f);
    const auto preEQLowCutArea = preEQKnobsArea.GetGridCell(0, 0, 1, 5);
    const auto preEQLowShelfArea = preEQKnobsArea.GetGridCell(0, 1, 1, 5);
    const auto preEQMidFrequencyArea = preEQKnobsArea.GetGridCell(0, 2, 1, 5);
    const auto preEQMidGainArea = preEQKnobsArea.GetGridCell(0, 3, 1, 5);
    const auto preEQHighShelfArea = preEQKnobsArea.GetGridCell(0, 4, 1, 5);

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto irYOffset = 34.0f;
    const auto modelSectionArea = IRECT(45.0f, 420.0f, b.R - 45.0f, 520.0f);
    const auto modelSectionTitleArea = sectionTitleArea(modelSectionArea);
    const auto modelArea = IRECT(b.MW() - fileWidth * 0.5f, 450.0f, b.MW() + fileWidth * 0.5f, 480.0f);
    const auto slimIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 14.f, modelArea.R + 6.f + 2.f * 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Six-column Post-EQ panel: cut filters at the edges and one grouped
    // frequency/gain/Q column per parametric band.
    const auto postEQSectionArea = IRECT(45.0f, 528.0f, b.R - 45.0f, 778.0f);
    const auto postEQTitleArea = sectionTitleArea(postEQSectionArea);
    const auto postEQBypassArea = sectionBypassArea(530.0f);
    const auto postEQColumnsArea = IRECT(105.0f, 556.0f, b.R - 55.0f, 734.0f);
    const auto postEQBandLabelsArea = IRECT(postEQColumnsArea.L, 552.0f, postEQColumnsArea.R, 570.0f);
    const auto postEQFrequencyRow = IRECT(postEQColumnsArea.L, 570.0f, postEQColumnsArea.R, 650.0f);
    const auto postEQGainRow = IRECT(postEQColumnsArea.L, 650.0f, postEQColumnsArea.R, 734.0f);
    const auto postEQLowCutSliderArea = IRECT(45.0f, 745.0f, 285.0f, 776.0f);
    const auto postEQHighCutSliderArea = IRECT(315.0f, 745.0f, 555.0f, 776.0f);
    const auto primaryPostEQArea = [](const IRECT& row, const int column) {
      return row.GetGridCell(0, column, 1, 4).GetCentredInside(kSectionKnobDiameter, row.H());
    };

    // Compact Reverb IR panel. Render and hit rectangles use the same logical
    // bounds so scale-mode resizing preserves alignment.
    const auto reverbSectionArea = IRECT(45.0f, 786.0f, b.R - 45.0f, 968.0f);
    const auto reverbTitleArea = sectionTitleArea(reverbSectionArea);
    const auto reverbBrowserArea = IRECT(b.MW() - 100.0f, 812.0f, b.MW() + 100.0f, 842.0f);
    const auto reverbBypassArea = sectionBypassArea(812.0f);
    const auto reverbKnobsArea = IRECT(85.0f, 846.0f, b.R - 85.0f, 966.0f);
    const auto reverbMixArea = reverbKnobsArea.GetGridCell(0, 0, 1, 5);
    const auto reverbPreDelayArea = reverbKnobsArea.GetGridCell(0, 1, 1, 5);
    const auto reverbLowCutArea = reverbKnobsArea.GetGridCell(0, 2, 1, 5);
    const auto reverbHighCutArea = reverbKnobsArea.GetGridCell(0, 3, 1, 5);
    const auto reverbWetLevelArea = reverbKnobsArea.GetGridCell(0, 4, 1, 5);

    // Areas for meters
    const auto inputMeterArea = IRECT(10.0f, 82.0f, 40.0f, 260.0f);
    const auto outputMeterArea = IRECT(b.R - 40.0f, 82.0f, b.R - 10.0f, 260.0f);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = _StageModel(fileName);
        // TODO error messages like the IR loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        mIRPath = fileName;
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    auto loadReverbIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (!fileName.GetLength())
        return;

      const WDL_String previousReverbIRPath(mReverbIRPath);
      const dsp::wav::LoadReturnCode retCode = _StageReverbIR(fileName);
      if (retCode == dsp::wav::LoadReturnCode::SUCCESS)
      {
        SendControlMsgFromDelegate(kCtrlTagReverbIRFileBrowser, kMsgTagLoadedReverbIR,
                                   mReverbIRPath.GetLength(), mReverbIRPath.Get());
        return;
      }

      std::stringstream message;
      message << "Failed to load Reverb IR file " << fileName.Get() << ":\n";
      message << dsp::wav::GetMsgForLoadReturnCode(retCode);
      _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load Reverb IR!", kMB_OK);

      // Candidate validation failed before exchange, so keep the previous IR
      // active and restore its filename in the browser.
      if (previousReverbIRPath.GetLength())
      {
        mReverbIRLoadFailed = false;
        SendControlMsgFromDelegate(kCtrlTagReverbIRFileBrowser, kMsgTagLoadedReverbIR,
                                   previousReverbIRPath.GetLength(), previousReverbIRPath.Get());
      }
      else
      {
        SendControlMsgFromDelegate(kCtrlTagReverbIRFileBrowser, kMsgTagLoadFailed);
      }
    };

    pGraphics->AttachControl(new ISVGControl(b, dangerBackgroundSVG));
    pGraphics->AttachControl(new ISVGControl(logoArea, dangerLogoSVG));
    pGraphics->AttachControl(new IVLabelControl(productTitleArea, "DANGER GUITAR AMPS", titleStyle));
    pGraphics->AttachControl(
      new IVLabelControl(productLegendArea, "MODEL AMPLIFIER  //  IMPULSE CABINET", shellLegendStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));
    pGraphics->AttachControl(new IVLabelControl(topSectionTitleArea, "INPUT / GATE / OUTPUT", sectionHeadingStyle));
    pGraphics->AttachControl(new IVLabelControl(preEQTitleArea, "PRE-EQ", sectionHeadingStyle));
    pGraphics->AttachControl(new IVLabelControl(modelSectionTitleArea, "MODEL / SPEAKER IR", sectionHeadingStyle));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    const std::string defaultReverbIRString = "No Reverb IR Loaded";
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
      kCtrlTagModelFileBrowser);

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG), kCtrlTagSlimmableIcon)
      ->SetAnimationEndActionFunction(showSlimOverlay)
      ->Hide(true);

    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(new IVLabelControl(reverbTitleArea, "REVERB IR", sectionHeadingStyle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(reverbBrowserArea, kMsgTagClearReverbIR, defaultReverbIRString.c_str(), "wav",
                                loadReverbIRCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get IRs", getUrl),
      kCtrlTagReverbIRFileBrowser);
    pGraphics->AttachControl(
      new NAMSwitchControl(reverbBypassArea, kReverbIRBypass, "Bypass", sectionControlStyle,
                           switchHandleBitmap));
    pGraphics->AttachControl(
      new NAMSwitchControl(preEQBypassArea, kPreEQBypass, "Bypass", sectionControlStyle,
                           switchHandleBitmap));
    pGraphics->AttachControl(
      new NAMSwitchControl(postEQBypassArea, kPostEQBypass, "Bypass", sectionControlStyle,
                           switchHandleBitmap));
    pGraphics->AttachControl(
      new NAMSwitchControl(gateSwitchArea, kNoiseGateActive, "Gate", style, switchHandleBitmap,
                           EDirection::Vertical));

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));
    const auto attachPreEQKnob = [&](const IRECT& area, const int paramIdx, const char* label,
                                     const char* tooltip) {
      auto* control = new NAMKnobControl(area, paramIdx, label, sectionControlStyle, knobBackgroundBitmap);
      control->SetTooltip(tooltip);
      pGraphics->AttachControl(control, -1, "PRE_EQ_CONTROLS");
    };
    attachPreEQKnob(preEQLowCutArea, kPreEQLowCut, "Low Cut",
                    "Removes low frequencies before the amp model.");
    attachPreEQKnob(preEQLowShelfArea, kPreEQLowShelfGain, "Body",
                    "Shapes low-end weight and fullness before the amp model.");
    attachPreEQKnob(preEQMidFrequencyArea, kPreEQMidFrequency, "Mid Freq",
                    "Selects the frequency controlled by Mid Gain.");
    attachPreEQKnob(preEQMidGainArea, kPreEQMidGain, "Mid Gain",
                    "Cuts or boosts the selected mid frequency before the amp model.");
    attachPreEQKnob(preEQHighShelfArea, kPreEQHighShelfGain, "Attack",
                    "Shapes pick definition and upper-frequency drive into the amp model.");
    const bool preEQBypassed = GetParam(kPreEQBypass)->Bool();
    pGraphics->ForControlInGroup("PRE_EQ_CONTROLS",
                                 [preEQBypassed](IControl* pControl) { pControl->SetDisabled(preEQBypassed); });
    pGraphics->AttachControl(
      new IVSliderControl(postEQLowCutSliderArea, kPostEQLowCut, " ", postEQCutSliderStyle, true,
                          EDirection::Horizontal, DEFAULT_GEARING, 8.0f, 2.0f, true),
      -1, "POST_EQ_CONTROLS");
    pGraphics->AttachControl(
      new IVSliderControl(postEQHighCutSliderArea, kPostEQHighCut, " ", postEQCutSliderStyle, true,
                          EDirection::Horizontal, DEFAULT_GEARING, 8.0f, 2.0f, true),
      -1, "POST_EQ_CONTROLS");
    const std::array<int, 4> postEQFrequencyParams{kPostEQBand1Frequency, kPostEQBand2Frequency,
                                                   kPostEQBand3Frequency, kPostEQBand4Frequency};
    const std::array<int, 4> postEQGainParams{kPostEQBand1Gain, kPostEQBand2Gain, kPostEQBand3Gain,
                                              kPostEQBand4Gain};
    for (int band = 0; band < 4; ++band)
    {
      const int column = band;
      pGraphics->AttachControl(
        new NAMPostEQBitmapKnobControl(primaryPostEQArea(postEQFrequencyRow, column), postEQFrequencyParams[band],
                                       kSectionKnobDiameter,
                                       compactControlStyle, knobBackgroundBitmap),
        -1, "POST_EQ_CONTROLS");
      pGraphics->AttachControl(
        new NAMPostEQBitmapKnobControl(primaryPostEQArea(postEQGainRow, column), postEQGainParams[band],
                                       kSectionKnobDiameter, compactControlStyle, knobBackgroundBitmap),
        -1, "POST_EQ_CONTROLS");
    }
    // Typography is attached after all Post-EQ knob graphics.
    pGraphics->AttachControl(new IVLabelControl(postEQTitleArea, "POST-EQ", sectionHeadingStyle));
    const std::array<const char*, 4> postEQBandLabels{"Low", "Low Mid", "High Mid", "High"};
    for (int band = 0; band < 4; ++band)
    {
      pGraphics->AttachControl(new IVLabelControl(postEQBandLabelsArea.GetGridCell(0, band, 1, 4),
                                                  postEQBandLabels[band], postEQTextStyle));
    }
    pGraphics->AttachControl(new IVLabelControl(IRECT(25.0f, postEQGainRow.T, 100.0f, postEQGainRow.B), "Gain",
                                                postEQTextStyle));
    pGraphics->AttachControl(new IVLabelControl(IRECT(25.0f, postEQFrequencyRow.T, 100.0f,
                                                      postEQFrequencyRow.B),
                                                "Freq", postEQTextStyle));
    pGraphics->AttachControl(new IVLabelControl(IRECT(postEQLowCutSliderArea.L, 734.0f,
                                                      postEQLowCutSliderArea.R, postEQLowCutSliderArea.T),
                                                "Low Cut", postEQTextStyle));
    pGraphics->AttachControl(new IVLabelControl(IRECT(postEQHighCutSliderArea.L, 734.0f,
                                                      postEQHighCutSliderArea.R, postEQHighCutSliderArea.T),
                                                "High Cut", postEQTextStyle));
    const bool postEQBypassed = GetParam(kPostEQBypass)->Bool();
    pGraphics->ForControlInGroup("POST_EQ_CONTROLS",
                                 [postEQBypassed](IControl* pControl) { pControl->SetDisabled(postEQBypassed); });
    pGraphics->AttachControl(
      new NAMKnobControl(reverbMixArea, kReverbIRMix, "Mix", sectionControlStyle, knobBackgroundBitmap), -1,
      "REVERB_CONTROLS");
    pGraphics->AttachControl(new NAMKnobControl(reverbPreDelayArea, kReverbIRPreDelay, "Pre-Delay", sectionControlStyle,
                                                knobBackgroundBitmap),
                             -1, "REVERB_CONTROLS");
    pGraphics->AttachControl(new NAMKnobControl(reverbLowCutArea, kReverbIRLowCut, "Low Cut", sectionControlStyle,
                                                knobBackgroundBitmap),
                             -1, "REVERB_CONTROLS");
    pGraphics->AttachControl(new NAMKnobControl(reverbHighCutArea, kReverbIRHighCut, "High Cut", sectionControlStyle,
                                                knobBackgroundBitmap),
                             -1, "REVERB_CONTROLS");
    pGraphics->AttachControl(new NAMKnobControl(reverbWetLevelArea, kReverbIRWetLevel, "Wet Level", sectionControlStyle,
                                                knobBackgroundBitmap),
                             -1, "REVERB_CONTROLS");
    const bool reverbBypassed = GetParam(kReverbIRBypass)->Bool();
    pGraphics->ForControlInGroup("REVERB_CONTROLS",
                                 [reverbBypassed](IControl* pControl) { pControl->SetDisabled(reverbBypassed); });

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, dangerBackgroundSVG, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "Slim", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{
  _DeallocateIOPointers();
}

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  _PrepareBuffers(numChannelsInternal, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();

  sample** gateTriggerOutput =
    _ProcessGateTriggerStage(mInputPointers, numChannelsInternal, numFrames, sampleRate, noiseGateActive);
  sample** preEQOutput = mPreEQStage.Process(gateTriggerOutput, numChannelsInternal, numFrames);
  sample** compressorOutput = mCompressorStage.Process(preEQOutput, numChannelsInternal, numFrames);
  sample** namOutput = _ProcessNAMStage(compressorOutput, numChannelsInternal, numFrames);
  sample** gateGainOutput = _ProcessGateGainStage(namOutput, numChannelsInternal, numFrames, noiseGateActive);
  sample** speakerIROutput = _ProcessSpeakerIRStage(gateGainOutput, numChannelsInternal, numFrames);
  sample** postEQOutput = mPostEQStage.Process(speakerIROutput, numChannelsInternal, numFrames);
  sample** reverbIROutput = mReverbIRStage.Process(postEQOutput, numChannelsInternal, numFrames);
  sample** hpfPointers = _ProcessDCBlockerStage(reverbIROutput, numChannelsInternal, numFrames, sampleRate);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

sample** NeuralAmpModeler::_ProcessGateTriggerStage(sample** inputs, const size_t numChannels, const size_t numFrames,
                                                    const double sampleRate, const bool active)
{
  if (!active)
    return inputs;

  const double time = 0.01;
  const double threshold = GetParam(kNoiseGateThreshold)->Value();
  const double ratio = 0.1; // Quadratic...
  const double openTime = 0.005;
  const double holdTime = 0.01;
  const double closeTime = 0.05;
  const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
  mNoiseGateTrigger.SetParams(triggerParams);
  mNoiseGateTrigger.SetSampleRate(sampleRate);
  return mNoiseGateTrigger.Process(inputs, numChannels, numFrames);
}

sample** NeuralAmpModeler::_ProcessNAMStage(sample** inputs, const size_t numChannels, const size_t numFrames)
{
  if (mModel != nullptr)
    mModel->process(inputs, mOutputPointers, static_cast<int>(numFrames));
  else
    _FallbackDSP(inputs, mOutputPointers, numChannels, numFrames);

  return mOutputPointers;
}

sample** NeuralAmpModeler::_ProcessGateGainStage(sample** inputs, const size_t numChannels, const size_t numFrames,
                                                 const bool active)
{
  return active ? mNoiseGateGain.Process(inputs, numChannels, numFrames) : inputs;
}

sample** NeuralAmpModeler::_ProcessSpeakerIRStage(sample** inputs, const size_t numChannels, const size_t numFrames)
{
  return (mIR != nullptr && GetParam(kIRToggle)->Value()) ? mIR->Process(inputs, numChannels, numFrames) : inputs;
}

sample** NeuralAmpModeler::_ProcessDCBlockerStage(sample** inputs, const size_t numChannels, const size_t numFrames,
                                                  const double sampleRate)
{
  // HPF for DC offset (Issue 271).
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, kDCBlockerFrequency);
  mHighPass.SetParams(highPassParams);
  return mHighPass.Process(inputs, numChannels, numFrames);
}

void NeuralAmpModeler::OnReset()
{
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = GetBlockSize();

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mPreEQStage.Prepare(sampleRate, maxBlockSize);
  mPostEQStage.Prepare(sampleRate, maxBlockSize);
  mReverbIRStage.Prepare(sampleRate, maxBlockSize);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  if (mNewModelLoadedInDSP)
  {
    if (auto* pGraphics = GetUI())
    {
      _UpdateControlsFromModel();
      mNewModelLoadedInDSP = false;
    }
  }
  if (mModelCleared)
  {
    if (auto* pGraphics = GetUI())
    {
      // FIXME -- need to disable only the "normalized" model
      // pGraphics->GetControlWithTag(kCtrlTagOutputMode)->SetDisabled(false);
      static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->ClearModelInfo();
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        p->Hide(true);
      if (auto* p = pGraphics->GetControlWithTag(kCtrlTagSlimKnob))
        p->Hide(true);
      pGraphics->SetAllControlsDirty();
      mModelCleared = false;
    }
  }
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  // If this isn't here when unserializing, then we know we're dealing with something before v0.8.0.
  WDL_String header("###NeuralAmpModeler###"); // Don't change this!
  chunk.PutStr(header.Get());
  // Plugin version, so we can load legacy serialized states in the future!
  WDL_String version(PLUG_VERSION_STR);
  chunk.PutStr(version.Get());
  // Model directory (don't serialize the model itself; we'll just load it again
  // when we unserialize)
  chunk.PutStr(mNAMPath.Get());
  chunk.PutStr(mIRPath.Get());
  chunk.PutStr(mReverbIRPath.Get());
  return SerializeParams(chunk);
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  // Look for the expected header. If it's there, then we'll know what to do.
  WDL_String header;
  int pos = startPos;
  pos = chunk.GetStr(header, pos);

  const char* kExpectedHeader = "###NeuralAmpModeler###";
  if (strcmp(header.Get(), kExpectedHeader) == 0)
  {
    return _UnserializeStateWithKnownVersion(chunk, pos);
  }
  else
  {
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  Plugin::OnUIOpen();

  if (mNAMPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
    // If it's not loaded yet, then mark as failed.
    // If it's yet to be loaded, then the completion handler will set us straight once it runs.
    if (mModel == nullptr && mStagedModel == nullptr)
      SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
  }

  if (mIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
    if (mIR == nullptr && mStagedIR == nullptr)
      SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mReverbIRPath.GetLength())
  {
    SendControlMsgFromDelegate(kCtrlTagReverbIRFileBrowser, kMsgTagLoadedReverbIR, mReverbIRPath.GetLength(),
                               mReverbIRPath.Get());
    if (mReverbIRLoadFailed)
      SendControlMsgFromDelegate(kCtrlTagReverbIRFileBrowser, kMsgTagLoadFailed);
  }

  if (mModel != nullptr)
  {
    _UpdateControlsFromModel();
  }
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  switch (paramIdx)
  {
    // Changes to the input gain
    case kCalibrateInput:
    case kInputCalibrationLevel:
    case kInputLevel: _SetInputGain(); break;
    // Changes to the output gain
    case kOutputLevel:
    case kOutputMode: _SetOutputGain(); break;
    // Reserved legacy tone-stack parameters. Retained for host/session
    // compatibility, but intentionally inactive in the V1 signal chain.
    case kToneBass:
    case kToneMid:
    case kToneTreble:
    case kEQActive: break;
    case kSlim: _ApplySlimParamToLoadedNAMs(); break;
    case kReverbIRBypass:
    case kReverbIRMix:
    case kReverbIRPreDelay:
    case kReverbIRLowCut:
    case kReverbIRHighCut:
    case kReverbIRWetLevel: _ApplyReverbIRParams(); break;
    case kPreEQBypass:
    case kPreEQLowCut:
    case kPreEQLowShelfGain:
    case kPreEQMidGain:
    case kPreEQMidFrequency:
    case kPreEQHighShelfGain: _ApplyPreEQParams(); break;
    case kPostEQBypass:
    case kPostEQLowCut:
    case kPostEQBand1Frequency:
    case kPostEQBand1Gain:
    case kPostEQBand2Frequency:
    case kPostEQBand2Gain:
    case kPostEQBand3Frequency:
    case kPostEQBand3Gain:
    case kPostEQBand4Frequency:
    case kPostEQBand4Gain:
    case kPostEQHighCut: _ApplyPostEQParams(); break;
    // Reserved legacy Post-EQ Q parameters. V1 uses fixed per-band Q.
    case kPostEQBand1Q:
    case kPostEQBand2Q:
    case kPostEQBand3Q:
    case kPostEQBand4Q: break;
    default: break;
  }
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      case kReverbIRBypass:
        pGraphics->ForControlInGroup("REVERB_CONTROLS",
                                     [active](IControl* pControl) { pControl->SetDisabled(active); });
        break;
      case kPreEQBypass:
        pGraphics->ForControlInGroup("PRE_EQ_CONTROLS",
                                     [active](IControl* pControl) { pControl->SetDisabled(active); });
        break;
      case kPostEQBypass:
        pGraphics->ForControlInGroup("POST_EQ_CONTROLS",
                                     [active](IControl* pControl) { pControl->SetDisabled(active); });
        break;
      default: break;
    }
  }
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: mShouldRemoveModel = true; return true;
    case kMsgTagClearIR: mShouldRemoveIR = true; return true;
    case kMsgTagClearReverbIR:
      mReverbIRPath.Set("");
      mReverbIRLoadFailed = false;
      mReverbIRStage.RequestClear();
      return true;
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ApplyDSPStaging()
{
  mReverbIRStage.ApplyStaged();
  // Remove marked modules
  if (mShouldRemoveModel)
  {
    mModel = nullptr;
    mNAMPath.Set("");
    mShouldRemoveModel = false;
    mModelCleared = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mShouldRemoveIR)
  {
    mIR = nullptr;
    mIRPath.Set("");
    mShouldRemoveIR = false;
  }
  // Move things from staged to live
  if (mStagedModel != nullptr)
  {
    mModel = std::move(mStagedModel);
    mStagedModel = nullptr;
    mNewModelLoadedInDSP = true;
    _UpdateLatency();
    _SetInputGain();
    _SetOutputGain();
  }
  if (mStagedIR != nullptr)
  {
    mIR = std::move(mStagedIR);
    mStagedIR = nullptr;
  }
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_FallbackDSP(iplug::sample** inputs, iplug::sample** outputs, const size_t numChannels,
                                    const size_t numFrames)
{
  for (auto c = 0; c < numChannels; c++)
    for (auto s = 0; s < numFrames; s++)
      outputs[c][s] = inputs[c][s];
}

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  // Model
  if (mStagedModel != nullptr)
  {
    mStagedModel->Reset(sampleRate, maxBlockSize);
  }
  else if (mModel != nullptr)
  {
    mModel->Reset(sampleRate, maxBlockSize);
  }

  // IR
  if (mStagedIR != nullptr)
  {
    const double irSampleRate = mStagedIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mStagedIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
    }
  }
}

void NeuralAmpModeler::_SetInputGain()
{
  iplug::sample inputGainDB = GetParam(kInputLevel)->Value();
  // Input calibration
  if ((mModel != nullptr) && (mModel->HasInputLevel()) && GetParam(kCalibrateInput)->Bool())
  {
    inputGainDB += GetParam(kInputCalibrationLevel)->Value() - mModel->GetInputLevel();
  }
  mInputGain = DBToAmp(inputGainDB);
}

void NeuralAmpModeler::_SetOutputGain()
{
  double gainDB = GetParam(kOutputLevel)->Value();
  if (mModel != nullptr)
  {
    const int outputMode = GetParam(kOutputMode)->Int();
    switch (outputMode)
    {
      case 1: // Normalized
        if (mModel->HasLoudness())
        {
          const double loudness = mModel->GetLoudness();
          const double targetLoudness = -18.0;
          gainDB += (targetLoudness - loudness);
        }
        break;
      case 2: // Calibrated
        if (mModel->HasOutputLevel())
        {
          const double inputLevel = GetParam(kInputCalibrationLevel)->Value();
          const double outputLevel = mModel->GetOutputLevel();
          gainDB += (outputLevel - inputLevel);
        }
        break;
      case 0: // Raw
      default: break;
    }
  }
  mOutputGain = DBToAmp(gainDB);
}

void NeuralAmpModeler::_ApplySlimParamToLoadedNAMs()
{
  const double v = GetParam(kSlim)->Value();
  auto apply = [v](ResamplingNAM* p) {
    if (p == nullptr)
      return;
    if (nam::SlimmableModel* s = p->GetSlimmableModel())
      s->SetSlimmableSize(v);
  };
  apply(mModel.get());
  apply(mStagedModel.get());
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath)
{
  WDL_String previousNAMPath = mNAMPath;
  try
  {
    auto dspPath = std::filesystem::u8path(modelPath.Get());
    std::unique_ptr<nam::DSP> model = nam::get_dsp(dspPath);

    // Check that the model has 1 input and 1 output channel
    if (model->NumInputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 input channel, but has " + std::to_string(model->NumInputChannels()));
    }
    if (model->NumOutputChannels() != 1)
    {
      throw std::runtime_error("Model must have 1 output channel, but has "
                               + std::to_string(model->NumOutputChannels()));
    }

    std::unique_ptr<ResamplingNAM> temp = std::make_unique<ResamplingNAM>(std::move(model), GetSampleRate());
    temp->Reset(GetSampleRate(), GetBlockSize());
    if (nam::SlimmableModel* slimmable = temp->GetSlimmableModel())
    {
      slimmable->SetSlimmableSize(GetParam(kSlim)->Value());
    }
    mStagedModel = std::move(temp);
    mNAMPath = modelPath;
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength(), mNAMPath.Get());
  }
  catch (std::runtime_error& e)
  {
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);

    if (mStagedModel != nullptr)
    {
      mStagedModel = nullptr;
    }
    mNAMPath = previousNAMPath;
    std::cerr << "Failed to read DSP module" << std::endl;
    std::cerr << e.what() << std::endl;
    return e.what();
  }
  return "";
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  // FIXME it'd be better for the path to be "staged" as well. Just in case the
  // path and the model got caught on opposite sides of the fence...
  WDL_String previousIRPath = mIRPath;
  const double sampleRate = GetSampleRate();
  dsp::wav::LoadReturnCode wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    mStagedIR = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = mStagedIR->GetWavState();
  }
  catch (std::runtime_error& e)
  {
    wavState = dsp::wav::LoadReturnCode::ERROR_OTHER;
    std::cerr << "Caught unhandled exception while attempting to load IR:" << std::endl;
    std::cerr << e.what() << std::endl;
  }

  if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mIRPath = irPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    if (mStagedIR != nullptr)
    {
      mStagedIR = nullptr;
    }
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageReverbIR(const WDL_String& irPath)
{
  const auto result = mReverbIRStage.StageFile(irPath.Get(), GetSampleRate(), std::max(GetBlockSize(), 1));
  if (result == dsp::wav::LoadReturnCode::SUCCESS)
  {
    mReverbIRPath = irPath;
    mReverbIRLoadFailed = false;
    _ApplyReverbIRParams();
  }
  else
  {
    mReverbIRLoadFailed = true;
  }
  return result;
}

void NeuralAmpModeler::_ApplyReverbIRParams()
{
  const bool bypassed = GetParam(kReverbIRBypass)->Bool();
  const double wetMix = GetParam(kReverbIRMix)->Value() * 0.01;
  const double preDelaySeconds = GetParam(kReverbIRPreDelay)->Value() * 0.001;
  mReverbIRStage.SetBypassed(bypassed);
  mReverbIRStage.SetWetMix(wetMix);
  mReverbIRStage.SetPreDelaySeconds(preDelaySeconds);
  mReverbIRStage.SetWetFilterFrequencies(GetParam(kReverbIRLowCut)->Value(), GetParam(kReverbIRHighCut)->Value());
  mReverbIRStage.SetWetOutputGain(std::pow(10.0, GetParam(kReverbIRWetLevel)->Value() / 20.0));
}

void NeuralAmpModeler::_ApplyPreEQParams()
{
  mPreEQStage.SetBypassed(GetParam(kPreEQBypass)->Bool());
  mPreEQStage.SetHighPassFrequency(GetParam(kPreEQLowCut)->Value());
  mPreEQStage.SetLowShelfGain(GetParam(kPreEQLowShelfGain)->Value());
  mPreEQStage.SetMid(GetParam(kPreEQMidGain)->Value(), GetParam(kPreEQMidFrequency)->Value());
  mPreEQStage.SetHighShelfGain(GetParam(kPreEQHighShelfGain)->Value());
}

void NeuralAmpModeler::_ApplyPostEQParams()
{
  mPostEQStage.SetBypassed(GetParam(kPostEQBypass)->Bool());
  mPostEQStage.SetHighPassFrequency(GetParam(kPostEQLowCut)->Value());
  mPostEQStage.SetBand(0, GetParam(kPostEQBand1Frequency)->Value(), GetParam(kPostEQBand1Gain)->Value());
  mPostEQStage.SetBand(1, GetParam(kPostEQBand2Frequency)->Value(), GetParam(kPostEQBand2Gain)->Value());
  mPostEQStage.SetBand(2, GetParam(kPostEQBand3Frequency)->Value(), GetParam(kPostEQBand3Gain)->Value());
  mPostEQStage.SetBand(3, GetParam(kPostEQBand4Frequency)->Value(), GetParam(kPostEQBand4Gain)->Value());
  mPostEQStage.SetLowPassFrequency(GetParam(kPostEQHighCut)->Value());
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut)
{
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = mInputGain;
#ifndef APP_API
  gain /= (float)nChansIn;
#endif
  // Assume _PrepareBuffers() was already called
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = mOutputGain;
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (mModel == nullptr)
  {
    return;
  }
  if (auto* pGraphics = GetUI())
  {
    ModelInfo modelInfo;
    modelInfo.sampleRate.known = true;
    modelInfo.sampleRate.value = mModel->GetEncapsulatedSampleRate();
    modelInfo.inputCalibrationLevel.known = mModel->HasInputLevel();
    modelInfo.inputCalibrationLevel.value = mModel->HasInputLevel() ? mModel->GetInputLevel() : 0.0;
    modelInfo.outputCalibrationLevel.known = mModel->HasOutputLevel();
    modelInfo.outputCalibrationLevel.value = mModel->HasOutputLevel() ? mModel->GetOutputLevel() : 0.0;

    static_cast<NAMSettingsPageControl*>(pGraphics->GetControlWithTag(kCtrlTagSettingsBox))->SetModelInfo(modelInfo);

    const bool disableInputCalibrationControls = !mModel->HasInputLevel();
    pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(disableInputCalibrationControls);
    pGraphics->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(disableInputCalibrationControls);
    {
      auto* c = static_cast<OutputModeControl*>(pGraphics->GetControlWithTag(kCtrlTagOutputMode));
      c->SetNormalizedDisable(!mModel->HasLoudness());
      c->SetCalibratedDisable(!mModel->HasOutputLevel());
    }

    if (auto* pSlimIcon = pGraphics->GetControlWithTag(kCtrlTagSlimmableIcon))
    {
      const bool show = mModel->GetSlimmableModel() != nullptr;
      pSlimIcon->Hide(!show);
    }
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  int latency = 0;
  if (mModel)
  {
    latency += mModel->GetLatency();
  }
  // Other things that add latency here...

  // Feels weird to have to do this.
  if (GetLatency() != latency)
  {
    SetLatency(latency);
  }
}

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Unserialization.cpp"
