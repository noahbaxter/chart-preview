# Repository Cleanup Plan

## Current Issues

### 1. Documentation Scattered Across Multiple Locations

**Problem**: Documentation exists in 3 places with different purposes:
- `/docs/` - Root level project docs (9 files)
- `/ChartPreview/docs/` - Plugin-specific docs (2 files + archive)
- `/ChartPreview/docs/archive/` - Old research notes

**Impact**: Hard to find documentation, unclear organization

### 2. Third-Party Dependencies Duplicated and Inconsistent

**Problem**: Third-party code exists in multiple locations with inconsistent naming:
- `/third_party/` (lowercase, 194MB total)
  - `JUCE/` (70MB) - Root level JUCE copy
  - `JUCE-reaper-embedded-fx-gui/` (88MB) - Reference project
  - `reaper-sdk/` (2.3MB)
  - `sws/` (29MB) - SWS extension source
  - `reaper_sws-arm64.dylib` (4.9MB) - SWS binary

- `/ChartPreview/third_party/` (lowercase, 71MB)
  - `JUCE/` (71MB) - **This is the JUCE we actually use** (local modified copy)

- `/ChartPreview/ThirdParty/` (PascalCase, 28KB)
  - `reaper-sdk/` (28KB) - Duplicate of REAPER SDK

**Impact**:
- Confusion about which JUCE is used (the one in ChartPreview)
- Duplicate REAPER SDKs
- Inconsistent naming (third_party vs ThirdParty)
- 265MB+ of third-party code total
- JUCE submodule changes leak into git status

### 3. Assets in Multiple Locations

**Problem**:
- `/assets/` - Root level (development assets)
- `/ChartPreview/Assets/` - Embedded plugin assets (used in build)

**Impact**: Unclear which assets are for what purpose

### 4. Git Submodule Issues

**Problem**:
- JUCE is a git submodule but shows untracked changes
- Not properly ignored in .gitignore
- Pollutes `git status` output

## Proposed Structure

```
chart-preview/
├── docs/                          # ALL documentation here
│   ├── README.md                  # Docs overview
│   ├── BUILDING.md
│   ├── CLAUDE.md                  # AI assistant guidance
│   ├── TODO.md
│   ├── development/               # Development guides
│   │   ├── JUCE_REAPER_MODIFICATIONS.md
│   │   ├── REAPER_INTEGRATION.md
│   │   └── VST2-REAPER-INTEGRATION.md
│   └── archive/                   # Historical research
│       └── REAPER_*.md
│
├── third_party/                   # External dependencies (ignored by git)
│   ├── README.md                  # How to obtain these
│   ├── JUCE/                      # Modified JUCE (ChartPreview uses this)
│   ├── reaper-sdk/                # REAPER SDK headers
│   └── reference/                 # Reference implementations
│       └── JUCE-reaper-embedded-fx-gui/
│
├── ChartPreview/                  # Main plugin project
│   ├── Assets/                    # Plugin embedded assets
│   ├── Audio/                     # Plugin embedded audio
│   ├── Source/                    # Plugin source code
│   ├── build-scripts/             # Build automation
│   ├── ChartPreview.jucer         # JUCE project file
│   └── [build output dirs ignored]
│
├── examples/                      # Example projects/MIDI files
│   ├── reaper/
│   ├── ableton/
│   └── test-midi/
│
├── assets/                        # Development-only assets (not embedded)
│   └── development/
│
├── .github/                       # CI/CD configuration
├── README.md                      # Main project README
└── LICENSE
```

## Cleanup Actions

### Phase 1: Documentation Consolidation

1. **Merge documentation to `/docs/`**
   - Move `/ChartPreview/docs/VST2-REAPER-INTEGRATION.md` → `/docs/development/`
   - Move `/ChartPreview/docs/archive/*` → `/docs/archive/`
   - Update `CLAUDE.md` to reference new paths
   - Remove empty `/ChartPreview/docs/`

2. **Organize by topic**
   - `/docs/` - User-facing docs (README, BUILDING, TODO)
   - `/docs/development/` - Developer guides (REAPER integration, JUCE mods)
   - `/docs/archive/` - Historical research (old investigation notes)

### Phase 2: Third-Party Cleanup

1. **Consolidate to single `/third_party/` directory**
   - Delete `/ChartPreview/ThirdParty/reaper-sdk/` (duplicate)
   - Move `/ChartPreview/third_party/JUCE/` → `/third_party/JUCE/` (the one we use)
   - Keep root `/third_party/` as the single source
   - Remove `/ChartPreview/third_party/` directory
   - Remove `/ChartPreview/ThirdParty/` directory

2. **Update `.jucer` file**
   - Change module paths from `third_party/JUCE/modules` to `../third_party/JUCE/modules`
   - Verify builds still work after path change

3. **Clean up reference materials**
   - Move `JUCE-reaper-embedded-fx-gui` to `/third_party/reference/`
   - Move `sws/` to `/third_party/reference/` (or remove if not needed)
   - Remove `reaper_sws-arm64.dylib` if not actively used
   - Keep only one copy of `reaper-sdk/`

4. **Add `/third_party/README.md`**
   ```markdown
   # Third-Party Dependencies

   ## Obtaining Dependencies

   ### JUCE Framework (Modified)
   This project uses a modified version of JUCE with custom REAPER integration.
   See `/docs/development/JUCE_REAPER_MODIFICATIONS.md` for details.

   Location: `/third_party/JUCE/`
   Branch: `reaper-vst2-extensions`

   ### REAPER SDK
   Download from: https://www.reaper.fm/sdk/plugin/
   Extract to: `/third_party/reaper-sdk/`
   ```

### Phase 3: Git Configuration

1. **Update `.gitignore`**
   ```
   # Third-party dependencies (entire directory)
   /third_party/

   # Except the README explaining how to get them
   !/third_party/README.md
   !/third_party/.gitkeep
   ```

2. **Update `.gitmodules`**
   - Remove JUCE submodule entry (we're tracking our modified version)
   - Or configure submodule to ignore all changes:
   ```
   [submodule "third_party/JUCE"]
       path = third_party/JUCE
       url = [your fork URL]
       ignore = all
   ```

3. **Clean git cache**
   ```bash
   git rm -r --cached ChartPreview/third_party
   git rm -r --cached ChartPreview/ThirdParty
   git rm -r --cached third_party/
   ```

### Phase 4: Examples and Assets

1. **Keep current structure** (already good)
   - `/examples/` for test projects
   - `/ChartPreview/Assets/` for embedded assets
   - `/assets/` for development assets

## Benefits After Cleanup

1. **Clear documentation hierarchy**
   - Single `/docs/` location for all documentation
   - Organized by purpose (user, developer, archive)

2. **Simplified third-party management**
   - Single `/third_party/` directory at root
   - No duplicates
   - Consistent naming
   - Properly ignored by git
   - Clear README explaining how to obtain dependencies

3. **Cleaner git status**
   - No JUCE submodule changes showing up
   - No confusion about what's tracked
   - Smaller repository size (if pushing)

4. **Easier maintenance**
   - Clear where to find things
   - New contributors can understand structure
   - CI/CD knows where to find dependencies

## Migration Steps (Execute in Order)

### Step 1: Backup
```bash
git stash
git checkout -b repo-cleanup
```

### Step 2: Documentation
```bash
# Create new structure
mkdir -p docs/development docs/archive

# Move files
mv ChartPreview/docs/VST2-REAPER-INTEGRATION.md docs/development/
mv ChartPreview/docs/archive/* docs/archive/
mv docs/REAPER_*.md docs/archive/  # Move old research to archive

# Remove empty dirs
rm -rf ChartPreview/docs
```

### Step 3: Third-Party
```bash
# Move the JUCE we actually use to root
mv ChartPreview/third_party/JUCE third_party/JUCE-chartpreview
rm -rf third_party/JUCE  # Remove old copy
mv third_party/JUCE-chartpreview third_party/JUCE

# Remove duplicates
rm -rf ChartPreview/ThirdParty
rm -rf ChartPreview/third_party

# Organize reference materials
mkdir -p third_party/reference
mv third_party/JUCE-reaper-embedded-fx-gui third_party/reference/
mv third_party/sws third_party/reference/ 2>/dev/null || true

# Create README
cat > third_party/README.md << 'EOF'
[content from proposal above]
EOF
```

### Step 4: Update Project Files
```bash
# Update .jucer file (manual edit in Projucer)
# Change: third_party/JUCE/modules
# To:     ../third_party/JUCE/modules

# Regenerate builds
cd ChartPreview
/Applications/JUCE/Projucer.app/Contents/MacOS/Projucer --resave ChartPreview.jucer
```

### Step 5: Update .gitignore
```bash
# Add to .gitignore:
echo "" >> .gitignore
echo "# -----------------------------------------------------------------------------" >> .gitignore
echo "# Third-Party Dependencies (obtain via third_party/README.md)" >> .gitignore
echo "# -----------------------------------------------------------------------------" >> .gitignore
echo "/third_party/" >> .gitignore
echo "!/third_party/README.md" >> .gitignore
echo "!/third_party/.gitkeep" >> .gitignore
```

### Step 6: Clean Git Cache
```bash
git rm -r --cached third_party/ 2>/dev/null || true
git add .gitignore
git add third_party/README.md
git add docs/
git status  # Verify looks clean
```

### Step 7: Test Build
```bash
cd ChartPreview
./build.sh
# Verify plugin builds successfully
```

### Step 8: Commit
```bash
git add -A
git commit -m "Refactor: Consolidate docs and third-party dependencies

- Move all documentation to /docs/ with clear hierarchy
- Consolidate third-party dependencies to single /third_party/ directory
- Remove duplicate REAPER SDK and JUCE copies
- Update .gitignore to exclude third-party directory
- Add third_party/README.md explaining how to obtain dependencies
- Update .jucer module paths to reference new location

This cleanup reduces confusion about which dependencies are used,
eliminates 141MB of duplicates, and provides clearer project structure."
```

## Rollback Plan

If anything breaks:
```bash
git checkout main
git branch -D repo-cleanup
git stash pop
```

## Post-Cleanup Validation

- [ ] `git status` shows clean working tree
- [ ] Documentation is easily findable in `/docs/`
- [ ] Build succeeds with `./build.sh`
- [ ] Plugin loads in REAPER
- [ ] VST2 REAPER integration still works
- [ ] CI/CD builds still pass
- [ ] No JUCE submodule changes show in git status

## Size Comparison

**Before**:
- Total repo: ~350MB (with duplicates)
- Git-tracked: ~50MB
- Third-party duplicates: ~141MB

**After**:
- Total repo: ~210MB (no duplicates)
- Git-tracked: ~50MB
- Third-party (ignored): ~160MB
- Single source of truth for all dependencies
