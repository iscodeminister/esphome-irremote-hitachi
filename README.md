# ESPHome IRremote Hitachi

An ESPHome external climate component for Hitachi air conditioners using the
`HITACHI_AC344` protocol from
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266).

## Features

- Cooling, heating, dry, fan-only, and off modes
- Auto, low, medium, and high fan modes
- Vertical swing control
- Optional current-temperature sensor
- Restores the climate state after reboot

## Requirements

- ESP32
- Arduino framework
- An infrared LED/transmitter connected to an output-capable GPIO

The component drives the infrared GPIO directly through IRremoteESP8266. Do
not configure ESPHome's `remote_transmitter` on the same pin.

## Usage

```yaml
external_components:
  - source: github://iscodeminister/esphome-irremote-hitachi
    components: [irremote_hitachi]

climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    pin: GPIO4
```

To report the current room temperature, provide an ESPHome sensor ID:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    pin: GPIO4
    sensor: room_temperature
```

See [examples/basic.yaml](examples/basic.yaml) for a complete minimal device
configuration.

## Configuration

| Option | Required | Description |
| --- | --- | --- |
| `pin` | Yes | GPIO connected to the infrared transmitter. |
| `sensor` | No | Sensor whose state is exposed as the current temperature. |

All standard ESPHome climate options are also supported.

## License

MIT
