# Danger Guitar Amps approved UI implementation specification

## Status and authority

This document is the implementation contract for the approved Danger Guitar
Amps main interface. It specifies presentation and UI composition only. It does
not authorize changes to audio behavior, parameter identity, routing, state,
serialization, or dependencies.

No canonical raster image is currently authoritative. In particular,
`danger-fidelity-final.png` shows the obsolete vertically separated NAM and
Cabinet composition and must not be used as a pixel baseline. Until a new
side-by-side composition is rendered and approved, the coordinate tables in
this document are the layout authority and the palette/typography tables are
the visual authority.

Coordinates are `[left, top, right, bottom)` in a 600 x 720 logical design
space. Right and bottom are exclusive.

The checked-in `DangerRackBackground.svg` and `DangerLogo.svg` identify
themselves as placeholders. They are not visual authorities. The current
600 x 800 layout in `NeuralAmpModeler.cpp` is useful evidence for bindings and
interaction behavior, but it is not the approved composition.

## Non-negotiable scope boundary

An implementation of this specification may change only UI layout, drawing,
UI-only control classes, UI resource declarations, artwork, fonts, and the
  plug-in window dimensions required to present the approved 600 x 720 canvas.

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

The approved interface is a compact industrial rack with six rows in
signal-flow order:

1. branded header;
2. input and gate;
3. pre-EQ;
4. independent NAM amp and Cabinet IR consoles sharing one horizontal row;
5. nine-band graphic post-EQ;
6. output and output meter.

The row order is fixed. NAM and Cabinet remain independent consoles with their
own borders, title strips, recessed displays, controls, hit regions, and status
information.

## Canvas, scale, and resizing

- Design canvas: exactly 600 x 720 logical px.
- The implementation target is `PLUG_WIDTH = 600` and `PLUG_HEIGHT = 720`;
  retain the existing four-times maximum-size policy.
- Root origin: `(0, 0)`; no layout calculation may depend on host chrome.
- Use one uniform scale for X and Y. Do not independently stretch the axes.
- Keep `EUIResizerMode::Scale`. Scaling must preserve the 5:6 aspect ratio.
- The background, hit rectangles, labels, and controls must use the same
  transform so rendering and pointer targets cannot drift apart.
- Pixel snapping: snap one-pixel strokes and fader index lines to device-pixel
  centers after scaling. Filled rectangles may land on whole device pixels.
- Verify 1x and 2x for clipping, text truncation, and bitmap selection.
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
accent, not a panel fill. Amber is the signal/status color. The implementation
must not acquire blue theme accents; the existing blue globe artwork may remain
only until a visually equivalent neutral/amber icon is supplied.

### Chassis treatment

- Outer chassis: `[8, 8, 592, 712)`, 5 px corner radius, 2 px worn-steel bevel.
- Side rails: `[0, 0, 9, 720)` and `[591, 0, 600, 720)` with a dark-to-steel-to-dark horizontal gradient.
- Corner screws: centers `(17,17)`, `(583,17)`, `(17,703)`, `(583,703)`;
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

The following rectangles are the target rack-frame bounds.

| Module | Bounds | Heading |
| --- | --- | --- |
| Header | `[18, 8, 582, 72)` | `DANGER GUITAR AMPS` |
| Input / Gate | `[20, 78, 580, 178)` | `INPUT / GATE` |
| Pre-EQ | `[20, 184, 580, 288)` | `PRE-EQ` |
| NAM amp | `[20, 294, 296, 402)` | `NAM AMP` |
| Cabinet IR | `[304, 294, 580, 402)` | `CABINET IR` |
| Post-EQ | `[20, 408, 580, 590)` | `NINE-BAND GRAPHIC POST-EQ` |
| Output | `[20, 596, 580, 712)` | `OUTPUT` |

Each heading occupies the first 22 logical px of its module. The visible
heading baseline is centered in that band. Panel content begins at least 24 px
below the panel top so labels cannot collide with the bevel or accent rail.

## Header

- Danger warning mark visual box: `[36, 22, 76, 62)`.
- Product-title box: `[105, 20, 515, 60)`.
- Settings button visual and hit box: `[548, 22, 574, 48)`.
- The title is exactly `DANGER GUITAR AMPS`.
- Do not show the old `MODEL AMPLIFIER // IMPULSE CABINET` subtitle in the
  approved main view.
- Hover on the settings control adds only the existing low-opacity cream wash;
  it must not resize or shift the gear.

## Input / Gate module

| Element | Bounds | Binding/copy |
| --- | --- | --- |
| Input meter | `[35, 104, 58, 170)` | existing `kCtrlTagInputMeter` sender |
| Input knob | `[120, 102, 210, 176)` | `kInputLevel`; label `Input` |
| Gate switch | `[270, 102, 330, 174)` | `kNoiseGateActive`; label `Gate`; vertical |
| Threshold knob | `[390, 102, 480, 176)` | `kNoiseGateThreshold`; label `Threshold` |

- Default display values are `0.0 dB` for Input and `-80.0 dB` for Threshold.
- The meter is a narrow recessed vertical slot. Preserve its existing meter
  range, peak behavior, clip behavior, and sender source.
- Output is intentionally absent from this module.

## Pre-EQ module

| Element | Bounds | Binding | Visible label |
| --- | --- | --- | --- |
| Bypass switch | `[42, 214, 132, 276)` | `kPreEQBypass` | `Bypass` |
| Low-cut knob | `[145, 206, 230, 286)` | `kPreEQLowCut` | `Low Cut` |
| Body knob | `[230, 206, 315, 286)` | `kPreEQLowShelfGain` | `Body` |
| Mid-frequency knob | `[315, 206, 400, 286)` | `kPreEQMidFrequency` | `Mid Freq` |
| Mid-gain knob | `[400, 206, 485, 286)` | `kPreEQMidGain` | `Mid Gain` |
| Attack knob | `[485, 206, 570, 286)` | `kPreEQHighShelfGain` | `Attack` |

The five-knob grid is `[145, 206, 570, 286)`, five equal 85 px cells. Every
label, bitmap, value, and hit region must remain within those cells and within
the Pre-EQ panel. Each knob has a 58 px bitmap diameter centered in its cell.
Default values are `120 Hz`, `0.0 dB`, `800 Hz`, `0.0 dB`, and `0.0 dB`.

When bypass is on, disable the five controls through the existing
`PRE_EQ_CONTROLS` group. Keep them visible and readable at reduced opacity.
Mouse events and tooltips while disabled retain current behavior.

## NAM amp module

- Console border: `[20, 294, 296, 402)`; it must not merge with the Cabinet border.
- Title strip: `[20, 294, 296, 316)`, text `NAM AMP`.
- Status caption: `[34, 318, 258, 336)`, text `MODEL STATUS`.
- Slimmable-model affordance: `[260, 318, 282, 336)`, conditional.
- Recessed browser surround: `[30, 338, 286, 394)`, 2 px amber outline, 4 px radius.
- Existing file-browser control/hit region: `[48, 348, 268, 384)`.
- Browser child hit regions, left to right: load `[48,348,76,384)`, previous
  `[76,348,100,384)`, next `[100,348,124,384)`, filename/menu
  `[124,348,238,384)`, and clear/get `[238,348,268,384)`.
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

- Console border: `[304, 294, 580, 402)`; it must not merge with the NAM border.
- Title strip: `[304, 294, 580, 316)`, text `CABINET IR`.
- Status caption: `[318, 318, 538, 336)`, text `CABINET STATUS`.
- Channel-format indicator: `[540, 318, 566, 336)`.
- Recessed browser surround: `[314, 338, 570, 394)`, 2 px amber outline, 4 px radius.
- IR enable icon/switch: `[322, 346, 350, 386)`; it must remain bound directly
  to `kIRToggle`.
- Existing file-browser control/hit region: `[350, 348, 552, 384)`.
- Browser child hit regions, left to right: load `[350,348,378,384)`, previous
  `[378,348,402,384)`, next `[402,348,426,384)`, filename/menu
  `[426,348,522,384)`, and clear/get `[522,348,552,384)`.
- Empty-state text: `NO CABINET IR LOADED` in amber.
- Preserve `kCtrlTagIRFileBrowser`, `kCtrlTagIRFormatIndicator`,
  `kMsgTagClearIR`, `kMsgTagLoadedIR`, `kMsgTagLoadFailed`, `.wav` filtering,
  channel-format indication, and all current loader behavior.
- Disabling the cabinet IR disables the browser using the existing
  `OnParamChangeUI()` path. Do not reinterpret bypass semantics.

## Nine-band graphic post-EQ module

The complete Post-EQ panel is 182 logical px high and must not exceed 190 px.

- Bypass switch: `[36, 436, 116, 476)`, bound to `kPostEQBypass`, label
  `Bypass`.
- Flat button: `[500, 434, 558, 460)`, text `FLAT`.
- Left scale legend: `[108, 454, 126, 582)`.
- Post-level fader: `[500, 462, 552, 582)`.

The nine exact fader cells are:

The band order and labels are fixed:

| Cell | Bounds | Binding | Label |
| ---: | --- | --- | --- |
| 0 | `[126, 446, 166, 582)` | `kGraphicEQ62HzGain` | `62.5` |
| 1 | `[166, 446, 205, 582)` | `kGraphicEQ125HzGain` | `125` |
| 2 | `[205, 446, 245, 582)` | `kGraphicEQ250HzGain` | `250` |
| 3 | `[245, 446, 284, 582)` | `kGraphicEQ500HzGain` | `500` |
| 4 | `[284, 446, 324, 582)` | `kGraphicEQ1kHzGain` | `1k` |
| 5 | `[324, 446, 363, 582)` | `kGraphicEQ2kHzGain` | `2k` |
| 6 | `[363, 446, 403, 582)` | `kGraphicEQ4kHzGain` | `4k` |
| 7 | `[403, 446, 442, 582)` | `kGraphicEQ8kHzGain` | `8k` |
| 8 | `[442, 446, 482, 582)` | `kGraphicEQ16kHzGain` | `16k` |

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
| Output knob | `[58, 620, 158, 708)` | existing `kOutputLevel` |
| Horizontal output meter | `[190, 632, 558, 696)` | existing `kCtrlTagOutputMeter` sender |

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
- Main knob diameter: 64 logical px; Pre-EQ knob diameter: 58 logical px.
- Preserve the current rotation range, gearing, text-entry behavior, radial
  indicator track, pointer glow, and hover response.
- Pointer dot is warning red at rest and may brighten on hover. Do not use an
  animated halo.
- Labels sit above and values below. Neither may overlap the bitmap at any
  supported scale.

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
  using the approved 600 x 720 and 48 x 40 view boxes respectively.
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
2. Open the VST3 in REAPER at a known uniform scale.
3. Use default state with no NAM model and no Cabinet IR loaded, Gate active,
   Pre-EQ bypassed, Post-EQ bypassed, all displayed gains at 0 dB, Input at
   0 dB, Threshold at -80 dB, and Output at 0 dB.
4. Confirm the complete 600 x 720 composition, especially the two independent
   side-by-side consoles and the Post-EQ height.
5. Record a candidate reference capture for explicit composition approval. No
   exact physical crop is required during this first implementation pass.

### Visual tolerances

- Panel and control geometry: within +/-2 logical px of the coordinate tables.
- Text baselines and control centers: within +/-2 logical px.
- Colours: visually equivalent to the named palette tokens; exact per-channel
  matching is not required for the first implementation pass.
- No clipped labels, overlapping values, missing scale ticks, asymmetric panel
  margins, or host-background gaps.
- Dynamic meter pixels may be masked, but meter well, ticks, scale labels, and
  clip region may not be masked.
- Once a side-by-side reference is approved, record its filename and hash in a
  later documentation revision before enabling strict pixel-diff acceptance.

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

The first UI implementation pass is complete only when the 600 x 720
composition meets these tolerances, every binding and interaction check passes,
the secondary surfaces remain functional, all target resource builds succeed,
and the change-scope audit confirms that no DSP, parameter, routing, state,
serialization, or `AudioDSPTools` behavior changed. Strict pixel-diff approval
begins only after a matching side-by-side reference image is approved.
