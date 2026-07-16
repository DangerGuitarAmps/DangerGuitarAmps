# Deprecated parameters

Danger Guitar Amps V1 reserves the following legacy tone-stack parameter IDs
for host automation and session-format compatibility:

| ID | Enum | Host name | V1 behaviour |
|---:|---|---|---|
| 2 | `kToneBass` | `Bass` | Inactive; restored legacy values are ignored |
| 3 | `kToneMid` | `Middle` | Inactive; restored legacy values are ignored |
| 4 | `kToneTreble` | `Treble` | Inactive; restored legacy values are ignored |
| 7 | `kEQActive` | `ToneStack` | Inactive; restored legacy values are ignored |

These IDs must not be deleted, renumbered, or reused. They are deliberately not
shown in the custom interface. Historical state readers still consume their
serialized values so that all later parameter values remain correctly aligned.

The active V1 chain routes Speaker IR directly into Post-EQ.

## Removed Reverb IR parameters

IDs 13–18 (`kReverbIRBypass` through `kReverbIRWetLevel`) remain host-visible
and serialized solely to preserve automation and state alignment. They do not
construct, load, or process a Reverb IR, and are not shown in the custom UI.
The historical Reverb IR path and stereo-format field are still consumed when
old sessions are read, then discarded safely.

## Replaced parametric Post-EQ parameters

The bypass at ID 25 (`kPostEQBypass`) is retained as the bypass for the active
nine-band graphic EQ. The earlier variable-frequency Post-EQ controls at IDs
26–39 remain reserved and serialized, but are inactive:

| IDs | Enums | V1 behaviour |
|---:|---|---|
| 26 | `kPostEQLowCut` | Inactive |
| 27–29 | `kPostEQBand1Frequency` through `kPostEQBand1Q` | Inactive |
| 30–32 | `kPostEQBand2Frequency` through `kPostEQBand2Q` | Inactive |
| 33–35 | `kPostEQBand3Frequency` through `kPostEQBand3Q` | Inactive |
| 36–38 | `kPostEQBand4Frequency` through `kPostEQBand4Q` | Inactive |
| 39 | `kPostEQHighCut` | Inactive |

Historical readers continue consuming these values to preserve state alignment.
The active fixed-frequency gains and Post Level are appended at IDs 40–49.
