# Danger Guitar Amps rebrand plan

## Goal and constraints

Create a distinct plug-in named **Danger Guitar Amps** that can be installed beside the official Neural Amp Modeler plug-in. Coexistence requires a new product identity at every host-visible and installer-visible layer; changing only the displayed name or file name is insufficient.

Keep the DSP implementation and its upstream relationship intact. In particular, do not rename `NeuralAmpModelerCore`, the `nam` namespace, or the `NeuralAmpModelerCore` submodule path. Do not alter model mathematics as part of the rebrand.

## Identity decisions

Use a single identity table as the source of truth before editing production files:

| Identity | Upstream value | Planned value |
| --- | --- | --- |
| Plug-in display name | `NeuralAmpModeler` | `Danger Guitar Amps` |
| Manufacturer display name | `Steven Atkinson` | A stable owner/publisher name chosen by the fork owner, for example `Danger Audio` |
| `PLUG_NAME` | `"NeuralAmpModeler"` | `"Danger Guitar Amps"` |
| `PLUG_MFR` | `"Steven Atkinson"` | the chosen manufacturer string |
| `PLUG_UNIQUE_ID` | `'1YEo'` | a newly assigned four-character code, provisionally `'DGAm'` |
| `PLUG_MFR_ID` | `'SDAa'` | a newly assigned manufacturer code, provisionally `'DnGA'` |
| Binary name | `NeuralAmpModeler` | `DangerGuitarAmps` |
| Bundle name | `NeuralAmpModeler` | `DangerGuitarAmps` |

The provisional four-character IDs must be checked for collisions across every product published by the chosen manufacturer before release. Once a build is distributed, freeze them permanently. Changing either ID later can make hosts treat the plug-in as another product and can break session recall.

## VST3 class identity and coexistence

iPlug2 derives the VST3 class identity from the configured plug-in and manufacturer IDs. Confirm this in the pinned iPlug2 implementation before editing, then generate the fork with the new `PLUG_UNIQUE_ID` and `PLUG_MFR_ID`. Validate the resulting module with Steinberg's VST3 validator and verify in at least REAPER that upstream and fork appear as separate entries.

Do not reuse the official VST3 class ID. Hosts cache VST3 plug-ins primarily by class identity, not by the visible name or bundle filename. A new class identity intentionally prevents existing projects containing the official plug-in from silently resolving to Danger Guitar Amps.

## Planned rename surface

### Product configuration

Update the fork's product configuration consistently:

- `PLUG_NAME`, `PLUG_MFR`, `PLUG_UNIQUE_ID`, and `PLUG_MFR_ID`.
- `PLUG_CLASS_NAME`, `BUNDLE_NAME`, `BUNDLE_MFR`, URLs, support email, and copyright ownership where legally appropriate.
- `BINARY_NAME` in the Windows property sheet so the linked binary and packaged bundle become `DangerGuitarAmps.vst3`.
- Format-specific names and entry points if formats beyond VST3 are later shipped. Allocate separate valid IDs where a format requires them; do not leave placeholder AAX identity values in a released AAX build.

The C++ product class may be renamed in a dedicated mechanical commit, but that is separate from—and must not affect—the `nam` namespace or core submodule identity.

### Binary, bundle, and VST3 metadata

The Windows deliverable should be:

```text
DangerGuitarAmps.vst3\
  Contents\
    x86_64-win\
      DangerGuitarAmps.vst3
```

Update every build/package reference that assumes `NeuralAmpModeler.vst3`. Ensure the VST3 factory reports `Danger Guitar Amps`, the chosen manufacturer, the new class identity, the intended category, and the fork's version. Test installation beside—not over—the official `NeuralAmpModeler.vst3` bundle.

### Windows resources

Audit `NeuralAmpModeler/resources/main.rc`, `resource.h`, icons, manifests, and resource-preparation scripts. Replace product-facing values such as `FileDescription`, `InternalName`, `OriginalFilename`, `ProductName`, company/manufacturer, copyright, dialog captions, and icon filenames. Keep version resources synchronized with the plug-in version. New artwork must not imply that the fork is the official Neural Amp Modeler product.

### Installer identity

Give the installer an independent identity and uninstall record:

- New Inno Setup `AppId` GUID; never reuse the upstream installer identity.
- `AppName`, publisher, URLs, default directory, Start Menu group, output filename, welcome text, and uninstall display name changed to Danger Guitar Amps.
- Source and destination bundle paths changed to `DangerGuitarAmps.vst3`.
- Upgrade behavior scoped only to prior Danger Guitar Amps installations.
- Uninstall logic prohibited from deleting `NeuralAmpModeler.vst3` or official user data.

Test install, upgrade, repair, and uninstall with both products installed.

### Application data, resources, and user files

Change `SHARED_RESOURCES_SUBPATH` and every product-owned application-data or resource directory to a fork-specific path such as `DangerGuitarAmps`. Audit `%APPDATA%`, `%LOCALAPPDATA%`, Documents, model/IR browsing defaults, logs, caches, preferences, recently used paths, and shared-resource lookup. Do not migrate, overwrite, or delete official NAM data automatically.

NAM model files are user content and should remain readable. If convenient, the fork may let users browse to existing `.nam` model libraries without taking ownership of those folders. Store new preferences and caches separately.

### Presets, saved state, and session compatibility

Treat the new VST3 class identity as a compatibility boundary:

- Existing DAW sessions using official Neural Amp Modeler must continue loading the official plug-in.
- Danger Guitar Amps must save and recall its own instances under its new identity.
- Do not promise automatic substitution between the two products.
- Preserve parameter order, IDs, ranges, defaults, channel layouts, and state serialization while rebranding so fork presets remain stable across fork releases.
- Add an explicit state schema/version only if a later functional change requires migration.
- Test empty/default state, all parameters, model and IR selections, missing external files, preset save/load, project save/reopen, and automation recall.

If importing an upstream preset or state is desired, design it later as an explicit, read-only import path with validation; do not achieve it by reusing the official class ID.

### Project and solution names

Eventually copy/rename the product-facing solution and projects to `DangerGuitarAmps.sln`, `DangerGuitarAmps-vst3`, and corresponding project filenames. Allocate new Visual Studio project GUIDs when the old and new projects might coexist in one solution. Update `.filters`, `.user` exclusions, scripts, resource paths, property-sheet imports, and solution entries mechanically.

Project/solution renames should not rename dependency directories, `NeuralAmpModelerCore`, its Git submodule mapping, or the `nam` namespace. Keep core references explicit so upstream core updates remain reviewable.

## Hard-coded string audit

Before each implementation phase, inventory case-sensitive and case-insensitive occurrences of at least:

```bash
git grep -n -i -e NeuralAmpModeler -e "Neural Amp Modeler" -e sdatkinson -e "Steven Atkinson"
```

Classify each result instead of applying a blind global replacement:

- **Replace:** host-visible product names, binary/bundle names, resource metadata, installer text, product-owned paths, support links, project labels, manuals, and fork artwork references.
- **Retain with attribution:** license notices, third-party notices, source history, upstream URLs needed by license/source-availability obligations, acknowledgements, and descriptions of compatible NAM model files.
- **Never rename for this rebrand:** `NeuralAmpModelerCore`, its submodule path, and the `nam` namespace.

Repeat the audit for spaced, lowercase, abbreviated, and filename variants. Inspect generated resource scripts and installer scripts because they can recreate old names after an apparently successful source rename.

## Documentation and artwork

Create fork-specific README, build/install guide, user guide, screenshots, icons, installer graphics, and UI branding. Clearly state that Danger Guitar Amps is a distinct fork/product and is not the official Neural Amp Modeler plug-in. Preserve required licenses, copyright notices, third-party notices, VST3 SDK notices, and upstream attribution. Review names, logos, and artwork for trademark and licensing concerns before distribution.

Do not rewrite historical or dependency documentation merely to remove upstream names. Documentation about model compatibility may accurately refer to the Neural Amp Modeler ecosystem.

## Small, reversible commit sequence

Keep every step buildable where practical and avoid mixing identity, artwork, and behavior:

1. **Document baseline build and identity map.** Add reproducible build notes, this plan, and a recorded clean upstream VST3 smoke test.
2. **Add coexistence tests/checklist.** Record upstream/fork install locations, REAPER discovery expectations, and validator commands without changing identity yet.
3. **Change core plug-in identity constants.** Update display/manufacturer strings and allocate the final plug-in/manufacturer IDs; build and verify that the VST3 class ID differs.
4. **Change binary and bundle names.** Update the Windows product property sheet and post-build/package references; verify the complete bundle layout.
5. **Separate application-data paths.** Change product-owned resource, preference, cache, and recent-file locations; test with upstream data present.
6. **Update Windows resource metadata.** Change version strings, captions, icon references, and executable metadata; inspect with Windows Properties and a resource viewer.
7. **Rename product projects and solution.** Perform only mechanical project/solution renames and GUID changes; rebuild from a clean checkout.
8. **Create independent installer identity.** Add the new `AppId`, paths, publisher text, and safe upgrade/uninstall rules; test side-by-side lifecycle operations.
9. **Replace documentation and artwork.** Add fork branding and required attribution; avoid production behavior changes.
10. **Audit remaining strings.** Classify every upstream-name occurrence, fix only unintended product-facing remnants, and record intentional exclusions.
11. **Compatibility test commit.** Add results for REAPER rescan, dual installation, project recall, automation, presets/state, model and IR paths, validator output, installer upgrade, and uninstall.

Tag the verified upstream build before the first identity change. Tag the first side-by-side release candidate only after both products load simultaneously and uninstalling either leaves the other operational.
