# ESPHome IRremote Hitachi

An ESPHome external climate component for Hitachi air conditioners using the
`HITACHI_AC1` and `HITACHI_AC344` protocols from
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266).
IRremoteESP8266 builds the protocol state, while ESPHome's `remote_transmitter`
produces the carrier and IR timings through `RemoteTransmitData`.

## Features

- Selectable HITACHI_AC1 or HITACHI_AC344 protocol
- HITACHI_AC1 variants R_LT0541_HTA_A and R_LT0541_HTA_B
- Hitachi IE-06T2 toggle-only power behavior for HITACHI_AC1 Model B
- Cooling, heating, dry, fan-only, and off modes
- Auto, low, medium, and high fan modes
- Vertical swing control where supported (the IE-06T2 Model B has no swing)
- Optional current-temperature sensor
- Restores the climate state after reboot
- ESPHome `remote_transmitter` output, including hardware-backed ESP32 RMT

## Requirements

- ESP32
- Arduino framework
- An ESPHome `remote_transmitter` connected to an infrared LED/transmitter

## Usage

Configure the IR output through ESPHome. The transmitter controls the GPIO and
carrier duty cycle:

```yaml
remote_transmitter:
  id: ir_transmitter
  pin: GPIO4
  carrier_duty_percent: 50%
```

For a 104-bit `HITACHI_AC1` remote using model bits `0b01`, configure variant
`R_LT0541_HTA_B`:

```yaml
external_components:
  - source: github://iscodeminister/esphome-irremote-hitachi
    components: [irremote_hitachi]

climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
```

For an AC using `HITACHI_AC344`:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC344
```

To report the current room temperature, provide an ESPHome sensor ID:

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
| `protocol` | No | `HITACHI_AC344` | `HITACHI_AC1` or `HITACHI_AC344`. |
| `model` | No | `R_LT0541_HTA_B` | AC1 remote variant: `R_LT0541_HTA_A` or `R_LT0541_HTA_B`. Ignored for AC344. |
| `sensor` | No | — | Sensor whose state is exposed as the current temperature. |

All standard ESPHome climate options are also supported.

## License

MIT
