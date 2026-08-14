# ESPHome IRremote Hitachi

**文件語言：繁體中文（預設）** · [English](README.en.md)

`irremote_hitachi` 是一個 ESPHome 外部 `climate` 元件，透過紅外線控制支援
`HITACHI_AC1` 與 `HITACHI_AC344` 協定的 Hitachi 空調。協定狀態由
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266) 建立，再由
ESPHome 的 `remote_transmitter` 輸出載波與紅外線時序。

本元件只負責紅外線發送，不包含紅外線接收或遙控器學習功能。

## 功能特色

- 支援 `HITACHI_AC1` 與 `HITACHI_AC344` 協定
- 支援 `HITACHI_AC1` 的 `R_LT0541_HTA_A` 與 `R_LT0541_HTA_B` 遙控器變體
- 支援關機、冷氣、暖氣、除濕與送風模式
- 支援自動、低、中、高風速
- 在協定與機型支援時提供垂直擺風
- 可選擇 ESPHome 溫度感測器回報目前室溫
- 重新啟動後還原 climate 狀態
- 使用 ESPHome `remote_transmitter`，可搭配 ESP32 硬體 RMT

## 支援矩陣

| 協定 | 機型 | 電源行為 | 垂直擺風 |
| --- | --- | --- | --- |
| `HITACHI_AC1` | `R_LT0541_HTA_A` | 絕對開／關 | 支援 |
| `HITACHI_AC1` | `R_LT0541_HTA_B`（IE-06T2） | 僅支援切換（toggle） | 不支援 |
| `HITACHI_AC344` | 不適用 | 絕對開／關 | 支援 |

`HITACHI_AC1` 是 104-bit 長訊框。IE-06T2 Model B 的電源指令不是絕對
狀態，而是瞬時切換，因此元件會在軟體中追蹤邏輯電源狀態；如果空調曾被原廠
遙控器或其他控制器操作，軟體狀態可能需要重新同步。

## 使用需求

- ESP32
- ESPHome Arduino framework
- 已連接紅外線 LED／發射電路的 ESPHome `remote_transmitter`
- 元件會自動加入 `IRremoteESP8266` 2.9.0

此元件的設定結構在 Python codegen 層限制為 ESP32 與 Arduino framework；不支援
ESP8266 或 ESP-IDF。

## 快速開始

### 1. 加入外部元件

```yaml
external_components:
  - source: github://iscodeminister/esphome-irremote-hitachi
    components: [irremote_hitachi]
```

### 2. 設定紅外線發射器

```yaml
remote_transmitter:
  id: ir_transmitter
  pin: GPIO4
  carrier_duty_percent: 50%
  # AC1 訊框較長；若遇到傳送不完整或 RMT underrun，請使用 blocking 模式。
  non_blocking: false
```

元件會在訊框上使用 38 kHz 載波。`non_blocking: false` 對較長的 AC1 訊框較穩定，
因此完整範例保留此設定。

### 3. 建立 climate 元件

以下範例適用於使用 `R_LT0541_HTA_B` 遙控器的 104-bit `HITACHI_AC1` 空調：

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
```

若使用 `HITACHI_AC344`，不需要設定 `model`：

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC344
```

若要回報目前室溫，可指定 ESPHome 感測器的 ID：

```yaml
climate:
  - platform: irremote_hitachi
    name: "Hitachi AC"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
    sensor: room_temperature
```

完整的最小裝置設定請參考 [examples/basic.yaml](examples/basic.yaml)。

## 設定欄位

| 欄位 | 必填 | 預設值 | 說明 |
| --- | --- | --- | --- |
| `transmitter_id` | 是 | — | 已設定的 ESPHome `remote_transmitter` ID。 |
| `protocol` | 否 | `HITACHI_AC344` | 可選 `HITACHI_AC1` 或 `HITACHI_AC344`。 |
| `model` | 否 | `R_LT0541_HTA_B` | 僅適用於 AC1：`R_LT0541_HTA_A` 或 `R_LT0541_HTA_B`。AC344 會忽略此欄位。 |
| `sensor` | 否 | — | 將其狀態值作為目前溫度的 ESPHome 感測器 ID。 |

ESPHome 標準 climate 欄位仍可使用。目標溫度以 1 °C 為步進，實際可用範圍會依
所選協定限制並自動截斷。

## 狀態與控制行為

- 沒有可還原狀態時，初始模式為關機、目標溫度 24 °C、風速自動、擺風關閉。
- 若有已保存的 climate 狀態，元件會在啟動時還原該狀態。
- AC1 Model B 的電源是 toggle 指令；元件只在邏輯電源狀態改變時傳送切換。
- AC1 Model B 不提供擺風功能；其他支援的協定／機型提供關閉或垂直擺風。
- 指定 `sensor` 後，感測器更新會同步到 climate 的目前溫度。

## 專案結構

```text
components/irremote_hitachi/
├── __init__.py              # ESPHome 元件套件 metadata
├── climate.py               # 設定 schema 與 C++ code generation
├── irremote_hitachi.h       # C++ 類別與協定介面
└── irremote_hitachi.cpp     # 狀態處理與 IR 訊框發送

examples/basic.yaml          # 最小 ESPHome 範例
LICENSE                      # MIT 授權
README.md                    # 預設繁體中文文件
README.en.md                 # English 備用文件
```

資料流如下：

1. `climate.py` 驗證 YAML 並建立 C++ 元件。
2. C++ 元件使用 IRremoteESP8266 建立 Hitachi 協定狀態。
3. 元件將狀態序列化為 ESPHome `RemoteTransmitData`。
4. `remote_transmitter` 以 38 kHz 載波從紅外線發射電路送出訊框。

## 疑難排解

- **空調沒有反應**：確認 `transmitter_id`、GPIO、紅外線發射電路、載波設定，以及
  `protocol`／`model` 是否與實際遙控器相符。
- **AC1 訊框傳送不完整**：先將 `remote_transmitter.non_blocking` 設為 `false`；
  AC1 訊框較長，完整範例已採用此設定。
- **Model B 電源方向相反**：Model B 只能傳送 toggle，無法從空調讀回實際電源狀態。
  若空調曾被其他遙控器操作，請先讓軟體狀態與空調狀態重新對齊。
- **平台驗證失敗**：此元件需要 ESP32 與 Arduino framework，且必須配置
  `remote_transmitter`。

## 授權條款

MIT
