<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to Calendar Plus

Calendar Plus combines a portable C core, thin native adapters and a Cinnamon JavaScript frontend. Contributions should preserve those boundaries, keep behaviour verifiable and avoid unnecessary repository complexity.

## Engineering rules

- Keep portable calendar, clock and event logic in `src/core/`.
- Keep GLib, GVariant and platform integration in `src/adapters/`.
- Keep project identity and the About helper in `src/app/`.
- Keep Cinnamon runtime code and settings in `src/cinnamon/`.
- Reuse the pinned Infiltratr Common public API when it is the correct shared abstraction; do not modify the submodule from this repository.
- Treat unsupported or ambiguous behaviour as unavailable rather than inventing results.
- Preserve the published runtime ABI unless a deliberate ABI change is part of the work. The neutral core headers are source-internal; Calendar Plus does not currently publish a third-party C SDK.
- Add deterministic regression coverage for behavioural, parser, lifecycle, timing, ABI or packaging changes.
- Do not commit build products, temporary extraction files or generated artifacts that the repository intentionally derives during validation.

## Build and validation

Clone recursively because Calendar Plus pins Infiltratr Common as a submodule:

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/Calendar-Plus.git
cd Calendar-Plus
make check
```

Before a release-equivalent change, run:

```bash
make release-check
```

Changes should compile cleanly under the repository warning policy and preserve the architecture, ABI, translation, runtime-integrity and packaging gates. Relevant changes in Cinnamon's stock calendar surface are tracked by the scheduled upstream-drift workflow and should be reviewed for compatibility rather than copied mechanically.

## Contribution workflow

`main` is the authoritative development and release branch. Maintainers work directly on `main`; external contributors may submit pull requests from forks for review. Pull requests are review vehicles, not a separate release branch, and release authority remains the tested commit on current `main`.

Keep commits focused. Do not combine unrelated formatting, behaviour, packaging and documentation changes without a clear reason.

## Documentation discipline

User and developer guidance belongs in `README.md`. Contribution policy belongs here, vulnerability handling belongs in `SECURITY.md`, and participation standards belong in `CODE_OF_CONDUCT.md`. Release history belongs in `debian/changelog`.

Do not add parallel README files, a second changelog, duplicated architecture notes or generated documentation when the information already has an authoritative home.

## Licence

Contributions are accepted under `GPL-3.0-or-later` unless explicitly agreed otherwise beforehand. Third-party material must retain its original attribution and use a licence compatible with the project.
