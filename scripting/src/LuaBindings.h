#pragma once
#include <array>
#include "EmulationSession.h"
#include "LuaRuntime.h"
#include "lauxlib.h"
#include "lua.h"

struct LuaRuntime::Impl {
    enum class Phase { Idle, Loading, BeforeFrame, AfterFrame, StateLoaded, Closing };

    EmulationSession &session;
    LuaLimits limits;
    Logger logger;
    lua_State *lua{nullptr};
    std::array<int, 3> handlers{LUA_NOREF, LUA_NOREF, LUA_NOREF};
    std::string name;
    std::string error;
    size_t used{0};
    size_t peak{0};
    uint64_t remaining{0};
    size_t logRemaining{0};
    bool active{false};
    bool instructionFault{false};
    bool memoryFault{false};
    Phase phase{Phase::Idle};

    Impl(EmulationSession &session, LuaLimits limits, Logger logger);

    ~Impl();

    void Close();

    void Deactivate();

    void Fail(std::string message);

    void Log(std::string_view message);

    void ResetBudget(uint64_t budget);

    void Dispatch(SessionEvent event, uint64_t frame);

    bool Load(std::string_view source, std::string_view label);

    static Impl &Self(lua_State *state);

    static void *Allocate(void *context, void *pointer, size_t oldSize, size_t newSize);

    static void Hook(lua_State *state, lua_Debug *debug);

    static int Traceback(lua_State *state);

    static int Initialize(lua_State *state);

    static int ProtectedCall(lua_State *state);

    static int ErrorHandler(lua_State *state);

    static int SetMetatable(lua_State *state);

    static int On(lua_State *state);

    static int LogMessage(lua_State *state);

    static int Print(lua_State *state);

    static int FrameCount(lua_State *state);

    static int Model(lua_State *state);

    static int CgbMode(lua_State *state);

    static int Peek(lua_State *state);

    static int PeekWord(lua_State *state);

    static int PeekWram(lua_State *state);

    static int Poke(lua_State *state);

    static int GetButtons(lua_State *state);

    static int SetButtons(lua_State *state);

    static int SaveState(lua_State *state);

    static int LoadState(lua_State *state);

    static int Pause(lua_State *state);

    static int Stop(lua_State *state);

    void RequirePhase(lua_State *state, Phase required) const;
};
