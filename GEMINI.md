# Diretrizes do Projeto C++ (Processamento de Áudio)

## 1. Padrão, Compilação e Ferramental
- Padrão: C++20.
- Build System: CMake.
- Gerenciamento de dependências: `FetchContent` via CMake (preferencial) ou `vcpkg`. Não baixe binários ou headers manuais sem manifesto.
- Avisos e Rigor: Tratar warnings como erros (`-Werror` no GCC/Clang ou `/WX` no MSVC). O `CMakeLists.txt` deve abstrair essas flags conforme o compilador detectado.
- Estrutura de pastas:
  - `include/`: Headers públicos (.hpp).
  - `src/`: Implementações internas (.cpp).
  - `tests/`: Testes unitários e benchmarks.
  - `third_party/`: Submódulos ou dependências isoladas (se estritamente necessário).

## 2. Restrições Arquiteturais e Diretrizes de Áudio (DSP)
- Separação de Threads:
  - **Thread Principal / UI / Host:** Responsável por I/O de disco, alocações dinâmicas, carregamento de arquivos e setup.
  - **Audio Thread (Loop de Renderização / Callback):**
    - Proibida alocação ou desalocação dinâmica de memória no heap (`new`, `delete`, `malloc`, redimensionamento de containers). Toda memória e buffer deve ser pré-alocada.
    - Proibidas chamadas de sistema bloqueantes (I/O síncrono, leitura/escrita em disco, printfs ou logs).
    - Proibido uso de locks bloqueantes (`std::mutex`, `std::condition_variable`). Prefira buffers circulares lock-free (SPSC - Single Producer, Single Consumer) e variáveis atômicas (`std::atomic`).
- Precisão Numérica:
  - Padronizar processamento interno de áudio em `float` (para desempenho) ou `double` (quando exigido por estabilidade numérica de filtros), explicitando a escolha nas assinaturas.
  - Tratar ou evitar denormais (denormalized numbers/underflow) em loops de filtros recursivos (IIR).
- Paradigma de Código:
  - RAII estrito em todo gerenciamento de recursos.
  - Proibidos raw pointers para posse de memória.
  - Prefira `std::expected` ou `std::optional` para erros em tempo de setup; na thread de áudio, erros devem ser silenciosos ou reportados por flags atômicas.

## 3. Fluxo de Git e Versionamento
- Controle de Branches:
  - A branch `master` deve sempre compilar e passar em todos os testes.
  - Todo novo desenvolvimento deve ser feito em branches isoladas seguindo o padrão:
    - `feat/nome-da-feature` para novas funcionalidades.
    - `fix/descricao-do-bug` para correções.
    - `refactor/descricao` para refatorações sem alteração de comportamento.
- Commits (Conventional Commits):
  - Mensagens em formato estruturado: `feat: add biquad filter implementation`, `fix: prevent heap allocation in audio callback`.
  - Commits devem ser pequenos e atômicos. Nunca commite arquivos gerados de build (`build/`, `.ninja`, binários ou `.vscode/`).
- Higiene:
  - Manter `.gitignore` configurado adequadamente para C++, CMake e artefatos de IDE.

## 4. Protocolo de Verificação e Testes
- Framework de testes: Catch2 ou GoogleTest integrado via CMake.
- Cobertura obrigatória:
  - Testes unitários para lógica de processamento de sinal, validação de limites de buffer e estabilidade matemática.
- Regra de Conclusão:
  - Nenhuma tarefa será considerada concluída sem que a compilação e a suíte de testes passem com 100% de sucesso via terminal:
    `cmake -B build && cmake --build build --config Release && ctest --test-dir build --output-on-failure`