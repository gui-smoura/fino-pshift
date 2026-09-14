#include "fino/ui.hpp"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace fino {

static const uint32_t BUFFER_SIZES[] = { 128, 256, 512, 1024 };
static const char* BUFFER_SIZE_LABELS[] = {
    "128 frames (~2.7 ms)",
    "256 frames (~5.3 ms)",
    "512 frames (~10.7 ms)",
    "1024 frames (~21.3 ms)"
};

UI::UI(AudioEngine& engine, AppConfig& config, const std::string& config_path)
    : m_engine(engine)
    , m_config(config)
    , m_config_path(config_path)
{
    // Match buffer frames index
    for (int i = 0; i < 4; ++i) {
        if (BUFFER_SIZES[i] == m_config.buffer_frames) {
            m_selected_buffer_idx = i;
            break;
        }
    }

    // Match saved capture device name
    const auto& cap_devs = m_engine.capture_devices();
    for (size_t i = 0; i < cap_devs.size(); ++i) {
        if (!m_config.capture_device_name.empty() && cap_devs[i].name == m_config.capture_device_name) {
            m_selected_capture_idx = static_cast<int>(i);
            break;
        }
        if (cap_devs[i].is_default && m_selected_capture_idx == -1) {
            m_selected_capture_idx = static_cast<int>(i);
        }
    }

    // Match saved playback device name
    const auto& play_devs = m_engine.playback_devices();
    for (size_t i = 0; i < play_devs.size(); ++i) {
        if (!m_config.playback_device_name.empty() && play_devs[i].name == m_config.playback_device_name) {
            m_selected_playback_idx = static_cast<int>(i);
            break;
        }
        if (play_devs[i].is_default && m_selected_playback_idx == -1) {
            m_selected_playback_idx = static_cast<int>(i);
        }
    }

    // Apply initial gain and bypass
    m_engine.processor().set_gain(PitchMath::db_to_linear(m_config.gain_db));
    m_engine.processor().set_bypass(m_config.bypass);
    apply_current_pitch();
}

void UI::setup_style() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;
    style.WindowPadding = ImVec2(14.0f, 14.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]             = ImVec4(0.09f, 0.11f, 0.14f, 1.00f);
    colors[ImGuiCol_Header]               = ImVec4(0.18f, 0.22f, 0.29f, 0.80f);
    colors[ImGuiCol_HeaderHovered]        = ImVec4(0.24f, 0.32f, 0.44f, 0.90f);
    colors[ImGuiCol_HeaderActive]         = ImVec4(0.14f, 0.48f, 0.76f, 1.00f);
    colors[ImGuiCol_Button]               = ImVec4(0.16f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.32f, 0.46f, 1.00f);
    colors[ImGuiCol_ButtonActive]         = ImVec4(0.12f, 0.46f, 0.74f, 1.00f);
    colors[ImGuiCol_FrameBg]              = ImVec4(0.13f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.17f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgActive]        = ImVec4(0.20f, 0.28f, 0.38f, 1.00f);
    colors[ImGuiCol_SliderGrab]           = ImVec4(0.20f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]     = ImVec4(0.35f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Tab]                  = ImVec4(0.13f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered]           = ImVec4(0.25f, 0.35f, 0.50f, 1.00f);
    colors[ImGuiCol_TabActive]            = ImVec4(0.16f, 0.44f, 0.72f, 1.00f);
    colors[ImGuiCol_TitleBgActive]        = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);
    colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.24f, 0.32f, 0.60f);
    colors[ImGuiCol_PlotHistogram]        = ImVec4(0.18f, 0.78f, 0.55f, 1.00f);
}

void UI::apply_current_pitch() {
    float ratio = 1.0f;
    if (m_config.mode == 0) {
        ratio = PitchMath::semitones_to_ratio(m_config.semitones, m_config.cents);
    } else {
        ratio = PitchMath::frequency_to_ratio(m_config.source_hz, m_config.target_hz);
    }
    m_engine.processor().set_pitch_ratio(ratio);
}

void UI::sync_config_from_ui() {
    const auto& cap_devs = m_engine.capture_devices();
    if (m_selected_capture_idx >= 0 && m_selected_capture_idx < static_cast<int>(cap_devs.size())) {
        m_config.capture_device_name = cap_devs[m_selected_capture_idx].name;
    }
    const auto& play_devs = m_engine.playback_devices();
    if (m_selected_playback_idx >= 0 && m_selected_playback_idx < static_cast<int>(play_devs.size())) {
        m_config.playback_device_name = play_devs[m_selected_playback_idx].name;
    }
    m_config.buffer_frames = BUFFER_SIZES[m_selected_buffer_idx];
    ConfigManager::save_to_file(m_config_path, m_config);
}

void UI::render_header() {
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), "FINO P-SHIFT");
    ImGui::SameLine();
    ImGui::TextDisabled("v1.0.0 (C++20 Real-Time DSP)");

    ImGui::SameLine(ImGui::GetWindowWidth() - 170.0f);
    if (m_engine.is_running()) {
        ImGui::TextColored(ImVec4(0.25f, 0.88f, 0.45f, 1.00f), "[ RUNNING ]");
        ImGui::SameLine();
        const float latency_ms = (static_cast<float>(m_engine.active_buffer_frames()) /
                                  static_cast<float>(m_engine.active_sample_rate())) * 1000.0f;
        ImGui::TextDisabled("%.1f ms", latency_ms);
    } else {
        ImGui::TextColored(ImVec4(0.90f, 0.45f, 0.25f, 1.00f), "[ STOPPED ]");
    }
    ImGui::EndGroup();
    ImGui::Separator();
}

void UI::render_device_section() {
    if (ImGui::CollapsingHeader("Dispositivos de Audio & I/O", ImGuiTreeNodeFlags_DefaultOpen)) {
        const auto& cap_devs = m_engine.capture_devices();
        const auto& play_devs = m_engine.playback_devices();

        // 1. Capture Device (Input e.g. VB-Audio Virtual Cable)
        const char* current_cap_label = (m_selected_capture_idx >= 0 && m_selected_capture_idx < static_cast<int>(cap_devs.size()))
            ? cap_devs[m_selected_capture_idx].name.c_str()
            : "Padrao do Sistema";

        if (ImGui::BeginCombo("Entrada (Cabo Virtual / Mic)", current_cap_label)) {
            for (size_t i = 0; i < cap_devs.size(); ++i) {
                const bool is_selected = (m_selected_capture_idx == static_cast<int>(i));
                char label[256];
                std::snprintf(label, sizeof(label), "%s%s", cap_devs[i].name.c_str(), cap_devs[i].is_default ? " [Padrao]" : "");
                if (ImGui::Selectable(label, is_selected)) {
                    m_selected_capture_idx = static_cast<int>(i);
                    sync_config_from_ui();
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // 2. Playback Device (Output e.g. Headphones)
        const char* current_play_label = (m_selected_playback_idx >= 0 && m_selected_playback_idx < static_cast<int>(play_devs.size()))
            ? play_devs[m_selected_playback_idx].name.c_str()
            : "Padrao do Sistema";

        if (ImGui::BeginCombo("Saida (Fone de Ouvido)", current_play_label)) {
            for (size_t i = 0; i < play_devs.size(); ++i) {
                const bool is_selected = (m_selected_playback_idx == static_cast<int>(i));
                char label[256];
                std::snprintf(label, sizeof(label), "%s%s", play_devs[i].name.c_str(), play_devs[i].is_default ? " [Padrao]" : "");
                if (ImGui::Selectable(label, is_selected)) {
                    m_selected_playback_idx = static_cast<int>(i);
                    sync_config_from_ui();
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        // Buffer size selector
        if (ImGui::Combo("Tamanho do Buffer", &m_selected_buffer_idx, BUFFER_SIZE_LABELS, 4)) {
            sync_config_from_ui();
            if (m_engine.is_running()) {
                m_engine.start(m_selected_capture_idx, m_selected_playback_idx, BUFFER_SIZES[m_selected_buffer_idx]);
            }
        }

        // Control Buttons
        if (ImGui::Button("Atualizar Lista")) {
            m_engine.refresh_devices();
        }

        ImGui::SameLine();
        if (m_engine.is_running()) {
            if (ImGui::Button("PARAR MOTOR DE AUDIO", ImVec2(200, 0))) {
                m_engine.stop();
            }
        } else {
            if (ImGui::Button("INICIAR MOTOR DE AUDIO", ImVec2(200, 0))) {
                m_engine.start(m_selected_capture_idx, m_selected_playback_idx, BUFFER_SIZES[m_selected_buffer_idx]);
            }
        }
    }
}

void UI::render_pitch_controls() {
    if (ImGui::CollapsingHeader("Controle de Pitch & Tom", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::BeginTabBar("PitchTabs")) {
            // Tab 1: Musical Semitones & Cents
            if (ImGui::BeginTabItem("Modo Musical (Semitons)")) {
                m_config.mode = 0;
                bool changed = false;

                changed |= ImGui::SliderFloat("Semitons", &m_config.semitones, -12.0f, 12.0f, "%.2f st");
                changed |= ImGui::SliderFloat("Cents (Ajuste Fino)", &m_config.cents, -100.0f, 100.0f, "%.1f cents");

                // Quick Semitone Buttons
                ImGui::Text("Atalhos Rapidos:");
                const float quick_semitones[] = { -12.0f, -3.5f, -2.0f, 0.0f, 2.0f, 3.5f, 12.0f };
                const char* quick_labels[] = { "-12 st", "-3.5 st", "-2 st", "Reset (0)", "+2 st", "+3.5 st", "+12 st" };
                for (int i = 0; i < 7; ++i) {
                    if (i > 0) ImGui::SameLine();
                    if (ImGui::Button(quick_labels[i])) {
                        m_config.semitones = quick_semitones[i];
                        m_config.cents = 0.0f;
                        changed = true;
                    }
                }

                if (changed) {
                    apply_current_pitch();
                    sync_config_from_ui();
                }

                const float ratio = PitchMath::semitones_to_ratio(m_config.semitones, m_config.cents);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "Fator Multiplicador: %.4fx  (Tom original: %.2f st)", ratio, m_config.semitones + (m_config.cents / 100.0f));

                ImGui::EndTabItem();
            }

            // Tab 2: Frequency Mapping (Hz -> Hz)
            if (ImGui::BeginTabItem("Modo Frequencia (Hz)")) {
                m_config.mode = 1;
                bool changed = false;

                changed |= ImGui::DragFloat("Frequencia Origem A4 (Hz)", &m_config.source_hz, 0.05f, 20.0f, 20000.0f, "%.2f Hz");
                changed |= ImGui::DragFloat("Frequencia Destino (Hz)", &m_config.target_hz, 0.05f, 20.0f, 20000.0f, "%.2f Hz");

                if (ImGui::Button("Resetar Frequencias (440.00 Hz)")) {
                    m_config.source_hz = 440.0f;
                    m_config.target_hz = 440.0f;
                    changed = true;
                }

                if (changed) {
                    apply_current_pitch();
                    sync_config_from_ui();
                }

                const NoteInfo note = PitchMath::find_nearest_note(m_config.target_hz, m_config.source_hz);
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.00f, 1.00f), "Nota Detectada: %s (%s)", note.note_name.c_str(), note.note_name_pt.c_str());
                ImGui::TextDisabled("Frequencia nominal padrao: %.2f Hz", note.nominal_frequency_hz);
                ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.60f, 1.00f), "Afinacao A4 Equivalente: %.2f Hz", note.equivalent_a4_hz);
                ImGui::Text("Desvio de Afinacao: %+.2f cents (%+.2f semitons)", note.cents_offset, note.semitones_offset);
                ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "Fator Multiplicador: %.4fx", note.ratio);

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        // Global Processing Controls (Gain & Bypass)
        ImGui::Spacing();
        bool bypass = m_config.bypass;
        if (ImGui::Checkbox("Bypass (Sem Pitch Shift)", &bypass)) {
            m_config.bypass = bypass;
            m_engine.processor().set_bypass(bypass);
            sync_config_from_ui();
        }

        ImGui::SameLine(250);
        if (ImGui::SliderFloat("Ganho Saida (dB)", &m_config.gain_db, -24.0f, 12.0f, "%.1f dB")) {
            m_engine.processor().set_gain(PitchMath::db_to_linear(m_config.gain_db));
            sync_config_from_ui();
        }
        ImGui::SameLine();
        if (ImGui::Button("0 dB")) {
            m_config.gain_db = 0.0f;
            m_engine.processor().set_gain(1.0f);
            sync_config_from_ui();
        }
    }
}

void UI::render_metering_and_latency() {
    if (ImGui::CollapsingHeader("Monitoramento em Tempo Real & VU Meters", ImGuiTreeNodeFlags_DefaultOpen)) {
        float in_l = 0.0f, in_r = 0.0f;
        float out_l = 0.0f, out_r = 0.0f;
        m_engine.processor().get_input_peaks(in_l, in_r);
        m_engine.processor().get_output_peaks(out_l, out_r);

        // Smooth VU meter falloff
        const float decay = 0.85f;
        m_in_peak_l_smooth = std::max(in_l, m_in_peak_l_smooth * decay);
        m_in_peak_r_smooth = std::max(in_r, m_in_peak_r_smooth * decay);
        m_out_peak_l_smooth = std::max(out_l, m_out_peak_l_smooth * decay);
        m_out_peak_r_smooth = std::max(out_r, m_out_peak_r_smooth * decay);

        ImGui::Text("Entrada (L / R):");
        char overlay_in_l[32], overlay_in_r[32];
        std::snprintf(overlay_in_l, sizeof(overlay_in_l), "%.1f dB", PitchMath::linear_to_db(m_in_peak_l_smooth));
        std::snprintf(overlay_in_r, sizeof(overlay_in_r), "%.1f dB", PitchMath::linear_to_db(m_in_peak_r_smooth));

        ImGui::ProgressBar(std::clamp(m_in_peak_l_smooth, 0.0f, 1.0f), ImVec2(180, 14), overlay_in_l);
        ImGui::SameLine();
        ImGui::ProgressBar(std::clamp(m_in_peak_r_smooth, 0.0f, 1.0f), ImVec2(180, 14), overlay_in_r);

        ImGui::Text("Saida (L / R):");
        char overlay_out_l[32], overlay_out_r[32];
        std::snprintf(overlay_out_l, sizeof(overlay_out_l), "%.1f dB", PitchMath::linear_to_db(m_out_peak_l_smooth));
        std::snprintf(overlay_out_r, sizeof(overlay_out_r), "%.1f dB", PitchMath::linear_to_db(m_out_peak_r_smooth));

        ImGui::ProgressBar(std::clamp(m_out_peak_l_smooth, 0.0f, 1.0f), ImVec2(180, 14), overlay_out_l);
        ImGui::SameLine();
        ImGui::ProgressBar(std::clamp(m_out_peak_r_smooth, 0.0f, 1.0f), ImVec2(180, 14), overlay_out_r);

        // Buffer fill ratio diagnostics
        const float fill = m_engine.buffer_fill_ratio();
        char overlay_buf[32];
        std::snprintf(overlay_buf, sizeof(overlay_buf), "Headroom: %.1f%%", fill * 100.0f);
        ImGui::Text("Buffer Ring SPSC:");
        ImGui::ProgressBar(fill, ImVec2(368, 12), overlay_buf);
    }
}

void UI::render_presets_section() {
    if (ImGui::CollapsingHeader("Gerenciamento de Presets")) {
        if (!m_config.presets.empty()) {
            std::vector<const char*> preset_names;
            for (const auto& p : m_config.presets) {
                preset_names.push_back(p.name.c_str());
            }

            if (m_selected_preset_idx >= static_cast<int>(preset_names.size())) {
                m_selected_preset_idx = 0;
            }

            ImGui::Combo("Presets Salvos", &m_selected_preset_idx, preset_names.data(), static_cast<int>(preset_names.size()));

            if (ImGui::Button("Carregar Preset")) {
                const auto& p = m_config.presets[m_selected_preset_idx];
                m_config.mode = p.mode;
                m_config.semitones = p.semitones;
                m_config.cents = p.cents;
                m_config.source_hz = p.source_hz;
                m_config.target_hz = p.target_hz;
                apply_current_pitch();
                sync_config_from_ui();
            }

            ImGui::SameLine();
            if (ImGui::Button("Excluir Preset") && m_config.presets.size() > 1) {
                m_config.presets.erase(m_config.presets.begin() + m_selected_preset_idx);
                if (m_selected_preset_idx >= static_cast<int>(m_config.presets.size())) {
                    m_selected_preset_idx = static_cast<int>(m_config.presets.size()) - 1;
                }
                sync_config_from_ui();
            }
        }

        ImGui::Spacing();
        ImGui::InputText("Nome do Novo Preset", m_new_preset_name, sizeof(m_new_preset_name));
        ImGui::SameLine();
        if (ImGui::Button("Salvar Atual Como Preset")) {
            if (m_new_preset_name[0] != '\0') {
                Preset new_preset;
                new_preset.name = m_new_preset_name;
                new_preset.mode = m_config.mode;
                new_preset.semitones = m_config.semitones;
                new_preset.cents = m_config.cents;
                new_preset.source_hz = m_config.source_hz;
                new_preset.target_hz = m_config.target_hz;

                m_config.presets.push_back(new_preset);
                m_selected_preset_idx = static_cast<int>(m_config.presets.size()) - 1;
                m_new_preset_name[0] = '\0';
                sync_config_from_ui();
            }
        }
    }
}

void UI::render() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    if (ImGui::Begin("FinoPShiftMain", nullptr, flags)) {
        render_header();
        render_device_section();
        render_pitch_controls();
        render_metering_and_latency();
        render_presets_section();
    }
    ImGui::End();
}

} // namespace fino
