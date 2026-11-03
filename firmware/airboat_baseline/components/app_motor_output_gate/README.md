# app_motor_output_gate

Pure C boundary between the safety snapshot and a future DRV8833 hardware
adapter. It preserves the three-axis A/B duty mapping only when the snapshot is
armed, not emergency-stopped, fresh, and internally consistent. Every other
valid safety state produces a zero-duty plan with `outputs_allowed=false`.

The component does not configure GPIO, LEDC, STBY, timers, or motor power. It is
not a watchdog and does not provide physical emergency stop. A future hardware
adapter must call it from the single-owner control loop and apply the returned
zero duties immediately when output is not allowed.

## Contract

- Axis order is propulsion, yaw, lift; each channel has A and B duties in
  permille (`0..1000`). Positive/negative direction selection is already
  represented by the safety snapshot's A/B values.
- An armed but command-less controller, DISARMED state, E-STOP, or stale command
  yields zero duties and disallows output.
- A fresh command is accepted only while armed, not E-STOP latched, and younger
  than the 250 ms command timeout.
- Out-of-range duties, two nonzero inputs on one bridge, or contradictory safety flags return
  `APP_MOTOR_OUTPUT_GATE_INVALID_SNAPSHOT`; the plan remains zeroed.
- APIs are stateless and thread-safe; callers own the snapshot and plan memory.
- Input and output storage must be distinct and stable during the call. This
  function checks a snapshot at its sampling instant; it does not measure its age
  after publication. The future adapter must use the owner loop's current tick,
  not a saved UI snapshot, and apply the plan in that same cycle.

## Integration status and dependencies

Depends only on the public `app_motor_safety` types and constants. The component
is built and tested independently; the installed 1.2.0 control task does not
invoke it. Hardware integration belongs to stage 4 after the arrival parameter
review. It has no resources, mutable state, or initialization/teardown sequence.

Minimal use inside the future owner loop: update the safety state with the
current monotonic time, pass that snapshot to `app_motor_output_gate_build()`,
then apply its plan. A non-OK result still supplies a zero plan and must trigger
the adapter's inhibited-output path. `out_plan=NULL` cannot receive a zero plan.

Run `host/test-host.ps1` for the portable mapping and fail-closed tests.
