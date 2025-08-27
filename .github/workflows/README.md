# GitHub Actions Build Workflow

This workflow automatically builds Chart Preview VST3 plugin for Windows, macOS, and Linux.

## Triggers

- **Push to branches**: main, develop, refac
- **Pull requests** to main, develop, refac  
- **Releases** (creates downloadable artifacts)

## Build Outputs

### Windows (windows-latest)
- **VST3**: `ChartPreview-VST3-Windows-x64.zip`
- **AU**: `ChartPreview-AU-Windows-x64.zip` (if available)
- **Standalone**: `ChartPreview-Standalone-Windows-x64.zip` (if available)

### macOS (macos-latest)  
- **VST3**: `ChartPreview-VST3-macOS.zip`

### Linux (ubuntu-22.04)
- **VST3**: `ChartPreview-VST3-Linux-x64.zip`

## Artifacts

Artifacts are available for 30 days after each build:
1. Go to the "Actions" tab
2. Click on a workflow run
3. Scroll down to "Artifacts" section
4. Download the built plugins

## Releases

When you create a GitHub release:
1. The workflow automatically builds all three platforms
2. Creates zip files with the plugins
3. Attaches them to the release as downloadable assets

## Troubleshooting

If builds fail:
1. Check the logs in the Actions tab
2. Common issues:
   - Missing dependencies
   - Projucer file changes
   - Path issues with spaces in "Chart Preview" folder name

## Local Testing

To test locally before pushing:
```bash
# macOS
cd ChartPreview
./build.sh

# Windows (in Visual Studio Command Prompt)
cd ChartPreview  
msbuild "Builds/VisualStudio2022/ChartPreview.sln" /p:Configuration=Release /p:Platform=x64

# Linux
cd ChartPreview
./build-linux.sh
```