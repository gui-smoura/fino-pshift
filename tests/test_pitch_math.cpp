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

TEST_CASE("PitchMath frequency mapping", "[pitch_math]") {
    // 440 Hz -> 432 Hz retuning
    float ratio_432 = fino::PitchMath::frequency_to_ratio(440.0f, 432.0f);
    REQUIRE_THAT(ratio_432, WithinAbs(432.0f / 440.0f, 1e-5f));

    // Octave double: 440 Hz -> 880 Hz
    float ratio_880 = fino::PitchMath::frequency_to_ratio(440.0f, 880.0f);
    REQUIRE_THAT(ratio_880, WithinAbs(2.0f, 1e-5f));

    // Invalid zero or negative frequency handling
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
