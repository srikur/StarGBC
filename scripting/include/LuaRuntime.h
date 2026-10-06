#pragma once
#include <functional>
#include <memory>
#include <string>
#include <string_view>

class EmulationSession;

struct LuaLimits {
    uint64_t instructionsPerFrame{1'000'000};
    size_t memoryBytes{32 * 1024 * 1024};
};

class LuaRuntime {
public:
    using Logger = std::function<void(std::string_view)>;

    explicit LuaRuntime(EmulationSession &session, LuaLimits limits = {}, Logger logger = {});

    ~LuaRuntime();

    LuaRuntime(const LuaRuntime &) = delete;

    LuaRuntime &operator=(const LuaRuntime &) = delete;

    bool LoadFile(const std::string &path) const;

    bool LoadString(std::string_view source, std::string_view name = "script") const;

    void Stop() const;

    [[nodiscard]] bool Active() const;

    [[nodiscard]] const std::string &LastError() const;

    [[nodiscard]] size_t MemoryUsed() const;

    [[nodiscard]] size_t PeakMemoryUsed() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
