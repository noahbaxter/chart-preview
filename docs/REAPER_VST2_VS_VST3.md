# Why VST2 Could Work for REAPER Integration

## The Fundamental Difference

### VST3 (Current Approach - Blocked)
- Uses COM-style interfaces with `queryInterface()`
- Requires proper interface inheritance from `FUnknown`
- Needs GUIDs and interface definitions
- JUCE hides the COM mechanics we need

### VST2 (Alternative Approach)
- Uses a **simple function pointer exchange**
- No COM interfaces, just C-style function calls
- REAPER passes a magic number and function pointer through the standard VST2 dispatcher

## How VST2 REAPER Integration Works

```cpp
// In your VST2 plugin's dispatcher function:
void* dispatcher(long opcode, long index, void* ptr) {
    if (opcode == 0xdeadbeef && index == 0xdeadf00d) {
        // ptr is a function pointer to get REAPER API functions!
        void* (*getFunc)(const char*) = (void*(*)(const char*))ptr;

        // Now you can get any REAPER function:
        auto MIDI_GetNote = (MIDI_GetNote_type)getFunc("MIDI_GetNote");
        auto GetPlayState = (GetPlayState_type)getFunc("GetPlayState");
        // etc...
    }
}
```

## Why This Is Simpler

1. **No Interface Queries** - Just check magic numbers in dispatcher
2. **No Linking Issues** - Pure function pointers, no SDK classes
3. **JUCE Supports It** - The dispatcher is exposed in AudioProcessor
4. **Well Documented** - Many REAPER extensions use this method

## Example Code That Actually Works

From various REAPER forums and SWS extensions:

```cpp
// In your AudioProcessor subclass:
void* getHostCallback(int32 opcode, int32 index, void* value, void* ptr, float opt) override
{
    // Check for REAPER's magic handshake
    if (opcode == 0xdeadbeef && index == 0xdeadf00d) {
        reaperGetFunc = (void*(*)(const char*))ptr;
        return (void*)1; // Tell REAPER we accepted it
    }
    return nullptr;
}
```

## The Trade-offs

### VST2 Pros:
- ✅ Much simpler REAPER integration
- ✅ No COM interface complexity
- ✅ Works today with JUCE
- ✅ Proven by many REAPER extensions

### VST2 Cons:
- ❌ VST2 is officially deprecated by Steinberg
- ❌ Some DAWs dropping VST2 support
- ❌ No official license for new developments
- ❌ Missing modern VST3 features

## Why We Started with VST3

- It's the modern standard
- Better for future compatibility
- The reference project showed it was "possible"
- Didn't realize JUCE's wrapper would block us

## Should We Switch?

If the goal is specifically REAPER integration for this feature, VST2 would work **today** with minimal changes. The question is whether losing VST3's future-proofing is worth the immediate functionality.

## References

- [REAPER Forums: VST2 GetFunc Method](https://forum.cockos.com/showthread.php?t=91614)
- SWS Extension source code uses this method
- Multiple JSFX scripts document this approach