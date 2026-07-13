# Contributing to Danger Guitar Amps

Thanks for helping improve Danger Guitar Amps. The project is a derivative of
the Neural Amp Modeler plug-in, so changes must preserve upstream attribution,
licensing, model compatibility, and existing session compatibility.

## Before starting

Search the [issue tracker](https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues)
before opening a new report or beginning a substantial change. For a feature or
architectural change, open an issue first and describe the user problem, proposed
scope, compatibility impact, and testing plan.

Never submit third-party NAM models, impulse responses, artwork, or other assets
unless you have permission to redistribute them under terms compatible with this
repository.

## Development setup

Clone the repository and all submodules:

```powershell
git clone --recursive https://github.com/DangerGuitarAmps/DangerGuitarAmps.git
cd DangerGuitarAmps
git submodule update --init --recursive
```

The supported Windows VST3 build uses Visual Studio 2022 with the Desktop
development with C++ workload, MSVC v143, and a Windows 10 or Windows 11 SDK.
See [`docs/WINDOWS_BUILD.md`](docs/WINDOWS_BUILD.md) for the established setup
and build procedure.

## Keep changes focused

- Use a dedicated branch and small, reversible commits.
- Avoid unrelated formatting or generated-file changes.
- Do not upgrade dependencies as part of an unrelated fix.
- Do not rename `NeuralAmpModelerCore`, the `nam` namespace, or inherited core
  paths solely for branding.
- Preserve the Danger Guitar Amps product, manufacturer, bundle, and VST3 class
  identities unless an issue explicitly authorises an identity migration.

## Audio and session compatibility

Parameter IDs, enum order, host-visible automation, and serialized state are
compatibility contracts. Do not remove, reorder, renumber, or reuse a parameter
ID. Deprecated IDs documented in
[`docs/DEPRECATED_PARAMETERS.md`](docs/DEPRECATED_PARAMETERS.md) remain reserved.

Code reachable from the audio callback must not perform file I/O, heap
allocation, or blocking lock acquisition. Load and prepare NAM models and IRs
off the audio thread, then hand complete state to processing using the existing
staging design.

DSP changes should include a clear signal-flow description, expected audible
effect, neutral/bypass behaviour, real-time safety analysis, and regression
test plan.

## Style

C++ uses the repository's `.clang-format` configuration. Format relevant C++
changes with:

```bash
bash format.bash
```

Keep documentation concise, use relative repository links, and distinguish
Danger Guitar Amps from the official Neural Amp Modeler projects.

## Testing checklist

Run checks appropriate to the change and state clearly what was not tested.
For Windows VST3 changes, verify at minimum:

- [ ] `NeuralAmpModeler-vst3` builds as `Release|x64`.
- [ ] The complete `DangerGuitarAmps.vst3` bundle is produced.
- [ ] The plug-in scans under its distinct Danger Guitar Amps identity.
- [ ] Existing Danger projects restore without parameter shifts.
- [ ] Input, gate, Pre-EQ, NAM, Speaker IR, Post-EQ, Reverb IR, and output stages
      retain their intended order and bypass behaviour.
- [ ] Mono and stereo host routing are checked where the change can affect them.
- [ ] NAM model, Speaker IR, and Reverb IR loading are checked where relevant.
- [ ] No new real-time allocation, file I/O, or blocking lock was introduced.
- [ ] `git diff --check` passes and no build output is staged.

Where available, also validate the VST3 using Steinberg's VST3PluginTestHost.

## Pull requests

A pull request should include:

- a concise explanation of the problem and solution;
- linked issues;
- every changed file and why it changed;
- build and manual-test results;
- parameter/state compatibility impact;
- real-time safety impact for audio changes;
- screenshots for custom-UI changes;
- any remaining limitations or follow-up work.

By contributing, you agree that your contribution may be distributed under the
licensing terms applicable to the repository and the files you modify. Preserve
all existing upstream copyright and licence notices.
