# esphome-yuhui-mppt

ESPHome component to monitor and control a Yuhui (钰辉) MPPT solar charger via RS485.

使用 RS485 与钰辉 MPPT 通讯的 ESPHome 组件，支持一条总线上挂多台 MPPT。
协议依据《MPPT远程通信协议 V1.0》（见 `docs/钰辉通讯文档.docx`）。

## 这个协议不是标准 Modbus

别被组件名骗了，它只是长得像：

| 项目 | 本标准 | 通用 Modbus RTU |
| --- | --- | --- |
| 帧长度 | 固定（由命令码决定：37 / 64 / 93 / 8 字节） | 变长（由字节计数推出） |
| 校验 | 累加和取低字节（不含 CRC16） | CRC16 |
| 帧间隔 | 帧内 < 1.5 字符，帧间 ≥ 3.5 字符 | 3.5 字符 |
| 查询间隔 | **≥ 1 秒** | 无硬性要求 |

`custom_modbus` 这一层按「先由命令码算出整帧长度 → 凑齐 → 验累加和 → 整帧交付」处理，
校验失败只丢 1 个字节重新找同步头，所以中途丢字节也能自己恢复。

## 已实现的命令

| 命令 | 用途 | 请求 | 应答 |
| --- | --- | --- | --- |
| `0xB1` | 全量查询 | 8 字节 | 93 字节 |
| `0xB2` | 仅查询设置参数 | 8 字节 | 64 字节 |
| `0xB3` | 仅查询实时数据 | 8 字节 | 37 字节（**默认轮询用这个**） |
| `0xC0` | 控制：允许/禁止充电、DC 输出开关、消音、开背光 | 8 字节 | 原样回抛 8 字节 |
| `0xD0` | 参数设置（电池类型、均充/浮充电压、限流…） | 8 字节 | 原样回抛 8 字节 |
| `0xDE` | 设置波特率 | 8 字节 | 无应答 |
| `0xDF` | 设置时钟（地址 0x00 为群控） | 8 字节 | 群控不回 |
| `0xEE` | 设备错误帧 | — | 8 字节，含错误码 |

## 最小配置

```yaml
external_components:
  - source: github://Depressboy/esphome-yuhui-mppt@main

uart:
  - id: uart_bus_0
    baud_rate: 9600
    tx_pin: GPIO16
    rx_pin: GPIO17

custom_modbus:
  - id: mod_0
    uart_id: uart_bus_0
    frame_gap: 30ms      # 半帧闲置多久算残帧

yuhui_mppt:
  - id: mppt_0
    custom_modbus_id: mod_0
    device_address: 0x01
    update_interval: 5s  # 协议要求 >= 1s，小于 1s 会校验报错
    rx_timeout: 300ms
    retry_count: 2
```

完整示例见 `esp32-example-chinese.yaml` / `esp8266-example-chinese.yaml`。

## 配置项

| 选项 | 默认值 | 说明 |
| --- | --- | --- |
| `device_address` | `0x01` | MPPT 面板上设置的通讯地址（0x01~0xF0） |
| `update_interval` | `5s` | 轮询周期，**必须 ≥ 1s** |
| `rx_timeout` | `300ms` | 单条命令等待应答的时间（93 字节帧约 97ms） |
| `retry_count` | `2` | 超时重发次数 |
| `poll_full_status` | `false` | 为 `true` 时轮询 0xB1（93 字节）而不是 0xB3 |
| `clock_broadcast` | `true` | 对时使用群控地址 0x00（协议规定群控不返回数据） |

配了 `rated_voltage` / `max_charge_current` / `battery_type` 时，
`poll_full_status` 会在 `setup()` 里自动置真，不用手动开。

## 传感器与实体的来源

- `pv_voltage` / `battery_voltage` / `charging_current` / `internal_temperature` /
  `external_temperature` / `daily_energy` / `total_energy` / `charging_power`：0xB3 就有
- `rated_voltage` / `max_charge_current` / `battery_type`：仅 0xB1、0xB2 有
- `last_error`：设备回 0xEE 错误帧时写入，中文描述 + 错误码

电量协议单位是**瓦时(Wh)**，组件统一换算成 kWh 上报。

## 注意事项

1. **查询间隔不能小于 1 秒**，这是协议硬性要求，快了会丢包或设备不应答。
2. **多机共总线**：每条 `yuhui_mppt` 一个 `device_address`，共用同一个 `custom_modbus` 即可，
   但轮询间隔要错开（用 `update_interval: 5s` / `6s` / `7s`）。
3. **对时**：组件会检查系统时间，SNTP 没同步时直接跳过，不会把 MPPT 时钟写到 1970 年。
4. **改波特率很危险**：`0xDE` 是广播且无应答，改完 MPPT 就不在当前速率上应答了，
   ESPHome 侧 `baud_rate` 和 MPPT 面板都得同步改。只有在 lambda 里手动调用。
5. **开关状态**没有单独确认帧，UI 会先乐观更新，下一轮查询拿到设备真实状态后纠正。

## 在 lambda 里调参数

```yaml
on_...:
  - lambda: |-
      // 设最大充电电流 40.00A（2 字节，两位小数）
      id(mppt_0).send_parameter_command(esphome::yuhui::PARAM_MAX_CHARGE_CURRENT, 4000, 2);
      // 设均充电压 57.60V
      id(mppt_0).send_parameter_command(esphome::yuhui::PARAM_EQUALIZE_VOLTAGE, 5760, 2);
```

参数码定义在 `yuhui_mppt.h` 的 `ParameterCode`，完整对照表见协议文档第五章。

## 更新记录

### v1.1.0

- 通讯层改为定长帧解析：按命令码取帧长、验累加和、失败只丢 1 字节重同步
- 修复控制命令（0xC0 回抛 8 字节）被 `len < 37` 丢弃、每次操作都等超时的问题
- 新增 0xEE 错误帧处理与 `last_error` 文本传感器
- 新增 0xB1 全量查询、0xB2 设置查询、0xDE 改波特率
- 对时支持群控地址 0x00，并检查系统时间是否已同步
- 新增超时重试、命令队列上限、连续失败告警（`status_set_warning`）
- 去掉未使用的成员和 `// ...existing code...` 残留
- 浮点统一用 `float`（ESP8266 上 `double` 是软件模拟，又慢又占 flash）
