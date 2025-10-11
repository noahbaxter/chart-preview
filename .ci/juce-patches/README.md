# JUCE Patches for REAPER Integration

These patches add REAPER API access to JUCE's VST2 wrapper.

## What These Patches Do

Stock JUCE doesn't automatically perform the REAPER API handshake. These patches add:

1. **01-add-reaper-api-handler.patch**: Adds `handleReaperApi()` virtual method to `VST2ClientExtensions`
2. **02-add-reaper-handshake.patch**: Adds automatic REAPER handshake using magic numbers `0xdeadbeef` and `0xdeadf00d`

## How CI/CD Applies These Patches

The GitHub Actions workflow:
1. Downloads official JUCE 8.0.0 from juce-framework/JUCE
2. Applies these patches using `git apply` or `patch` command
3. Builds the plugin with patched JUCE

## Base JUCE Version

These patches are designed for **JUCE 8.0.0**.

## Testing Patches Locally

To test if patches apply cleanly:

```bash
# Download fresh JUCE 8.0.0
cd /tmp
curl -L "https://github.com/juce-framework/JUCE/releases/download/8.0.0/juce-8.0.0-osx.zip" -o juce.zip
unzip juce.zip
cd JUCE

# Apply patches
git init
git add .
git commit -m "Initial JUCE 8.0.0"
git apply /path/to/.ci/juce-patches/*.patch

# Check if patches applied successfully
git diff --stat
```

## Why Not Use a Submodule?

We chose to vendor JUCE directly rather than use a submodule because:
- Patches are small and stable
- No need to maintain a JUCE fork
- Simpler for contributors (no submodule initialization)
- CI can always download fresh JUCE and apply patches

## Documentation

See `docs/JUCE_REAPER_MODIFICATIONS.md` for detailed explanation of what these modifications do and how to use them in your code.
