# Development Guide: Local-Only Strategy & Setup

## Overview

This document explains how the Chart Preview repository handles local-only files, external dependencies, and git configuration to maintain a clean main branch that only contains code changes.

## Local-Only Philosophy

The repository separates **what gets committed** from **how you work locally**:

- **Committed to git** (main branch): Source code, assets, build configs, documentation
- **Local-only** (never committed): Build outputs, logs, AI/scaffolding, analysis notes, session state

This keeps the repository clean and prevents developer-local preferences from affecting others.

## Git Configuration

### Branch Protection

The repository uses a **main-only strategy** with simple feature branches for larger features:

- **main**: Stable, deployed branch with all releases
- **feature/*** (optional): Feature branches for new features, merged when complete
- All commits go through CI/CD verification before or after push

### No Git Submodules

This project **does not use git submodules**. While `.git/config` historically had submodule entries, they were orphaned (no `.gitmodules` file) and unused. The broken entries have been removed.

**Why no submodules?** Because external dependencies are managed differently:
- JUCE: Downloaded at CI time, patches applied for REAPER integration
- REAPER SDK: Only one header file needed (committed to `.ci/reaper-headers/`)
- CI downloads clean, vanilla JUCE and applies patches from `.ci/juce-patches/`

This approach is cleaner than maintaining a JUCE fork as a submodule.

## File Tracking Strategy

### .gitignore (Project-Level Rules)

Located at repository root, `.gitignore` defines what should NEVER be committed by any developer:

**Build Artifacts**
- `**/Builds/`, `**/build/`, `**/JuceLibraryCode/`
- `ChartPreview/releases/` (local development builds)
- `ChartPreview/build-scripts/releases/`

**Compiled Binaries**
- `.exe`, `.dll`, `.dylib`, `.so`, `.vst3`, `.component`, `.lib`, `.a`

**Development Files**
- `.o`, `.obj`, `.d`, `.pch`, `.gch`, `.opensdf`, `.sdf`
- `*.log` (build and process logs)

**IDE/Editor Files**
- `.vscode/` (directory excluded, but settings.json was removed from tracking)
- `*xcuserdata/`, `*.xcuserstate` (Xcode)
- `*.user`, `*.suo` (Visual Studio)

**Operating System Files**
- `.DS_Store`, `.AppleDouble`, `._*` (macOS)
- `Thumbs.db`, `Desktop.ini` (Windows)

**Third-Party Dependencies**
- `third_party/*` (all external dependencies excluded)
- Only `third_party/README.md` and `.gitkeep` are tracked
- Reason: Dependencies obtained at build time; manual setup for local dev

**AI/Sessions Framework**
- `sessions/` (task management framework - local only)
- `.claude/` (Claude Code configuration - local only)
- `.claude-code/` (legacy Claude Code settings - local only)

**Analysis & Notes**
- `*_ANALYSIS.md` (development analysis files)

### .git/info/exclude (Developer-Local Preferences)

Located at `.git/info/exclude` (not tracked in git), this file defines YOUR personal exclusions that don't affect other developers:

Use this for:
- Personal scratch files (`scratch.md`, `notes.txt`)
- Personal debugging files (`*_debug.*`, `debug-output.txt`)
- Temporary test outputs (`*.test-output`)
- IDE backup files (`.swp`, `.swo`, `*~`)
- Personal configuration (`.env.local`, `config.local.json`)

Example entry:
```
# My personal debugging notes
my-debug-notes.md
```

**Philosophy**:
- If EVERY developer should exclude it → add to `.gitignore`
- If only YOU want to exclude it → add to `.git/info/exclude`
- If uncertain → start with `.git/info/exclude`, promote to `.gitignore` if needed

## Pre-Commit Hook

A **pre-commit hook** at `.git/hooks/pre-commit` automatically prevents accidental commits of local-only files.

### How It Works

Before each `git commit`, the hook runs and checks if you're trying to commit any files matching these patterns:
- `sessions/` (AI scaffolding)
- `.claude/` (Claude Code config)
- `*.log` (build logs)
- `*_ANALYSIS.md` (analysis files)
- `ChartPreview/releases/` (local builds)

If any blocked files are detected, the commit is **rejected** with an error message.

### Override (If Needed)

If you legitimately need to commit one of these files (should be rare):
```bash
git commit --no-verify
```

But please reconsider - if it's truly local-only, use `.git/info/exclude` instead.

## Local Development Setup

### Getting Started

1. **Clone the repository**
   ```bash
   git clone https://github.com/noahbaxter/chart-preview.git
   cd chart-preview
   ```

2. **Get JUCE** (follow `third_party/README.md`)
   - Download JUCE 8.0.0 from GitHub releases
   - Extract to `third_party/JUCE/`
   - Build scripts will apply necessary patches automatically

3. **Build Locally**
   ```bash
   # VST3 build (simple)
   ./build-scripts/build-local-test.sh

   # AU build (full test with shared library)
   ./build-scripts/build-local-au.sh
   ```

### Understanding Build Outputs

When you build locally, artifacts are generated in:
- `ChartPreview/Builds/MacOSX/build/` (Xcode builds)
- `ChartPreview/Builds/VisualStudio2022/x64/` (Windows)
- `ChartPreview/Builds/LinuxMakefile/build/` (Linux)
- `ChartPreview/releases/` (local release outputs)

**These are all excluded from git.** Each build generates fresh artifacts.

### Logs and Analysis

If you generate build logs or analysis files during development:
```bash
au_build.log              # Git-ignored via *.log pattern
MY_DEBUG_NOTES.md         # Add to .git/info/exclude if personal
CI_BUILD_ANALYSIS.md      # Git-ignored via *_ANALYSIS.md pattern
```

Add personal files to `.git/info/exclude` if they're only for you.

## External Dependencies

### JUCE Framework

**Status**: Downloaded at build time, not tracked

**Local Setup**:
1. Download JUCE 8.0.0 from https://github.com/juce-framework/JUCE/releases/download/8.0.0/
2. Extract to `third_party/JUCE/`
3. REAPER patches will be applied by build scripts

**CI Process**:
- CI downloads vanilla JUCE
- Applies patches from `.ci/juce-patches/`
- Creates symlink to `third_party/JUCE/`

**Patches Applied** (see `.ci/juce-patches/`):
- Patches for REAPER plugin integration
- Patches for VST3 platform-specific features

### REAPER SDK

**Status**: Single header file tracked, rest excluded

**Tracked**: `.ci/reaper-headers/reaper_vst3_interfaces.h`

**Local Setup** (optional, for research):
```bash
# If you want to study REAPER SDK
cd third_party/reaper-sdk/
git clone https://github.com/justinfrankel/reaper-sdk.git .
```

This directory is Git-ignored, so local clones won't be committed.

## Troubleshooting

### "ERROR: Attempting to commit local-only files"

The pre-commit hook blocked your commit. This is intentional!

**Solutions**:
1. Remove the files: `git rm --cached filename`
2. If they should stay local, add to `.git/info/exclude`
3. Use `git commit --no-verify` only if you're sure (rare)

### "Why can't I commit my analysis file?"

Analysis files matching `*_ANALYSIS.md` are excluded to keep the repository focused on code changes. If it's useful for the project:
- Move it to `docs/` with a proper name
- Commit it intentionally without the `_ANALYSIS.md` suffix
- Update `.gitignore` if it should be project-wide

### "My sessions/ directory got uncommitted"

Good! Sessions framework is local-only. It creates work logs, task files, and state locally on your machine. This is intentional - it's not version-controlled because:
- Session state is personal to each developer
- Tasks live locally; you don't commit task files
- Work logs are local transcripts of your sessions

## Best Practices

1. **Regular Audits**: Occasionally run `git status --ignored` to see what's being ignored
2. **Keep .git/info/exclude Updated**: Add patterns as you discover personal files you want excluded
3. **Test Before Pushing**: Your local builds might work but CI builds might fail if dependencies aren't set up correctly
4. **Document Local Setup**: If you add local-only dependencies, update `.git/info/exclude` and this guide
5. **Use Feature Branches**: For experimental work, create a feature branch instead of committing to main

## Questions or Issues?

If the local-only strategy doesn't match your workflow:
1. Check this guide first
2. Check `.gitignore` and `.git/info/exclude` for current patterns
3. Consider if a pattern should be added to either file
4. If you find a bug or gap, document it in a local analysis file or task

Remember: The goal is clean main branch, flexible local development.
