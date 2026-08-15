# ESPHome IRremote Hitachi

**繁體中文** · [English](README.en.md)

`irremote_hitachi` 是一個 ESPHome 外部 `climate` 元件，主要用來控制採用
`HITACHI_AC1` 104-bit（13-byte）紅外線協定的舊款日立定頻冷氣，也保留對
`HITACHI_AC344` 協定的支援。

本元件以 [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)
建立協定狀態，再透過 ESPHome 的 `remote_transmitter` 發送紅外線訊號。針對部分
`HITACHI_AC1` 遙控器的實際行為與上游函式庫不完全一致之處，本專案也加入了相容性修正。

本元件只負責發送紅外線訊號，不包含紅外線接收或遙控器學習功能。

> [!IMPORTANT]
> 本元件並非適用於所有日立冷氣。新款變頻冷氣可能採用完全不同的紅外線協定；即使遙控器
> 外觀相似，也不代表能使用 `HITACHI_AC1`。使用前，建議先確認原廠遙控器發送的紅外線格式。

## 適用範圍

本元件是為了補足 ESPHome 對部分舊款日立定頻冷氣的控制需求，主要針對：

- `HITACHI_AC1` 104-bit（13-byte）紅外線封包
- `R_LT0541_HTA_A` 與 `R_LT0541_HTA_B` 遙控器型號
- `IE-06T2` 類型遙控器
- 使用上述協定的舊款日立定頻窗型或分離式冷氣

其中，`IE-06T2` 所使用的 Model B 與一般 `HITACHI_AC1` 實作有些差異，本專案已針對實機行為處理。

## 已知相關機種與遙控器

根據 IRremoteESP8266 上游資料，已知與 `HITACHI_AC1` 相關的設備包括：

- Hitachi `LT0541-HTA` 遙控器
- Hitachi `R-LT0541-HTA/Y.K.1.1-1 V2.3` 遙控器
- Hitachi Series VI 冷氣（約 2007 年）
- Hitachi `KAZE-312KSDP`
- Hitachi `IE-06T2` 類型遙控器（本專案已針對 Model B 進行實機相容性處理）

不同地區、年份與機型即使使用外觀相似的遙控器，也可能採用不同的紅外線編碼。因此，這份清單
只能視為已知採用相關協定的設備，並不表示所有列出的機型都經過本專案實機驗證。

## 功能

- 支援 `HITACHI_AC1` 與 `HITACHI_AC344` 協定
- 支援 `HITACHI_AC1` 的 `R_LT0541_HTA_A` 與 `R_LT0541_HTA_B` 遙控器型號
- 支援關機、冷氣、暖氣、除濕與送風模式
- 支援自動、低速、中速與高速風量
- 在所選協定與型號支援時提供上下擺風控制
- 可使用 ESPHome 溫度感測器顯示目前室溫
- ESPHome 重新啟動後可還原 climate 狀態
- 透過 ESPHome `remote_transmitter` 發送 38 kHz 紅外線訊號

## 支援對照表

| 協定 | 型號 | 電源控制 | 上下擺風 |
| --- | --- | --- | --- |
| `HITACHI_AC1` | `R_LT0541_HTA_A` | 指定開／關 | 支援 |
| `HITACHI_AC1` | `R_LT0541_HTA_B`（IE-06T2） | 僅支援切換（toggle） | 不支援 |
| `HITACHI_AC344` | 不適用 | 指定開／關 | 支援 |

## IE-06T2 / Model B 相容性修正

### 電源指令是 toggle，而不是指定開或關

`IE-06T2` 類型的 `HITACHI_AC1 Model B` 遙控器使用瞬時的 `POWER TOGGLE` 指令，
而不是分別指定 `POWER = ON` 或 `POWER = OFF` 狀態。換句話說，冷氣每收到一次有效的電源指令，
只會將當下狀態反轉：

```text
OFF → ON
ON  → OFF
```

因此，Model B 不能直接套用一般以狀態為基礎的冷氣電源控制方式。

IRremoteESP8266 的通用 `IRHitachiAc1` 實作會同時處理 `Power` 與 `PowerToggle`，
但 `IE-06T2 / R_LT0541_HTA_B` 的實際行為主要依賴瞬時的 `PowerToggle` 指令。
如果直接將 ESPHome 的開／關狀態套入一般 AC1 電源流程，冷氣的實際狀態可能會與
ESPHome 顯示的邏輯狀態不同步。

因此，本元件針對 Model B 做了以下處理：

- 由 ESPHome 維護邏輯電源狀態
- 只有要求的電源狀態改變時才設定 `PowerToggle`
- 不將 `Power` bit 視為實際的絕對電源狀態
- 發送完成後立即清除 toggle 指令
- 修改溫度或風速時不會意外切換電源

由於冷氣不會回傳實際電源狀態，如果曾使用原廠遙控器或其他控制器操作冷氣，ESPHome 的
邏輯狀態仍可能與實機不同步，此時需要手動將兩者重新對齊。

### Model B 沒有擺風控制

本專案測試的 `IE-06T2 Model B` 沒有對應的擺風功能，因此元件會：

- 不在 ESPHome Climate UI 顯示擺風控制
- 發送前清除 `SwingToggle`、`SwingV` 與 `SwingH`

這樣可以避免送出原廠遙控器不會產生的狀態。

## 關於重複傳送

一般紅外線遙控器重複發送同一筆資料，有時可以提高接收成功率；但對
`IE-06T2 Model B` 的電源 toggle 指令而言，不應以重送多個完整電源封包來提高可靠度。

每個有效的 toggle 都代表一次開／關切換。若冷氣將重複封包分別視為有效指令，就可能發生：

```text
關 → 開 → 關 → 開
```

因此，本元件會讓使用者的一次電源狀態變更只對應一次邏輯上的 toggle 動作。

## 使用需求

- ESP32
- ESPHome Arduino framework
- 紅外線 LED 或紅外線發射電路
- 可作為輸出的 GPIO
- ESPHome `remote_transmitter`

本元件的設定結構目前限定使用 ESP32 與 Arduino framework，不支援 ESP8266 或 ESP-IDF。
元件會自動加入 IRremoteESP8266 2.9.0。

## ESPHome 設定

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
  # AC1 訊框較長；若傳送不完整或出現 RMT underrun，請使用 blocking 模式。
  non_blocking: false
```

元件會以 38 kHz 載波發送訊框。對較長的 AC1 訊框而言，`non_blocking: false` 通常較穩定，
因此完整範例採用這項設定。

### 3. 設定 climate 元件

#### HITACHI_AC1 / IE-06T2 Model B

```yaml
climate:
  - platform: irremote_hitachi
    name: "日立冷氣"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
```

#### HITACHI_AC1 Model A

```yaml
climate:
  - platform: irremote_hitachi
    name: "日立冷氣"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_A
```

#### HITACHI_AC344

`HITACHI_AC344` 不需要設定 `model`：

```yaml
climate:
  - platform: irremote_hitachi
    name: "日立冷氣"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC344
```

完整的最小裝置設定請參考 [examples/basic.yaml](examples/basic.yaml)。

## 外接室溫感測器

可指定既有的 ESPHome 溫度感測器：

```yaml
climate:
  - platform: irremote_hitachi
    name: "日立冷氣"
    transmitter_id: ir_transmitter
    protocol: HITACHI_AC1
    model: R_LT0541_HTA_B
    sensor: room_temperature
```

這個溫度只會顯示為 ESPHome Climate 的目前室溫，不表示冷氣會透過紅外線回傳室溫。

## 設定選項

| 選項 | 必填 | 預設值 | 說明 |
| --- | --- | --- | --- |
| `transmitter_id` | 是 | — | 已設定的 ESPHome `remote_transmitter` ID。 |
| `protocol` | 否 | `HITACHI_AC344` | 可選擇 `HITACHI_AC1` 或 `HITACHI_AC344`。 |
| `model` | 否 | `R_LT0541_HTA_B` | AC1 遙控器型號；可選擇 `R_LT0541_HTA_A` 或 `R_LT0541_HTA_B`。AC344 會忽略此設定。 |
| `sensor` | 否 | — | 用來顯示目前室溫的 ESPHome 感測器 ID。 |

其他 ESPHome Climate 標準選項也可以使用。目標溫度以 1 °C 為單位調整，可設定範圍會依所選協定限制。

## 相容性提醒

如果無法控制你的日立冷氣，請不要只根據品牌、窗型或分離式、遙控器外觀，或按鍵配置來判斷相容性。
最可靠的方式仍是擷取原始紅外線訊號，確認：

- 是否為約 104-bit（13-byte）的封包
- Header timing 是否符合 `HITACHI_AC1`
- 封包起始 bytes
- Model bits
- Mode、Fan 與 Temperature 欄位
- Checksum
- Power 是否採用 toggle

確認紅外線協定後，再選擇對應的 `protocol` 與 `model`。

## 疑難排解

- **冷氣沒有反應**：確認 `transmitter_id`、GPIO、紅外線發射電路與載波設定，並檢查
  `protocol`／`model` 是否與實際遙控器相符。
- **AC1 訊框傳送不完整**：先將 `remote_transmitter.non_blocking` 設為 `false`。
  AC1 訊框較長，完整範例已採用這項設定。
- **Model B 電源狀態相反**：Model B 只能傳送 toggle，也無法讀回冷氣的實際電源狀態。
  如果曾使用其他遙控器，請先讓 ESPHome 顯示的狀態與冷氣實際狀態重新對齊。
- **平台驗證失敗**：本元件需要 ESP32、Arduino framework，以及已設定的 `remote_transmitter`。

## 授權條款

MIT
