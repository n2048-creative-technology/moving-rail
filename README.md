# moving-rail

Firmware, PCB, and 3D-printed parts for a motorized rail/turntable-style rig:
a stepper motor moves a carriage (for a projector or camera, per the 3D
parts) back and forth along a track, with a rotary encoder for manual
positioning and up to several saved stop positions recorded to EEPROM.

**Status: working core, rough edges.** The stepper motion and position
recording work; a couple of leftover TODOs and dead code from an earlier
remote-control library (`RemoteXY`) are still in the firmware — see Known
limitations.

## Hardware

- Wemos D1 Mini (ESP8266)
- Stepper motor + driver (STEP/DIR interface, e.g. A4988/DRV8825-style)
- Rotary encoder (quadrature, for manual jog/positioning)
- 2x limit switches (homing/end-of-travel)
- 3D-printed mechanical parts: `plate.stl`, `tripod.stl`, `disk.stl`,
  `stepper end.stl`, `tensor end.stl`, `off-center-tensor-end.stl`,
  `inner flex.stl`, `cam socket mount.stl`, `encoder mount.stl`
- `electronics.fzz` — Fritzing wiring diagram for the driver/encoder/switch
  connections

### Pin map (Wemos D1 Mini, from `moving_projector_v2.ino`)

| Signal | Pin | Notes |
|---|---|---|
| Rotary encoder A | D2 (GPIO4) | interrupt-driven quadrature counting |
| Rotary encoder B | D1 (GPIO5) | |
| Stepper DIR | D6 (GPIO12) | via `AccelStepper` |
| Stepper STEP | D7 (GPIO13) | |
| Stepper enable | D8 (GPIO15) | |
| Status LED | D4 (GPIO2) | |
| Limit switch 1 | D5 (GPIO14) | pull-up |
| Limit switch 2 | D0 (GPIO16) | pull-down |
| Mode button | D3 (GPIO0) | tied to the FLASH button — boot fails if held LOW |

## Firmware

`moving_projector_v2.ino` (Arduino IDE, `ESP8266WiFi` + `AccelStepper` +
built-in `EEPROM`). Serial commands (over USB, from the `Commands` struct in
the source): `help`, `clear`, `home`, `rec`, `debug [0|1]`,
`save <target>`, `run`, `stop`, `mem`, `pos`.

Positions are recorded to EEPROM as signed 16-bit values: 0–60000 is a
position, negative is a pause duration in milliseconds — see
`readIntFromEEPROM`/`writeIntIntoEEPROM` for the encoding.

Flash with the Arduino IDE (board: a generic ESP8266 board matching your
Wemos D1 Mini variant) or convert to PlatformIO.

## Known limitations

- Leftover dead code and comments from an earlier `RemoteXY` remote-control
  integration (`#include <ESP8266WiFi.h>` plus unused-looking scaffolding);
  the current control path is the serial `Commands` interface, not RemoteXY.
- A `// TODO you setup code` comment sits in `setup()` — worth reviewing
  before treating this as fully finished.
- No documented max travel / mechanical limits beyond what the limit
  switches enforce at runtime.
