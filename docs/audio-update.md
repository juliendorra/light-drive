# Audio update 1.3.11

Sparse ambient chimes move up two octaves for the small Rev0 sounder. Their quiet pulse width, spacing and rests remain. Hazard cues retain precedence, and the existing game/save behavior is retained.

Validation uses the actual C++ audio backend and native Rev0 builds. Digital
levels and source checks do not establish measured acoustic loudness or
physical timer margin. These releases retain each public repository's
existing build and save setup; workspace console builds also included a
separate SD-save overlay that is not part of this audio change.

## Release build validation

Built against the clean upstream SDK at
`e1f8d05940ddb7bbef89c13326da75c003f02904` with GCC 16.2.

```text
LightDrive: build
flash (sections): 46610 B
image: 46904 B of 50944 (save pages free: 2; two need <= 50432)
static RAM: 18404 B of 18416 (stack 2048 B separate)
```
