# Architecture

[中文](architecture.zh-CN.md) · [Pipeline](pipeline.md) · [Roadmap](roadmap.html)

## Current baseline

```text
Mosaico display / touch
          │
          ▼
      app_airboat_ui ── read-only snapshots ──► display
          │
          ▼
   bounded command inbox
          │
          ▼
   app_motor_control (single owner, 10 ms cycle)
          │
          ▼
   app_motor_safety (pure C state machine)
          │
          └──► app_motor_output_gate (host-tested; stage-4 integration)
                              │
                              └──► future app_motor_drv8833 ──► DRV8833 ──► motor

   app_imu (BMI270 owner) ──► synchronized read-only snapshot
```

`main` only composes components and keeps their lifetimes. The UI posts requests; it never drives a GPIO. The motor task owns the safety state. The IMU task owns the BMI270 handle and publishes value snapshots. This makes the same safety model testable on a PC before a motor is powered.

## Face and body choreography — planned firmware

```text
touch / pose / idle timer → companion state → expression timeline
                                            ├→ face / short sound
                                            └→ motion intent → command inbox → safety owner → driver
```

The [interactive exhibit](roadmap.html#companion) previews looking around, one
slow turn, a gentle yaw sway, and quiet hovering. One monotonic timeline will
publish `expression_snapshot`: action ID, elapsed time, gaze, blink, motion
intent, and expiry. Automatic idle motion runs once, settles, and cools down;
manual input, timeout, and E-STOP cancel it. Hardware motion follows stage 4–6
bench and tether validation, then heading and drift observations. A turn uses
measured accumulated yaw; hovering in position is a separate closed-loop goal.
Decorative features stay in the evolving exterior design.

The web animation is implemented. The firmware state/timeline components and
physical movement are planned. The installed device baseline remains 1.2.0.

## Safety contract

- Start in `DISARMED`; ARM clears stale commands.
- Commands use `-1000..1000` permille and expire after 250 ms. Expiry returns to `DISARMED`.
- E-STOP is latched. CLEAR returns to `DISARMED`; a new ARM is required.
- Direction changes pass through `0/0` for at least 50 ms.
- A bounded FIFO holds eight ordinary requests. DISARM and E-STOP use a priority slot and clear the FIFO.
- `app_motor_output_gate` returns a zero plan unless the snapshot is armed, fresh, consistent, and single-bridge safe.

The software E-STOP is a state transition. The physical motor battery still needs an accessible disconnect during every bench test.

## Device and update boundary

```text
host tools → live Mosaico discovery → explicit device identity
          → device status / inventory → selected update workflow
```

Use the current discovery result and verify the device identity, boot session, mode, version, capabilities, and layout before an update. A historical COM number is only a hint. Normal mode runs the application; Vibe Mode supplies the retained update path; ROM download mode is a recovery boundary.

The application keeps the Mosaico retained partition prefix and the product update contract. The bootstrap script pins the Mosaico workspace to commit `1872fd88c01d7807556d01fd2a19864e40a307c9`; the application source is copied into that workspace for build and simulator use.

## Stage 4 adapter

After the ordered DRV8833 boards and the AirPuff motor kit arrive:

1. record the board package, motor current, battery voltage, connector and wire ratings;
2. run `scripts/check-arrival.py` and resolve every `pending` or `incompatible` result;
3. add `app_motor_drv8833` as the only GPIO/LEDC owner, called from the motor owner loop after the current safety tick;
4. initialize GPIO low, hold STBY inactive, and apply `0/0` before enabling the bridge;
5. test one propeller-off motor with a current-limited supply; only then map the other two axes.

The Mosaico Interaction module currently occupies the six planned H1 control signals. The first motor test therefore removes Interaction with all power off; camera hardware stays stored until the motor pin map is stable.

## Lift and sensing boundary

The first airframe uses independent main and side balloon modules on one load frame. Measure each module and the complete suspended mass separately. A BMP581 in a ventilated gondola can provide ambient pressure and relative height trend. It does not measure ground distance or balloon internal pressure; leak sensing is a separate feature.
