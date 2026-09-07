# fino-pshift

Processador de áudio em tempo real com **Pitch Shifting** de alta fidelidade e baixa latência desenvolvido em **C++20**, interface gráfica **Dear ImGui**, motor DSP **Signalsmith Stretch** e backend de áudio multiplataforma **miniaudio**.

---

## Recursos Principais

- **Motor DSP Signalsmith Stretch**: Pitch shifting polifônico com preservação acústica de formantes e transientes (voz e instrumentos), sem artefatos metálicos ou granular stutter.
- **Áudio Multiplataforma (miniaudio)**: Baixa latência com WASAPI no Windows e suporte a ALSA/PulseAudio/JACK no Linux.
- **Arquitetura Lock-Free Real-Time (Strict Audio Thread Safety)**:
  - Buffer circular **SPSC** (*Single Producer Single Consumer*) com alinhamento de cacheline (`alignas(64)`).
  - Zero alocações de memória no heap (`new`, `delete`, `malloc`) dentro dos callbacks de áudio.
  - Comunicação atômica (`std::atomic<float>`) com **suavização linear** de parâmetros para eliminar *clicks* e ruídos ao movimentar sliders.
  - Proteção de hardware contra números denormais (FTZ e DAZ).
- **Interface Gráfica Moderna (Dear ImGui + GLFW + OpenGL3)**:
  - **Modo Musical**: Sliders contínuos de Semitons (-12 a +12 st) e Cents (-100 a +100 cents), além de atalhos rápidos (-12st, -2st, 0, +2st, +12st).
  - **Modo Frequência**: Mapeamento direto de Hz para Hz com presets imediatos (ex: 440 Hz $\rightarrow$ 432 Hz Verdi, 440 Hz $\rightarrow$ 528 Hz Solfeggio).
  - **VU Meters em Tempo Real**: Medidores de pico estéreo para entrada e saída com indicação em dB e decaimento suave.
  - **Diagnóstico de Buffer**: Visualização de taxa de preenchimento do ring buffer e latência estimada em milissegundos.
  - **Persistência de Sessão e Presets**: Salva automaticamente os dispositivos de entrada/saída, latência preferida e presets customizados em `config.json`.

---

## Como Usar no Windows (com VB-Audio Cable)

Para processar o som do computador em tempo real e redirecioná-lo para o seu fone:

1. Instale o driver gratuito **VB-Audio Virtual Cable** (ou VoiceMeeter).
2. Defina a saída de som do Windows (ou do aplicativo específico, como navegador/reprodutor) para **CABLE Input (VB-Audio Virtual Cable)**.
3. Abra o **fino-pshift**:
   - No dropdown **Entrada**, selecione **CABLE Output (VB-Audio Virtual Cable)**.
   - No dropdown **Saída**, selecione o seu **Fone de Ouvido** (ou caixas de som).
   - Clique em **INICIAR MOTOR DE AUDIO**.
4. Ajuste o pitch desejado na aba de semitons ou escolha o preset de retonificação (ex: `440 -> 432 Hz`).
5. O áudio do seu computador será alterado em tom e reproduzido instantaneamente no fone com latência imperceptível.

---

## Compilação e Testes

### Pré-requisitos
- Compilador compatível com C++20 (GCC 12+, Clang 15+ ou MSVC 2022 v19.30+).
- [CMake](https://cmake.org/) (versão 3.20 ou superior).
- [Ninja](https://ninja-build.org/) (opcional, mas recomendado para builds rápidos).

### Compilar

```bash
cmake -B build -G Ninja
cmake --build build --config Release
```

O executável final estará em:
- `build/fino-pshift.exe` (Windows) ou `build/fino-pshift` (Linux)

### Executar a Suíte de Testes Unitários (Catch2 v3)

```bash
ctest --test-dir build --output-on-failure
```

---

## Estrutura do Código

```
fino-pshift/
├── CMakeLists.txt              # Script de build com FetchContent para todas as dependências
├── GEMINI.md                   # Diretrizes arquiteturais de C++20 e DSP em tempo real
├── README.md                   # Documentação do projeto
├── include/fino/
│   ├── audio_engine.hpp        # Abstração de captura e reprodução via miniaudio
│   ├── config_manager.hpp      # Serialização e persistência de presets (nlohmann::json)
│   ├── pitch_math.hpp          # Funções matemáticas puras e conversões semitons <-> Hz
│   ├── pitch_processor.hpp     # Wrapper do Signalsmith Stretch, smoothing e VU meters
│   ├── spsc_ring_buffer.hpp    # Buffer circular lock-free SPSC para áudio real-time
│   └── ui.hpp                  # Definição dos layouts e componentes Dear ImGui
├── src/
│   ├── audio_engine.cpp        # Callbacks de áudio WASAPI/miniaudio e sincronização
│   ├── config_manager.cpp      # Implementação de leitura/escrita do config.json
│   ├── main.cpp                # Ponto de entrada, loop GLFW/OpenGL3 e ciclo de vida
│   ├── pitch_processor.cpp     # Processamento DSP estéreo e proteção FTZ/DAZ
│   └── ui.cpp                  # Renderização da interface gráfica moderna
└── tests/
    ├── CMakeLists.txt          # Configuração do executável fino_tests via Catch2
    ├── test_dsp_processor.cpp  # Testes headless com ondas senoidais e bypass
    ├── test_pitch_math.cpp     # Testes matemáticos de semitons, cents, Hz e decibéis
    └── test_spsc_ring_buffer.cpp # Testes multithread e verificação de limites lock-free
```

---

## Licença

Código aberto sob a licença MIT.
