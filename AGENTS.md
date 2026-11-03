# AirBoat collaboration entry

Read `docs/pipeline.md`, `docs/architecture.md`, `docs/known-solutions.md`, and
`docs/design-system.md` before changing the project. Chinese pipeline and
architecture pages are available alongside the English versions.

- The application source is `firmware/airboat_baseline/`. Review its README and
  `tools/quality/` rules. Keep `main` as the composition root.
- Run `scripts/bootstrap-mosaico-workspace.ps1` to synchronize source into the
  pinned SDK workspace. Edit the public application first, then synchronize.
- Use the upstream workspace's `AGENTS.md` and product CLI for device work.
  Select the target from live discovery and verify identity before an update.
- Record build, update, live health, and physical acceptance separately. Keep
  device IDs, logs, backups, and local research outside the public tree.
- New hardware features enter through the stage gates. Publish measured results
  as a short, anonymized acceptance record before changing their stage state.
- Keep English/Chinese entry pages and all stage numbers aligned. Share the
  Mosaico visual tokens across device UI, diagrams, and the roadmap.

DIY instructions use this order: purpose, preparation, actions, completion
signal, result location. Explain a new term once; put engineering detail after
the actions. Add only recurring or costly issues to the solutions handbook.
