# Danger Guitar Amps

Danger Guitar Amps is a Windows x64 VST3 guitar-processing plug-in built around
the open-source [Neural Amp Modeler](https://github.com/sdatkinson/neural-amp-modeler)
technology. It adds a distinct plug-in identity and a compact rack-style signal
chain for loading NAM models, speaker impulse responses, corrective EQ, and a
separate reverb impulse response.

This is an independent derivative project. It is not the official Neural Amp
Modeler plug-in and is not affiliated with or endorsed by the Neural Amp Modeler
project or its maintainers.

## Release status

The current release candidate is **Danger Guitar Amps 1.0.0-rc2**. RC2 is
intended for testing before the final V1 release. Save backups of important DAW
projects and report reproducible problems through the
[issue tracker](https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues).

The RC2 Windows binary is currently distributed unsigned. Windows may therefore
show an unrecognised-publisher warning when handling a downloaded archive.

## Features

- Loads compatible Neural Amp Modeler (`.nam`) amplifier models.
- Loads a dedicated speaker/cabinet WAV impulse response.
- Noise gate with adjustable threshold.
- Pre-EQ with low cut, body, mid-frequency/mid-gain, and attack controls.
- Four-band corrective Post-EQ with dedicated low-cut and high-cut filters.
- Independent reverb WAV impulse response with mix, pre-delay, wet filtering,
  wet level, and bypass controls.
- Input and output gain controls.
- Distinct VST3 identity, so it can coexist with the official Neural Amp Modeler
  plug-in.
- Version-aware session restoration for Danger Guitar Amps projects.

## Signal chain

```text
Input
-> Noise Gate
-> Pre-EQ
-> NAM model
-> Speaker IR
-> Post-EQ
-> Reverb IR
-> Output
```

The Pre-EQ, Post-EQ, and Reverb IR stages can be bypassed independently. The
legacy NAM Bass/Middle/Treble tone-stack parameter IDs remain reserved for old
session compatibility but are not part of the active V1 signal path.

## System requirements

- Windows 10 or Windows 11, 64-bit.
- A 64-bit VST3-compatible host such as REAPER.
- An x64 processor suitable for real-time NAM model processing.
- Compatible `.nam` model files and WAV impulse responses are supplied by the
  user; they are not bundled with the plug-in.

## Installation

1. Download the Windows x64 VST3 RC2 archive from the repository's
   [Releases](https://github.com/DangerGuitarAmps/DangerGuitarAmps/releases)
   page.
2. Close the DAW before installing or replacing the plug-in.
3. Extract the complete `DangerGuitarAmps.vst3` directory. Do not extract or
   copy only the binary inside it.
4. Copy the bundle to:

   ```text
   C:\Program Files\Common Files\VST3\DangerGuitarAmps.vst3
   ```

5. Start the DAW and rescan its VST3 paths. In REAPER, open **Options >
   Preferences > Plug-ins > VST**, then use **Re-scan**. If an older build is
   cached, use **Clear cache and re-scan VST paths for all plug-ins**.
6. Search for **Danger Guitar Amps** in the host's plug-in browser.

For a reproducible source build, see
[`docs/WINDOWS_BUILD.md`](docs/WINDOWS_BUILD.md). That document records the
upstream baseline procedure; the current Danger bundle is named
`DangerGuitarAmps.vst3`.

## Reporting issues

Before filing a bug, search the
[existing issues](https://github.com/DangerGuitarAmps/DangerGuitarAmps/issues).
Use the bug-report template and include:

- the exact Danger Guitar Amps version;
- Windows and DAW versions;
- sample rate, buffer size, and audio interface;
- whether the problem occurs without a NAM model, Speaker IR, or Reverb IR;
- the smallest set of steps that reproduces the problem;
- crash logs, screenshots, and non-commercial test files when relevant and
  legally shareable.

Security-sensitive reports should not include credentials, private model
collections, or other confidential material in a public issue.

## Building and contributing

Clone recursively because the project uses Git submodules. The supported V1
Windows build uses Visual Studio 2022, the Desktop development with C++ workload,
MSVC v143, the Windows 10/11 SDK, and the `NeuralAmpModeler-vst3` project in
`NeuralAmpModeler/NeuralAmpModeler.sln` with `Release|x64` selected.

Read [`CONTRIBUTING.md`](CONTRIBUTING.md) before proposing code or documentation
changes.

## Credits and upstream attribution

Danger Guitar Amps is derived from
[NeuralAmpModelerPlugin](https://github.com/sdatkinson/NeuralAmpModelerPlugin)
and retains its core architecture and compatibility work. Neural Amp Modeler and
NeuralAmpModelerCore were created by
[Steven Atkinson](https://github.com/sdatkinson).

The project also uses:

- [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore)
- [AudioDSPTools](https://github.com/sdatkinson/AudioDSPTools)
- [iPlug2](https://github.com/iPlug2/iPlug2)
- [Eigen](https://eigen.tuxfamily.org/)
- the Steinberg VST3 SDK

Thank you to all upstream authors and contributors. Their copyright statements
and licence terms remain applicable to their work.

## Licence

The inherited NeuralAmpModelerPlugin code is provided under the MIT licence; see
the root [`LICENSE`](LICENSE), which preserves the upstream copyright and licence
notice. NeuralAmpModelerCore, AudioDSPTools, iPlug2, Eigen, the VST3 SDK, and
other bundled components are covered by their own licences.

See [`NeuralAmpModeler/installer/ThirdPartyNotices.txt`](NeuralAmpModeler/installer/ThirdPartyNotices.txt)
and the licence files distributed with each dependency for the complete notices.
Do not remove those files when redistributing source or binaries.
