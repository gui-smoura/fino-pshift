#include <catch2/catch_test_macros.hpp>
#include "fino/pitch_processor.hpp"
#include <cmath>
#include <vector>
#include <numbers>

TEST_CASE("PitchProcessor basic configuration and state", "[dsp]") {
    fino::PitchProcessor processor;
    processor.configure(2, 48000.0f, 1024);

    REQUIRE(processor.channels() == 2);
    REQUIRE(processor.sample_rate() == 48000.0f);
    REQUIRE(processor.current_pitch_ratio() == 1.0f);

    float in_l = 0.0f, in_r = 0.0f;
    processor.get_input_peaks(in_l, in_r);
    REQUIRE(in_l == 0.0f);
    REQUIRE(in_r == 0.0f);
}

TEST_CASE("PitchProcessor sine wave headless processing", "[dsp]") {
    fino::PitchProcessor processor;
    const float sample_rate = 48000.0f;
    const size_t block_size = 512;
    processor.configure(2, sample_rate, block_size);

    // Generate 440 Hz stereo sine wave (interleaved)
    const size_t num_blocks = 10;
    std::vector<float> input(block_size * 2);
    std::vector<float> output(block_size * 2, 0.0f);

    for (size_t i = 0; i < block_size; ++i) {
        const float t = static_cast<float>(i) / sample_rate;
        const float sample = std::sin(2.0f * std::numbers::pi_v<float> * 440.0f * t) * 0.8f;
        input[i * 2] = sample;     // Left
        input[i * 2 + 1] = sample; // Right
    }

    // Shift pitch by +7 semitones (perfect fifth)
    processor.set_pitch_ratio(fino::PitchMath::semitones_to_ratio(7.0f));

    for (size_t b = 0; b < num_blocks; ++b) {
        processor.process(input.data(), output.data(), block_size);

        // Verify output integrity: no NaN, no Inf, bounded within [-1.5, 1.5]
        for (size_t i = 0; i < block_size * 2; ++i) {
            REQUIRE_FALSE(std::isnan(output[i]));
            REQUIRE_FALSE(std::isinf(output[i]));
            REQUIRE(output[i] >= -1.5f);
            REQUIRE(output[i] <= 1.5f);
        }
    }

    // Verify peak meters were populated
    float in_l = 0.0f, in_r = 0.0f;
    float out_l = 0.0f, out_r = 0.0f;
    processor.get_input_peaks(in_l, in_r);
    processor.get_output_peaks(out_l, out_r);

    REQUIRE(in_l > 0.5f);
    REQUIRE(in_r > 0.5f);
    REQUIRE(out_l > 0.0f);
    REQUIRE(out_r > 0.0f);
}

TEST_CASE("PitchProcessor bypass mode", "[dsp]") {
    fino::PitchProcessor processor;
    processor.configure(2, 48000.0f, 256);
    processor.set_bypass(true);

    std::vector<float> input(512);
    for (size_t i = 0; i < 512; ++i) {
        input[i] = static_cast<float>(i) / 512.0f;
    }

    std::vector<float> output(512, 0.0f);
    processor.process(input.data(), output.data(), 256);

    for (size_t i = 0; i < 512; ++i) {
        REQUIRE(output[i] == input[i]);
    }
}
