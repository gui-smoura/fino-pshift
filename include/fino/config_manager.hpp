#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace fino {

struct Preset {
    std::string name;
    int mode{0}; // 0 = Musical (Semitones/Cents), 1 = Frequency (Hz)
    float semitones{0.0f};
    float cents{0.0f};
    float source_hz{440.0f};
    float target_hz{432.0f};
};

struct AppConfig {
    std::string capture_device_name{""};
    std::string playback_device_name{""};
    uint32_t buffer_frames{512};
    uint32_t sample_rate{48000};

    int mode{0}; // 0 = Musical, 1 = Frequency
    float semitones{0.0f};
    float cents{0.0f};
    float source_hz{440.0f};
    float target_hz{432.0f};

    bool bypass{false};
    float gain_db{0.0f};

    std::vector<Preset> presets;
};

class ConfigManager {
public:
    static AppConfig get_default_config();

    static bool load_from_file(const std::string& filepath, AppConfig& config);
    static bool save_to_file(const std::string& filepath, const AppConfig& config);
};

} // namespace fino
