#pragma once

#include <cmath>
#include <algorithm>
#include <string>

namespace fino {

struct NoteInfo {
    int midi_note{69};                  // MIDI note number (69 = A4)
    std::string note_name{"A4"};        // e.g. "C5", "G#4", "A4"
    std::string note_name_pt{"La 4"};   // e.g. "Do 5", "Sol# 4", "La 4"
    float nominal_frequency_hz{440.0f}; // Standard frequency of this note at A4 = source_a4_hz
    float equivalent_a4_hz{440.0f};     // A4 reference frequency resulting from target_hz
    float cents_offset{0.0f};           // Deviation in cents from source_a4_hz
    float semitones_offset{0.0f};       // Deviation in semitones
    float ratio{1.0f};                  // Pitch shift multiplier ratio (equivalent_a4_hz / source_a4_hz)
};

/**
 * @brief Pure math functions for pitch and frequency calculations.
 * Suitable for constexpr and zero-allocation DSP logic.
 */
class PitchMath {
public:
    static constexpr float MIN_PITCH_RATIO = 0.25f;  // -24 semitones (-2 octaves)
    static constexpr float MAX_PITCH_RATIO = 4.0f;   // +24 semitones (+2 octaves)
    static constexpr float DEFAULT_BASE_HZ = 440.0f; // Concert pitch A4

    /**
     * @brief Converts semitones and cents offset into a frequency multiplier ratio.
     * Ratio = 2 ^ ((semitones + cents / 100) / 12)
     */
    [[nodiscard]] static inline float semitones_to_ratio(float semitones, float cents = 0.0f) noexcept {
        const float total_semitones = semitones + (cents / 100.0f);
        const float ratio = std::pow(2.0f, total_semitones / 12.0f);
        return clamp_ratio(ratio);
    }

    /**
     * @brief Converts a frequency multiplier ratio into musical semitones.
     * Semitones = 12 * log2(ratio)
     */
    [[nodiscard]] static inline float ratio_to_semitones(float ratio) noexcept {
        if (ratio <= 0.0f) {
            return 0.0f;
        }
        return 12.0f * std::log2(ratio);
    }

    /**
     * @brief Clamps a pitch ratio within safe audible bounds [0.25, 4.0].
     */
    [[nodiscard]] static constexpr float clamp_ratio(float ratio) noexcept {
        return std::clamp(ratio, MIN_PITCH_RATIO, MAX_PITCH_RATIO);
    }

    /**
     * @brief Finds the nearest 12-TET musical note for a given target frequency,
     * and calculates the resulting equivalent A4 reference pitch.
     *
     * Example:
     * - 528.00 Hz corresponds to C5 (nominal 523.25 Hz at A4=440),
     *   yielding equivalent A4 = 528.00 / 2^(3/12) = 444.00 Hz (+15.67 cents).
     * - 417.00 Hz corresponds to G#4 (nominal 415.30 Hz at A4=440),
     *   yielding equivalent A4 = 417.00 / 2^(-1/12) = 441.78 Hz (+7.00 cents).
     * - 432.00 Hz corresponds to A4 (nominal 440.00 Hz at A4=440),
     *   yielding equivalent A4 = 432.00 Hz (-31.76 cents).
     */
    [[nodiscard]] static inline NoteInfo find_nearest_note(float target_hz, float source_a4_hz = DEFAULT_BASE_HZ) {
        NoteInfo info;
        if (target_hz <= 10.0f || source_a4_hz <= 10.0f) {
            return info;
        }

        // Calculate continuous MIDI note float relative to source A4 (MIDI 69)
        const float midi_float = 69.0f + 12.0f * std::log2(target_hz / source_a4_hz);
        const int midi_note = std::clamp(static_cast<int>(std::lround(midi_float)), 0, 127);
        info.midi_note = midi_note;

        // Note names
        static constexpr const char* NOTE_NAMES[] = {
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        static constexpr const char* NOTE_NAMES_PT[] = {
            "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"
        };

        const int note_index = (midi_note % 12 + 12) % 12;
        const int octave = (midi_note / 12) - 1;
        info.note_name = std::string(NOTE_NAMES[note_index]) + std::to_string(octave);
        info.note_name_pt = std::string(NOTE_NAMES_PT[note_index]) + " " + std::to_string(octave);

        const int semitones_from_a4 = midi_note - 69;
        const float note_interval_factor = std::pow(2.0f, static_cast<float>(semitones_from_a4) / 12.0f);

        info.nominal_frequency_hz = source_a4_hz * note_interval_factor;
        info.equivalent_a4_hz = target_hz / note_interval_factor;

        const float ratio = clamp_ratio(info.equivalent_a4_hz / source_a4_hz);
        info.ratio = ratio;
        info.semitones_offset = 12.0f * std::log2(ratio);
        info.cents_offset = info.semitones_offset * 100.0f;

        return info;
    }

    /**
     * @brief Converts source A4 frequency (Hz) and target frequency (Hz) into a pitch multiplier ratio,
     * correctly mapping through the nearest musical note's equivalent A4 reference pitch.
     */
    [[nodiscard]] static inline float frequency_to_ratio(float source_a4_hz, float target_hz) noexcept {
        if (source_a4_hz <= 0.001f || target_hz <= 0.001f) {
            return 1.0f;
        }
        const NoteInfo info = find_nearest_note(target_hz, source_a4_hz);
        return info.ratio;
    }

    /**
     * @brief Direct raw frequency ratio without note retuning (for raw mathematical tests).
     */
    [[nodiscard]] static inline float direct_ratio(float f_in, float f_out) noexcept {
        if (f_in <= 0.001f || f_out <= 0.001f) {
            return 1.0f;
        }
        return clamp_ratio(f_out / f_in);
    }

    /**
     * @brief Linear decibel to linear amplitude conversion.
     */
    [[nodiscard]] static inline float db_to_linear(float db) noexcept {
        return std::pow(10.0f, db / 20.0f);
    }

    /**
     * @brief Linear amplitude to decibel conversion.
     */
    [[nodiscard]] static inline float linear_to_db(float lin) noexcept {
        if (lin <= 1e-5f) {
            return -100.0f;
        }
        return 20.0f * std::log10(lin);
    }
};

} // namespace fino
