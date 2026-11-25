# Local hardware analysis inputs

The original uploaded schematic and PCB were inspected locally without changes.
Publishing those original design files was not part of the requested firmware
upload, so `*.kicad_sch` and `*.kicad_pcb` in this directory are Git-ignored.

To reproduce the connectivity check, copy your original files here as:

```text
hardware/Hbridge.kicad_sch
hardware/Hbridge.kicad_pcb
```

Then run `python3 scripts/check_hardware_nets.py` at the repository root.
[The hardware review](../docs/HARDWARE_REVIEW_KO.md) records their SHA-256 values,
connector mapping, detected driver/sensor configuration and validation scope.
