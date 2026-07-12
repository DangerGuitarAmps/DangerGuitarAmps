# Reproducible upstream Windows VST3 build

This procedure reproduces the successful build of the unmodified upstream Neural Amp Modeler VST3 on Windows x64.

## Prerequisites

- Windows 10 or Windows 11, x64.
- Visual Studio 2022 Community (or another Visual Studio 2022 edition) with the **Desktop development with C++** workload. Include the MSVC v143 x64/x86 build tools and a Windows 10 or Windows 11 SDK.
- Git for Windows, including Git Bash.
- REAPER x64 for the installation check.

## Clone the source and submodules

From PowerShell:

```powershell
cd C:\Dev\GuitarPlugin
git clone --recursive https://github.com/sdatkinson/NeuralAmpModelerPlugin.git
cd NeuralAmpModelerPlugin
git submodule update --init --recursive
git status
```

Before building, `git status` should report a clean working tree. The recursive operations populate `AudioDSPTools`, `NeuralAmpModelerCore`, `eigen`, `iPlug2`, and their nested submodules.

## Download the VST3 SDK

Run the iPlug2 downloader from Git Bash:

```bash
cd /c/Dev/GuitarPlugin/NeuralAmpModelerPlugin/iPlug2/Dependencies/IPlug
./download-vst3-sdk.sh
```

This creates `iPlug2/Dependencies/IPlug/VST3_SDK` and checks out the SDK's required submodules.

## Build the VST3

1. Open `NeuralAmpModeler/NeuralAmpModeler.sln` in Visual Studio 2022.
2. In the solution configuration selectors, choose **Release** and **x64**.
3. In Solution Explorer, select the `NeuralAmpModeler-vst3` project.
4. Build that project with **Build > Build NeuralAmpModeler-vst3** (or right-click the project and choose **Build**).
5. Confirm that Visual Studio reports a successful build.

The linker output is:

```text
NeuralAmpModeler\build-win\vst3\x64\Release\NeuralAmpModeler.vst3
```

The post-build script packages that binary into the deployable bundle:

```text
NeuralAmpModeler\build-win\NeuralAmpModeler.vst3\
  Contents\
    x86_64-win\
      NeuralAmpModeler.vst3
```

When the system VST3 directory already exists and is writable, the upstream post-build step may also copy the bundle to `C:\Program Files\Common Files\VST3\NeuralAmpModeler.vst3`. Otherwise, install it manually as described below.

## Install and rescan in REAPER

1. Close REAPER before replacing an existing copy.
2. Copy the complete `NeuralAmpModeler\build-win\NeuralAmpModeler.vst3` directory to:

   ```text
   C:\Program Files\Common Files\VST3\NeuralAmpModeler.vst3
   ```

   Administrator permission may be required. Copy the bundle directory, not only the inner binary.
3. Start REAPER x64.
4. Open **Options > Preferences > Plug-ins > VST**.
5. Confirm that `C:\Program Files\Common Files\VST3` is covered by the VST plug-in paths.
6. Choose **Re-scan > Clear cache and re-scan VST paths for all plug-ins**. A normal **Re-scan** is sufficient on later unchanged-path builds, but clearing the cache is the reliable first-install check.
7. Create or select a track, open **FX**, search for `NeuralAmpModeler`, and insert the VST3.
8. Verify that the UI opens and audio passes through before making any rebrand changes.

## Reproducibility check

After the build, source control should still be clean:

```powershell
git status
```

Build products are ignored and should not be committed. This document describes the upstream baseline only; it does not require changes to production source, project files, dependencies, DSP, or build configuration.
