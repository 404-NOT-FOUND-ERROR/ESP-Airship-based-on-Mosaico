# Known solutions

These are selected solutions that reduce repeat troubleshooting. One-off session logs stay outside the public repository.

## GPIO-01 — Interaction and H1 share the motor signals

**Symptom**: the right Interaction module and the planned six DRV8833 inputs would use the same H1 signals.

**Basis**: the Mosaico BSP maps the Interaction header to the same six logical lines used by the first motor plan; the right H1 is rotated relative to the left.

**Action**: remove Interaction with all power off for the first motor test. Reserve H1 for the finished motor harness and keep the module identification line documented. Re-evaluate the pin map before returning Interaction.

**Check**: compare the pinned BSP mapping with the firmware GPIO table, then verify continuity on the received harness before powering a bridge.

**Scope**: the current Mosaico H1 layout and this three-axis plan.

## CTRL-01 — Expire commands with a monotonic clock

**Symptom**: a delayed command can arrive after a newer session and briefly refresh an old output.

**Action**: use the device monotonic timer or the host monotonic tick. Check the 250 ms age at both submit and control-tick time; an expired command returns to DISARMED. Re-ARM and send a new command to resume.

**Check**: test expiry, delayed packets, timer wrap, queue-full behavior, and E-STOP priority in the portable C tests.

**Scope**: `app_motor_safety` and `app_motor_control`; it does not replace a physical battery disconnect.

## IMU-01 — Select the current sensor boot window

**Symptom**: retained device logs can contain samples from an earlier BMI270 startup.

**Action**: select the newest sensor-start marker first, then apply the monotonic uptime window. Reject timestamp or counter rollback inside that window. Keep the raw log and the filtered report together.

**Check**: replay a log with overlapping startup windows and verify old samples are excluded.

**Scope**: Iris log collection with retained startup markers.

## DISPLAY-01 — Keep partial refresh aligned

**Symptom**: a simulator screenshot can look correct while a physical partial refresh is shifted or leaves a trail.

**Action**: retain the display adapter's four-pixel refresh alignment and verify the complete ARM → E-STOP → ARM → CLEAR → ARM → DISARM sequence on the physical screen.

**Check**: inspect the full frame, touch feedback, zeroed axes, and absence of offset, artifacts, or residual pixels.

**Scope**: the Mosaico CO5300 display configuration in this application.
