# app_motor_control

This component owns the software motor-safety state on a dedicated periodic
task. The ESP-IDF task and Windows simulator worker use a nominal 10 ms cycle
and a monotonic clock. Both expose the same public API.

`app_motor_control_post()` accepts ARM, DISARM, E-STOP and CLEAR requests.
`app_motor_control_submit()` accepts a three-axis command in the range
`-1000..1000` and records its receipt time before enqueueing. The shared private
inbox holds at most eight ordinary requests. A full inbox rejects ordinary
requests with `APP_MOTOR_CONTROL_QUEUE_FULL`. DISARM and E-STOP use a separate
priority slot and discard pending ordinary requests; E-STOP takes precedence
over a pending DISARM. Both platform workers take requests under the same
lifecycle lock used by producers, so priority and FIFO ordering have identical
semantics. At most eight ordinary requests are processed between safety ticks,
followed by a final priority check.

Only the worker calls `app_motor_safety`. It applies queued command timestamps,
ticks the safety state on a nominal 10 ms cycle, and publishes a snapshot. A
priority request posted during a cycle is handled by the owner loop on its
final priority check or following cycle. This describes software state
handling; there is no GPIO/PWM adapter or physical E-STOP yet. UI and future
input adapters exchange requests and copied snapshots; they do not access the
safety handle. Call these APIs from task/thread context, not an ISR. Stop all
producers and readers before `app_motor_control_destroy()`. Destroy requests a
final DISARM and waits up to 1000 ms; on timeout the handle remains valid so
the caller can retry after the worker exits.

The command watchdog is 250 ms and reversal dead time is 50 ms. This component
does not configure GPIO, LEDC, DRV8833 or motor power. Its E-STOP is a software
latch; physical output shutdown and a separate power cutoff remain future work.

Run `host/test-host.ps1` for the Windows worker and shared inbox tests. These
tests validate ownership, deterministic queue capacity and priority ordering,
command receipt time, the command watchdog and shutdown. They do not validate
ESP-IDF scheduling or physical motors.
