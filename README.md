# RC Car Lights Control

Lighting controller for a large-scale RC truck. An RP2040 board sits between the
FlySky receiver and a **MFC-03** multi-function unit, reads the receiver's **iBUS**
stream directly, and drives the vehicle's LED lighting from it — while still letting the
MFC-03 keep control of the things it does best (sound, engine start-up sequence,
turn-signal timing, brake detection).

The MCU is both a *consumer* of the MFC-03 (it watches the MFC's own light and
turn-signal outputs) and a *producer* for it (it synthesises a servo signal into the MFC's
light channel so the MFC's internal "lights on" state stays in sync).

```
FlySky FS-i6X  ──2.4GHz──►  FS-iA6B RX ──┬── iBUS (UART, 115200) ──► RP2040 ──► LED outputs
                                          │                            ▲  │
                                          └── (channels) ──► MFC-03 ───┘  │
                                                                ▲          │
                                                                └──────────┘
                                                            synthesised servo signal
```

## Repository layout

| Path | Contents |
| --- | --- |
| `FW/` | Firmware — C, Raspberry Pi Pico SDK ≥ 2.1.0 |
| `HW/rccarcontrol/` | KiCad 9 project: schematic, PCB, gerbers, DRC report |
| `doc/` | MFC-03 manual, FS-i6X manual, output mapping spreadsheet, wiring manual |
| `sim/` | Multisim analog simulation (`Design1.ms14`) |

## Hardware

The board is built around a **USB-C RP2040 "Pico C" module** (`HW/.../rccarcontrol.kicad_sch`,
symbol `Pico_type_c`). Note this is *not* pin-compatible with a genuine Raspberry Pi Pico:
the module breaks out **GPIO29 on pad 35** and **GPIO23 on pad 37**, where a stock Pico has
`ADC_VREF` and `3V3_EN`. The firmware drives both of those pins, so it will not run
correctly on a stock Pico without remapping `FRONT_RAMP_LIGHT_OUT_PIN` and
`TURN_R_FRONT_OUT_PIN`.

Interfacing:

- **Inputs from the MFC-03** go through `EL3H7` optocouplers (U2–U8), so they are
  **active-low** and the firmware enables internal pull-ups on all of them.
- **Outputs** drive `IRLML0030` N-MOSFETs. Solder jumpers `JP3`–`JP7` select `+7V2_LED` or
  `+5V_LED` per LED group; `JP1`/`JP2` bring the receiver / external supply onto `+5V`.
- **Reverse light** (`REVERSE_LIGHT_IN`, opto U1) is wired straight to Q1/Q2 in hardware and
  never reaches the MCU — there is intentionally no firmware for it.

### Pin map

Verified against the PCB netlist. "Pad" is the module pad number.

**Outputs**

| GPIO | Pad | Net | Firmware | Type |
| --- | --- | --- | --- | --- |
| 0 | 1 | `LED_POWER` | `lights.c` | digital — master LED supply gate |
| 1 | 2 | `LOW_BEAM_OUT` | `lights.c` | digital |
| 2 | 4 | `HIGH_BEAM_OUT` | `lights.c` | digital |
| 3 | 5 | `FOG_LIGHT_OUT` | `lights.c` | digital |
| 4 | 6 | `AUX_1_OUT` | `aux1.c` | PWM slice 2A, 50 Hz servo, **inverted** polarity |
| 5 | 7 | `AUX_2_OUT` | `main.c` | digital — toggled every loop pass (loop-rate test point) |
| 6 | 9 | `TURN_R_PWM_OUT` | `turn_light.c` | PWM slice 3A, 2 kHz |
| 7 | 10 | `TURN_L_PWM_OUT` | `turn_light.c` | PWM slice 3B, 2 kHz |
| 8 | 11 | `TURN_L_FRONT_OUT` | `turn_light.c` | digital — short front-indicator pulse |
| 9 | 12 | `SEARCH_LIGHT_OUT` | `lights.c` | digital |
| 10 | 14 | `REAR_LIGHT_PWM_OUT` | `rear_light.c` | PWM slice 5A, 500 Hz |
| 11 | 15 | `SIDE_LIGHT_1_OUT` | `lights.c` | digital |
| 12 | 16 | `SIDE_LIGHT_2_OUT` | `lights.c` | digital |
| 13 | 17 | `RX_PWM_OUT` | `to_mfc.c` | PWM slice 6B, 50 Hz servo → MFC light channel |
| 14 | 19 | `BEACON_SMALL_PWM_OUT` | `beacons.c` | PWM slice 7A, 50 Hz servo |
| 15 | 20 | `BEACON_BIG_PWM_OUT` | `beacons.c` | PWM slice 7B, 50 Hz servo |
| 23 | 37 | `TURN_R_FRONT_OUT` | `turn_light.c` | digital |
| 25 | — | on-board LED | `main.c` | boot-blink only |
| 27 | 32 | `DAY_LIGHT_OUT` | `lights.c` | digital — on permanently after init |
| 28 | 34 | `POSITION_LIGHT_OUT` | `lights.c` | digital |
| 29 | 35 | `FRONT_RAMP_OUT` | `lights.c` | digital |

**Inputs** (all pulled up, active low)

| GPIO | Pad | Net | Firmware | Purpose |
| --- | --- | --- | --- | --- |
| 16 | 21 | `TURN_R_IN` | `turn_light.c` | MFC right indicator |
| 17 | 22 | `LIGHT_0_IN` | `lights.c` | configured, **not read** |
| 18 | 24 | `LIGHT_1_IN` | `lights.c` | MFC light state + start-up flash pattern |
| 19 | 25 | `LIGHT_2_IN` | `lights.c` | configured, **not read** |
| 20 | 26 | `LIGHT_3_IN` | `lights.c` | configured, **not read** |
| 21 | 27 | `RX_SBUS_IN` | `ibus.c` | UART1 RX — iBUS, 115200 8N1 |
| 22 | 29 | `TURN_L_IN` | `turn_light.c` | MFC left indicator |
| 26 | 31 | `REAR_BRAKE_IN` | `rear_light.c` | ADC0 — MFC rear/brake light level |

### Transmitter channel map

From `FW/ibus.h` (`ibus_channel`), 1-based as shown on the FS-i6X:

| Ch | Name | Used by |
| --- | --- | --- |
| 1 | Steering | — (passes to MFC directly) |
| 2 | MFC light channel | `to_mfc.c`, `aux1.c` |
| 3 | Throttle | — |
| 4 | Throttle X | — |
| 5 | Mode switch | `aux1.c` |
| 6 | Beacon switch | `beacons.c` |
| 7 | Gear switch | — |
| 8 | Light switch | `lights.c` |
| 9 / 10 | Pot A / Pot B | — |

Channels 1, 3, 4, 7 and the pots are declared but not consumed by the firmware.

## Firmware

### Structure

Cooperative, non-blocking super-loop. Every module exposes `*_init()` (called once) and
`*_service()` (called every pass, returns immediately). No RTOS, no blocking delays after
start-up, no dynamic allocation. All timing uses `time_us_32()` deadlines compared as
signed differences, so 32-bit wrap is handled correctly.

| Module | Responsibility |
| --- | --- |
| `main.c` | Boot blink, init order, super-loop |
| `ibus.c` | iBUS frame decoding — the only interrupt-driven part |
| `lights.c` | Light-state machine, side lights, MFC light-state sync, start-up flash replay |
| `turn_light.c` | Indicator soft fade-in / fade-out driven by MFC edges |
| `rear_light.c` | Tail / brake light PWM, ADC brake detection |
| `beacons.c` | Rotating beacons, fog + search light, beacon configuration mode |
| `to_mfc.c` | Synthesised servo output back into the MFC light channel |
| `aux1.c` | Diverts the light channel to an auxiliary servo output (crane) |
| `switch.h` | Shared 3-position-switch state machine constants |

### iBUS decoder

`ibus.c` decodes the standard 32-byte FlySky iBUS servo frame:

```
0x20 0x40 <14 × uint16 LE channel values> <uint16 LE checksum>
```

The checksum is validated by summing all 32 bytes and requiring `0xFFFF`. Frame
synchronisation is done by **inter-byte gap**, not by header alone: a byte arriving more
than `SBUS_NEXT_FRAME_TIMEGAP` (3 ms) after the previous one is treated as a frame start.
Frames arrive roughly every 7 ms and each takes ~2.7 ms, so the gap is unambiguous.

The ISR writes into `channel_data`; `ibus_service()` copies it to `channel_data_saved`
with interrupts disabled, and `ibus_get_channel()` reads only the snapshot.

### Light state machine (`lights.c`)

Channel 8 acts as a momentary 3-position switch. Short pull **up** steps forward, holding
up for >1 s steps back:

```
OFF ──► POSITION ──► LOW_BEAM ──► HIGH_BEAM ──► RAMP
    ◄──          ◄──          ◄──           ◄──          (hold >1 s)
```

Pushing **down** flashes the front ramp light for as long as it is held.

Side lights (`SIDE_LIGHT_1/2`) have their own state: off, on with the position lights, a
double-flash alternating beacon pattern (when the rotating beacons are on), or a slow
0.5 s blink to indicate beacon-configuration mode.

### MFC light-state sync

The MFC-03 beeps when the engine is switched off with its lights still on, so its internal
state must track the real one. `update_lights_in_mfc()` compares `l_state` against
`LIGHT_1_IN` and, when they disagree, asks `to_mfc.c` to emit one 100 ms pulse train on the
synthesised light channel — the equivalent of one stick flick. It repeats every 0.5 s until
the states match.

### Start-up flash replay

On engine start the MFC-03 flashes its light output rapidly. `detect_starting_seq()`
detects this (edges closer than 25 ms apart), records the on/off durations into a 64-entry
ring buffer — duration in bits 0–14, level in bit 15 — and replays them onto `LED_POWER`
so the real headlights flicker in step with the MFC's simulated start-up. While the replay
is active, MFC light-state sync is suspended.

### Turn signals

The MFC drives the blink timing; the firmware only reshapes it. On each falling edge of
`TURN_L_IN` / `TURN_R_IN` it pulses the front indicator hard for 100 ms and simultaneously
ramps the PWM channel up (100 counts / 10 ms), holds at full for 100 ms, then ramps down
(50 counts / 7.5 ms) — roughly a 600 ms incandescent-style fade. Re-triggering is ignored
while a fade is still running.

### Rear / brake light

`REAR_BRAKE_IN` is sampled on ADC0 through a 32-sample moving average. Below
`REAR_LIGHT_BRAKE_LEVEL` the tail light PWM goes to full (brake); otherwise it follows the
main light state at 25 % duty, or off.

### Beacons

Two servo-protocol beacon outputs, driven as if by a stick:

- **Short pull up** — both beacons on, side lights switch to the alternating flash pattern.
- **Hold up >1 s** — both off. The small beacon has to be cycled through all four of its
  internal modes to reach off, which `beacon_small_off()` does with eight 50 ms pulses.
- **Hold up >5 s** — enter configuration mode: big beacon on, side lights blink slowly. In
  config, short up = next mode, short down = next brightness, hold up >5 s = exit.
- **Short pull down** — cycles the rear search light and the fog light. Hold down >1 s
  turns both off. The fog light only comes on if the main lights are on.

### Auxiliary output

When channel 5 is above 1800, `aux1.c` mirrors the channel-2 value onto `AUX_1_OUT`
(inverted polarity) for a crane servo, and tells `to_mfc.c` to park the MFC light channel at
centre so the MFC ignores it.

## Building

Requires the Raspberry Pi Pico SDK **2.1.0 or later**, `arm-none-eabi-gcc`, CMake ≥ 3.12
and `picotool`.

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cd FW
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Output is `build/car_lighting.uf2` (plus `.elf`, `.bin`, `.hex`, `.map`). The build prints
memory usage and a section size summary.

`CMakeLists.txt` hardcodes `picotool_DIR` to a local absolute path; override it with
`-Dpicotool_DIR=...` or edit it if picotool lives elsewhere. `stdio` over both USB and UART
is disabled — GPIO0/GPIO1 are used as light outputs, so UART0 is unavailable. To get printf
debugging you must free those pins first.

## Flashing

Hold `BOOTSEL` while plugging the board in and copy the `.uf2` to the `RPI-RP2` drive.

`FW/up.bat` automates this on Windows: it pokes the USB CDC port at 1200 baud to force the
board into BOOTSEL, finds the `RPI-RP2` volume and copies the image. It references helper
tools under `f:\_Work\_Programing\rpi\` and expects the artefact in
`cmake-build-debug-system-arm\`, so the paths need adjusting for the current build
directory before it will work.

## Known limitations

- **No receiver failsafe.** `ibus_get_channel()` returns the last valid frame forever. If
  the link drops, the lighting freezes in its last state and the synthesised MFC channel
  keeps repeating a stale value.
- **Boot blocks on iBUS.** `main()` spins in `while (!ibus_data_valid())` before any
  outputs are initialised, so with no receiver signal the board never leaves start-up.
- **No watchdog.**
- `LIGHT_0_IN`, `LIGHT_2_IN`, `LIGHT_3_IN` are wired and configured but unused.
