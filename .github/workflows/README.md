# Workflows

Releases are built and published by the plugins hub (`noahbaxter/_plugins`), not here.
This repo only says *when*, and validates pull requests.

## Stable releases

`release-dispatch.yml` fires the hub:

- push to `main` -> hub dry run (builds all three platforms, publishes nothing)
- push a `vX.Y.Z` tag -> hub publish

The hub then builds, signs and notarizes macOS, builds the Windows Inno installer,
zips the Linux bundles, promotes them to the CDN, cuts the GitHub release on this
repo, and publishes the changelog + update manifest to dichoticstudios.com.

Needs the `HUB_DISPATCH_TOKEN` secret (a PAT with Actions: read/write on `_plugins`).
Plugin config lives in the hub's `plugins.json`, including `cmakeArgs`, which passes
`-DBUILD_CHANNEL=RELEASE` so hub builds don't report themselves as dev builds.

Shipping a release: bump `VERSION`, add the matching `## X.Y.Z` section to
`CHANGELOG.md` (the hub fails the publish if it's missing), merge to `main`, then
tag `vX.Y.Z`. The tag must match `VERSION` or the hub refuses before building.

## Prereleases

Still published from this repo, because the hub only handles stable:

- `release-dev.yml`: push to `dev` -> `dev-latest` prerelease
- `release-beta.yml`: push to `beta` touching `VERSION` -> `beta-latest` prerelease

`UpdateChecker` reads those two tags for the DEV and BETA channels, so retiring
either workflow retires that channel.

`beta` is a release channel, not a development line. It does not have to exist:
cut it from `dev` when a beta is wanted, push a `VERSION` bump, delete it after.
Keeping a long-lived `beta` is what let it fork from the trunk before.

## Pull requests

`pr-check.yml` runs `build.yml` on PRs to `main`: all three platforms, plus
pluginval (strictness 5) and the unit tests. Apple secrets are not passed, so PRs
skip signing and notarization.

## Reusable build

`build.yml` is `workflow_call` only. Inputs: `version_string` (required) and
`build_channel` (`RELEASE`/`DEV`). Called by `pr-check.yml` and the two prerelease
workflows.
