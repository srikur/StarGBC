#include "LuaBindings.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <lualib.h>

namespace {
    constexpr auto kSnapshotType = "StarGBC.snapshot";
    constexpr std::array<std::pair<const char *, Keys>, 8> kButtons{
        {
            {"A", Keys::A},
            {"B", Keys::B},
            {"Start", Keys::Start},
            {"Select", Keys::Select},
            {"Up", Keys::Up},
            {"Down", Keys::Down},
            {"Left", Keys::Left},
            {"Right", Keys::Right}
        }
    };

    lua_Integer Integer(lua_State *state, const int index, const lua_Integer maximum) {
        luaL_argcheck(state, lua_isinteger(state, index), index, "expected an integer");
        const auto value = lua_tointeger(state, index);
        luaL_argcheck(state, value >= 0 && value <= maximum, index, "integer out of range");
        return value;
    }

    std::string_view String(lua_State *state, const int index) {
        luaL_checktype(state, index, LUA_TSTRING);
        size_t length;
        const char *text = lua_tolstring(state, index, &length);
        return {text, length};
    }
} // namespace

void LuaRuntime::Impl::RequirePhase(lua_State *state, const Phase required) const {
    if (phase != required)
        luaL_error(state, "operation is not allowed in this callback phase");
}

int LuaRuntime::Impl::ProtectedCall(lua_State *state) {
    const int arguments = lua_gettop(state);
    if (lua_toboolean(state, lua_upvalueindex(2))) {
        luaL_checktype(state, 2, LUA_TFUNCTION);
        lua_pushvalue(state, 2);
        lua_pushcclosure(state, ErrorHandler, 1);
        lua_replace(state, 2);
    }
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, arguments, LUA_MULTRET);
    const auto &self = Self(state);
    if (self.memoryFault)
        return luaL_error(state, "Lua memory limit exceeded");
    if (self.instructionFault)
        return luaL_error(state, "Lua instruction budget exceeded");
    return lua_gettop(state);
}

int LuaRuntime::Impl::ErrorHandler(lua_State *state) {
    const auto &self = Self(state);
    // A limit raised by a count hook must not enter a user error handler:
    // Lua has temporarily disabled hooks until that hook unwinds.
    if (self.instructionFault || self.memoryFault) {
        lua_pushvalue(state, 1);
        return 1;
    }
    lua_settop(state, 1);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, 1, 1);
    return 1;
}

int LuaRuntime::Impl::SetMetatable(lua_State *state) {
    luaL_checktype(state, 1, LUA_TTABLE);
    if (!lua_isnil(state, 2)) {
        luaL_checktype(state, 2, LUA_TTABLE);
        lua_pushliteral(state, "__gc");
        lua_rawget(state, 2);
        if (!lua_isnil(state, -1))
            return luaL_error(state, "Lua table finalizers are not supported");
        lua_pop(state, 1);
    }
    // Lua suppresses instruction hooks inside __gc. Reject registration of
    // finalizers, while retaining ordinary metatables and their other methods.
    lua_settop(state, 2);
    lua_pushvalue(state, lua_upvalueindex(1));
    lua_insert(state, 1);
    lua_call(state, 2, 1);
    return 1;
}

int LuaRuntime::Impl::Initialize(lua_State *state) {
    const luaL_Reg libraries[] = {
        {LUA_GNAME, luaopen_base},
        {LUA_TABLIBNAME, luaopen_table},
        {LUA_STRLIBNAME, luaopen_string},
        {LUA_MATHLIBNAME, luaopen_math},
        {LUA_UTF8LIBNAME, luaopen_utf8}
    };
    for (const auto &library : libraries) {
        luaL_requiref(state, library.name, library.func, 1);
        lua_pop(state, 1);
    }
    for (const char *name : {"load", "loadfile", "dofile"}) {
        lua_pushnil(state);
        lua_setglobal(state, name);
    }
    for (const char *name : {"pcall", "xpcall"}) {
        lua_getglobal(state, name);
        lua_pushboolean(state, std::string_view(name) == "xpcall");
        lua_pushcclosure(state, ProtectedCall, 2);
        lua_setglobal(state, name);
    }
    lua_getglobal(state, "setmetatable");
    lua_pushcclosure(state, SetMetatable, 1);
    lua_setglobal(state, "setmetatable");
    lua_pushcfunction(state, Print);
    lua_setglobal(state, "print");
    lua_getglobal(state, "math");
    lua_getfield(state, -1, "randomseed");
    lua_pushinteger(state, 0);
    lua_call(state, 1, 0);
    lua_pop(state, 1);

    const luaL_Reg root[] = {
        {"on", On}, {"log", LogMessage}, {"frame_count", FrameCount}, {"model", Model}, {"is_cgb_mode", CgbMode},
        {"pause", Pause}, {"stop", Stop}, {nullptr, nullptr}
    };
    luaL_newlib(state, root);
    lua_pushinteger(state, 1);
    lua_setfield(state, -2, "api_version");
    const luaL_Reg memory[] = {{"peek8", Peek}, {"peek16_le", PeekWord}, {"peek_wram", PeekWram}, {"poke8", Poke}, {nullptr, nullptr}};
    luaL_newlib(state, memory);
    lua_setfield(state, -2, "memory");
    constexpr luaL_Reg joypad[] = {{"get", GetButtons}, {"set", SetButtons}, {nullptr, nullptr}};
    luaL_newlib(state, joypad);
    lua_setfield(state, -2, "joypad");
    constexpr luaL_Reg snapshot[] = {{"save", SaveState}, {"load", LoadState}, {nullptr, nullptr}};
    luaL_newlib(state, snapshot);
    lua_setfield(state, -2, "state");
    lua_setglobal(state, "stargbc");
    luaL_newmetatable(state, kSnapshotType);
    lua_pushliteral(state, "StarGBC snapshot");
    lua_setfield(state, -2, "__metatable");
    lua_pop(state, 1);
    return 0;
}

int LuaRuntime::Impl::On(lua_State *state) {
    auto &self = Self(state);
    self.RequirePhase(state, Phase::Loading);
    const auto event = String(state, 1);
    luaL_checktype(state, 2, LUA_TFUNCTION);
    size_t index;
    if (event == "before_frame")
        index = 0;
    else if (event == "after_frame")
        index = 1;
    else if (event == "state_loaded")
        index = 2;
    else
        return luaL_error(state, "unknown event");
    lua_pushvalue(state, 2);
    const int reference = luaL_ref(state, LUA_REGISTRYINDEX);
    luaL_unref(state, LUA_REGISTRYINDEX, self.handlers[index]);
    self.handlers[index] = reference;
    return 0;
}

int LuaRuntime::Impl::LogMessage(lua_State *state) {
    try {
        Self(state).Log(String(state, 1));
        return 0;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::Print(lua_State *state) {
    try {
        std::string message;
        const int count = lua_gettop(state);
        for (int i = 1; i <= count && message.size() < 4096; ++i) {
            size_t length;
            const char *text = luaL_tolstring(state, i, &length);
            if (i > 1)
                message += '\t';
            message.append(text, std::min(length, 4096 - message.size()));
            lua_pop(state, 1);
        }
        Self(state).Log(message);
        return 0;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::FrameCount(lua_State *state) {
    lua_pushinteger(state, Self(state).session.FrameCount());
    return 1;
}

int LuaRuntime::Impl::Model(lua_State *state) {
    std::string name(ModelName(Self(state).session.Machine().GetModel()));
    std::ranges::transform(name, name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    lua_pushlstring(state, name.data(), name.size());
    return 1;
}

int LuaRuntime::Impl::CgbMode(lua_State *state) {
    lua_pushboolean(state, Self(state).session.Machine().IsInCgbMode());
    return 1;
}

int LuaRuntime::Impl::Peek(lua_State *state) {
    const auto address = static_cast<uint16_t>(Integer(state, 1, 0xFFFF));
    try {
        lua_pushinteger(state, Self(state).session.Machine().DebugPeek(address));
        return 1;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::PeekWord(lua_State *state) {
    const auto address = static_cast<uint16_t>(Integer(state, 1, 0xFFFE));
    try {
        auto &gb = Self(state).session.Machine();
        lua_pushinteger(state, gb.DebugPeek(address) | (gb.DebugPeek(address + 1) << 8));
        return 1;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::PeekWram(lua_State *state) {
    const auto bank = static_cast<uint8_t>(Integer(state, 1, 7));
    const auto offset = static_cast<uint16_t>(Integer(state, 2, 0xFFF));
    try {
        lua_pushinteger(state, Self(state).session.Machine().DebugPeekWram(bank, offset));
        return 1;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::Poke(lua_State *state) {
    auto &self = Self(state);
    self.RequirePhase(state, Phase::BeforeFrame);
    const auto address = static_cast<uint16_t>(Integer(state, 1, 0xFFFF));
    const auto value = static_cast<uint8_t>(Integer(state, 2, 0xFF));
    try {
        self.session.Machine().DebugPoke(address, value);
        return 0;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::GetButtons(lua_State *state) {
    lua_newtable(state);
    const auto pressed = Self(state).session.PhysicalButtons();
    for (const auto &[name, key] : kButtons) {
        lua_pushboolean(state, pressed & static_cast<uint8_t>(key));
        lua_setfield(state, -2, name);
    }
    return 1;
}

int LuaRuntime::Impl::SetButtons(lua_State *state) {
    const auto &self = Self(state);
    self.RequirePhase(state, Phase::BeforeFrame);
    luaL_checktype(state, 1, LUA_TTABLE);
    uint8_t mask = 0, pressed = 0;
    lua_pushnil(state);
    while (lua_next(state, 1)) {
        if (lua_type(state, -2) != LUA_TSTRING || !lua_isboolean(state, -1))
            return luaL_error(state, "buttons require string names and boolean values");
        const auto name = String(state, -2);
        const auto button = std::ranges::find_if(kButtons, [&](const auto &item) { return name == item.first; });
        if (button == kButtons.end())
            return luaL_error(state, "unknown button name");
        const auto bit = static_cast<uint8_t>(button->second);
        mask |= bit;
        if (lua_toboolean(state, -1))
            pressed |= bit;
        lua_pop(state, 1);
    }
    self.session.SetOverrides(mask, pressed);
    return 0;
}

int LuaRuntime::Impl::SaveState(lua_State *state) {
    const auto &self = Self(state);
    if (self.phase == Phase::Closing)
        return luaL_error(state, "runtime is closing");
    auto *bytes = static_cast<std::byte *>(lua_newuserdatauv(state, kGameboyStateSize, 0));
    SerializeInto(self.session.Machine(), bytes);
    luaL_setmetatable(state, kSnapshotType);
    return 1;
}

int LuaRuntime::Impl::LoadState(lua_State *state) {
    const auto &self = Self(state);
    if (self.phase != Phase::BeforeFrame && self.phase != Phase::AfterFrame)
        return luaL_error(state, "state.load requires a frame callback");
    auto *bytes = static_cast<std::byte *>(luaL_checkudata(state, 1, kSnapshotType));
    try {
        self.session.QueueState({bytes, kGameboyStateSize});
        return 0;
    } catch (const std::exception &e) {
        return luaL_error(state, "%s", e.what());
    }
}

int LuaRuntime::Impl::Pause(lua_State *state) {
    const auto &self = Self(state);
    if (self.phase == Phase::Closing)
        return luaL_error(state, "runtime is closing");
    self.session.SetPaused(true);
    return 0;
}

int LuaRuntime::Impl::Stop(lua_State *state) {
    auto &self = Self(state);
    if (self.phase == Phase::Loading || self.phase == Phase::Closing)
        return luaL_error(state, "stop requires an event callback");
    self.Deactivate();
    return 0;
}
