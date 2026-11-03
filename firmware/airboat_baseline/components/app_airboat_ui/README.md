# app_airboat_ui

The shared GSP controller presents AirBoat state on the Mosaico display and
desktop simulator. It posts ARM, DISARM, E-STOP and CLEAR events to the caller-
owned `app_motor_control` service and reads its published snapshot every
50 ms. The GSP owner loop never reads or mutates the safety state directly.

There is no GPIO, LEDC, motor-power or network command adapter. The control
service currently exercises only the software interlock; no physical motor is
connected or driven. The E-STOP is a software latch until a future output
adapter and independent hardware cutoff are implemented and tested.

The caller owns both UI and control-service lifetimes. Destroy the UI first,
then stop the control service. Board APIs and operating-system headers do not
cross this component's public interface.
