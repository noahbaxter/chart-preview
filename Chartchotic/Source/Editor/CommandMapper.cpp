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
        { 'W', false, WriteCommand::ToggleWriteMode },
        { 'Q', true,  WriteCommand::ToggleSubMode },
        { 'S', true,  WriteCommand::ToggleSnap },
        { 'T', true,  WriteCommand::CycleTuplet },
        { '[', true,  WriteCommand::StepDown },
        { ']', true,  WriteCommand::StepUp },
        { juce::KeyPress::deleteKey,    true, WriteCommand::DeleteSelection },
        { juce::KeyPress::backspaceKey, true, WriteCommand::DeleteSelection },
        { juce::KeyPress::escapeKey,    true, WriteCommand::DeselectAll },
        { 'B', true,  WriteCommand::ToggleBarMode },
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
    for (const auto& kb : keyBindings)
    {
        if (kb.keyCode != code) continue;
        if (kb.requiresWriteMode && !writeModeActive) continue;
        return kb.command;
    }
    return WriteCommand::None;
}
