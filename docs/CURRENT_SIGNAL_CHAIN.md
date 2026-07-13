# Current signal chain

This document describes the processing in the current Danger Guitar Amps branch without proposing or applying production changes. The implementation remains the inherited Neural Amp Modeler processing architecture.

## Executive summary

The exact amplitude-processing path is:

```text
host input
  -> input channel collapse + input gain
  -> gate trigger/envelope analysis (audio passes through unchanged)
  -> NAM model, or passthrough when no model is loaded
  -> gate gain reduction calculated from the pre-NAM signal
  -> three-band tone stack, or bypass
  -> impulse response, or bypass
  -> 5 Hz DC-blocking high-pass filter
  -> output gain + mono-to-host-channel broadcast
  -> host output
```

The gate is deliberately split around NAM: `Trigger` analyzes the gained input before NAM, while `Gain` applies that envelope after NAM. It is therefore inaccurate to represent the implementation as one contiguous gate stage either entirely before or entirely after NAM.

## Exact current stage order

### 1. Input gain and mono preparation

`NeuralAmpModeler::ProcessBlock()` calls `_PrepareBuffers()` and then `_ProcessInput()`.

Responsible code:

- Function: `NeuralAmpModeler::_ProcessInput()` in `NeuralAmpModeler.cpp`.
- Gain value: `NeuralAmpModeler::mInputGain`.
- User parameter: `kInputLevel`.
- Calibration parameters: `kCalibrateInput` and `kInputCalibrationLevel`.
- Gain calculation: `NeuralAmpModeler::_SetInputGain()`.
- Internal buffers: `mInputArray` and `mInputPointers`.

For a plug-in build, all connected input channels are summed to the single internal channel and divided by the number of connected input channels. `mInputGain` is applied during that copy. If the loaded model publishes an input calibration and calibration is enabled, `_SetInputGain()` adds the difference between the user's calibration level and the model's input level before converting dB to amplitude.

### 2. Gate trigger before NAM

When `kNoiseGateActive` is true, `ProcessBlock()` constructs `TriggerParams`, updates `mNoiseGateTrigger`, and calls:

```cpp
mNoiseGateTrigger.Process(mInputPointers, 1, numFrames)
```

Responsible code:

- Detector: `dsp::noise_gate::Trigger` in `AudioDSPTools/dsp/NoiseGate.{h,cpp}`.
- Member: `NeuralAmpModeler::mNoiseGateTrigger`.
- Threshold: `kNoiseGateThreshold`.
- Enable: `kNoiseGateActive`.
- Fixed constants supplied per block: detector time 10 ms, ratio 0.1, open 5 ms, hold 10 ms, close 50 ms.
- Listener: `mNoiseGateGain`, registered in the plug-in constructor with `mNoiseGateTrigger.AddListener(&mNoiseGateGain)`.

`Trigger::Process()` calculates a per-sample gain-reduction envelope from the gained pre-NAM signal. It then copies its input unchanged to its output. The audio sent into NAM is therefore not attenuated at this point.

### 3. NAM processing

If `mModel` is non-null, `ProcessBlock()` calls:

```cpp
mModel->process(triggerOutput, mOutputPointers, nFrames)
```

Responsible code:

- Live model member: `std::unique_ptr<ResamplingNAM> mModel`.
- Staging member: `std::unique_ptr<ResamplingNAM> mStagedModel`.
- Wrapper: `ResamplingNAM` in `NeuralAmpModeler.h`.
- Encapsulated model: `std::unique_ptr<nam::DSP> mEncapsulated`.
- Model factory: `nam::get_dsp()` from `NeuralAmpModelerCore/NAM/get_dsp.{h,cpp}`.
- Optional sample-rate conversion: `ResamplingNAM::mResampler` and `mBlockProcessFunc`.

If no model is live, `_FallbackDSP()` copies the trigger output into `mOutputArray`, making this stage a passthrough.

### 4. Gate gain reduction after NAM

When the gate is enabled, `ProcessBlock()` calls:

```cpp
mNoiseGateGain.Process(mOutputPointers, 1, numFrames)
```

Responsible code:

- Gain applicator: `dsp::noise_gate::Gain`.
- Member: `NeuralAmpModeler::mNoiseGateGain`.
- Envelope source: the `Trigger` listener update performed earlier in the same block.

The gain reduction was calculated from the pre-NAM signal but is multiplied into the post-NAM signal. When the gate is bypassed, `mOutputPointers` pass directly to the tone stack.

### 5. Tone stack

When `kEQActive` is true and `mToneStack` is non-null, `ProcessBlock()` calls `mToneStack->Process()`.

Responsible code:

- Interface: `dsp::tone_stack::AbstractToneStack`.
- Implementation: `dsp::tone_stack::BasicNamToneStack` in `ToneStack.{h,cpp}`.
- Member: `std::unique_ptr<AbstractToneStack> mToneStack`.
- Construction: `NeuralAmpModeler::_InitToneStack()`.
- Enable: `kEQActive` (`ToneStack`).
- Bands: `kToneBass`, `kToneMid`, and `kToneTreble`.
- Parameter updates: `NeuralAmpModeler::OnParamChange()` calls `mToneStack->SetParam()`.

`BasicNamToneStack::Process()` runs three filters in series:

1. `recursive_linear_filter::LowShelf mToneBass`, fixed at 150 Hz, up to ±20 dB.
2. `recursive_linear_filter::Peaking mToneMid`, fixed at 425 Hz, up to ±15 dB with gain-dependent Q.
3. `recursive_linear_filter::HighShelf mToneTreble`, fixed at 1.8 kHz, up to ±10 dB.

### 6. Impulse response

When `mIR` is non-null and `kIRToggle` is true, `ProcessBlock()` calls `mIR->Process()`.

Responsible code:

- Processor: `dsp::ImpulseResponse` in `AudioDSPTools/dsp/ImpulseResponse.{h,cpp}`.
- Live member: `std::unique_ptr<dsp::ImpulseResponse> mIR`.
- Staging member: `std::unique_ptr<dsp::ImpulseResponse> mStagedIR`.
- Enable/bypass: `kIRToggle`.
- Persisted path: `WDL_String mIRPath`.
- WAV loader: `dsp::wav::Load()`.

The current processor is mono time-domain convolution. It keeps at most 8,192 IR samples, reverses and scales the resampled impulse into an `Eigen::VectorXf`, maintains a history buffer, and computes one dot product per output sample. Additional output channels, if requested internally, receive copies of channel zero; the plug-in currently processes one internal channel.

### 7. DC blocker

After the IR, the signal always passes through `recursive_linear_filter::HighPass mHighPass` at 5 Hz. The filter parameters are rebuilt and assigned in every `ProcessBlock()` call. This stage is not one of the six user-facing stages, but it is part of the exact current chain and precedes output gain.

### 8. Output gain and host output

`_ProcessOutput()` multiplies by `mOutputGain` and broadcasts the internal mono signal to every connected host output channel.

Responsible code:

- Function: `NeuralAmpModeler::_ProcessOutput()`.
- Member: `NeuralAmpModeler::mOutputGain`.
- User parameter: `kOutputLevel`.
- Mode parameter: `kOutputMode` (`Raw`, `Normalized`, `Calibrated`).
- Gain calculation: `NeuralAmpModeler::_SetOutputGain()`.

Normalized mode can compensate from model loudness metadata. Calibrated mode can compensate from model output-level metadata and the configured input calibration. The standalone build clamps final samples to ±1; the plug-in build does not.

## Code executed inside `ProcessBlock()`

The current callback performs all of the following:

1. Queries connected channel counts and sample rate.
2. Saves the floating-point environment and disables denormals.
3. Calls `_PrepareBuffers()`.
4. Calls `_ProcessInput()` to collapse channels and apply input gain.
5. Calls `_ApplyDSPStaging()` to clear or replace model/IR objects.
6. Reads enable and threshold parameters.
7. Configures and processes the gate trigger.
8. Processes the live NAM model or passthrough fallback.
9. Applies gate gain reduction.
10. Processes or bypasses the tone stack.
11. Processes or bypasses the IR.
12. Reconfigures and processes the 5 Hz high-pass filter.
13. Restores the floating-point environment.
14. Calls `_ProcessOutput()` to apply output gain and broadcast mono.
15. Calls `_UpdateMeters()`, which invokes the input/output `NAMSender` instances.

No model or WAV file is intentionally opened inside `ProcessBlock()`. However, object replacement, buffer resizing, destruction, and host latency updates do occur there and are real-time concerns.

## Model staging and handoff

### Load/stage side

The model browser completion handler calls `_StageModel()` outside `ProcessBlock()`.

`_StageModel()`:

1. Saves the previous `mNAMPath`.
2. Converts the selected path with `std::filesystem::u8path()`.
3. Calls `nam::get_dsp(path)`, which checks the filesystem, opens the model with `std::ifstream`, parses JSON, constructs the selected NeuralAmpModelerCore model, and allocates its weights/state.
4. Validates one input and one output channel.
5. Wraps the model in a newly allocated `ResamplingNAM`.
6. Calls `Reset()` and prewarms/prepares the model and resampler.
7. Applies the current `kSlim` value when supported.
8. Moves the complete object into `mStagedModel` and stores `mNAMPath`.
9. Notifies the UI of success or restores the previous path and clears the staged object on failure.

### Audio-thread side

At the start of the next `ProcessBlock()`, `_ApplyDSPStaging()` moves `mStagedModel` into `mModel`. It then updates latency and recalculates input/output gains from model metadata. `mNewModelLoadedInDSP` tells `OnIdle()` to update model-dependent controls later on the non-audio/UI side.

Clear requests set atomic `mShouldRemoveModel`; `_ApplyDSPStaging()` clears `mModel`, clears the path, updates latency and gains, and sets atomic `mModelCleared` for `OnIdle()`.

The intent is to construct complete models away from the audio callback and switch only complete objects. The implementation does not synchronize reads/writes to `mStagedModel` or `mModel`; only the request/status flags are atomic. That is a C++ data race risk. Replacing or clearing `unique_ptr` can also run a large model destructor on the audio thread.

## IR load, preparation, bypass, and processing

### Loading

The IR browser completion handler calls `_StageIR()` outside `ProcessBlock()`.

`_StageIR()`:

1. Saves the previous path.
2. Constructs `dsp::ImpulseResponse` in `mStagedIR` using the current sample rate.
3. Its filename constructor calls `dsp::wav::Load()`, which opens and reads the WAV using `std::ifstream` and fills `mRawAudio`.
4. `_SetWeights()` copies or cubic-resamples the raw audio, truncates to 8,192 samples, allocates the Eigen weight vector, applies gain compensation, reverses the impulse, and sets required history length.
5. On success, `_StageIR()` stores `mIRPath` and notifies the UI. On failure it destroys the staged object, restores the prior path, and reports failure.

### Sample-rate preparation

`OnReset()` calls `_ResetModelAndIR()`. If a staged/live IR sample rate differs from the host sample rate, the code copies its raw data through `GetData()` and constructs a replacement staged `ImpulseResponse` at the new rate. The replacement becomes live through `_ApplyDSPStaging()` in a later audio block.

### Bypass and processing

- `mIR == nullptr`: bypass.
- `kIRToggle == false`: bypass while retaining the live IR.
- Otherwise `ImpulseResponse::Process()` prepares output/history buffers, updates history, performs time-domain convolution, advances history, and returns its owned output pointers.
- Clear requests set `mShouldRemoveIR`; the audio callback destroys `mIR` and clears `mIRPath` in `_ApplyDSPStaging()`.

## Parameter lifecycle

### Declaration and stable order

`EParams` in `NeuralAmpModeler.h` defines this order:

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

The first six indices are also used to place the six main knobs. Their values and order are therefore both state-compatibility and UI-layout contracts.

### Initialization

The `NeuralAmpModeler` constructor initializes parameters with `GetParam(...)->InitGain`, `InitDouble`, `InitBool`, or `InitEnum`. Defaults/ranges are:

| Parameter | Default | Range/values |
| --- | ---: | --- |
| Input | 0 dB | -20 to +20 dB |
| Threshold | -80 dB | -100 to 0 dB |
| Bass | 5 | 0 to 10 |
| Middle | 5 | 0 to 10 |
| Treble | 5 | 0 to 10 |
| Output | 0 dB | -40 to +40 dB |
| NoiseGateActive | true | boolean |
| ToneStack | true | boolean |
| IRToggle | true | boolean |
| CalibrateInput | false | boolean |
| InputCalibrationLevel | 12 dBu | -60 to +60 dBu |
| OutputMode | Normalized | Raw, Normalized, Calibrated |
| Slim | 0 | 0 to 1 |

`OnParamChange()` recalculates input/output gain, updates one tone-stack band, or applies the slim value depending on the changed index. `OnParamChangeUI()` only enables/disables dependent controls.

### Serialization

`SerializeState()` writes, in order:

1. The compatibility header `###NeuralAmpModeler###` (explicitly marked not to change).
2. `PLUG_VERSION_STR`.
3. `mNAMPath`.
4. `mIRPath`.
5. `SerializeParams(chunk)`, which writes parameters in their current enum/index order.

The model and IR audio data are not embedded; their paths are persisted and the files are loaded again on restore.

### Restoration and migration

`UnserializeState()` detects the header and selects known-version or legacy parsing in `Unserialization.cpp`. Version-specific readers consume positional values using historical name lists, migrate old keys (`Gate` to `Threshold`, `OutNorm` to `OutputMode`), and provide defaults for parameters introduced later.

`_UnserializeApplyConfig()`:

- resolves current parameters by their names;
- enters the iPlug parameter mutex;
- calls `IParam::Set()` for restored values;
- calls `OnParamReset(kPresetRecall)`;
- leaves the mutex;
- restores model/IR paths;
- synchronously calls `_StageModel()` and `_StageIR()` when paths are non-empty.

Restoration therefore uses a lock and performs file I/O, parsing, allocation, model reset/prewarm, and IR preparation. It must not run on the real-time audio thread.

## Allocation, locking, file-I/O, and state-change audit

### Operations reachable from `ProcessBlock()`

- `_PrepareBuffers()` resizes `mInputArray`/`mOutputArray` when channel count or block size changes and can allocate. Channel changes also delete and allocate raw pointer arrays.
- Every `dsp::DSP::_PrepareBuffers()` can resize output vectors and raw pointer arrays. This affects gate trigger/gain, tone filters, the IR, and the high-pass filter on first use or size changes.
- `dsp::History::_EnsureHistorySize()` can resize the IR history vector inside `ImpulseResponse::Process()`.
- `Trigger::_PrepareBuffers()` can resize multiple nested vectors.
- `Trigger::Process()` copies the nested gain-reduction vector into every `Gain` listener via `Gain::SetGainReductionDB()`. Copy assignment may allocate when capacities do not already match and always performs an O(channels × frames) copy.
- `_ApplyDSPStaging()` moves staged pointers and destroys replaced/cleared models or IRs on the audio thread. Destruction can release large allocations.
- `_ApplyDSPStaging()` calls `SetLatency()` indirectly through `_UpdateLatency()`, potentially causing a host interaction from the audio callback.
- A slimmable NAM may atomically exchange a `shared_ptr` to staged data during model processing; final reference destruction can occur on the audio thread.
- No explicit plug-in mutex is acquired in the normal sample path. Some NeuralAmpModelerCore container configuration methods use mutexes, but the normal container process selects its active model atomically rather than taking the configuration lock.

### Operations outside normal `ProcessBlock()`

- `_StageModel()`: filesystem access, file opening/parsing, registry lock use inside `nam::get_dsp()`, extensive allocation, model construction, reset and prewarm.
- `_StageIR()`: WAV file I/O, vector/Eigen allocation, resampling and weight construction.
- `_ResetModelAndIR()`: resets/prewarms models and can copy/reconstruct/resample IRs; called by `OnReset()`.
- Serialization/restoration: chunk and string work; restoration takes the iPlug parameter mutex and then performs model/IR file I/O and allocation.
- UI resource and third-party notice access performs file I/O on UI actions, not in audio processing.

## Explicit real-time risks

1. **Unsynchronized staged ownership:** UI/host-side writes to `mStagedModel` and `mStagedIR` race audio-thread reads/moves under the C++ memory model.
2. **Audio-thread destruction:** replacing or clearing a `unique_ptr` can execute expensive destructors and heap frees in `ProcessBlock()`.
3. **Dynamic block/channel allocation:** plug-in, gate, filter, and IR buffer preparation can allocate when dimensions change.
4. **IR history allocation:** first processing and larger blocks can grow history in the audio callback.
5. **Gate envelope copying:** a nested vector is copied from trigger to gain every block.
6. **Host latency update:** model exchange can call `SetLatency()` from inside `ProcessBlock()`.
7. **Exceptions in audio code:** buffer/channel validation can throw exceptions from functions called by `ProcessBlock()`.
8. **Restore-thread assumption:** state restoration performs locking and synchronous model/IR file loading; safety depends on the host not invoking it on its real-time thread.
9. **Parameter/update concurrency:** tone-stack coefficients and slim-model configuration can be modified by parameter callbacks while processing unless iPlug's callback/thread guarantees or the underlying classes provide sufficient synchronization.

These risks are inherited observations, not evidence that a failure has occurred. They should be addressed incrementally with measurement and regression tests rather than mixed into one signal-chain rewrite.
