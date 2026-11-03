# app_motor_safety

Portable command-safety state machine for three signed DRV8833 channels. The
component has no GPIO, PWM, networking, display, FreeRTOS, or driver dependency.
The application owns the controller handle; a future hardware adapter will
translate each `input_*_permille` value into PWM duties.

## Contract

- Creation starts disarmed with zero commands. Arming never replays an old command.
- Arming and disarming invalidate the command. After a motor has run, the next
  tick returns 0/0 and starts a 50 ms restart guard while preserving direction.
- Commands are signed `-1000..1000`; each motor maps to A-input PWM for positive
  values, B-input PWM for negative values, and `0/0` for stop/coast.
- Inputs clamp to the normalized range. Hardware-specific startup duty remains
  a later motor characterization result.
- Commands expire at 250 ms. Expiry returns zero duties and automatically disarms
  the controller, so a recovered link must explicitly arm again before motion.
  Call `tick()` from one owner loop at a regular rate; the eventual hardware
  adapter must apply returned zero duties in that same loop.
- Submission also checks expiry before accepting a packet; a late packet cannot
  refresh an expired session even when it arrives before the next owner tick.
- A direction reversal first returns `0/0`, then holds both inputs low for at
  least 50 ms before applying the opposite input. These are software timing
  safeguards, not a substitute for a physical power disconnect.
- Emergency stop latches and disarms. Clearing it is allowed only while disarmed;
  clearing does not arm or restore the last command.
- All APIs are single-owner and not thread-safe. `now_ms` uses unsigned 32-bit
  elapsed-time arithmetic; call intervals must remain below half the counter range.

The component currently calculates snapshots only. No DRV8833 pins or motor power
are initialized by this application baseline.

## Desktop dry run

From the Mosaico workspace root, compile the optimized host tests and console:

```powershell
.\projects\airboat_baseline\components\app_motor_safety\host\test-host.ps1
```

Then start `.tools\airboat-host-test\airboat_motor_console.exe`. Commands are
`arm`, `cmd <propulsion> <yaw> <lift>`, `advance <ms>`, `disarm`, `estop`,
`clear`, `state`, and `quit`. Signed commands are `-1000..1000`; the simulated
clock advances only when `advance` is entered. The console uses the firmware's
same state machine and never opens GPIO, PWM, USB, or a motor driver.
