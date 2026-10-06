#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include "LuaBindings.h"

namespace {
    constexpr size_t kMaxSourceBytes = 1024 * 1024;
    constexpr int kHookInterval = 1000;

    std::string ErrorMessage(lua_State *state, const char *fallback) {
        if (lua_type(state, -1) != LUA_TSTRING)
            return fallback;
        size_t length;
        const char *message = lua_tolstring(state, -1, &length);
        return {message, std::min(length, size_t{4096})};
    }
} // namespace

LuaRuntime::Impl::Impl(EmulationSession &host, const LuaLimits configuration, Logger output) : session(host), limits(configuration),
                                                                                               logger(std::move(output)) {
    if (!limits.instructionsPerFrame || limits.instructionsPerFrame > std::numeric_limits<uint64_t>::max() / 10 || !limits.memoryBytes)
        throw std::invalid_argument("Lua limits must be positive and instruction budget must not overflow");
    if (!logger)
        logger = [](std::string_view message) { std::cerr << message << '\n'; };
}

LuaRuntime::Impl::~Impl() { Close(); }

void LuaRuntime::Impl::Deactivate() {
    active = false;
    session.SetEventHandler({});
    session.CancelQueuedState();
    session.ClearOverrides();
}

void LuaRuntime::Impl::ResetBudget(const uint64_t budget) {
    remaining = budget;
    logRemaining = 64 * 1024;
    instructionFault = memoryFault = false;
}

void LuaRuntime::Impl::Close() {
    Deactivate();
    if (!lua)
        return;
    phase = Phase::Closing;
    ResetBudget(limits.instructionsPerFrame);
    lua_close(lua);
    lua = nullptr;
    handlers.fill(LUA_NOREF);
    phase = Phase::Idle;
}

void LuaRuntime::Impl::Fail(std::string message) {
    error = std::move(message);
    Deactivate();
    logger("[Lua " + name + "] " + error);
}

void LuaRuntime::Impl::Log(const std::string_view message) {
    const auto size = std::min({message.size(), size_t{4096}, logRemaining});
    if (!size)
        return;
    logRemaining -= size;
    logger("[Lua " + name + "] " + std::string(message.substr(0, size)));
}

LuaRuntime::Impl &LuaRuntime::Impl::Self(lua_State *state) { return **static_cast<Impl **>(lua_getextraspace(state)); }

void *LuaRuntime::Impl::Allocate(void *context, void *pointer, const size_t oldSize, const size_t newSize) {
    auto &self = *static_cast<Impl *>(context);
    const size_t previous = pointer ? oldSize : 0;
    if (!newSize) {
        std::free(pointer);
        self.used -= previous;
        return nullptr;
    }
    if (newSize > self.limits.memoryBytes - (self.used - previous)) {
        self.memoryFault = true;
        return nullptr;
    }
    void *result = std::realloc(pointer, newSize);
    if (!result) {
        self.memoryFault = true;
        return nullptr;
    }
    self.used = self.used - previous + newSize;
    self.peak = std::max(self.peak, self.used);
    return result;
}

void LuaRuntime::Impl::Hook(lua_State *state, lua_Debug *) {
    auto &self = Self(state);
    if (self.memoryFault)
        luaL_error(state, "Lua memory limit exceeded");
    if (self.instructionFault || self.remaining <= kHookInterval) {
        self.instructionFault = true;
        luaL_error(state, "Lua instruction budget exceeded");
    }
    self.remaining -= kHookInterval;
}

int LuaRuntime::Impl::Traceback(lua_State *state) {
    const char *message = lua_tostring(state, 1);
    luaL_traceback(state, state, message ? message : "non-string Lua error", 1);
    return 1;
}

bool LuaRuntime::Impl::Load(const std::string_view source, const std::string_view label) {
    Close();
    error.clear();
    name = label;
    peak = 0;
    if (source.size() > kMaxSourceBytes) {
        Fail("script exceeds the 1 MiB source limit");
        return false;
    }
    ResetBudget(limits.instructionsPerFrame * 10);
    lua = lua_newstate(Allocate, this, 0);
    if (!lua) {
        Fail("cannot allocate Lua environment within the memory limit");
        return false;
    }
    *static_cast<Impl **>(lua_getextraspace(lua)) = this;
    lua_sethook(lua, Hook, LUA_MASKCOUNT, kHookInterval);
    phase = Phase::Loading;
    lua_pushcfunction(lua, Initialize);
    int status = lua_pcall(lua, 0, 0, 0);
    if (status == LUA_OK) {
        lua_pushcfunction(lua, Traceback);
        status = luaL_loadbufferx(lua, source.data(), source.size(), name.c_str(), "t");
        if (status == LUA_OK)
            status = lua_pcall(lua, 0, 0, 1);
    }
    phase = Phase::Idle;
    if (status != LUA_OK || memoryFault || instructionFault) {
        Fail(memoryFault ? "Lua memory limit exceeded" : instructionFault ? "Lua instruction budget exceeded" : ErrorMessage(lua, "Lua load failed"));
        Close();
        return false;
    }
    lua_settop(lua, 0);
    active = true;
    session.SetEventHandler([this](const SessionEvent event, const uint64_t frame) { Dispatch(event, frame); });
    return true;
}

void LuaRuntime::Impl::Dispatch(const SessionEvent event, const uint64_t frame) {
    if (!active || !lua)
        return;
    if (event == SessionEvent::BeforeFrame || event == SessionEvent::StateLoaded)
        ResetBudget(limits.instructionsPerFrame);
    const int ref = handlers[static_cast<size_t>(event)];
    if (ref == LUA_NOREF)
        return;
    phase = event == SessionEvent::BeforeFrame ? Phase::BeforeFrame : event == SessionEvent::AfterFrame ? Phase::AfterFrame : Phase::StateLoaded;
    lua_pushcfunction(lua, Traceback);
    lua_rawgeti(lua, LUA_REGISTRYINDEX, ref);
    const int arguments = event == SessionEvent::StateLoaded ? 0 : 1;
    if (arguments)
        lua_pushinteger(lua, static_cast<lua_Integer>(frame));
    const int status = lua_pcall(lua, arguments, 0, 1);
    phase = Phase::Idle;
    if (status != LUA_OK || memoryFault || instructionFault) {
        Fail(memoryFault
                 ? "Lua memory limit exceeded"
                 : instructionFault
                       ? "Lua instruction budget exceeded"
                       : ErrorMessage(lua, "Lua callback failed"));
    }
    lua_settop(lua, 0);
    if (!active)
        Close();
}

LuaRuntime::LuaRuntime(EmulationSession &session, const LuaLimits limits, Logger logger) : impl_(
    std::make_unique<Impl>(session, limits, std::move(logger))) {}

LuaRuntime::~LuaRuntime() = default;

bool LuaRuntime::LoadString(const std::string_view source, const std::string_view name) const { return impl_->Load(source, name); }

bool LuaRuntime::LoadFile(const std::string &path) const {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream || stream.tellg() < 0 || stream.tellg() > static_cast<std::streamoff>(kMaxSourceBytes)) {
        impl_->Close();
        impl_->name = path;
        impl_->Fail("cannot read script or script exceeds the 1 MiB source limit");
        return false;
    }
    std::string source((stream.tellg()), '\0');
    stream.seekg(0);
    if (!stream.read(source.data(), static_cast<std::streamsize>(source.size()))) {
        impl_->Close();
        impl_->name = path;
        impl_->Fail("failed to read script");
        return false;
    }
    return impl_->Load(source, "@" + path);
}

void LuaRuntime::Stop() const { impl_->Close(); }

bool LuaRuntime::Active() const { return impl_->active; }

const std::string &LuaRuntime::LastError() const { return impl_->error; }

size_t LuaRuntime::MemoryUsed() const { return impl_->used; }

size_t LuaRuntime::PeakMemoryUsed() const { return impl_->peak; }
