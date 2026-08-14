# ESPHome IRremote Hitachi

[繁體中文（預設）](README.md) · **English**

`irremote_hitachi` is an ESPHome external `climate` component for controlling Hitachi
air conditioners that use the `HITACHI_AC1` or `HITACHI_AC344` protocol. The protocol
state is built with [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266),
then emitted through ESPHome's `remote_transmitter` as carrier and IR timings.

This component transmits infrared commands only; it does not implement IR receiving or
remote-control learning.

## Features

- Supports the `HITACHI_AC1` and `HITACHI_AC344` protocols
- Supports the `R_LT0541_HTA_A` and `R_LT0541_HTA_B` `HITACHI_AC1` remote variants
- Supports off, cooling, heating, dry, and fan-only modes
- Supports auto, low, medium, and high fan speeds
- Provides vertical swing when supported by the selected protocol and model
- Can expose an ESPHome temperature sensor as the current room temperature
- Restores the climate state after reboot
- Uses ESPHome `remote_transmitter`, including hardware-backed ESP32 RMT

## Support matrix

| Protocol | Model | Power behavior | Vertical swing |
| --- | --- | --- | --- |
| `HITACHI_AC1` | `R_LT0541_HTA_A` | Absolute on/off | Supported |
| `HITACHI_AC1` | `R_LT0541_HTA_B` (IE-06T2) | Toggle only | Not supported |
| `HITACHI_AC344` | Not applicable | Absolute on/off | Supported |

`HITACHI_AC1` uses a 104-bit long frame. IE-06T2 Model B does not accept an absolute
power state; it accepts a momentary toggle command instead. The component therefore
tracks logical power in software. If the unit is operated by the original remote or
another controller, the software state can become out of sync and may need to be
resynchronized.

## Requirements

- ESP32
- ESPHome Arduino framework
- An ESPHome `remote_transmitter` connected to a suitable IR LED/transmitter circuit
- The component automatically adds `IRremoteESP8266` 2.9.0

The Python code-generation layer restricts this component to ESP32 and the Arduino
framework. ESP8266 and ESP-IDF are not supported.

## Quick start

### 1. Add the external component

```yaml
external_components:
  - source: github://iscodeminister/esphome-irremote-hitachi
    components: [irremote_hitachi]
```

### 2. Configure the IR transmitter

```yaml
remote_transmitter:
  id: ir_transmitter
  pin: GPIO4
  carrier_duty_percent: 50%
  # AC1 frames are long; use blocking mode if transmission is incomplete or RMT underruns occur.
  non_blocking: false
```

The component uses a 38 kHz carrier for the frames. `non_blocking: false` is more
reliable for the longer AC1 frames, so the complete example keeps this setting.

### 3. Add the climate component

The following example targets a 104-bit `HITACHI_AC1` unit using the
`R_LT0541_HTA_B` remote variant:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
```

For `HITACHI_AC344`, `model` is not needed:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC344
```

To expose the current room temperature, provide an ESPHome sensor ID:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
    sensor: room_temperature
```

See [examples/basic.yaml](examples/basic.yaml) for a complete minimal device
configuration.

## Configuration

| Option | Required | Default | Description |
| --- | --- | --- | --- |
| `transmitter_id` | Yes | — | ID of the configured ESPHome `remote_transmitter`. |
| `protocol` | No | `HITACHI_AC344` | Either `HITACHI_AC1` or `HITACHI_AC344`. |
| `model` | No | `R_LT0541_HTA_B` | AC1 variant: `R_LT0541_HTA_A` or `R_LT0541_HTA_B`. Ignored for AC344. |
| `sensor` | No | — | ESPHome sensor ID whose state is exposed as the current temperature. |

Standard ESPHome climate options remain available. The target temperature uses a
1 °C step, and the supported range is clamped according to the selected protocol.

## State and control behavior

- Without a restored state, the component starts off with a 24 °C target, automatic fan,
  and swing off.
- If a saved climate state is available, it is restored during startup.
- AC1 Model B uses a toggle-only power command; a toggle is sent only when logical power
  changes.
- AC1 Model B has no swing capability. Other supported protocol/model combinations expose
  swing off and vertical swing.
- When `sensor` is configured, sensor updates are published as the climate current
  temperature.

## Project structure

```text
components/irremote_hitachi/
├── __init__.py              # ESPHome component package metadata
├── climate.py               # Configuration schema and C++ code generation
├── irremote_hitachi.h       # C++ class and protocol interface
└── irremote_hitachi.cpp     # State handling and IR frame transmission

examples/basic.yaml          # Minimal ESPHome example
LICENSE                      # MIT license
README.md                    # Default Traditional Chinese documentation
README.en.md                 # English fallback documentation
```

The data flow is:

1. `climate.py` validates YAML and creates the C++ component.
2. The C++ component uses IRremoteESP8266 to build the Hitachi protocol state.
3. The component serializes that state into ESPHome `RemoteTransmitData`.
4. `remote_transmitter` sends the frame through the IR circuit using a 38 kHz carrier.

## Troubleshooting

- **The air conditioner does not respond**: verify `transmitter_id`, GPIO, IR hardware,
  carrier settings, and that `protocol`/`model` match the actual remote.
- **AC1 frames are incomplete**: set `remote_transmitter.non_blocking` to `false` first;
  AC1 frames are long and the complete example already uses this setting.
- **Model B power is inverted**: Model B can only send a toggle and cannot report the unit's
  actual power state. If another remote has been used, resynchronize the software and unit
  state before sending more commands.
- **Platform validation fails**: this component requires ESP32 with the Arduino framework
  and a configured `remote_transmitter`.

## License

MIT
