# Danger Guitar Amps Windows installer

The Windows installer uses Inno Setup 6 and the authoritative definition in
`DangerGuitarAmps.iss`. It packages only the validated x64 VST3 bundle from:

```text
NeuralAmpModeler/build-win/DangerGuitarAmps.vst3
```

The bundle is installed to the system VST3 directory:

```text
C:\Program Files\Common Files\VST3\DangerGuitarAmps.vst3
```

The fixed Inno Setup `AppId` is:

```text
{480062A0-2F26-47C3-A851-D9C5CB0BBD7E}
```

This is the stable Danger Guitar Amps upgrade and uninstall identity. Do not
change it for routine V1.x releases. It is deliberately unrelated to the
official Neural Amp Modeler installer identity.

Build from the repository root with:

```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" `
  "NeuralAmpModeler\installer\DangerGuitarAmps.iss"
```

The output is written to `release-artifacts`. The RC2 installer is unsigned and
must not be installed automatically as part of a build.

The root `LICENSE` and `ThirdPartyNotices.txt` are shown or installed by the
installer. Preserve all upstream Neural Amp Modeler and dependency attribution.
The installer creates no desktop or Start Menu shortcuts and does not install
models or impulse responses.
