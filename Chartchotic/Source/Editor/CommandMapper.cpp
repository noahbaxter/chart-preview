#include "CommandMapper.h"

CommandMapper::CommandMapper()
{
    bindings = {
        // Shift+Left: paint (must precede non-shift sustain bindings)
        { SubMode::Draw, EventType::Down, MouseButton::Left,  ModifierFlags::Shift, WriteCommand::BeginPaint },
        { SubMode::Draw, EventType::Drag, MouseButton::Left,  ModifierFlags::Shift, WriteCommand::ContinuePaint },
        { SubMode::Draw, EventType::Up,   MouseButton::Left,  ModifierFlags::Shift, WriteCommand::CommitPaint },
        // Left: sustain
        { SubMode::Draw, EventType::Down, MouseButton::Left,  ModifierFlags::None, WriteCommand::BeginSustain },
        { SubMode::Draw, EventType::Drag, MouseButton::Left,  ModifierFlags::None, WriteCommand::UpdateSustain },
        { SubMode::Draw, EventType::Up,   MouseButton::Left,  ModifierFlags::None, WriteCommand::CommitSustain },
        // Right: erase
        { SubMode::Draw, EventType::Down, MouseButton::Right, ModifierFlags::None, WriteCommand::BeginErase },
        { SubMode::Draw, EventType::Drag, MouseButton::Right, ModifierFlags::None, WriteCommand::ContinueErase },
        { SubMode::Draw, EventType::Up,   MouseButton::Right, ModifierFlags::None, WriteCommand::EndErase },
        // Edit mode — left click/drag: select, marquee, or move
        { SubMode::Edit, EventType::Down, MouseButton::Left,  ModifierFlags::None, WriteCommand::SelectAt },
        { SubMode::Edit, EventType::Drag, MouseButton::Left,  ModifierFlags::None, WriteCommand::ContinueMarquee },
        { SubMode::Edit, EventType::Up,   MouseButton::Left,  ModifierFlags::None, WriteCommand::CommitMarquee },
        // Alt+Left: axis-locked move
        { SubMode::Edit, EventType::Down, MouseButton::Left,  ModifierFlags::Alt,  WriteCommand::SelectAt },
        { SubMode::Edit, EventType::Drag, MouseButton::Left,  ModifierFlags::Alt,  WriteCommand::ContinueMove },
        { SubMode::Edit, EventType::Up,   MouseButton::Left,  ModifierFlags::Alt,  WriteCommand::CommitMove },
        // Double-click
        { SubMode::Edit, EventType::DoubleClick, MouseButton::Left, ModifierFlags::None, WriteCommand::DoubleClick },
    };

    keyBindings = {
        // Everything reachable here sits in the left-hand block, because the
        // right hand is on the mouse the whole time you are charting.
        // QWERT row: modes.
        { 'Q', ModifierFlags::None,  true,  WriteCommand::ToggleSubMode },
        { 'W', ModifierFlags::None,  false, WriteCommand::ToggleWriteMode },
        { 'E', ModifierFlags::None,  true,  WriteCommand::ToggleSnap },
        // Shift+T cycles 3/5/7, plain T toggles the last-used value on and off
        { 'T', ModifierFlags::Shift, true,  WriteCommand::CycleTuplet },
        { 'T', ModifierFlags::None,  true,  WriteCommand::ToggleTuplet },

        // Home row: note-type slots, meaning depends on the active instrument.
        { 'A', ModifierFlags::None,  true,  WriteCommand::ModifierSlot1 },
        { 'S', ModifierFlags::None,  true,  WriteCommand::ModifierSlot2 },
        { 'D', ModifierFlags::None,  true,  WriteCommand::ModifierSlot3 },
        { 'F', ModifierFlags::None,  true,  WriteCommand::ModifierSlot4 },
        { 'G', ModifierFlags::None,  true,  WriteCommand::ModifierSlot5 },

        // ZXCVB row: tools.
        { 'B', ModifierFlags::None,  true,  WriteCommand::ToggleBarMode },

        // Grid stepping. The wheel (Alt+scroll) is the primary control.
        { '[', ModifierFlags::None,  true,  WriteCommand::StepDown },
        { ']', ModifierFlags::None,  true,  WriteCommand::StepUp },

        { juce::KeyPress::deleteKey,    ModifierFlags::None, true, WriteCommand::DeleteSelection },
        { juce::KeyPress::backspaceKey, ModifierFlags::None, true, WriteCommand::DeleteSelection },
        { juce::KeyPress::escapeKey,    ModifierFlags::None, true, WriteCommand::DeselectAll },
    };
}

MouseButton CommandMapper::buttonFromContext(const AuthoringContext& ctx)
{
    if (ctx.leftButton)  return MouseButton::Left;
    if (ctx.rightButton) return MouseButton::Right;
    return MouseButton::None;
}

ModifierFlags CommandMapper::modifiersFromContext(const AuthoringContext& ctx)
{
    auto flags = ModifierFlags::None;
    if (ctx.mods.isShiftDown()) flags = flags | ModifierFlags::Shift;
    if (ctx.mods.isCtrlDown())  flags = flags | ModifierFlags::Ctrl;
    if (ctx.mods.isAltDown())   flags = flags | ModifierFlags::Alt;
    return flags;
}

WriteCommand CommandMapper::resolve(SubMode mode, EventType event,
                                    const AuthoringContext& ctx) const
{
    MouseButton btn = buttonFromContext(ctx);
    ModifierFlags mods = modifiersFromContext(ctx);

    for (const auto& b : bindings)
    {
        if (b.mode != mode || b.event != event || b.button != btn)
            continue;
        if (b.modifiers != ModifierFlags::None && !(mods & b.modifiers))
            continue;
        if (b.modifiers == ModifierFlags::None && mods != ModifierFlags::None)
            continue;
        return b.command;
    }
    return WriteCommand::None;
}

WriteCommand CommandMapper::resolveKey(bool writeModeActive,
                                       const juce::KeyPress& key) const
{
    int code = key.getKeyCode();
    auto mods = modifiersFromKeyPress(key);

    for (const auto& kb : keyBindings)
    {
        if (kb.keyCode != code) continue;
        if (kb.requiresWriteMode && !writeModeActive) continue;
        // Same matching rules as the mouse table: a binding that names a
        // modifier requires it, a binding that names none requires a bare key.
        // This is what keeps Shift+T off the plain-T binding, and it stops
        // host shortcuts like Cmd+S from firing an editor command.
        if (kb.modifiers != ModifierFlags::None && !(mods & kb.modifiers))
            continue;
        if (kb.modifiers == ModifierFlags::None && mods != ModifierFlags::None)
            continue;
        return kb.command;
    }
    return WriteCommand::None;
}

ModifierFlags CommandMapper::modifiersFromKeyPress(const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    auto flags = ModifierFlags::None;
    if (mods.isShiftDown())   flags = flags | ModifierFlags::Shift;
    if (mods.isCtrlDown())    flags = flags | ModifierFlags::Ctrl;
    if (mods.isAltDown())     flags = flags | ModifierFlags::Alt;
    if (mods.isCommandDown()) flags = flags | ModifierFlags::Cmd;
    return flags;
}
