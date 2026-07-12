# v0.1 modular signal-chain plan

## Objective

Evolve the current mono chain into the first stable Danger Guitar Amps modular chain:

```text
Input
  -> Gate
  -> Pre-EQ
  -> Compressor
  -> NAM
  -> Speaker IR
  -> Post-EQ
  -> Reverb IR
  -> Output
```

This is a plan only. It does not authorize production changes, dependency upgrades, parameter renames, or state-format changes.

## Compatibility policy

- Retain iPlug2 and NeuralAmpModelerCore.
- Keep all existing `EParams` identifiers at their current numeric values and in their current order.
- Append any new v0.1 parameters after `kSlim`; never insert them between existing entries.
- Preserve the state header `###NeuralAmpModeler###` until a deliberately versioned migration changes it.
- Preserve current model and IR path restoration while extending state with explicit version migration.
- Start every new stage neutral or bypassed so an old session initially sounds the same after migration.
- Preserve mono internal processing for v0.1 unless stereo reverb becomes an explicit, separately tested requirement.

## Proposed stage semantics

### Input

Reuse the current channel collapse, `mInputGain`, `_SetInputGain()`, and calibration behavior. Move buffer preparation out of the callback where possible. Input gain remains first.

### Gate

Reuse `dsp::noise_gate::Trigger` and `Gain`, but present them as one chain stage with explicit detector and gain-application points. For the documented chain to be literal, both analysis and attenuation should occur before Pre-EQ and NAM. This changes the current behavior, where attenuation is applied after NAM, so it requires an A/B decision and regression fixtures.

Minimum safe gate work:

- Preallocate all trigger/gain buffers for the maximum block size in `OnReset()`.
- Replace the per-block nested-vector copy with an immutable span/view or direct shared preallocated envelope buffer.
- Keep threshold and enable mapped to existing `kNoiseGateThreshold` and `kNoiseGateActive`.
- Preserve existing time/open/hold/close constants initially; expose them only in a later parameter append/migration.

If exact legacy tone is more important than a literal diagram for v0.1, document the gate as a split detector/applicator and retain the post-NAM application until a later version. Do not silently change this behavior.

### Pre-EQ

Use the existing recursive filter primitives. The minimum-risk v0.1 behavior is a flat/bypassed new Pre-EQ while the existing three tone parameters continue controlling Post-EQ. If a working Pre-EQ is required in v0.1, append new uniquely named parameters and build it from `LowShelf`, `Peaking`, and `HighShelf` without reassigning existing IDs.

### Compressor

Add a new internal compressor stage with no dependency upgrade. It should provide:

- preallocated detector and gain buffers;
- smoothing coefficients prepared outside the sample loop;
- no locks, file I/O, exceptions, or allocations in `Process()`;
- bypass as the migration/default state;
- appended parameters only, such as enable, threshold, ratio, attack, release, and makeup gain.

For the smallest first step, implement the class and tests with the stage permanently bypassed before adding public parameters in a separate commit.

### NAM

Reuse `ResamplingNAM`, `nam::get_dsp()`, NeuralAmpModelerCore model types, calibration metadata, output-mode logic, and current load validation. Model creation and prewarm remain off the audio thread. Replace the unsynchronized staged `unique_ptr` handoff with an explicit real-time-safe exchange mechanism before adding more modules around it.

### Speaker IR

Treat the existing `mIR`, `mStagedIR`, `mIRPath`, `kIRToggle`, browser, loader, and `dsp::ImpulseResponse` as the Speaker IR stage. This preserves existing sessions and user expectations.

The current 8,192-sample, mono time-domain convolution is suitable as the initial speaker-cabinet path, subject to preallocation and performance measurement. Rename only visible labels first; do not rename existing parameter IDs or serialized path keys.

### Post-EQ

Move the current `BasicNamToneStack` conceptually to Post-EQ and retain:

- `kEQActive`;
- `kToneBass`;
- `kToneMid`;
- `kToneTreble`;
- current frequencies, gains, and Q behavior for compatibility.

No parameter migration is required if this stage keeps the current behavior. A later version can add frequency/Q controls by appending parameters.

### Reverb IR

Add a second, separately owned IR slot with its own path, bypass, staging exchange, and processor instance. Do not alias or reuse `mIRPath`, `mIR`, `mStagedIR`, or `kIRToggle`, because those belong to Speaker IR compatibility.

The existing `dsp::ImpulseResponse` loading/resampling infrastructure can be reused for a short proof of concept. It should not be the final general reverb engine: the 8,192-sample limit is roughly 171 ms at 48 kHz, and direct time-domain convolution cost grows with IR length. A useful reverb requires a longer tail and likely preallocated partitioned convolution. That can be implemented locally without changing iPlug2 or NeuralAmpModelerCore, but it should be its own measured stage and commit.

### Output

Reuse the 5 Hz DC blocker, `_SetOutputGain()`, `mOutputGain`, output modes, `_ProcessOutput()`, and meter update. Decide explicitly whether the DC blocker belongs immediately before Output or is an always-on utility after Reverb IR. Keep current placement and behavior for v0.1 unless tests justify moving it.

## Recommended minimal architecture

Avoid a broad framework rewrite. Introduce a small orchestration layer with statically known stage members and no per-block dynamic dispatch requirement.

### New files

1. `NeuralAmpModeler/SignalChain.h`
   - Defines a lightweight `SignalChain` owned by `NeuralAmpModeler`.
   - Holds stage instances or non-owning references to existing processors.
   - Defines `Prepare(sampleRate, maxBlockSize, channels)` and `Process(input, output, frames)`.
   - Contains no file loading or UI logic.

2. `NeuralAmpModeler/SignalChain.cpp`
   - Implements the fixed v0.1 order.
   - Uses preallocated scratch buffers.
   - Contains bypass routing without heap allocation.

3. `NeuralAmpModeler/Compressor.h` and `Compressor.cpp`
   - Adds the only genuinely new DSP algorithm required by the proposed chain.
   - Can be deferred until the orchestration and real-time buffer work are stable.

4. Optional `NeuralAmpModeler/RealtimeExchange.h`
   - A narrowly scoped single-producer/single-consumer ownership handoff for prepared models and IRs.
   - Prefer a proven fixed-slot/atomic-pointer protocol with non-audio-thread reclamation; do not invent an unbounded queue.

### Existing files requiring focused changes

1. `NeuralAmpModeler.h`
   - Add the chain/stage members and appended parameter IDs.
   - Keep every existing parameter value unchanged.
   - Replace direct staged ownership fields only after the exchange mechanism is proven.

2. `NeuralAmpModeler.cpp`
   - Delegate the sample path to `SignalChain::Process()`.
   - Prepare capacity in `OnReset()`.
   - Keep model/IR browser callbacks and staging outside audio processing.
   - Extend `OnParamChange()` with new stage parameters.

3. `ToneStack.{h,cpp}`
   - Reuse `BasicNamToneStack` as Post-EQ unchanged at first.
   - Optionally extract a reusable three-band EQ class only when Pre-EQ needs active controls.

4. `AudioDSPTools/dsp/ImpulseResponse.{h,cpp}`
   - Prefer no initial functional rewrite for Speaker IR.
   - Add an explicit `Prepare()`/capacity contract and remove callback-time growth before instantiating a second IR.
   - Avoid changing the AudioDSPTools submodule unless the fork intentionally owns that change; a local wrapper/adapter may keep the first release easier to rebase.

5. `Unserialization.cpp`
   - Add one new version migration after parameter definitions are final.
   - Supply bypass/neutral defaults for all appended stages and a separate empty Reverb IR path.
   - Preserve historical readers and names.

6. GUI files
   - Add controls only after DSP/state contracts exist.
   - Bind old controls to old IDs and new controls to appended IDs.

7. Visual Studio project/filter files
   - Add only the new source/header entries; do not rename the solution or existing projects for this stage.

## Reuse matrix

| v0.1 stage | Existing code to reuse | Required adaptation |
| --- | --- | --- |
| Input | `_ProcessInput`, `_SetInputGain`, calibration metadata | Preallocate buffers; isolate from orchestration |
| Gate | `noise_gate::Trigger` and `Gain` | Remove nested-vector copy/allocation; decide legacy split versus literal pre-NAM gate |
| Pre-EQ | Recursive filter primitives; optionally `BasicNamToneStack` pattern | New instance and appended IDs, initially bypassed/flat |
| Compressor | None suitable identified | New preallocated local class |
| NAM | `ResamplingNAM`, `nam::get_dsp`, `mModel` behavior | Safe ownership exchange and off-thread reclamation |
| Speaker IR | Existing IR browser, `mIRPath`, `kIRToggle`, `ImpulseResponse` | Treat current slot as speaker; preallocate history/output |
| Post-EQ | `BasicNamToneStack`, existing EQ IDs | Primarily relocation in orchestration; retain behavior |
| Reverb IR | WAV loading/resampling concepts from `ImpulseResponse` | Separate state/path and longer efficient convolution engine |
| Output | DC blocker, `_SetOutputGain`, `_ProcessOutput`, meters | Preallocate; keep behavior |

## Real-time design requirements

Before declaring the v0.1 chain stable:

- No `new`, `delete`, `make_unique`, vector growth, model destruction, IR destruction, file I/O, JSON parsing, mutex acquisition, logging, or host latency negotiation may occur in `ProcessBlock()`.
- All stages must have a `Prepare()` operation called from an allowed non-real-time/reset context with maximum block size and channel count.
- Processing must tolerate any host block size up to prepared capacity without resizing.
- A larger-than-prepared block must use a deterministic bounded fallback or chunking path, not allocate or throw.
- Model/IR handoff must have defined producer/consumer synchronization and non-audio reclamation.
- Bypass transitions should be click-free; use smoothing/crossfades prepared in advance.
- Parameters read by the audio thread should use iPlug's supported synchronization pattern or atomically published plain values/coefficient snapshots.
- Coefficient changes should be calculated outside inner sample loops and published without locking.
- State restoration must never synchronously load a model or IR if the host can invoke it on an audio thread; schedule preparation and publish the finished object later.
- Latency changes should be communicated from a safe host/control callback, not while processing samples.

## Suggested reversible implementation sequence

1. **Characterization tests only.** Capture impulse/frequency responses, silence/gate behavior, model bypass, IR bypass, output modes, parameter/state round trips, and representative audio hashes.
2. **Preallocation fixes.** Add explicit prepare/capacity handling to plug-in scratch buffers, gate, filters, and current IR without changing stage order.
3. **Safe model exchange.** Replace unsynchronized staging and audio-thread destruction; verify repeated rapid model changes under load.
4. **Safe Speaker IR exchange.** Apply the same ownership/reclamation design to the existing IR slot.
5. **SignalChain orchestration.** Move the unchanged current stages behind a fixed chain interface and confirm bit-identical or tolerance-bounded output.
6. **Post-EQ naming only.** Treat the existing tone stack as Post-EQ without changing IDs or coefficients.
7. **Pre-EQ stage.** Add a bypassed/flat prepared instance, then append parameters in a separate commit if required.
8. **Compressor stage.** Add bypassed processor and tests, then expose appended parameters.
9. **Speaker IR naming/UI.** Rename visible labels only; retain `kIRToggle` and `mIRPath` state meaning.
10. **Reverb IR infrastructure.** Add separate path, safe staging, bypass, and short-IR prototype.
11. **Efficient reverb convolution.** Replace the prototype with measured preallocated partitioned convolution before enabling long user IRs.
12. **State migration.** Add the new version reader/defaults only after the parameter list and path schema are frozen.
13. **UI exposure.** Add controls and automation bindings without moving old IDs.
14. **Release verification.** Run allocation instrumentation, real-time stress, state migration, host rescan, automation, latency, and audio regression suites.

Each step should build and be revertible independently. Do not combine real-time ownership fixes, parameter additions, and audible stage-order changes in one commit.

## Acceptance criteria for the first stable chain

- Existing projects restore every old parameter, model path, and speaker IR path correctly.
- Existing IDs retain their numeric values and automation meaning.
- With all new stages neutral/bypassed, output matches the current chain within an explicitly recorded tolerance.
- Both model and IR can be changed repeatedly without data races, callback allocation, callback destruction, or glitches attributable to ownership exchange.
- No locks or file I/O are observed on the real-time thread.
- Speaker and reverb IR state are independent.
- Reverb processing meets a declared maximum IR length and CPU budget.
- Reported latency and tail size include every enabled stage accurately.
- The Windows x64 Release VST3 builds, scans separately from official NAM, and passes model loading, audio, automation, save/reopen, and bypass tests.
