#pragma once

#include "AuthoringTypes.h"
#include <vector>

enum class SubMode;

struct Binding
{
    SubMode       mode;
    EventType     event;
    MouseButton   button;
    ModifierFlags modifiers;
    WriteCommand  command;
};

class CommandMapper
{
public:
    CommandMapper();

    WriteCommand resolve(SubMode mode, EventType event,
                         const AuthoringContext& ctx) const;

    const std::vector<Binding>& getBindings() const { return bindings; }
    void setBindings(std::vector<Binding> newBindings) { bindings = std::move(newBindings); }

private:
    std::vector<Binding> bindings;

    static MouseButton buttonFromContext(const AuthoringContext& ctx);
    static ModifierFlags modifiersFromContext(const AuthoringContext& ctx);
};
