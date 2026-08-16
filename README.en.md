# ESPHome IRremote Hitachi

[繁體中文](README.md) · **English**

`irremote_hitachi` is an external ESPHome `climate` component designed primarily for
older fixed-speed Hitachi air conditioners that use the 104-bit (13-byte)
`HITACHI_AC1` infrared protocol. It also supports the `HITACHI_AC344` protocol.

The component uses [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)
to build the protocol state, then sends the infrared signal through ESPHome's
`remote_transmitter`. It also includes compatibility fixes for cases where the actual
behavior of certain `HITACHI_AC1` remotes differs from the upstream library.

This component only transmits infrared signals. It does not support IR receiving or
remote-control learning.

> [!IMPORTANT]
> This is not a universal component for every Hitachi air conditioner. Newer inverter
> models may use completely different infrared protocols. A similar-looking remote does
> not necessarily mean that the unit uses `HITACHI_AC1`. Check the signal format of the
> original remote before using this component.

## Intended use

This component fills a gap in ESPHome support for some older fixed-speed Hitachi air
conditioners. It primarily targets:

- 104-bit (13-byte) `HITACHI_AC1` infrared frames
- `R_LT0541_HTA_A` and `R_LT0541_HTA_B` remote models
- IE-06T2-type remotes
- Older fixed-speed Hitachi window or split air conditioners that use these protocols

The Model B behavior used by the IE-06T2 differs from a typical `HITACHI_AC1`
implementation, so this project handles its observed hardware behavior separately.

## Known related units and remotes

According to the upstream IRremoteESP8266 documentation, devices associated with
`HITACHI_AC1` include:

- Hitachi `LT0541-HTA` remote
- Hitachi `R-LT0541-HTA/Y.K.1.1-1 V2.3` remote
- Hitachi Series VI air conditioner (circa 2007)
- Hitachi `KAZE-312KSDP`
- Hitachi IE-06T2-type remote (this project includes hardware-tested Model B compatibility)

Similar-looking remotes from different regions, years, or product lines may use
different infrared encodings. Treat this list as a list of known related devices, not
as a claim that every listed unit has been tested with this project.

## Features

- Supports the `HITACHI_AC1` and `HITACHI_AC344` protocols
- Supports the `R_LT0541_HTA_A` and `R_LT0541_HTA_B` `HITACHI_AC1` remote models
- Supports off, cool, heat, dry, and fan-only modes
- Supports auto, low, medium, and high fan speeds
- Provides vertical swing when supported by the selected protocol and model
- Can display an ESPHome temperature sensor as the current room temperature
- Can optionally synchronize Model B power state from an ESPHome power sensor
- Restores the climate state after ESPHome restarts
- Sends 38 kHz infrared signals through ESPHome's `remote_transmitter`

## Support matrix

| Protocol | Model | Power control | Vertical swing |
| --- | --- | --- | --- |
| `HITACHI_AC1` | `R_LT0541_HTA_A` | Explicit on/off | Supported |
| `HITACHI_AC1` | `R_LT0541_HTA_B` (IE-06T2) | Toggle only | Not supported |
| `HITACHI_AC344` | Not applicable | Explicit on/off | Supported |

## IE-06T2 / Model B compatibility

### Power is a toggle, not an explicit on/off state

The IE-06T2-type `HITACHI_AC1 Model B` remote uses a momentary `POWER TOGGLE`
command rather than separate `POWER = ON` and `POWER = OFF` states. Every valid power
command simply reverses the unit's current state:

```text
OFF → ON
ON  → OFF
```

Model B therefore cannot use the usual state-based power control directly.

The general `IRHitachiAc1` implementation in IRremoteESP8266 handles both `Power` and
`PowerToggle`, but the actual behavior of the `IE-06T2 / R_LT0541_HTA_B` mainly depends
on the momentary `PowerToggle` command. Applying ESPHome's on/off state directly to the
usual AC1 power flow could leave the unit's real state out of sync with the logical
state shown in ESPHome.

For Model B, this component therefore:

- Tracks the logical power state in ESPHome
- Sets `PowerToggle` only when the requested power state changes
- Does not treat the `Power` bit as an explicit power state
- Clears the toggle command immediately after transmission
- Avoids toggling power when only the temperature or fan speed changes

The air conditioner does not report its actual power state. If it is operated with the
original remote or another controller, its state can still get out of sync with ESPHome
and may need to be realigned manually.

### Power-meter assisted power state

Hitachi AC1 remotes such as `R_LT0541_HTA_B` use a power-toggle command rather than an absolute ON/OFF command. Because infrared communication is one-way, using the original remote or a missed IR transmission can cause ESPHome's assumed power state to become incorrect.

An optional power sensor can be used to synchronize the actual ON/OFF state. This feedback option applies to the toggle-only AC1 Model B path:

```yaml
sensor:
  - platform: homeassistant
    id: ac_power
    entity_id: sensor.living_room_ac_power

climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
    power_sensor: ac_power
    power_on_threshold: 15
    power_off_threshold: 5
    power_on_delay: 3s
    power_off_delay: 30s
```

Configuration options:

- `power_sensor`: an existing ESPHome `sensor::Sensor` representing the AC's power in watts.
- `power_on_threshold`: the minimum wattage that can be considered ON. This is required when `power_sensor` is configured.
- `power_off_threshold`: the maximum wattage that can be considered OFF. This is required when `power_sensor` is configured.
- `power_on_delay`: how long the reading must remain at or above the ON threshold before ON is confirmed. The default is `3s`.
- `power_off_delay`: how long the reading must remain at or below the OFF threshold before OFF is confirmed. The default is `30s`.

The thresholds use hysteresis. Readings between `power_off_threshold` and `power_on_threshold` keep the last confirmed state, so the component does not repeatedly switch state around one boundary. The OFF delay is normally longer because a real AC can briefly draw little power during startup, fan changes, or control transitions.

Calibrate the thresholds from the complete appliance power profile:

1. Measure the AC while it is OFF or in standby.
2. Measure it while the indoor unit is actually ON, including periods when the compressor is idle.
3. Put `power_off_threshold` above the maximum OFF/standby reading and `power_on_threshold` below the minimum reading while the AC is ON.

Do not choose the ON/OFF thresholds from compressor power consumption. This component detects whether the air conditioner itself is ON, not whether its compressor is currently running. For example, an illustrative profile might be:

```text
AC OFF/standby: 1–3 W
AC ON with indoor fan running: 20–40 W
compressor running: 700 W+
```

A possible configuration for that example is `power_off_threshold: 5` and `power_on_threshold: 15`; these values are examples, not universal defaults. A non-inverter compressor can stop after reaching the target temperature while the indoor fan and control electronics remain powered. Choosing thresholds such as `power_off_threshold: 300` and `power_on_threshold: 500` could therefore report the AC as OFF whenever the compressor cycles off.

The power sensor should ideally measure only the air conditioner's circuit or device. If it includes unrelated loads, another appliance can keep the reading above the ON threshold and cause incorrect state detection. Meter feedback synchronizes only ON/OFF; it does not infer cool, dry, fan, heat, temperature, fan speed, or swing settings. If the sensor is omitted, the component continues to use its existing logical toggle behavior unchanged.

### Model B has no swing control

The IE-06T2 Model B tested by this project has no corresponding swing function. The
component therefore:

- Hides swing control in the ESPHome Climate UI
- Clears `SwingToggle`, `SwingV`, and `SwingH` before transmission

This prevents the component from generating states that the original remote would not send.

## Repeated transmissions

Repeating the same frame can sometimes improve reception for ordinary infrared remotes.
For an IE-06T2 Model B power toggle, however, sending multiple complete power frames is
not a safe way to improve reliability.

Every valid toggle changes the power state once. If the air conditioner accepts each
repeated frame as a separate command, the result could be:

```text
OFF → ON → OFF → ON
```

This component therefore maps one user-requested power state change to one logical
toggle action.

## Requirements

- ESP32
- ESPHome Arduino framework
- An infrared LED or suitable IR transmitter circuit
- An output-capable GPIO
- ESPHome `remote_transmitter`

The component's configuration is currently restricted to ESP32 with the Arduino
framework. ESP8266 and ESP-IDF are not supported. The component automatically adds
IRremoteESP8266 2.9.0.

## ESPHome configuration

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
  # AC1 frames are long. Use blocking mode if frames are incomplete or RMT underruns occur.
  non_blocking: false
```

The component sends frames with a 38 kHz carrier. `non_blocking: false` is generally
more reliable for the longer AC1 frames, so the complete example uses this setting.

### 3. Configure the climate component

#### HITACHI_AC1 / IE-06T2 Model B

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
```

#### HITACHI_AC1 Model A

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_A
```

#### HITACHI_AC344

`model` is not needed for `HITACHI_AC344`:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC344
```

See [examples/basic.yaml](examples/basic.yaml) for a complete minimal device configuration.

## External room temperature sensor

You can provide the ID of an existing ESPHome temperature sensor:

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
    sensor: room_temperature
```

This value is only displayed as the current temperature in ESPHome Climate. It does not
mean that the air conditioner reports its room temperature over infrared.

## Configuration options

| Option | Required | Default | Description |
| --- | --- | --- | --- |
| `transmitter_id` | Yes | — | ID of the configured ESPHome `remote_transmitter`. |
| `protocol` | No | `HITACHI_AC344` | Either `HITACHI_AC1` or `HITACHI_AC344`. |
| `model` | No | `R_LT0541_HTA_B` | AC1 remote model: `R_LT0541_HTA_A` or `R_LT0541_HTA_B`. Ignored for AC344. |
| `sensor` | No | — | ESPHome sensor ID used to display the current room temperature. |
| `power_sensor` | No | — | ESPHome power sensor ID for Model B ON/OFF synchronization. |
| `power_on_threshold` | With `power_sensor` | — | Required wattage threshold for confirming ON; must be greater than `power_off_threshold`. |
| `power_off_threshold` | With `power_sensor` | — | Required wattage threshold for confirming OFF; must be non-negative. |
| `power_on_delay` | No | `3s` | Time above the ON threshold before ON is confirmed. |
| `power_off_delay` | No | `30s` | Time below the OFF threshold before OFF is confirmed. |

Standard ESPHome Climate options are also available. The target temperature uses 1 °C
steps, and its allowed range depends on the selected protocol.

## Compatibility notes

If this component cannot control your Hitachi air conditioner, do not judge
compatibility from the brand, window/split form factor, remote appearance, or button
layout alone. The most reliable method is to capture the original infrared signal and
check:

- Whether the frame is approximately 104 bits (13 bytes)
- Whether the header timing matches `HITACHI_AC1`
- Initial frame bytes
- Model bits
- Mode, fan, and temperature fields
- Checksum
- Whether power uses a toggle

Once the protocol is confirmed, select the matching `protocol` and `model`.

## Troubleshooting

- **The air conditioner does not respond:** Check `transmitter_id`, the GPIO, the IR
  transmitter circuit, and carrier settings. Also verify that `protocol` and `model`
  match the actual remote.
- **AC1 frames are incomplete:** Set `remote_transmitter.non_blocking` to `false` first.
  AC1 frames are long, and the complete example already uses this setting.
- **Model B shows the opposite power state:** Model B can only send a toggle and cannot
  read the actual power state from the unit. Configure the optional power-meter feedback
  for automatic synchronization, or realign the state shown in ESPHome manually.
- **Platform validation fails:** This component requires an ESP32, the Arduino framework,
  and a configured `remote_transmitter`.

## License

MIT
