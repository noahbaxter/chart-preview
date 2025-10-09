# Third-Party Dependencies

This directory contains external dependencies required to build ChartPreview. These are **not tracked in git** to keep the repository size manageable.

## Required Dependencies

### JUCE Framework (Modified)

**Version**: 8.0.0 (with custom REAPER extensions)
**Location**: `third_party/JUCE/`
**Branch**: `reaper-vst2-extensions`

This project uses a **modified version** of JUCE with custom VST2 REAPER integration.

**Modifications**:
- Added `handleReaperApi()` callback to `VST2ClientExtensions`
- Modified VST2 wrapper to automatically perform REAPER handshake
- See `/docs/development/JUCE_REAPER_MODIFICATIONS.md` for complete details

**To obtain**:
```bash
cd third_party
git clone https://github.com/juce-framework/JUCE.git
cd JUCE
git checkout develop

# Apply our modifications (see docs/development/JUCE_REAPER_MODIFICATIONS.md)
# Or use our modified fork if available
```

### REAPER SDK

**Version**: Latest
**Location**: `third_party/reaper-sdk/`

**To obtain**:
1. Download from: https://www.reaper.fm/sdk/plugin/
2. Extract to `third_party/reaper-sdk/`

**Required files**:
- `sdk/reaper_plugin.h`
- `sdk/reaper_plugin_functions.h`
- `sdk/reaper_vst3_interfaces.h`
- `sdk/reaper_plugin_fx_embed.h`

## Reference Materials

The `reference/` subdirectory contains example projects for reference only:

### JUCE-reaper-embedded-fx-gui
- **Source**: https://github.com/GavinRay97/JUCE-reaper-embedded-fx-gui
- **Purpose**: Reference implementation of VST3 REAPER UI embedding
- **Note**: Demonstrates different functionality (UI embedding vs API access)

### SWS Extension
- **Source**: https://www.sws-extension.org/
- **Purpose**: Reference for REAPER API usage patterns
- **Note**: Advanced REAPER extension example

## Directory Structure

```
third_party/
├── README.md              # This file
├── JUCE/                  # Modified JUCE framework (not tracked)
├── reaper-sdk/            # REAPER SDK headers (not tracked)
└── reference/             # Reference implementations (not tracked)
    ├── JUCE-reaper-embedded-fx-gui/
    └── sws/
```

## Build System Integration

The ChartPreview `.jucer` project file references:
- JUCE modules: `../third_party/JUCE/modules/`

All paths are relative from the `ChartPreview/` directory.

## Troubleshooting

**Problem**: "Cannot find JUCE modules"
**Solution**: Ensure `third_party/JUCE/` exists and contains the modules directory

**Problem**: "REAPER headers not found"
**Solution**: Download REAPER SDK and extract to `third_party/reaper-sdk/`

**Problem**: Build fails with REAPER API errors
**Solution**: Check that you're using our modified JUCE (see JUCE_REAPER_MODIFICATIONS.md)
