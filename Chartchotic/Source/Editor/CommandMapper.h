#pragma once

#include "AuthoringTypes.h"
#include <vector>

struct Binding
{
    SubMode       mode;
    EventType     event;
    MouseButton   button;
    ModifierFlags modifiers;
    WriteCommand  command;
};

struct KeyBinding
{
    int           keyCode;
    bool          requiresWriteMode;
    WriteCommand  command;
};

class CommandMapper
{
public:
    CommandMapper();

    WriteCommand resolve(SubMode mode, EventType event,
                         const AuthoringContext& ctx) const;

    WriteCommand resolveKey(bool writeModeActive,
                            const juce::KeyPress& key) const;

private:
    std::vector<Binding>    bindings;
    std::vector<KeyBinding> keyBindings;

    static MouseButton   buttonFromContext(const AuthoringContext& ctx);
    static ModifierFlags modifiersFromContext(const AuthoringContext& ctx);
};
