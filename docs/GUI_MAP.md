# Existing GUI map

This document records the inherited Neural Amp Modeler interface before the first Danger Guitar Amps visual-shell changes. It describes the UI only; DSP, parameters, model/IR loading, and serialization are outside this map.

## Canvas and graphics setup

- Framework: iPlug2 `IGraphics`; no JUCE or web view.
- Logical size: `PLUG_WIDTH = 600`, `PLUG_HEIGHT = 400`.
- Frame rate: `PLUG_FPS = 60`.
- Resize mode: `EUIResizerMode::Scale`, with a corner resizer and maximum size of four times the logical dimensions.
- Desktop scale: `1.0`; iOS uses `GetScaleForScreen(...) * 0.85`.
- Input features: text entry, mouse-over, tooltips, multi-touch.
- Root bounds: `(0, 0, 600, 400)`.
- Main area: root padded by 20, `(20, 20, 580, 380)`.
- Content area: main area padded by 10, `(30, 30, 570, 370)`.
- Title area: top 50 logical pixels of the content area.

The inherited root is drawn with `Background.jpg` through `AttachBackground()`, followed by a full-window `Lines.png` bitmap overlay.

## Fonts and styles

Two bundled TrueType fonts are loaded by logical resource name:

- `Roboto-Regular.ttf` as `Roboto-Regular`: general labels, values, buttons, settings, and help text.
- `Michroma-Regular.ttf` as `Michroma-Regular`: 30 px main and settings titles.

The inherited palette is defined in `Colors.h`:

- `NAM_1`: raisin black `(29, 26, 31)`.
- `NAM_2` / `NAM_THEMECOLOR`: azure `(80, 133, 232)`.
- `NAM_3`: blue-grey `(162, 178, 191)`.
- `NAM_THEMEFONTCOLOR`: near-white `(242, 242, 242)`.
- Red is reserved for meter clipping.

The main `IVStyle` displays both labels and values. Knob label/value text is `DEFAULT_TEXT_SIZE + 3`; the title uses Michroma at 30 px. Radio buttons derive their on/off colours from the theme colour.

## Main-page geometry and controls

### Title and decorative layers

- Full-window background: `Background.jpg` plus `@2x` and `@3x` variants.
- Full-window line overlay: `Lines.png` plus `@2x` and `@3x` variants.
- Visible title: `NEURAL AMP MODELER` in the 50 px title area.
- Model icon: an SVG control placed to the left of the model browser.

### Six-knob row

`NAM_KNOB_HEIGHT` is 120. The row is the top 120 px of the content area, inset horizontally by 20 and shifted down by 75. It is divided into six equal grid cells in parameter-enum order. Each `NAMKnobControl` draws a bitmap knob, vector indicator track, value, and label.

| Position | Binding | Parameter name/range | Visible label source |
| --- | --- | --- | --- |
| 1 | `kInputLevel` (0) | Input, -20 to +20 dB | parameter name `Input` |
| 2 | `kNoiseGateThreshold` (1) | Threshold, -100 to 0 dB | parameter name `Threshold` |
| 3 | `kToneBass` (2) | Bass, 0 to 10 | parameter name `Bass` |
| 4 | `kToneMid` (3) | Middle, 0 to 10 | parameter name `Middle` |
| 5 | `kToneTreble` (4) | Treble, 0 to 10 | parameter name `Treble` |
| 6 | `kOutputLevel` (5) | Output, -40 to +40 dB | parameter name `Output` |

The Bass, Middle, and Treble controls belong to the `EQ_KNOBS` control group. No explicit control tags are assigned to the six main knobs; each is linked directly by parameter index.

### Switches

- Noise-gate switch: below the threshold knob; bound to `kNoiseGateActive`; label `Noise Gate`.
- EQ switch: below the middle knob; bound to `kEQActive`; label `EQ`.
- Both use `NAMSwitchControl` and the `SlideSwitchHandle` raster family.

### Model and IR browsers

Both browsers are 200×30 logical pixels and centred horizontally near the bottom of the content area.

- Model browser: first row, shifted up by 1 px; control tag `kCtrlTagModelFileBrowser`; clear message `kMsgTagClearModel`; extension `nam`; completion handler calls `_StageModel()`.
- IR browser: model row shifted down by 38 px; control tag `kCtrlTagIRFileBrowser`; clear message `kMsgTagClearIR`; extension `wav`; completion handler calls `_StageIR()`.
- IR enable SVG switch: bound directly to `kIRToggle` and positioned to the left of the IR browser.
- Each browser retains load, previous, next, filename/menu, clear, and external-resource actions.
- Delegate messages `kMsgTagLoadFailed`, `kMsgTagLoadedModel`, and `kMsgTagLoadedIR` update browser state and labels.

The browser background is the `FileBackground` raster family. Child buttons use `File.svg`, `Cross.svg`, `ArrowLeft.svg`, `ArrowRight.svg`, and `Globe.svg`.

### Slimmable-model affordance

- `SlimmableIcon.svg` appears to the right of the model browser only when a loaded model supports slimming; tag `kCtrlTagSlimmableIcon`.
- Activating it reveals a full-window dim backdrop (`kCtrlTagSlimOverlayBackdrop`) and a centred `NAMKnobControl` bound to `kSlim` (`kCtrlTagSlimKnob`).
- Clicking the backdrop dismisses the overlay.

### Meters and settings

- Input meter: narrow vertical region at the left, tag `kCtrlTagInputMeter`; fed by `mInputSender` from `ProcessBlock()`.
- Output meter: matching region at the right, tag `kCtrlTagOutputMeter`; fed by `mOutputSender` from `ProcessBlock()`.
- Settings button: 20×20 centred inside a 50×50 top-right corner area; uses `Gear.svg`.
- Meter visuals use the `MeterBackground` raster family, theme fill, and red clip indication.

## Settings page

`NAMSettingsPageControl` covers the full window and initially remains hidden. It reuses the root background bitmap and includes:

- `SETTINGS` title and close button.
- Input calibration level editor bound to `kInputCalibrationLevel`, tag `kCtrlTagInputCalibrationLevel`.
- `Calibrate Input` switch bound to `kCalibrateInput`, tag `kCtrlTagCalibrateInput`.
- Output-mode radio control bound to `kOutputMode`, tag `kCtrlTagOutputMode`; enum values remain Raw, Normalized, and Calibrated.
- Model-information text, about/version text, links, and third-party-notice access.

The controls above are conditionally disabled from model metadata, but their parameter identities do not change.

## Parameter and control-tag invariants

The parameter enum order is:

1. `kInputLevel`
2. `kNoiseGateThreshold`
3. `kToneBass`
4. `kToneMid`
5. `kToneTreble`
6. `kOutputLevel`
7. `kNoiseGateActive`
8. `kEQActive`
9. `kIRToggle`
10. `kCalibrateInput`
11. `kInputCalibrationLevel`
12. `kOutputMode`
13. `kSlim`

The control tags are model browser, IR browser, input meter, output meter, settings box, output mode, calibrate input, input calibration level, slimmable icon, slim backdrop, and slim knob, in that order. The visual-shell stage must preserve both enum orders and all direct parameter bindings.

## Asset inventory

### Raster families

| Base resource | 1× | 2× | 3× |
| --- | --- | --- | --- |
| Background | 600×400 JPG | 1200×800 JPG | 1800×1200 JPG |
| FileBackground | 427×33 PNG | 866×67 PNG | 1293×100 PNG |
| InputLevelBackground | 212×33 PNG | 432×67 PNG | 646×100 PNG |
| KnobBackground | 92×92 PNG | 188×188 PNG | 280×280 PNG |
| Lines | 594×396 PNG | 1206×804 PNG | 1800×1200 PNG |
| MeterBackground | 28×215 PNG | 57×437 PNG | 85×652 PNG |
| SlideSwitchHandle | 18×18 PNG | 36×36 PNG | 54×54 PNG |

### SVG resources

| File | Declared size/view box |
| --- | --- |
| `ArrowLeft.svg` | view box 800×800 |
| `ArrowRight.svg` | view box 800×800 |
| `Cross.svg` | declared 800×800, view box 24×24 |
| `File.svg` | declared 800×800, view box 24×24 |
| `Gear.svg` | declared 500×500 |
| `Globe.svg` | declared/view box 800×800 |
| `IRIconOff.svg` | view box 63×62 |
| `IRIconOn.svg` | view box 63×62 |
| `ModelIcon.svg` | view box 121×36 |
| `SlimmableIcon.svg` | declared/view box 128×64 |

Windows embeds fonts, SVGs, and raster scale variants through `resources/main.rc` and identifiers in `resources/resource.h`. `config.h` maps logical resource filenames used by IGraphics. The old assets must remain until replacement resources build and load successfully.
