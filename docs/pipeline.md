# Development pipeline

[中文](pipeline.zh-CN.md) · [Architecture](architecture.md) · [Roadmap](roadmap.html)

As of 2026-11-04, stages 0–3B are verified. Stage 4 is in arrival preparation; the DRV8833 and material kit are ordered and await delivery. Firmware output is still a software plan, not a physical motor result.

Each stage has a clear entry and exit. Build success, device update, live health, and physical behavior are recorded separately.

| Stage | Deliverable | Exit gate | State |
| --- | --- | --- | --- |
| 0. Offline baseline | portable components, host tests, simulator | fixed toolchain and tests pass | Verified |
| 1. Device baseline | live discovery, identity and inventory | one selected Mosaico target, no guessed port | Verified |
| 2. Update preparation | retained-app restore path and package checks | update method chosen from live layout | Verified |
| 3A. Display and safety UI | display, touch, safety state machine, Vibe Mode round trip | stable run and on-device interaction pass | Verified |
| 3B. BMI270 baseline | static samples, six poses, six rotation directions, recovery | samples, pose mapping, application-level recovery, and physical UI regression pass; electrical I²C fault recovery is future hardware work | Verified |
| 4. Single motor | arrival review, DRV8833 adapter and propeller-off bench | one motor passes default state, direction, timeout, E-STOP, restart, current and temperature checks | Arrival prep |
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

When the parcel arrives, open [the single-motor checklist](../hardware/drv8833/arrival-single-motor-checklist.md), photograph and weigh the parts, fill the record template, and run the parameter screen. Once every required value has a source, review the single-channel firmware and pass its PC tests before connecting the bench. The first powered test uses one secured motor with its propeller removed.
