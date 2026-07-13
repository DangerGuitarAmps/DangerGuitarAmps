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

## Post-EQ Q parameters

The following host parameters also remain reserved and serialized, but are
inactive in V1:

| ID | Enum | Host name | Fixed V1 Q |
|---:|---|---|---:|
| 29 | `kPostEQBand1Q` | `PostEQBand1Q` | 0.8 |
| 32 | `kPostEQBand2Q` | `PostEQBand2Q` | 1.2 |
| 35 | `kPostEQBand3Q` | `PostEQBand3Q` | 1.4 |
| 38 | `kPostEQBand4Q` | `PostEQBand4Q` | 0.9 |

Historical readers continue consuming these values to preserve state alignment,
but state application and host changes intentionally do not affect active DSP.
