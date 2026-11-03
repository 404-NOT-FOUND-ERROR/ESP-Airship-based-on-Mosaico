# main: device composition root

Initializes platform services, starts Recovery, loads the UI bundle, prepares the
display, starts GSP, attaches the application controller and starts an isolated
BMI270 sampler, in that order. It
contains no input handling, animation or drawing rules and exports no headers.

Private dependencies: `app_display`, `app_airboat_ui`, `app_imu`, Iris, Recovery, bundle loading,
screen mirroring, NVS and logging. Shared product/platform components retain
their names. Central dependency pins/overrides live in `idf_component.yml`.

ESP-IDF invokes `app_main()` once in task context; it takes no parameters and
returns after startup. Its private composition context retains only owned opaque
handles; UI/controller and sampler context lifetime extend until reboot. Bundle or display
initialization failure leaves Recovery reachable;
IMU task creation errors are logged, and hardware readiness/failure is published
by its worker without aborting the UI. Other unrecoverable initialization errors
use ESP-IDF's error-check policy.
Build/run the project through the workspace launcher described in the root README.
