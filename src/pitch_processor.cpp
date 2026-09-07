#include "fino/pitch_processor.hpp"

#include <cmath>
#include <algorithm>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#include <xmmintrin.h>
#include <pmmintrin.h>
#endif

namespace fino {

PitchProcessor::PitchProcessor() {
    // Initial pre-allocation of scratch buffers
    configure(2, 48000.0f, 2048);
}

void PitchProcessor::enable_denormal_flush() noexcept {
#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
    // Enable Flush-To-Zero (FTZ) and Denormals-Are-Zero (DAZ)
    _mm_setcsr(_mm_getcsr() | 0x8040);
#endif
}

void PitchProcessor::configure(int channels, float sample_rate, size_t max_block_frames) {
    m_channels = (channels >= 2) ? 2 : 1;
    m_sample_rate = (sample_rate > 8000.0f) ? sample_rate : 48000.0f;
    m_max_block_frames = std::max<size_t>(max_block_frames, 2048);

    // Signalsmith Stretch configuration for balanced latency and acoustic quality
    // At 48kHz, block size ~ 2048 to 3072 gives clean pitch without high latency
    const int block_samples = std::max(1024, static_cast<int>(m_sample_rate * 0.06f)); // ~60ms window
    const int interval_samples = std::max(256, block_samples / 4);

    m_stretch.configure(m_channels, block_samples, interval_samples);
    m_stretch.setTransposeFactor(m_current_ratio);

    // Pre-allocate scratch vectors up-front to prevent any dynamic heap allocation in audio thread
    const size_t scratch_size = std::max<size_t>(m_max_block_frames * 2, 8192);
    m_in_left.assign(scratch_size, 0.0f);
    m_in_right.assign(scratch_size, 0.0f);
    m_out_left.assign(scratch_size, 0.0f);
    m_out_right.assign(scratch_size, 0.0f);

    reset();
}

void PitchProcessor::reset() {
    m_stretch.reset();
    m_current_ratio = m_target_ratio.load(std::memory_order_relaxed);
    m_stretch.setTransposeFactor(m_current_ratio);

    m_in_peak_l.store(0.0f, std::memory_order_relaxed);
    m_in_peak_r.store(0.0f, std::memory_order_relaxed);
    m_out_peak_l.store(0.0f, std::memory_order_relaxed);
    m_out_peak_r.store(0.0f, std::memory_order_relaxed);
}

void PitchProcessor::set_pitch_ratio(float ratio) noexcept {
    m_target_ratio.store(PitchMath::clamp_ratio(ratio), std::memory_order_release);
}

void PitchProcessor::set_bypass(bool bypass) noexcept {
    m_bypass.store(bypass, std::memory_order_release);
}

void PitchProcessor::set_gain(float gain) noexcept {
    const float safe_gain = std::clamp(gain, 0.0f, 4.0f);
    m_gain.store(safe_gain, std::memory_order_release);
}

void PitchProcessor::get_input_peaks(float& left, float& right) const noexcept {
    left = m_in_peak_l.load(std::memory_order_relaxed);
    right = m_in_peak_r.load(std::memory_order_relaxed);
}

void PitchProcessor::get_output_peaks(float& left, float& right) const noexcept {
    left = m_out_peak_l.load(std::memory_order_relaxed);
    right = m_out_peak_r.load(std::memory_order_relaxed);
}

void PitchProcessor::process(const float* interleaved_input, float* interleaved_output, size_t frames) noexcept {
    enable_denormal_flush();

    if (interleaved_input == nullptr || interleaved_output == nullptr || frames == 0) {
        return;
    }

    // Safety guard against oversized blocks
    const size_t process_frames = std::min(frames, m_in_left.size());
    const bool is_stereo = (m_channels == 2);
    const float gain = m_gain.load(std::memory_order_relaxed);
    const bool bypass = m_bypass.load(std::memory_order_relaxed);

    float peak_in_l = 0.0f;
    float peak_in_r = 0.0f;

    // 1. Deinterleave and calculate input peaks
    if (is_stereo) {
        for (size_t i = 0; i < process_frames; ++i) {
            const float l = interleaved_input[i * 2];
            const float r = interleaved_input[i * 2 + 1];
            m_in_left[i] = l;
            m_in_right[i] = r;

            const float abs_l = std::abs(l);
            const float abs_r = std::abs(r);
            if (abs_l > peak_in_l) peak_in_l = abs_l;
            if (abs_r > peak_in_r) peak_in_r = abs_r;
        }
    } else {
        for (size_t i = 0; i < process_frames; ++i) {
            const float val = interleaved_input[i];
            m_in_left[i] = val;
            const float abs_val = std::abs(val);
            if (abs_val > peak_in_l) peak_in_l = abs_val;
        }
        peak_in_r = peak_in_l;
    }

    m_in_peak_l.store(peak_in_l, std::memory_order_relaxed);
    m_in_peak_r.store(peak_in_r, std::memory_order_relaxed);

    // 2. Bypass processing path
    if (bypass) {
        float peak_out_l = 0.0f;
        float peak_out_r = 0.0f;

        if (is_stereo) {
            for (size_t i = 0; i < process_frames; ++i) {
                const float l = m_in_left[i] * gain;
                const float r = m_in_right[i] * gain;
                interleaved_output[i * 2] = l;
                interleaved_output[i * 2 + 1] = r;

                const float abs_l = std::abs(l);
                const float abs_r = std::abs(r);
                if (abs_l > peak_out_l) peak_out_l = abs_l;
                if (abs_r > peak_out_r) peak_out_r = abs_r;
            }
        } else {
            for (size_t i = 0; i < process_frames; ++i) {
                const float val = m_in_left[i] * gain;
                interleaved_output[i] = val;
                const float abs_val = std::abs(val);
                if (abs_val > peak_out_l) peak_out_l = abs_val;
            }
            peak_out_r = peak_out_l;
        }

        m_out_peak_l.store(peak_out_l, std::memory_order_relaxed);
        m_out_peak_r.store(peak_out_r, std::memory_order_relaxed);
        return;
    }

    // 3. Smooth parameter interpolation towards target pitch ratio
    const float target_ratio = m_target_ratio.load(std::memory_order_relaxed);
    if (std::abs(m_current_ratio - target_ratio) > 0.0001f) {
        m_current_ratio += (target_ratio - m_current_ratio) * m_smoothing_factor;
        m_stretch.setTransposeFactor(m_current_ratio);
    }

    // 4. Run Signalsmith Stretch DSP
    float* inputs[2] = { m_in_left.data(), m_in_right.data() };
    float* outputs[2] = { m_out_left.data(), m_out_right.data() };

    m_stretch.process(inputs, static_cast<int>(process_frames), outputs, static_cast<int>(process_frames));

    // 5. Apply gain and calculate output peaks, then interleave back
    float peak_out_l = 0.0f;
    float peak_out_r = 0.0f;

    if (is_stereo) {
        for (size_t i = 0; i < process_frames; ++i) {
            const float l = m_out_left[i] * gain;
            const float r = m_out_right[i] * gain;
            interleaved_output[i * 2] = l;
            interleaved_output[i * 2 + 1] = r;

            const float abs_l = std::abs(l);
            const float abs_r = std::abs(r);
            if (abs_l > peak_out_l) peak_out_l = abs_l;
            if (abs_r > peak_out_r) peak_out_r = abs_r;
        }
    } else {
        for (size_t i = 0; i < process_frames; ++i) {
            const float val = m_out_left[i] * gain;
            interleaved_output[i] = val;
            const float abs_val = std::abs(val);
            if (abs_val > peak_out_l) peak_out_l = abs_val;
        }
        peak_out_r = peak_out_l;
    }

    m_out_peak_l.store(peak_out_l, std::memory_order_relaxed);
    m_out_peak_r.store(peak_out_r, std::memory_order_relaxed);
}

} // namespace fino
