# ESP-Mosaico airboat_baseline

The AirBoat baseline uses the same GSP 1.5.1 scene and portable C UI on PC and
ESP-Mosaico. It is a software-only safety stage; no motor GPIO is initialized.

From the public repository root, prepare the pinned Mosaico workspace first:

```sh
.\scripts\bootstrap-mosaico-workspace.ps1
.\scripts\run-host-checks.ps1
```

The recorded device baseline is regular AirBoat 1.2.0, with stage 3B physical
display/touch acceptance complete. Continue with the [AirBoat pipeline](../../docs/pipeline.md)
and [architecture](../../docs/architecture.md). Device installation is a separate
operation: use the current Mosaico discovery result and the product tools' live
identity checks before selecting an update method.

Offline verification from the ESP AirBoat root:

```powershell
.\scripts\run-host-checks.ps1
.\scripts\run-mosaico-sim.ps1 -Headless
```

Architecture and the next device checklist: [architecture](../../docs/architecture.md)
and [single-motor checklist](../../hardware/drv8833/arrival-single-motor-checklist.md).

Application components:

Application version has one source: the top-level CMake `project(... VERSION ...)`.
Keep `CONFIG_APP_PROJECT_VER_FROM_CONFIG` disabled, including reused sdkconfig
files. Verify the version inside the resulting image before installation.

- `components/app_airboat_ui`: portable status UI; posts actions and reads snapshots.
- `components/app_motor_control`: 10 ms single-owner task, bounded request queue, and synchronized snapshots.
- `components/app_motor_safety`: platform-independent command-safety state machine.
- `components/app_motor_output_gate`: pure C snapshot-to-duty validation, tested offline;
  integration into the owner task and GPIO/PWM adapter belongs to stage 4.
- `components/app_display`: BSP-to-GSP display and optional touch policy.
- `components/app_imu`: isolated BMI270 lifecycle, 50 Hz sensor-frame snapshots and failure/staleness reporting.
- `main`: device startup and dependency composition only.
- `components/app_airboat_ui/host`: desktop adapter and its separate CMake build,
  reusing the same controller and public header; excluded from the ESP32 build.
- `components/app_airboat_ui/ui/main.json`: authored scene; generated bindings and bundles stay in the build.

`project sim` discovers a unique scene under `components/*/ui/main.json` (or the
older root `ui/main.json`). With multiple scenes, select one using `--scene PATH`,
relative to the project. This previews only that scene; omit `--scene` to run the
PC backend and its compiled application behavior.

Dependency direction is `main -> app_airboat_ui -> app_motor_control -> app_motor_safety`,
`main -> app_display`, and `main -> app_imu`; these components have no dependency
on `main`. The GSP owner loop is separate from the safety-state owner. The IMU
worker is the sole BSP BMI270 owner; the UI and motor service consume no sensor
internals. This stage has no GPIO/PWM or network adapter, so snapshots are not
physical motor outputs. Public headers live in `include/`; private state stays
in implementation files. Each component README includes its contract and usage.
The standalone output gate depends only on public `app_motor_safety` types. It
will receive a current-tick snapshot and apply its plan within the same future
owner-loop cycle; the current task does not call it.

When adding behavior, keep `main` as a composition root. Add cohesive `app_*`
components only for independent responsibilities, declare public/private CMake
dependencies explicitly, and keep the graph acyclic. Avoid cross-module globals;
pass handles, context and value snapshots. Keep each public API comment within
three lines, describing its purpose and key preconditions/results. Put detailed
ownership, lifecycle, threading and timeout contracts in the component README;
document units and ranges beside fields.
Third-party dependencies retain upstream names. Pins and workspace overrides live
in `main/idf_component.yml` and the root CMake configuration. Apply local patches
in a dedicated dependency checkout and select it with `override_path`; never edit
`managed_components/` in place.

Git, editor and hook entry files stay at the root; format rules, project policy
and setup instructions live in `tools/quality/`. See its README to enable hooks.
The `tools/quality/policy.json` prefix rule checks local application component names.
Architecture and API semantics also require code review; formatter/static-analysis
success alone does not establish those contracts.
