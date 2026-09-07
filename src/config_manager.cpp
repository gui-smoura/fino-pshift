#include "fino/config_manager.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace fino {

using json = nlohmann::json;

AppConfig ConfigManager::get_default_config() {
    AppConfig cfg;
    cfg.capture_device_name = "";
    cfg.playback_device_name = "";
    cfg.buffer_frames = 512;
    cfg.sample_rate = 48000;
    cfg.mode = 0;
    cfg.semitones = 0.0f;
    cfg.cents = 0.0f;
    cfg.source_hz = 440.0f;
    cfg.target_hz = 432.0f;
    cfg.bypass = false;
    cfg.gain_db = 0.0f;

    // Factory presets
    cfg.presets = {
        {"432 Hz Retuning (Verdi)", 1, 0.0f, 0.0f, 440.0f, 432.0f},
        {"528 Hz Retuning (Solfeggio)", 1, 0.0f, 0.0f, 440.0f, 528.0f},
        {"Nightcore (+2 Semitones)", 0, 2.0f, 0.0f, 440.0f, 440.0f},
        {"Vaporwave / Slow (-2 Semitones)", 0, -2.0f, 0.0f, 440.0f, 440.0f},
        {"Male to Female (+3.5 Semitones)", 0, 3.5f, 0.0f, 440.0f, 440.0f},
        {"Female to Male (-3.5 Semitones)", 0, -3.5f, 0.0f, 440.0f, 440.0f},
        {"Sub-Bass Octave (-12 Semitones)", 0, -12.0f, 0.0f, 440.0f, 440.0f}
    };

    return cfg;
}

bool ConfigManager::load_from_file(const std::string& filepath, AppConfig& config) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        config = get_default_config();
        return false;
    }

    try {
        json j;
        file >> j;

        config.capture_device_name = j.value("capture_device_name", "");
        config.playback_device_name = j.value("playback_device_name", "");
        config.buffer_frames = j.value("buffer_frames", 512u);
        config.sample_rate = j.value("sample_rate", 48000u);
        config.mode = j.value("mode", 0);
        config.semitones = j.value("semitones", 0.0f);
        config.cents = j.value("cents", 0.0f);
        config.source_hz = j.value("source_hz", 440.0f);
        config.target_hz = j.value("target_hz", 432.0f);
        config.bypass = j.value("bypass", false);
        config.gain_db = j.value("gain_db", 0.0f);

        config.presets.clear();
        if (j.contains("presets") && j["presets"].is_array()) {
            for (const auto& item : j["presets"]) {
                Preset p;
                p.name = item.value("name", "Unnamed");
                p.mode = item.value("mode", 0);
                p.semitones = item.value("semitones", 0.0f);
                p.cents = item.value("cents", 0.0f);
                p.source_hz = item.value("source_hz", 440.0f);
                p.target_hz = item.value("target_hz", 432.0f);
                config.presets.push_back(p);
            }
        }

        if (config.presets.empty()) {
            config.presets = get_default_config().presets;
        }

        return true;
    } catch (const std::exception&) {
        config = get_default_config();
        return false;
    }
}

bool ConfigManager::save_to_file(const std::string& filepath, const AppConfig& config) {
    json j;
    j["capture_device_name"] = config.capture_device_name;
    j["playback_device_name"] = config.playback_device_name;
    j["buffer_frames"] = config.buffer_frames;
    j["sample_rate"] = config.sample_rate;
    j["mode"] = config.mode;
    j["semitones"] = config.semitones;
    j["cents"] = config.cents;
    j["source_hz"] = config.source_hz;
    j["target_hz"] = config.target_hz;
    j["bypass"] = config.bypass;
    j["gain_db"] = config.gain_db;

    json presets_array = json::array();
    for (const auto& p : config.presets) {
        json pj;
        pj["name"] = p.name;
        pj["mode"] = p.mode;
        pj["semitones"] = p.semitones;
        pj["cents"] = p.cents;
        pj["source_hz"] = p.source_hz;
        pj["target_hz"] = p.target_hz;
        presets_array.push_back(pj);
    }
    j["presets"] = presets_array;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        return false;
    }

    try {
        file << j.dump(4);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

} // namespace fino
