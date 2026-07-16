# Danger Guitar Amps approved UI implementation specification

## Status and authority

This document is the implementation contract for the approved Danger Guitar
Amps main interface. It specifies presentation and UI composition only. It does
not authorize changes to audio behavior, parameter identity, routing, state,
serialization, or dependencies.

The visual source of truth is:

- Reference: `C:\Dev\GuitarPlugin\danger-fidelity-final.png`
- SHA-256: `E25CFF17E8A55D5281BCF637050DCCC081B9ADB41E66ED2E1A548D40C2106A4A`
- Full image: 1920 x 1080 px
- Exact plug-in crop in that image: `[242, 156, 716, 946)`
- Cropped reference size: 474 x 790 px
- Approved design coordinate system: 600 x 1000 logical px
- Reference render scale: 0.79 physical px per logical px on both axes

Coordinates in this document are `[left, top, right, bottom)` in the 600 x
1000 logical design space. Right and bottom are exclusive. If a detail in this
document conflicts with the reference image, the reference image wins for
appearance and this document wins for behavior and scope.

The checked-in `DangerRackBackground.svg` and `DangerLogo.svg` identify
themselves as placeholders. They are not visual authorities. The current
600 x 800 layout in `NeuralAmpModeler.cpp` is useful evidence for bindings and
interaction behavior, but it is not the approved composition.

## Non-negotiable scope boundary

An implementation of this specification may change only UI layout, drawing,
UI-only control classes, UI resource declarations, artwork, fonts, and the
plug-in window dimensions required to present the approved 600 x 1000 canvas.

It must not:

- change `EParams`, parameter order, IDs, names, ranges, defaults, units, or
  automation behavior;
- change `ECtrlTags`, `EMsgTags`, their order, or their meanings;
- change `ProcessBlock()`, any signal-chain stage, channel layout, latency,
  meter sender source, or processing order;
- change model or IR loading, clearing, staging, iteration, or failure behavior;
- change `SerializeState()`, `UnserializeState()`, migration logic, state keys,
  external-file paths, or recall behavior;
- expose, remove, repurpose, or reorder reserved/deprecated parameters;
- change product identity, bundle identity, manufacturer identity, installer
  identity, or shared-resource paths;
- modify anything under `AudioDSPTools/` or `NeuralAmpModelerCore/`;
- add JUCE, a web view, another graphics framework, or a new runtime dependency.

The implementation remains iPlug2 `IGraphics`. Existing control bindings are
moved and reskinned, not replaced with new parameters or messages.

## Visual target at a glance

The approved interface is a narrow, vertically stacked industrial rack. It has
seven visual modules in signal-flow order:

1. branded header;
2. input and gate;
3. pre-EQ;
4. NAM amp model;
5. cabinet IR;
6. nine-band graphic post-EQ;
7. output and output meter.

The layout must read top-to-bottom. Do not combine the NAM amp and cabinet IR
into one row, put Output back in the Input/Gate row, or return to the shorter
600 x 800 arrangement.

## Canvas, scale, and resizing

- Design canvas: exactly 600 x 1000 logical px.
- The implementation target is `PLUG_WIDTH = 600` and `PLUG_HEIGHT = 1000`;
  retain the existing four-times maximum-size policy.
- Root origin: `(0, 0)`; no layout calculation may depend on host chrome.
- Use one uniform scale for X and Y. Do not independently stretch the axes.
- The approved screenshot is the design rendered at exactly 0.79x, producing
  474 x 790 physical px. Its geometry proves the intended 3:5 aspect ratio.
- Keep `EUIResizerMode::Scale`. Scaling must preserve the 3:5 aspect ratio.
- The background, hit rectangles, labels, and controls must use the same
  transform so rendering and pointer targets cannot drift apart.
- Pixel snapping: snap one-pixel strokes and fader index lines to device-pixel
  centers after scaling. Filled rectangles may land on whole device pixels.
- Minimum supported visual check: 0.79x. Also verify 1x and 2x for clipping,
  text truncation, and bitmap selection.
- Retain 60 FPS, text entry, mouse-over, tooltips, multi-touch, and the existing
  corner resizer behavior.

## Palette and materials

Use these base tokens. Opacity is applied in linear drawing operations; do not
bake hover or disabled states into replacement bitmaps.

| Token | Value | Use |
| --- | --- | --- |
| `rack.black` | `#090A09` | root, side rails, deepest recesses |
| `rack.header` | `#0A0B0A` | branded header face |
| `rack.panel` | `#242520` | primary module face |
| `rack.panel.dark` | `#0E100E` | panel title strips and inset areas |
| `rack.recess` | `#020403` | browser wells and meter slots |
| `rack.edge.dark` | `#080908` | lower/right bevel and screw fill |
| `rack.edge.light` | `#5F6059` | worn metal edge, ticks, screw outline |
| `danger.red` | `#B1251F` | logo, switches, fader caps, warning accent |
| `signal.amber` | `#B9782D` | active rail, values, status, browser outline |
| `legend.cream` | `#E2DAC2` | primary labels, title, fader index lines |
| `legend.muted` | `#979489` | inactive legends before compositing |
| `clip.red` | iPlug `COLOR_RED` | meter clipping only |

The large faces must stay very dark and low-contrast. Red is a compact hardware
accent, not a panel fill. Amber is the signal/status color. The approved image
must not acquire blue theme accents; the existing blue globe artwork may remain
only until a visually equivalent neutral/amber icon is supplied.

### Chassis treatment

- Outer chassis: `[8, 8, 592, 992)`, 5 px corner radius, 2 px worn-steel bevel.
- Side rails: `[0, 0, 9, 1000)` and `[591, 0, 600, 1000)` with a dark-to-steel-to-dark horizontal gradient.
- Corner screws: centers `(17,17)`, `(583,17)`, `(17,983)`, `(583,983)`;
  radius 4 px; dark fill and muted-steel 1 px outline.
- Each module has a beveled frame, a recessed near-black title strip, two small
  face screws per side, and a 3 px amber/red vertical accent at its left edge.
- Do not use photographic texture. Very subtle deterministic vector variation
  is acceptable, but it must not create capture-to-capture pixel noise.

## Typography

Load the existing bundled fonts by their current logical names.

| Role | Font | Size | Colour | Alignment |
| --- | --- | ---: | --- | --- |
| Product title | Michroma Regular | 22 px | `legend.cream` | centered vertically, left-to-right in header |
| Module heading | Roboto Regular | 14 px | `legend.cream` | centered in title strip |
| Control label | Roboto Regular | 12 px | `legend.cream` | centered above control |
| Control value | Roboto Regular | 11 px | `signal.amber` | centered below control |
| Micro legend/status caption | Roboto Regular | 9 px | muted steel or amber | centered |
| Graphic-EQ scale/value | Roboto Regular | 8 px | muted steel/amber | centered |
| Button text | Roboto Regular | 10 px | `legend.cream` | centered |

Use the exact uppercase copy shown below. Do not substitute a platform font,
synthetic bold, title case, or smart punctuation. Text must remain live vector
text; do not rasterize labels into the background.

## Module geometry

The following rectangles are the approved rack-frame bounds. A 2 px tolerance
in the source screenshot is anti-aliasing, not permission to move panels.

| Module | Bounds | Heading |
| --- | --- | --- |
| Header | `[18, 8, 582, 94)` | `DANGER GUITAR AMPS` |
| Input / Gate | `[20, 95, 580, 220)` | `INPUT / GATE` |
| Pre-EQ | `[20, 228, 580, 356)` | `PRE-EQ` |
| NAM amp | `[20, 366, 580, 484)` | `NAM AMP` |
| Cabinet IR | `[20, 494, 580, 612)` | `CABINET IR` |
| Post-EQ | `[20, 624, 580, 888)` | `NINE-BAND GRAPHIC POST-EQ` |
| Output | `[20, 897, 580, 992)` | `OUTPUT` |

Each heading occupies the first 24 logical px of its module. The visible
heading baseline is centered in that band. Panel content begins at least 26 px
below the panel top so labels cannot collide with the bevel or accent rail.

## Header

- Danger warning mark visual box: `[36, 30, 76, 68)`.
- Product-title box: `[105, 28, 515, 65)`.
- Settings button visual and hit box: `[548, 31, 574, 57)`.
- The title is exactly `DANGER GUITAR AMPS`.
- Do not show the old `MODEL AMPLIFIER // IMPULSE CABINET` subtitle in the
  approved main view.
- Hover on the settings control adds only the existing low-opacity cream wash;
  it must not resize or shift the gear.

## Input / Gate module

| Element | Bounds | Binding/copy |
| --- | --- | --- |
| Input meter | `[35, 123, 66, 210)` | existing `kCtrlTagInputMeter` sender |
| Input knob | `[132, 122, 232, 215)` | `kInputLevel`; label `Input` |
| Gate switch | `[278, 126, 342, 211)` | `kNoiseGateActive`; label `Gate`; vertical |
| Threshold knob | `[395, 122, 495, 215)` | `kNoiseGateThreshold`; label `Threshold` |

- Default display values are `0.0 dB` for Input and `-80.0 dB` for Threshold.
- The meter is a narrow recessed vertical slot. Preserve its existing meter
  range, peak behavior, clip behavior, and sender source.
- Output is intentionally absent from this module.

## Pre-EQ module

| Element | Bounds | Binding | Visible label |
| --- | --- | --- | --- |
| Bypass switch | `[55, 260, 155, 323)` | `kPreEQBypass` | `Bypass` |
| Low-cut knob | grid cell 0 | `kPreEQLowCut` | `Low Cut` |
| Body knob | grid cell 1 | `kPreEQLowShelfGain` | `Body` |
| Mid-frequency knob | grid cell 2 | `kPreEQMidFrequency` | `Mid Freq` |
| Mid-gain knob | grid cell 3 | `kPreEQMidGain` | `Mid Gain` |
| Attack knob | grid cell 4 | `kPreEQHighShelfGain` | `Attack` |

The five-knob grid is `[170, 255, 590, 354)`, five equal 84 px cells. Each knob
has a 70 px bitmap diameter centered in its cell, with the label above and value
below. Default values shown in the approved reference are `120 Hz`, `0.0 dB`,
`800 Hz`, `0.0 dB`, and `0.0 dB`.

When bypass is on, disable the five controls through the existing
`PRE_EQ_CONTROLS` group. Keep them visible and readable at reduced opacity.
Mouse events and tooltips while disabled retain current behavior.

## NAM amp module

- Status caption: box `[75, 395, 555, 420)`, text `MODEL STATUS`.
- Browser surround: `[75, 420, 555, 478)`, 2 px amber outline, 4 px radius.
- Existing file-browser control/hit region: `[110, 430, 520, 468)`.
- Empty-state text: `NO MODEL LOADED` in amber.
- Picker text when invoked: retain the existing platform-specific
  `Select model...` or directory variant.
- Keep the existing file, previous, next, filename/menu, clear/get, globe,
  extension filter, completion handler, and failure display behavior.
- Preserve `kCtrlTagModelFileBrowser`, `kMsgTagClearModel`,
  `kMsgTagLoadedModel`, `kMsgTagLoadFailed`, and `.nam` filtering.
- The slimmable-model affordance remains conditional and must not reserve a
  visible blank box when hidden.

## Cabinet IR module

- Status caption: box `[75, 523, 555, 548)`, text `CABINET STATUS`.
- Browser surround: `[75, 548, 555, 606)`, 2 px amber outline, 4 px radius.
- Existing file-browser control/hit region: `[110, 558, 520, 596)`.
- IR enable icon/switch: `[75, 553, 105, 601)`; it must remain bound directly
  to `kIRToggle`.
- Empty-state text: `NO CABINET IR LOADED` in amber.
- Preserve `kCtrlTagIRFileBrowser`, `kCtrlTagIRFormatIndicator`,
  `kMsgTagClearIR`, `kMsgTagLoadedIR`, `kMsgTagLoadFailed`, `.wav` filtering,
  channel-format indication, and all current loader behavior.
- Disabling the cabinet IR disables the browser using the existing
  `OnParamChangeUI()` path. Do not reinterpret bypass semantics.

## Nine-band graphic post-EQ module

- Bypass switch: `[50, 674, 145, 731)`, bound to `kPostEQBypass`, label
  `Bypass`.
- Flat button: `[493, 674, 555, 708)`, text `FLAT`.
- Nine-band grid: `[126, 690, 484, 862)`, nine equal-width cells.
- Post-level fader: `[500, 720, 553, 862)`.

The band order and labels are fixed:

| Cell | Binding | Label |
| ---: | --- | --- |
| 0 | `kGraphicEQ62HzGain` | `62.5` |
| 1 | `kGraphicEQ125HzGain` | `125` |
| 2 | `kGraphicEQ250HzGain` | `250` |
| 3 | `kGraphicEQ500HzGain` | `500` |
| 4 | `kGraphicEQ1kHzGain` | `1k` |
| 5 | `kGraphicEQ2kHzGain` | `2k` |
| 6 | `kGraphicEQ4kHzGain` | `4k` |
| 7 | `kGraphicEQ8kHzGain` | `8k` |
| 8 | `kGraphicEQ16kHzGain` | `16k` |

Faders have a black recessed 8 px slot, a 1.5 px muted-steel center rail, five
horizontal scale marks, and a 16 x 10 px red cap with a 1 px cream index. The
0 dB reference is visually stronger than the other ticks. Show `+12`, `+6`,
`0`, `-6`, and `-12` on the left scale. Show one-decimal `dB` values below each
band.

The post-level legend is `POST LEVEL`. Its 0 dB reference is two-thirds of the
normalized travel because the parameter range is -12 to +6 dB. The Flat action
must continue to set all nine gains to normalized `0.5` and post level to
normalized `2/3`, with the existing begin/send/end host-notification sequence.
It must not reset any other parameter.

When Post-EQ bypass is on, disable the ten faders through the existing
`POST_EQ_CONTROLS` group. The Flat button remains enabled, matching the current
behavior. Do not introduce a DSP-side reset or bypass path.

## Output module

| Element | Bounds | Binding |
| --- | --- | --- |
| Output knob | `[70, 900, 180, 990)` | existing `kOutputLevel` |
| Horizontal output meter | `[205, 923, 565, 980)` | existing `kCtrlTagOutputMeter` sender |

- Label the knob `Output`; default display is `0.0 dB`.
- The output meter is horizontal in the approved UI. This is a UI-only drawing
  change: preserve the sender, tag, meter minimum/maximum, peak hold, average,
  and clip color.
- Scale labels are `-60`, `-36`, `-18`, `-6`, and `0` dB, left to right.
- Do not add an output mute, limiter, clipper, routing switch, or gain stage.

## Shared control rendering

### Knobs

- Reuse the existing three-resolution `KnobBackground` bitmap family unless a
  replacement reproduces the approved black hardware knob exactly.
- Main knob diameter: 76 logical px; Pre-EQ knob diameter: 70 logical px.
- Preserve the current rotation range, gearing, text-entry behavior, radial
  indicator track, pointer glow, and hover response.
- Pointer dot is warning red at rest and may brighten on hover. Do not use an
  animated halo.
- Labels sit above and values below. Neither may overlap the bitmap at 0.79x.

### Switches

- Track is black/recessed when off and warning red when on.
- Reuse the scaled `SlideSwitchHandle` bitmap family and current click/drag
  semantics.
- Bypass switches are horizontal. Gate is vertical.
- State is communicated by position and color, never color alone; labels remain
  visible in both states.

### Browser controls

- Preserve all current child-control action functions and menu behavior.
- Keep the 45-character filename ellipsis behavior and full filename tooltip.
- Empty and failed states must remain distinguishable. Failed text keeps the
  existing `(FAILED)` prefix.
- Icons must have at least a 24 x 24 logical hit target even if the visible mark
  is smaller.

### Hover, disabled, and focus

- Hover wash: `legend.cream` at 10% opacity, 2 px radius.
- Disabled controls remain at the same position and size with reduced opacity;
  do not hide them, collapse layout, or replace their text.
- Keyboard/text-entry focus must use a thin amber outline and must not modify
  parameter values merely by receiving focus.
- No hover or animation may cause layout movement.

## Secondary surfaces

The approved reference covers the main page. The settings page and slimmable
overlay are not authorized for structural redesign by this specification.

- Preserve all current settings controls, bindings, model-metadata disable
  rules, links, notices, close behavior, and animation.
- Reskin their background, text, and accents with the palette above only where
  required to avoid a visually unrelated legacy-blue surface.
- Preserve the full-window 45% black slim-overlay backdrop, centered `kSlim`
  knob, conditional icon visibility, and click-outside dismissal.
- Do not claim pixel approval for either secondary surface until a dedicated
  approved reference is added.

## Parameter and message binding checklist

The implementation must reuse these existing bindings exactly:

| Visible function | Existing identity |
| --- | --- |
| Input | `kInputLevel` |
| Gate threshold | `kNoiseGateThreshold` |
| Gate switch | `kNoiseGateActive` |
| Output | `kOutputLevel` |
| Cabinet enable | `kIRToggle` |
| Pre-EQ bypass and five knobs | existing `kPreEQ*` parameters |
| Post-EQ bypass | `kPostEQBypass` |
| Nine graphic bands and post level | existing `kGraphicEQ*` parameters |
| Model browser | `kCtrlTagModelFileBrowser` and existing model messages |
| Cabinet browser | `kCtrlTagIRFileBrowser` and existing IR messages |
| Input/output meters | `kCtrlTagInputMeter`, `kCtrlTagOutputMeter` |
| Settings | `kCtrlTagSettingsBox` and existing settings tags |
| Slim overlay | `kCtrlTagSlimmableIcon`, backdrop, and knob tags |

The legacy tone-stack, removed Reverb IR, and reserved parametric Post-EQ
parameters remain reserved for state compatibility. This UI does not expose,
delete, reorder, or repurpose them.

## Resource implementation rules

- Replace placeholder Danger chassis/logo art with deterministic vector assets
  using the approved 600 x 1000 and 48 x 40 view boxes respectively.
- Keep logical resource filenames stable where practical. If files are added,
  register them consistently in `config.h`, `resource.h`, Windows `.rc`, and
  Apple project resources without altering product metadata.
- Keep 1x/2x/3x bitmap families complete. A missing scale variant is a release
  blocker because it changes sharpness and fitted bounds.
- Do not delete inherited resources until every supported target has loaded the
  replacements successfully.
- Fonts must remain embedded resources; no system-font dependency is allowed.

## Verification and acceptance

### Visual capture

1. Build the UI-only implementation with no functional changes.
2. Open the VST3 in REAPER using the same 1920 x 1080 Windows environment and
   0.79x host render scale as the approved capture.
3. Use default state with no NAM model and no cabinet IR loaded, Gate active,
   Pre-EQ bypassed, Post-EQ bypassed, all displayed gains at 0 dB, Input at
   0 dB, Threshold at -80 dB, and Output at 0 dB.
4. Crop exactly to the plug-in view. The result must be 474 x 790 px.
5. Compare against the canonical crop from `[242,156,716,946)`.

### Visual tolerances

- Solid fills and long rules: maximum 2 RGB levels per channel from target.
- Panel/control geometry: no edge more than 1 physical px from target at 0.79x.
- Text baselines and control centers: no more than 1 physical px deviation.
- Anti-aliased text/vector edges: maximum 12 RGB levels per channel within a
  one-pixel edge neighborhood.
- No clipped labels, overlapping values, missing scale ticks, asymmetric panel
  margins, or host-background gaps.
- Dynamic meter pixels may be masked, but meter well, ticks, scale labels, and
  clip region may not be masked.
- Any reference change requires a new filename/hash and explicit approval; do
  not silently move the baseline.

### Functional regression checks

- Every visible control automates the same existing parameter.
- Model and cabinet load, iterate, menu-select, clear, fail, and recall exactly
  as before.
- Gate, Pre-EQ, cabinet IR, and Post-EQ enabled/disabled UI states follow the
  existing parameter callbacks.
- Flat sends only the ten existing Post-EQ parameter changes.
- Input and output meters receive the same sender data as before.
- Settings and slim overlay open, close, disable, and restore as before.
- Save/reopen and preset/session recall produce byte-compatible state behavior.

### Change-scope audit

Before accepting implementation, inspect `git diff --name-only` and reject any
change under `AudioDSPTools/`, `NeuralAmpModelerCore/`, `SignalChain.*`,
`ToneStack.*`, or `Unserialization.cpp`. Inspect diffs to `NeuralAmpModeler.h`
and `NeuralAmpModeler.cpp` manually: only UI composition/drawing is permitted;
parameter enums, initialization, processing, serialization, and loading logic
must be unchanged.

## Definition of done

The UI implementation is complete only when the canonical main-page capture
meets the visual tolerances, every binding and interaction check passes, the
secondary surfaces remain functional, all target resource builds succeed, and
the change-scope audit confirms that no DSP, parameter, routing, state,
serialization, or `AudioDSPTools` behavior changed.
