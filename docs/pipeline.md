# Development pipeline

[中文](pipeline.zh-CN.md) · [Architecture](architecture.md) · [Roadmap](roadmap.html)

Each stage has a clear entry and exit. Build success, device update, live health, and physical behavior are recorded separately.

| Stage | Deliverable | Exit gate | State |
| --- | --- | --- | --- |
| 0. Offline baseline | portable components, host tests, simulator | fixed toolchain and tests pass | Verified |
| 1. Device baseline | live discovery, identity and inventory | one selected Mosaico target, no guessed port | Verified |
| 2. Update preparation | retained-app restore path and package checks | update method chosen from live layout | Verified |
| 3. Sensor/UI baseline | display, touch, BMI270 and pose records | UI chain, static sample, pose mapping and recovery checks pass | Verified |
| 4. Single motor | DRV8833 adapter and propeller-off bench | current, direction, timeout, E-STOP, restart and temperature pass | Next |
| 5. Three channels | propulsion, yaw and lift outputs | power and fault sequences pass together | Planned |
| 6. Modular lift | balloon frame, mass and tether tests | near-neutral balance and controlled tether motion | Planned |
| 7. Optional features | camera, remote input, barometer and closed loop | each feature has its own acceptance record | Planned |

## Working rules

- Keep the last verified baseline installable before changing the next boundary.
- Use a small, named component for each ownership boundary; keep `main` as the composition root.
- Keep future hardware behind the safety snapshot. UI text and animation never represent physical motor output.
- Attach measurements to the stage that uses them: board parameters at arrival, current and temperature at bench, lift and mass at airframe assembly.
- Record only recurring, expensive, or architecture-affecting problems in `known-solutions.md`.

## Next session

When the parcel arrives, open [the single-motor checklist](../hardware/drv8833/arrival-single-motor-checklist.md), photograph and weigh the parts, fill the record template, run the parameter screen, then stop at the first unresolved field. No motor power is connected during this preparation.
