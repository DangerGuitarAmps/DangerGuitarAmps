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
const float kControlLabelTextSize = 14.0f;
const float kSectionHeadingTextSize = 13.0f;
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
const IVStyle titleStyle = DEFAULT_STYLE.WithValueText(IText(23, PluginColors::OFF_WHITE, "Michroma-Regular"))
                             .WithDrawFrame(false)
                             .WithShadowOffset(1.f);
const IVStyle shellLegendStyle = DEFAULT_STYLE.WithValueText(IText(9, PluginColors::NAM_3, "Roboto-Regular"))
                                   .WithDrawFrame(false);
const IVStyle sectionHeadingStyle =
  DEFAULT_STYLE.WithValueText(IText(kSectionHeadingTextSize, PluginColors::OFF_WHITE, "Roboto-Regular"))
    .WithDrawFrame(false)
    .WithDrawShadows(false);
const IVStyle sectionControlStyle =
  style.WithLabelText(style.labelText.WithSize(kControlLabelTextSize)).WithValueText(style.valueText.WithSize(11.f));
const float kSectionKnobDiameter = 80.0f;
const float kSectionBypassX = 55.0f;
const float kSectionBypassWidth = 100.0f;
const float kSectionBypassHeight = 40.0f;
const float kCompactControlTextSize = 10.0f;
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
  GetParam(kGraphicEQ62HzGain)->InitGain("Post EQ 62.5 Hz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ125HzGain)->InitGain("Post EQ 125 Hz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ250HzGain)->InitGain("Post EQ 250 Hz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ500HzGain)->InitGain("Post EQ 500 Hz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ1kHzGain)->InitGain("Post EQ 1 kHz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ2kHzGain)->InitGain("Post EQ 2 kHz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ4kHzGain)->InitGain("Post EQ 4 kHz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ8kHzGain)->InitGain("Post EQ 8 kHz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQ16kHzGain)->InitGain("Post EQ 16 kHz", 0.0, -12.0, 12.0, 0.1);
  GetParam(kGraphicEQPostLevel)->InitGain("Post Level", 0.0, -12.0, 6.0, 0.1);
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
    constexpr float kDesignWidth = 600.0f;
    constexpr float kDesignHeight = 800.0f;
    const float scaleX = b.W() / kDesignWidth;
    const float scaleY = b.H() / kDesignHeight;
    const auto X = [scaleX](const float value) { return value * scaleX; };
    const auto Y = [scaleY](const float value) { return value * scaleY; };
    const auto R = [scaleX, scaleY](const float left, const float top, const float right, const float bottom) {
      return IRECT(left * scaleX, top * scaleY, right * scaleX, bottom * scaleY);
    };

    const auto mainArea = b.GetPadded(-X(16.0f));
    const auto contentArea = mainArea.GetPadded(-X(8.0f));
    const auto titleHeight = Y(62.0f);
    const auto titleArea = contentArea.GetFromTop(titleHeight);
    const auto logoArea = titleArea.GetFromLeft(X(44.0f)).GetCentredInside(X(36.0f), Y(32.0f));
    const auto productTitleArea = titleArea.GetReducedFromLeft(X(48.0f)).GetFromTop(Y(38.0f));
    const auto productLegendArea = titleArea.GetReducedFromLeft(X(48.0f)).GetFromBottom(Y(17.0f));

    // Shared section-title geometry keeps headings clear of panel borders.
    const auto sectionTitleArea = [scaleY](const IRECT& section) {
      return IRECT(section.L, section.T + kSectionHeadingTopPadding * scaleY, section.R,
                   section.T + (kSectionHeadingTopPadding + kSectionHeadingHeight) * scaleY);
    };
    const auto topSectionArea = R(45.0f, 68.0f, 555.0f, 210.0f);
    const auto inputKnobArea = R(110.0f, 87.0f, 190.0f, 199.0f);
    const auto gateSwitchArea = R(225.0f, 92.0f, 275.0f, 194.0f);
    const auto noiseGateArea = R(310.0f, 87.0f, 390.0f, 199.0f);
    const auto outputKnobArea = R(410.0f, 87.0f, 490.0f, 199.0f);
    const auto topSectionTitleArea = sectionTitleArea(topSectionArea);

    // Compact Pre-EQ panel. The logical render and hit rectangles are the
    // same, so scale-mode resizing preserves control alignment.
    const auto preEQSectionArea = R(45.0f, 216.0f, 555.0f, 350.0f);
    const auto preEQTitleArea = sectionTitleArea(preEQSectionArea);
    const auto sectionBypassArea = [&R](const float top) {
      return R(kSectionBypassX, top, kSectionBypassX + kSectionBypassWidth, top + kSectionBypassHeight);
    };
    const auto preEQBypassArea = sectionBypassArea(253.0f);
    const auto preEQKnobsArea = IRECT(X(155.0f), preEQTitleArea.B + Y(kSectionHeadingBottomPadding),
                                      X(555.0f), Y(350.0f));
    const auto preEQLowCutArea = preEQKnobsArea.GetGridCell(0, 0, 1, 5);
    const auto preEQLowShelfArea = preEQKnobsArea.GetGridCell(0, 1, 1, 5);
    const auto preEQMidFrequencyArea = preEQKnobsArea.GetGridCell(0, 2, 1, 5);
    const auto preEQMidGainArea = preEQKnobsArea.GetGridCell(0, 3, 1, 5);
    const auto preEQHighShelfArea = preEQKnobsArea.GetGridCell(0, 4, 1, 5);

    // Areas for model and IR
    const auto fileWidth = X(200.0f);
    const auto irYOffset = Y(34.0f);
    const auto modelSectionArea = R(45.0f, 356.0f, 555.0f, 450.0f);
    const auto modelSectionTitleArea = sectionTitleArea(modelSectionArea);
    const auto modelArea = IRECT(b.MW() - fileWidth * 0.5f, Y(382.0f), b.MW() + fileWidth * 0.5f, Y(412.0f));
    const auto slimIconArea =
      IRECT(modelArea.R + X(6.f), modelArea.MH() - Y(14.f), modelArea.R + X(62.f), modelArea.MH() + Y(14.f));
    const auto modelIconArea = modelArea.GetFromLeft(X(30)).GetTranslated(-X(40), Y(10));
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irFormatArea = IRECT(irArea.R + X(5.0f), irArea.T, irArea.R + X(55.0f), irArea.B);
    const auto irSwitchArea = irArea.GetFromLeft(X(30.0f)).GetHShifted(-X(40.0f)).GetScaledAboutCentre(0.6f);

    // Nine fixed-frequency bands and a post-EQ level occupy one compact rack panel.
    const auto postEQSectionArea = R(45.0f, 456.0f, 555.0f, 790.0f);
    const auto postEQTitleArea = sectionTitleArea(postEQSectionArea);
    const auto postEQBypassArea = sectionBypassArea(474.0f);
    const auto postEQFlatArea = R(472.0f, 474.0f, 535.0f, 504.0f);
    const auto graphicEQArea = R(72.0f, 510.0f, 470.0f, 770.0f);
    const auto postLevelArea = R(482.0f, 510.0f, 535.0f, 770.0f);

    // Areas for meters
    const auto inputMeterArea = R(10.0f, 68.0f, 40.0f, 210.0f);
    const auto outputMeterArea = R(560.0f, 68.0f, 590.0f, 210.0f);

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
    pGraphics->AttachControl(new IVLabelControl(irFormatArea, "", shellLegendStyle), kCtrlTagIRFormatIndicator);
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
    const std::array<int, 9> graphicEQParams{
      kGraphicEQ62HzGain, kGraphicEQ125HzGain, kGraphicEQ250HzGain, kGraphicEQ500HzGain, kGraphicEQ1kHzGain,
      kGraphicEQ2kHzGain, kGraphicEQ4kHzGain, kGraphicEQ8kHzGain, kGraphicEQ16kHzGain};
    const std::array<const char*, 9> graphicEQLabels{"62.5", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
    for (int band = 0; band < 9; ++band)
    {
      const auto cell = graphicEQArea.GetGridCell(0, band, 1, 9).GetPadded(-X(3.0f));
      pGraphics->AttachControl(new NAMGraphicEQSliderControl(cell, graphicEQParams[band], graphicEQLabels[band],
                                                             postEQCutSliderStyle),
                               -1, "POST_EQ_CONTROLS");
    }
    pGraphics->AttachControl(
      new IVSliderControl(postLevelArea, kGraphicEQPostLevel, "Level", postEQCutSliderStyle, true,
                          EDirection::Vertical, DEFAULT_GEARING, 8.0f, 3.0f, true),
      -1, "POST_EQ_CONTROLS");
    const std::array<int, 10> graphicEQFlatParams{
      kGraphicEQ62HzGain, kGraphicEQ125HzGain, kGraphicEQ250HzGain, kGraphicEQ500HzGain, kGraphicEQ1kHzGain,
      kGraphicEQ2kHzGain, kGraphicEQ4kHzGain, kGraphicEQ8kHzGain, kGraphicEQ16kHzGain, kGraphicEQPostLevel};
    pGraphics->AttachControl(new IVButtonControl(
      postEQFlatArea,
      [graphicEQFlatParams](IControl* pCaller) {
        auto* delegate = pCaller->GetDelegate();
        for (const int paramIdx : graphicEQFlatParams)
        {
          delegate->BeginInformHostOfParamChangeFromUI(paramIdx);
          const double normalizedZero = paramIdx == kGraphicEQPostLevel ? 2.0 / 3.0 : 0.5;
          delegate->SendParameterValueFromUI(paramIdx, normalizedZero);
          delegate->EndInformHostOfParamChangeFromUI(paramIdx);
        }
      }, "FLAT", compactControlStyle));
    pGraphics->AttachControl(new IVLabelControl(postEQTitleArea, "POST-EQ", sectionHeadingStyle));
    const bool postEQBypassed = GetParam(kPostEQBypass)->Bool();
    pGraphics->ForControlInGroup("POST_EQ_CONTROLS",
                                 [postEQBypassed](IControl* pControl) { pControl->SetDisabled(postEQBypassed); });

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

    const auto slimKnobArea = b.GetCentredInside(X(100.f), Y(NAM_KNOB_HEIGHT + 24.f));
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
  const size_t numChannelsNAM = kNumChannelsNAM;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  // Disable floating point denormals
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();

  // NAM remains mono, but downstream IR stages can become stereo. Allocate
  // both working channels before processing so the audio callback never has to
  // grow a buffer when a staged stereo IR becomes active.
  _PrepareBuffers(kMaximumInternalChannels, numFrames);
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsNAM);
  _ApplyDSPStaging();
  const size_t downstreamChannels = mIR != nullptr && mIR->GetNumIRChannels() == 2 ? 2 : 1;
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();

  sample** gateTriggerOutput =
    _ProcessGateTriggerStage(mInputPointers, numChannelsNAM, numFrames, sampleRate, noiseGateActive);
  sample** preEQOutput = mPreEQStage.Process(gateTriggerOutput, numChannelsNAM, numFrames);
  sample** compressorOutput = mCompressorStage.Process(preEQOutput, numChannelsNAM, numFrames);
  sample** namOutput = _ProcessNAMStage(compressorOutput, numChannelsNAM, numFrames);
  sample** gateGainOutput = _ProcessGateGainStage(namOutput, numChannelsNAM, numFrames, noiseGateActive);
  sample** irInput = _ExpandNAMOutputForIR(gateGainOutput, numFrames, downstreamChannels);
  sample** speakerIROutput = _ProcessSpeakerIRStage(irInput, downstreamChannels, numFrames);
  sample** postEQOutput = mPostEQStage.Process(speakerIROutput, downstreamChannels, numFrames);
  sample** hpfPointers = _ProcessDCBlockerStage(postEQOutput, downstreamChannels, numFrames, sampleRate);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // This is where we exit mono for whatever the output requires.
  _ProcessOutput(hpfPointers, outputs, numFrames, downstreamChannels, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsNAM, numChannelsExternalOut);
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

sample** NeuralAmpModeler::_ExpandNAMOutputForIR(sample** inputs, const size_t numFrames, const size_t numChannels)
{
  if (numChannels == 1)
    return inputs;
  for (size_t frame = 0; frame < numFrames; ++frame)
    mOutputPointers[1][frame] = inputs[0][frame];
  return mOutputPointers;
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
  _PrepareBuffers(kMaximumInternalChannels, std::max(maxBlockSize, 1));
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, GetBlockSize());
  mPreEQStage.Prepare(sampleRate, maxBlockSize);
  mPostEQStage.Prepare(sampleRate, maxBlockSize);
  _UpdateLatency();
}

void NeuralAmpModeler::OnIdle()
{
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);

  // Format labels are editor-only state. Keep all IGraphics access off the
  // real-time thread and show no format when no file is loaded.
  if (auto* pGraphics = GetUI())
  {
    if (auto* cabinet = pGraphics->GetControlWithTag(kCtrlTagIRFormatIndicator))
      cabinet->As<IVLabelControl>()->SetStr((mIR != nullptr || mStagedIR != nullptr)
                                             ? (mIRChannelFormat.load(std::memory_order_relaxed) == 2 ? "STEREO" : "MONO")
                                             : "");
  }

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
  const bool paramsSerialized = SerializeParams(chunk);
  WDL_String formatMarker("DangerIRFormatV1");
  chunk.PutStr(formatMarker.Get());
  const int cabinetChannels = mIRChannelFormat.load(std::memory_order_relaxed);
  const int reverbChannels = 1; // Legacy extension field retained for old readers.
  chunk.Put(&cabinetChannels);
  chunk.Put(&reverbChannels);
  return paramsSerialized;
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
    case kReverbIRWetLevel: break; // Reserved legacy Reverb IDs; intentionally inactive.
    case kPreEQBypass:
    case kPreEQLowCut:
    case kPreEQLowShelfGain:
    case kPreEQMidGain:
    case kPreEQMidFrequency:
    case kPreEQHighShelfGain: _ApplyPreEQParams(); break;
    case kPostEQLowCut:
    case kPostEQBand1Frequency:
    case kPostEQBand1Gain:
    case kPostEQBand2Frequency:
    case kPostEQBand2Gain:
    case kPostEQBand3Frequency:
    case kPostEQBand3Gain:
    case kPostEQBand4Frequency:
    case kPostEQBand4Gain:
    case kPostEQHighCut:
    // Reserved legacy Post-EQ Q parameters. V1 uses fixed per-band Q.
    case kPostEQBand1Q:
    case kPostEQBand2Q:
    case kPostEQBand3Q:
    case kPostEQBand4Q: break;
    case kGraphicEQ62HzGain:
    case kGraphicEQ125HzGain:
    case kGraphicEQ250HzGain:
    case kGraphicEQ500HzGain:
    case kGraphicEQ1kHzGain:
    case kGraphicEQ2kHzGain:
    case kGraphicEQ4kHzGain:
    case kGraphicEQ8kHzGain:
    case kGraphicEQ16kHzGain:
    case kGraphicEQPostLevel:
    case kPostEQBypass: _ApplyPostEQParams(); break;
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
    case kMsgTagClearIR:
    {
      std::lock_guard<std::mutex> lock(mSpeakerIRExchangeMutex);
      mRetiredIR.reset();
      mShouldRemoveIR = true;
      return true;
    }
    case kMsgTagClearReverbIR:
      mReverbIRPath.Set("");
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

  if (mSpeakerIRExchangeMutex.try_lock())
  {
    if (mShouldRemoveIR)
    {
      mRetiredIR = std::move(mIR);
      mIRPath.Set("");
      mIRChannelFormat = 1;
      mShouldRemoveIR = false;
    }
    if (mStagedIR != nullptr)
    {
      mRetiredIR = std::move(mIR);
      mIRChannelFormat = static_cast<int>(mStagedIR->GetNumIRChannels());
      mIR = std::move(mStagedIR);
    }
    mSpeakerIRExchangeMutex.unlock();
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
      mStagedIR->Prepare(static_cast<size_t>(std::max(maxBlockSize, 1)));
    }
  }
  else if (mIR != nullptr)
  {
    const double irSampleRate = mIR->GetSampleRate();
    if (irSampleRate != sampleRate)
    {
      const auto irData = mIR->GetData();
      mStagedIR = std::make_unique<dsp::ImpulseResponse>(irData, sampleRate);
      mStagedIR->Prepare(static_cast<size_t>(std::max(maxBlockSize, 1)));
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
  int candidateChannels = 1;
  try
  {
    auto irPathU8 = std::filesystem::u8path(irPath.Get());
    auto candidate = std::make_unique<dsp::ImpulseResponse>(irPathU8.string().c_str(), sampleRate);
    wavState = candidate->GetWavState();
    if (wavState == dsp::wav::LoadReturnCode::SUCCESS)
    {
      candidateChannels = static_cast<int>(candidate->GetNumIRChannels());
      candidate->Prepare(static_cast<size_t>(std::max(GetBlockSize(), 1)));
      std::lock_guard<std::mutex> lock(mSpeakerIRExchangeMutex);
      mRetiredIR.reset();
      mStagedIR = std::move(candidate);
    }
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
    mIRChannelFormat = candidateChannels;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength(), mIRPath.Get());
  }
  else
  {
    mIRPath = previousIRPath;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  }

  return wavState;
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
  const std::array<int, danger::signal_chain::GraphicEQStage::kNumBands> params{
    kGraphicEQ62HzGain, kGraphicEQ125HzGain, kGraphicEQ250HzGain, kGraphicEQ500HzGain, kGraphicEQ1kHzGain,
    kGraphicEQ2kHzGain, kGraphicEQ4kHzGain, kGraphicEQ8kHzGain, kGraphicEQ16kHzGain};
  for (std::size_t band = 0; band < params.size(); ++band)
    mPostEQStage.SetBandGain(band, GetParam(params[band])->Value());
  mPostEQStage.SetPostLevelDB(GetParam(kGraphicEQPostLevel)->Value());
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
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() < numFrames);
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
  if (nChansIn < 1 || nChansIn > kMaximumInternalChannels)
    throw std::runtime_error("Invalid internal channel count.");
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
    {
      if (cout >= static_cast<int>(kMaximumInternalChannels))
      {
        outputs[cout][s] = 0.0;
        continue;
      }
      const size_t cin = nChansIn == 1 ? 0 : std::min(static_cast<size_t>(cout), nChansIn - 1);
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
    }
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
