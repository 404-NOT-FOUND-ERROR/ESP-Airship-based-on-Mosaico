# app_imu

Owns the Mosaico BMI270 lifecycle and a read-only six-axis sampler. This component
does not depend on GSP, motor safety or motor outputs. BSP and Bosch implementation
details remain private; public snapshots use sensor-native X/Y/Z, `g`, `deg/s` and
device monotonic microseconds. Board/body-axis mapping and orientation fusion are
separate work.

```c
app_imu_handle_t imu;
esp_err_t err = app_imu_start(&imu);
/* ESP_OK means the task was created; read state/valid for hardware readiness. */
app_imu_snapshot_t snapshot;
if (err == ESP_OK && app_imu_read(imu, &snapshot) == ESP_OK && snapshot.valid) {
    /* Use snapshot.accel_g / snapshot.gyro_dps as measurements. */
}
```

## Ownership and lifecycle

`main` owns the opaque handle for the boot lifetime. The single worker exclusively
initializes, configures, samples and deinitializes BSP's singleton BMI270. Call
`start` once per boot; subsequent calls return `ESP_ERR_INVALID_STATE`. Other
components must not initialize or read that sensor in parallel. Shared I2C and
power rails belong to BSP; cleanup releases only the BMI270 device.

Task creation/allocation failures roll back the reservation and allocations, leave
the output NULL and return `ESP_ERR_NO_MEM`. Hardware initialization runs in the
worker after task creation; a failure publishes FAULTED, attempts BSP cleanup and
terminates that worker. It leaves display/Iris services running.

`stop(handle, timeout_ms)` is terminal and callable by one controlling task, with
a timeout of 0..60000 ms. It requests cooperative shutdown and waits for cleanup.
Timeout returns `ESP_ERR_TIMEOUT`; keep the handle, inspect it and retry stop later.
A completed cleanup returns its ESP error, zero on success. Worker termination
releases task storage; the small context and completion semaphore remain owned by
the application until reboot, including after stop. There is intentionally no
free/restart API; this avoids freeing a context while a reader or worker still
uses it. Stop does not forcibly kill an in-flight BSP operation.

## Sampling and concurrency

ODR is 100 Hz, ranges are +/-4 g and +/-500 deg/s. A 4096-byte task at priority 3
waits 100 ms for startup then polls at a nominal 20 ms interval (driver and logging
time add to this). It checks both data-ready bits, then performs one combined
Bosch data read. This associates both measurements with one host acquisition;
it is not hardware synchronization or FIFO capture. Timestamp is recorded after
the read. Logs summarize at most once per second; the snapshot retains every
accepted acquisition. No heap allocation occurs in the sampling loop.

A private spinlock protects only fixed-size state copies/updates. Hardware calls,
logging and delays occur outside it. `read` works across task owners, never from
ISR, and recomputes validity at read time. Sample age above 100 ms, a current read
error, startup/fault/stop state or a backward clock invalidates the copy. Previous
successful values remain available for diagnosis; callers must check `valid`.

Read failures count cumulatively and consecutively; five consecutive failures
publish FAULTED and reduce retry rate to 5 Hz. A successful later read restores
RUNNING, clears consecutive errors and last error, and preserves total errors.
Counters saturate at UINT32_MAX. Startup failures terminate instead of blindly
reinitializing the shared bus. `last_error` holds an ESP error code; Bosch I/O
failures map to `ESP_FAIL`, missing data-ready bits to `ESP_ERR_NOT_FINISHED`.

## Dependencies and validation

Public: portable types and ESP error codes. Private: `esp-mosaico-bsp`, FreeRTOS,
`esp_timer`, logging. The private model is pure C for boundary/error/freshness tests.
Run `host/test-host.ps1` or the root offline check. Firmware compilation verifies
SDK/BSP integration; stationary/motion data, axes, runtime timing, bus contention
and hardware cleanup are separate device acceptance items.

## Bounded diagnostic build

`CONFIG_AIRBOAT_IMU_DIAGNOSTIC_FAULT_TEST` defaults to disabled. In the temporary
diagnostic profile, five successful application reads become `ESP_FAIL` once,
five seconds after the first accepted hardware read. The worker then follows the
same model, invalidation and 200 ms retry path as a real read error. Diagnostic
logs identify FAULTED and the next real successful read, including monotonic
timestamps and cumulative errors. No public API, I2C transaction, bus ownership
or sensor configuration changes.

Freeze each profile's effective configuration and artifact hashes. Validate the
diagnostic image on the selected device, then rebuild and install the profile
with this option disabled. Device acceptance covers application error propagation
and recovery; electrical I2C faults and recovery from a failed hardware
initialization are separate tests. Application version comes from the project
CMake file; distinguish the diagnostic profile by its option and ELF SHA.
