#pragma once

#include <SDL3/SDL.h>
#include <functional>
#include "DebugGraphics.h"
#include "Debugger.h"

struct ImGuiContext;

enum class DebugPanel { Memory, Breakpoints, Screen, Tiles, Maps, Sprites, Palettes, Hardware, Trace, Help };

class DebuggerWindow {
public:
    DebuggerWindow(EmulationSession &session, SDL_Window *window, SDL_Renderer *renderer, std::function<void(uint8_t)> save_state,
                   std::function<void(uint8_t)> load_state);
    ~DebuggerWindow();
    DebuggerWindow(const DebuggerWindow &) = delete;
    DebuggerWindow &operator=(const DebuggerWindow &) = delete;

    bool handle_event(const SDL_Event &event);
    void render();
    void toggle();
    void show_panel(DebugPanel panel) { requested_panel_ = panel; }
    bool handle_shortcut(const SDL_KeyboardEvent &key);
    [[nodiscard]] bool visible() const { return visible_; }
    [[nodiscard]] Debugger &controller() { return debugger_; }

private:
    void release_resources();
    bool begin_panel(DebugPanel panel, const char *label);
    void action(const std::function<void()> &run);
    void draw_cpu(const MachineInspection &state);
    void draw_memory();
    void draw_breakpoints();
    void draw_graphics(const GraphicsInspection &graphics);
    void draw_palettes(const GraphicsInspection &graphics);
    void draw_sprites(const GraphicsInspection &graphics);
    void draw_maps(const GraphicsInspection &graphics);
    void draw_hardware(const MachineInspection &state);
    void draw_trace();
    void draw_help();
    void draw_image(SDL_Texture *texture, const uint32_t *pixels, int width, int height, float scale);

    EmulationSession &session_;
    Gameboy &machine_;
    Debugger debugger_;
    std::function<void(uint8_t)> save_state_;
    std::function<void(uint8_t)> load_state_;
    SDL_Window *window_{};
    SDL_Renderer *renderer_{};
    SDL_Texture *screen_{};
    SDL_Texture *tiles_{};
    SDL_Texture *map_{};
    SDL_Texture *sprites_{};
    ImGuiContext *context_{};
    bool platform_ready_{};
    bool renderer_ready_{};
    bool visible_{true};
    bool game_input_{};
    int game_width_{};
    int game_height_{};
    bool follow_instruction_{true};
    bool cpu_memory_{};
    bool memory_follow_pc_{};
    int memory_source_{};
    int memory_bank_{};
    uint16_t memory_address_{WRAM_BEGIN};
    uint16_t code_address_{};
    uint16_t edit_address_{WRAM_BEGIN};
    uint8_t edit_value_{};
    uint16_t breakpoint_address_{};
    uint16_t breakpoint_end_{};
    uint16_t breakpoint_bank_{};
    bool breakpoint_banked_{};
    bool breakpoint_range_{};
    int breakpoint_kind_{};
    int tile_bank_{};
    int tile_palette_{};
    bool tile_objects_{};
    bool high_map_{};
    bool map_overlay_{true};
    int selected_sprite_{};
    int save_slot_{1};
    std::string error_;
    std::optional<DebugPanel> requested_panel_;
};
