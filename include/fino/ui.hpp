#pragma once

#include "audio_engine.hpp"
#include "config_manager.hpp"

struct GLFWwindow;

namespace fino {

class UI {
public:
    UI(AudioEngine& engine, AppConfig& config, const std::string& config_path);
    ~UI() = default;

    void setup_style();
    void render();

private:
    void render_header();
    void render_device_section();
    void render_pitch_controls();
    void render_metering_and_latency();
    void render_presets_section();

    void apply_current_pitch();
    void sync_config_from_ui();

    AudioEngine& m_engine;
    AppConfig& m_config;
    std::string m_config_path;

    int m_selected_capture_idx{-1};
    int m_selected_playback_idx{-1};
    int m_selected_buffer_idx{2}; // 0=128, 1=256, 2=512, 3=1024

    char m_new_preset_name[64]{""};
    int m_selected_preset_idx{0};

    // Metering smooth decay values
    float m_in_peak_l_smooth{0.0f};
    float m_in_peak_r_smooth{0.0f};
    float m_out_peak_l_smooth{0.0f};
    float m_out_peak_r_smooth{0.0f};
};

} // namespace fino
