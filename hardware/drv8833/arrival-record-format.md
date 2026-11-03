# 到货参数记录说明

目的：让卖家资料、实物信息和单路电气筛查使用同一份记录。第一次收货按[台架单](arrival-single-motor-checklist.md)操作；JSON 由开发者整理，你提供照片和参数即可。

复制 [模板](arrival-record.template.json)，填写数值时保留数字类型。单位放在字段名：`_v` 是伏特、`_a` 是安培、`_ms` 是毫秒。未知值保留 `null`；`source` 写卖家原始回复、数据手册页码或测量记录路径。

| 字段组 | 填什么 |
|---|---|
| `selected_axis` | `propulsion` 推进、`yaw` 偏航、`lift` 升降；每次只筛一台电机 |
| `driver` | 实际板号、`DRV8833`、`PW/PWP/RTY` 封装；板级连续/峰值能力、峰值时长及对应电压和环境温度；3.3V 逻辑及 STBY 兼容性 |
| `motors` | 三台分别填写；类型 `two_wire_brushed_dc` 表示两线有刷；允许电压、空载/原配桨工作电流、启动电流及时长 |
| `test_power` | `current_limited_supply` 限流电源或 `battery` 电池；本次电压范围、持续/峰值电流和时长；电池另填整数 `cells=1`，最高电压按满充 4.2V |
| `harness` | 成品插头型号、线束最弱环节的持续/峰值电流和时长、实际极性检查 |
| `tools_available` | 工具是否有，未确认保持 `null` |
| `mass_collection` | 全部重量到货时统一录入克；气囊独立称量并记录净升力 |
| `manual_acceptance` | 拔下 Interaction、针号复核、固件 SHA、构建/更新/台架结果，随对应步骤填写 |

板级电流数据条件应覆盖本次供电电压。TI 的 5V 额定值需配合模块在 1S 电压的实际资料评审；不能仅把额定条件数字改成 3.0V。连续能力按实际温度与散热条件评审，启动值来自规格或限流测量记录。

工具入口：`scripts/check-airboat-arrival.py --record <记录路径> --output <结果路径>`。它检查所选单路的数值类型、资料缺项、电压范围、连续/启动电流和启动时长；另外两路按 `selected_axis` 分别筛查。照片真实性、温度条件、余量、电源设置、实物走线和固件验收由工程评审与台架确认。

| 输出状态 | 含义 / 下一步 | 退出码 |
|---|---|---:|
| `pending` | 补齐列出的资料 | 2 |
| `incompatible` | 已填参数冲突；更换配件或重新设计 | 1 |
| `invalid_record` | 修正 JSON 结构、类型或非法数值 | 1 |
| `parameters_complete` | 自动筛查通过，进入参数评审和单路固件适配 | 0 |

所有结果的 `hardware_released` 固定为 `false`。动力上电由台架单的参数、固件和现场条件共同确认。模板核验显示 `pending` 是预期结果，模板和工具自测属于离线准备验收。
