#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "fino/pitch_math.hpp"

using Catch::Matchers::WithinAbs;

TEST_CASE("PitchMath semitones and cents conversions", "[pitch_math]") {
    // 0 semitones = 1.0 ratio
    REQUIRE_THAT(fino::PitchMath::semitones_to_ratio(0.0f), WithinAbs(1.0f, 1e-4f));

    // +12 semitones (one octave up) = 2.0 ratio
    REQUIRE_THAT(fino::PitchMath::semitones_to_ratio(12.0f), WithinAbs(2.0f, 1e-4f));

    // -12 semitones (one octave down) = 0.5 ratio
    REQUIRE_THAT(fino::PitchMath::semitones_to_ratio(-12.0f), WithinAbs(0.5f, 1e-4f));

    // +100 cents = +1 semitone
    float ratio_1st = fino::PitchMath::semitones_to_ratio(1.0f, 0.0f);
    float ratio_100c = fino::PitchMath::semitones_to_ratio(0.0f, 100.0f);
    REQUIRE_THAT(ratio_1st, WithinAbs(ratio_100c, 1e-4f));

    // Round-trip conversion: ratio -> semitones -> ratio
    float semitones = 3.5f;
    float ratio = fino::PitchMath::semitones_to_ratio(semitones);
    float back_semitones = fino::PitchMath::ratio_to_semitones(ratio);
    REQUIRE_THAT(back_semitones, WithinAbs(semitones, 1e-4f));
}

TEST_CASE("PitchMath note detection and A4 equivalence", "[pitch_math]") {
    // 1. 432.00 Hz: Note is A4, equivalent A4 = 432.00 Hz
    fino::NoteInfo info_432 = fino::PitchMath::find_nearest_note(432.00f, 440.00f);
    REQUIRE(info_432.note_name == "A4");
    REQUIRE(info_432.midi_note == 69);
    REQUIRE_THAT(info_432.equivalent_a4_hz, WithinAbs(432.00f, 0.05f));
    REQUIRE_THAT(info_432.cents_offset, WithinAbs(-31.76f, 0.1f));

    // 2. 528.00 Hz: Note is C5 (MIDI 72), equivalent A4 = 528 / 2^(3/12) ≈ 444.00 Hz
    fino::NoteInfo info_528 = fino::PitchMath::find_nearest_note(528.00f, 440.00f);
    REQUIRE(info_528.note_name == "C5");
    REQUIRE(info_528.midi_note == 72);
    REQUIRE_THAT(info_528.nominal_frequency_hz, WithinAbs(523.25f, 0.1f));
    REQUIRE_THAT(info_528.equivalent_a4_hz, WithinAbs(444.01f, 0.05f));
    REQUIRE_THAT(info_528.cents_offset, WithinAbs(15.67f, 0.1f));

    // 3. 417.00 Hz: Note is G#4 (MIDI 68), equivalent A4 = 417 * 2^(1/12) ≈ 441.78 Hz
    fino::NoteInfo info_417 = fino::PitchMath::find_nearest_note(417.00f, 440.00f);
    REQUIRE(info_417.note_name == "G#4");
    REQUIRE(info_417.midi_note == 68);
    REQUIRE_THAT(info_417.equivalent_a4_hz, WithinAbs(441.80f, 0.05f));
    REQUIRE_THAT(info_417.cents_offset, WithinAbs(7.07f, 0.1f));

    // 4. Invalid zero or negative frequencies
    fino::NoteInfo info_zero = fino::PitchMath::find_nearest_note(0.0f, 440.0f);
    REQUIRE(info_zero.midi_note == 69);
    REQUIRE(info_zero.ratio == 1.0f);
}

TEST_CASE("PitchMath frequency mapping retuning", "[pitch_math]") {
    // 440 Hz -> 432 Hz retuning
    float ratio_432 = fino::PitchMath::frequency_to_ratio(440.0f, 432.0f);
    REQUIRE_THAT(ratio_432, WithinAbs(432.0f / 440.0f, 1e-4f));

    // 440 Hz -> 528 Hz retuning: maps to C5 -> equivalent A4 ≈ 444.01 Hz
    float ratio_528 = fino::PitchMath::frequency_to_ratio(440.0f, 528.0f);
    REQUIRE_THAT(ratio_528, WithinAbs(444.01f / 440.0f, 1e-3f));

    // Direct ratio testing
    float direct_double = fino::PitchMath::direct_ratio(440.0f, 880.0f);
    REQUIRE_THAT(direct_double, WithinAbs(2.0f, 1e-5f));

    // Invalid frequency handling
    REQUIRE(fino::PitchMath::frequency_to_ratio(0.0f, 432.0f) == 1.0f);
    REQUIRE(fino::PitchMath::frequency_to_ratio(440.0f, -10.0f) == 1.0f);
}

TEST_CASE("PitchMath clamping boundaries", "[pitch_math]") {
    // Extreme semitone values must be clamped to safe limits [0.25, 4.0]
    REQUIRE(fino::PitchMath::semitones_to_ratio(48.0f) == 4.0f);
    REQUIRE(fino::PitchMath::semitones_to_ratio(-48.0f) == 0.25f);
}

TEST_CASE("PitchMath dB and linear amplitude conversions", "[pitch_math]") {
    REQUIRE_THAT(fino::PitchMath::db_to_linear(0.0f), WithinAbs(1.0f, 1e-4f));
    REQUIRE_THAT(fino::PitchMath::db_to_linear(-6.0206f), WithinAbs(0.5f, 1e-3f));
    REQUIRE_THAT(fino::PitchMath::linear_to_db(1.0f), WithinAbs(0.0f, 1e-4f));
    REQUIRE_THAT(fino::PitchMath::linear_to_db(0.5f), WithinAbs(-6.0206f, 1e-3f));
    REQUIRE(fino::PitchMath::linear_to_db(0.0f) == -100.0f);
}
