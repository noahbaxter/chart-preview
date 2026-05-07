#include "CommandMapper.h"

CommandMapper::CommandMapper()
{
    bindings = {
        { SubMode::Draw, EventType::Down, MouseButton::Left,  ModifierFlags::None, WriteCommand::BeginSustain },
        { SubMode::Draw, EventType::Drag, MouseButton::Left,  ModifierFlags::None, WriteCommand::UpdateSustain },
        { SubMode::Draw, EventType::Up,   MouseButton::Left,  ModifierFlags::None, WriteCommand::CommitSustain },
        { SubMode::Draw, EventType::Down, MouseButton::Right, ModifierFlags::None, WriteCommand::BeginErase },
        { SubMode::Draw, EventType::Drag, MouseButton::Right, ModifierFlags::None, WriteCommand::ContinueErase },
        { SubMode::Draw, EventType::Up,   MouseButton::Right, ModifierFlags::None, WriteCommand::EndErase },
    };

    keyBindings = {
        { 'W', false, WriteCommand::ToggleWriteMode },
        { 'Q', true,  WriteCommand::ToggleSubMode },
        { 'S', true,  WriteCommand::ToggleSnap },
        { 'T', true,  WriteCommand::CycleTuplet },
        { '[', true,  WriteCommand::StepDown },
        { ']', true,  WriteCommand::StepUp },
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
