# DRV8833 motor stage

[中文](#中文) · [Arrival record](arrival-record-format.md) · [Single-motor checklist](arrival-single-motor-checklist.md) · [Wiring](wiring.csv)

The project uses two soldered-header dual H-bridge boards. Three bridges are planned: propulsion, yaw, and lift. The fourth bridge stays unused until a separate requirement exists.

## Electrical boundary

| Signal | First bench meaning |
| --- | --- |
| `VM` | separate, verified 1S motor supply; confirm the received motor and battery before connecting |
| `GND` | common reference with Mosaico H1 ground |
| `AIN1/AIN2` | 3.3 V logic inputs; PWM is applied directly to the selected input |
| `BIN1/BIN2` | second bridge inputs; hold unused inputs at the board's documented safe low |
| `STBY` | 3.3 V wake signal; confirm the received board's pull network |
| `AO1/AO2`, `BO1/BO2` | two-wire brushed motor outputs |

The DRV8833 data sheet gives different current capability by package and test condition. A product title is not a release criterion. At arrival, record the complete chip marking, package, board revision, motor startup/current data, battery voltage, connector rating, and wire rating. The parameter screen keeps `hardware_released=false`; the bench record releases one channel only after physical evidence.

## First connection

1. Disconnect USB and all batteries. Remove the right Interaction module; it uses the six H1 signals reserved by this motor plan.
2. Remove all propellers and fix the motor and driver board to a bench.
3. Confirm the received board's pin labels and connector polarity with a meter. The H1 socket orientation is not inferred from a photograph.
4. Connect only driver board 1, bridge A, and one motor. Keep the motor supply disconnected until the parameter gate and firmware gate pass.
5. Use an accessible current-limited supply or battery disconnect. USB powers Mosaico; motor power is separate; grounds are common.

The planned H1 mapping is recorded in `wiring.csv`. It is a design input for the stage-4 firmware, not proof that a physical board is already connected.

## Bench exit gate

With the propeller removed, test the lowest reliable PWM for a short pulse in each direction. Confirm `0/0` before changing direction, then test DISARM, E-STOP, 250 ms command expiry, control disconnect, restart, current, voltage drop, connector temperature, and driver temperature. Stop immediately on an unexpected direction, excess current, fast heating, smell, noise, restart, or an output that does not return to zero.

Never hold a rotor, block a motor, or perform a deliberate stall-current test by hand.

## 中文

本项目使用两块焊好排针的双路 H 桥板，规划三路：推进、偏航、升降，第四路暂不使用。

到货时记录完整芯片丝印和封装、板号、电机启动/工作电流、电池电压、插头和线径。参数筛查的 `hardware_released` 永远为 `false`；只有拆桨台架完成后才放行一条通道。

首轮动作：断开 USB 和电池，取下右侧 Interaction，拆下桨叶，固定电机和驱动板；先只接驱动板 1 的 A 桥和一台电机。Mosaico USB 单独供电，电机电源单独限流，双方共地。`wiring.csv` 是阶段 4 的规划表，实际接线按收到的板子丝印和固件版本逐项核对。

拆桨后短时测试低占空比正反转，换向先回到 `0/0`。再验证 DISARM、E-STOP、250 ms 超时、断线、重启、电流、电压变化、插头和驱动板温升。不能用手捏住转轴、压住桨叶或主动堵转测电流。
