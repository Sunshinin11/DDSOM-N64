# Dungeons & Dragons: Shadow over Mystara — Nintendo 64 Port (Playtest)

[🇧🇷 Português (Brasil)](#-português-brasil) | [🇺🇸 English](#-english)

> **[Galeria / Gallery — Pendente / Pending]**: Capturas de tela autênticas do Ares e do console em `docs/media/` serão adicionadas após a validação visual interativa de gameplay, seleção de personagem e multiplayer.  
> *Authentic Ares and real-hardware screenshots in `docs/media/` are pending interactive capture of gameplay, character select, and multiplayer.*

---

# 🇧🇷 Português (Brasil)

Port nativo de **Dungeons & Dragons: Shadow over Mystara** (Capcom, 1996 — placa arcade CPS-2) para **Nintendo 64**, construído sobre a base do [cps2recomp](https://github.com/tmcleroy/cps2recomp) e compilado com o SDK [libdragon](https://github.com/DragonMinded/libdragon) (branch `preview`).

A lógica da CPU **Motorola 68000** do arcade é recompilada estaticamente para código C++ nativo executado no processador **MIPS VR4300** do N64 (`tools/recomp.py --native-time`), sem emulador de 68000 ou fallback de interpretador na ROM final. O vídeo utiliza o **RDP** do N64 (`port_n64/rdp_video.cpp`) com gráficos convertidos para CI4 e máscaras de prioridade por software quando necessário. A ROM padrão (**`port_n64/ddsom_native_playtest.z64`**) reproduz áudio **WAV64** (12.019 Hz mono em VADPCM/ULC) pelo mixer de 8 canais no RSP do `libdragon`, enquanto a compatibilidade atual da placa inclui **EEPROM serial de 4 KiB** com gravação em dois slots protegidos por CRC e **registradores legados** de entrada, vídeo e controle da CPS-2/CPS-B (`port_n64/board.cpp`). O driver alternativo de áudio baseado no Z80 1.15 recompilado (`NATIVE_AUDIO=1`, `port_n64/audio_native.cpp`) é mantido exclusivamente como **experimental** e não é vinculado à ROM de playtest.

> ⚠️ **Aviso Legal**: Este repositório **NÃO contém ROMs, chaves de descriptografia, gráficos extraídos, amostras sonoras ou binários derivados do jogo original**. Cada usuário deve fornecer sua própria cópia legítima do conjunto `ddsom` para gerar os ativos localmente e compilar a ROM.

---

## Índice (PT-BR)

- [Arquitetura e Compatibilidade](#-arquitetura-e-compatibilidade)
- [Requisitos e Dependências Verificadas](#-requisitos-e-dependências-verificadas)
- [Arquivos de Origem Exigidos](#-arquivos-de-origem-exigidos)
- [Preparação dos Recursos e Compilação](#-preparação-dos-recursos-e-compilação)
- [Controles](#-controles)
- [Estado dos Testes e Medições de FPS](#-estado-dos-testes-e-medições-de-fps)
- [Limitações Conhecidas e Pendências](#-limitações-conhecidas-e-pendências)
- [Galeria (`docs/media/`)](#-galeria-docsmedia)
- [Créditos e Licenças](#-créditos-e-licenças)

---

## Arquitetura e Compatibilidade

1. **Recompilação Estática 68000 → MIPS (`tools/recomp.py --native-time`)**: As 6.961 entradas registradas em [`port_n64/native_entries.txt`](port_n64/native_entries.txt) são traduzidas para unidades C++ compiladas nativamente em `-Os` para o VR4300. Contadores de ciclos do 68000 foram removidos do código gerado; o avanço do quadro é limitado por ociosidade de ponto fixo das escritas do agendador e orçamento de blocos, com despacho por tabela em hash e atalho de espera em vblank.
2. **Renderização RDP (`port_n64/rdp_video.cpp`)**: Os tiles CPS-2 são convertidos offline por [`port_n64/ci4.cpp`](port_n64/ci4.cpp) para CI4 com mapa de meios-blocos vazios (`empty.bin`), mantidos no cartucho e carregados em cache associativo de 4 vias na RDRAM via DMA. Prioridades irregulares entre sprites e planos utilizam máscaras compactadas de posse por pixel.
3. **Áudio Padrão WAV64 (`port_n64/audio.cpp`)**: Ferramenta offline (`port_n64/gen_sound_bank.cpp`) executa os 1.168 códigos sonoros (`0x0000`–`0x048F`) no programa Z80 v1.15 e chip QSound HLE de referência e converte cada áudio único com `audioconv64` para `sXXXX.wav64` (12.019 Hz mono: compressão **1 = VADPCM** para efeitos e **2 = ULC** para músicas em loop) indexados em `snd/table.bin`. Em tempo de execução, o N64 reproduz até 8 canais pelo mixer RSP do `libdragon` com cache LRU de 24 arquivos.
4. **Áudio Alternativo (Experimental, `NATIVE_AUDIO=1`)**: `port_n64/audio_native.cpp` e `port_n64/prepare_test_assets.py` preparam e testam a execução direta do driver Z80 v1.15 recompilado com 16 vozes PCM de 8 bits. Permanece **desativado na ROM padrão** por custo elevado de CPU/memória e ausência de efeitos de eco/DSP.
5. **Compatibilidade de Placa e EEPROM (`port_n64/board.cpp`, `port_n64/save.cpp`)**: Emula na medida necessária o mapa de registradores legados da placa (entradas, multiplicador CPS-B, registradores de objetos e fila de comandos de som `0xF18000`) e persiste configurações/recordes em **EEPROM 4K** serial com cabeçalho versionado, sequência e CRC.

---

## Requisitos e Dependências Verificadas

### Para Jogar
- **Hardware Real**: Console Nintendo 64 com **Expansion Pak (8 MiB RDRAM)** obrigatório + Flashcart compatível com EEPROM 4K (SummerCart64, EverDrive-64 X7/v2.5+, etc.).
- **Emulador**: [Ares](https://ares-emulator.github.io/) (testado na **v148** com **paraLLEl-RDP** e **Expansion Pak de 8 MiB** habilitado).

### Para Compilar (Ambiente Verificado)
- **Sistema**: Linux ou Windows 10/11 com **WSL2** (Ubuntu).
- **SDK N64**: [libdragon](https://github.com/DragonMinded/libdragon) (branch `preview`, instalado em `/opt/libdragon` ou definido em `N64_INST`), contendo:
  - `mips64-elf-gcc` **16.2.0**
  - `audioconv64` e `mkdfs` (`n64.mk`)
- **Ferramentas do Host** (versões verificadas):
  - `g++` **15.2.0** (suporte a C++20)
  - `GNU Make` **4.4.1**
  - `Python` **3.14.4** (compatível com Python 3.10+)
  - Biblioteca de desenvolvimento `zlib` (`zlib1g-dev`)

Exemplo de instalação das dependências no Ubuntu / WSL2 (além do toolchain `libdragon preview` instalado em `/opt/libdragon`):
```sh
sudo apt-get update
sudo apt-get install -y build-essential git make python3 zlib1g-dev
```

---

## Arquivos de Origem Exigidos

Posicione os **21 arquivos** do conjunto europeu de **Dungeons & Dragons: Shadow over Mystara (`ddsom`, Europe 960619)** dentro da pasta `rom/ddsom/` (conforme [`games/ddsom/game.cfg`](games/ddsom/game.cfg)):

- **Programa 68000**: `dd2e.03e`, `dd2e.04e`, `dd2e.05e`, `dd2e.06e`, `dd2e.07`, `dd2e.08`, `dd2e.09`, `dd2e.10`
- **Gráficos**: `dd2.13m`, `dd2.15m`, `dd2.17m`, `dd2.19m`, `dd2.14m`, `dd2.16m`, `dd2.18m`, `dd2.20m`
- **Programa de Som Z80**: `dd2.01`, `dd2.02`
- **Amostras QSound**: `dd2.11m`, `dd2.12m`
- **Chave CPS-2**: `ddsom.key`

*(Opcionalmente informe outro caminho via `--rom /caminho/para/ddsom` ou variável `ROM=...`.)*

---

## Preparação dos Recursos e Compilação

No WSL2 / Linux, na raiz do repositório:

```sh
# 1. Preparar todos os recursos a partir de rom/ddsom/:
#    - gera game_info.h
#    - compila e executa port_n64/dump.cpp e port_n64/ci4.cpp
#    - compila e executa port_n64/gen_sound_bank.cpp + audioconv64 (snd/table.bin + WAV64)
#    - traduz o código 68000 para work/ddsom/n64nt/ via tools/recomp.py --native-time
python3 port_n64/prepare_assets.py --n64-inst /opt/libdragon
# (ou: make -C port_n64 prepare N64_INST=/opt/libdragon)

# 2. Compilar a única ROM de saída: port_n64/ddsom_native_playtest.z64
make -C port_n64 N64_INST=/opt/libdragon -j4
```

Para executar a preparação (caso ainda não feita), toda a suíte de testes no host e a compilação da ROM em um só passo:

```sh
bash port_n64/build_native_test.sh
```

**Saída única do build:** `port_n64/ddsom_native_playtest.z64`

Para abrir a ROM no Ares (Windows PowerShell) gravando o log de telemetria em `port_n64/build/`:

```powershell
powershell -ExecutionPolicy Bypass -File port_n64\run_native_test.ps1 -ShowWindow -Seconds 120
```

---

## Controles

Mapeamento definido em [`port_n64/main.cpp`](port_n64/main.cpp):

| Controle N64 | Ação no Jogo |
| --- | --- |
| **D-Pad** / **Analógico** | Mover personagem / navegar menus |
| **Z** | Inserir ficha (*Coin*) |
| **Start** | Iniciar (*Start*) |
| **B** | Atacar (*Attack*) |
| **A** | Pular (*Jump*) |
| **C-Direita** | Selecionar (*Select*) |
| **C-Baixo** | Usar item / magia (*Use*) |

---

## Estado dos Testes e Medições de FPS

A ROM atual é classificada como **playtest**.

### Testes aprovados no host (cópia limpa, código atual)
- **Equivalência da placa (`board-test`)**: `POLL SKIPPED 249584 exact repeated passes`, `NATIVE FALLBACK instructions=0 entries=0`, **`BOARD PASS 3000 frames`** (CPU, registradores, flags, RAM, vídeo, paleta e fila de som idênticos ao interpretador de referência).
- **Equivalência sem contagem de tempo (`native-time-test`)**: **`NATIVE TIME PASS: 6000 frames identical`** (`work/ddsom/n64test` com ciclos vs `work/ddsom/n64nt` `--native-time` em 6.000 quadros).
- **Renderizador RDP (`video-test`)**: **`RDP PASS 80`** — 80 cenários em 4 modos de caminho/cache (320 verificações ao todo), com igualdade pixel a pixel frente ao renderizador CPS-2 de referência (`video/video.cpp`).
- **Sequenciador de som (`sound-test`)**: **`SOUND SEQUENCE PASS 30000 IRQ ticks, 1195 commands, 42639 identical register writes; native fallback=0`**.
- **Protocolo de áudio, PCM e EEPROM**: `AUDIO PROTOCOL PASS`, `PCM PASS` e `SAVE PASS` (compilados com `-fsanitize=address,undefined`).
- **Auditoria do binário (`verify_native_test.py`)**: cabeçalho N64 válido (`80371240`), zero símbolos de interpretador ou áudio experimental vinculados, 641 arquivos `.wav64` verificados contra todas as 912 referências ativas de `snd/table.bin`.

### Medições de desempenho identificadas (Ares)
- **ROM**: `port_n64/ddsom_native_playtest.z64` (`profile=0`, `benchmark_frames=0`, `44.564.480` bytes, SHA-256 `6c6bb4d0a6b7c7674e2e02900dbdd419ec7be66ea798b3318d87de5477406c29`)
- **Ambiente**: Ares **v148** (paraLLEl-RDP, Expansion Pak ativo), captura de 40 s no arranque / modo *attract* (1.320 quadros, `interp_total=0`, `missing=0` no áudio):
  - **Telas leves / introdução / menus** (quadros 120–300, 660–840, 1320): **58,50 – 59,87 FPS**
  - **Cenas densas do modo attract** (quadros 540–600, 960–1200, 360–440 retângulos): **25,23 – 31,35 FPS**

---

## Limitações Conhecidas e Pendências

- **Quedas abaixo de 60 FPS**: Cenas com grande quantidade de tiles e sprites sobrepostos ainda são limitadas pelo custo de desenho/composição no N64.
- **Áudio WAV64 pré-renderizado**: Usa banco pré-gravado de 8 canais a 12.019 Hz; efeitos dinâmicos de eco/DSP do QSound em tempo real não estão presentes na ROM padrão.
- **Testes pendentes**:
  - Validação interativa completa de campanha, seleção de personagens, multiplayer local e controle físico pelo usuário no Ares e em **hardware real de Nintendo 64**.
  - Cobertura de endereços do 68000 (`native_entries.txt`) cobre a inicialização e os trechos explorados até o momento; cenas avançadas ainda não visitadas podem exigir novos pontos de entrada se alcançadas.
  - Capturas de tela autênticas para `docs/media/`.

---

## Galeria (`docs/media/`)

> **Pendente**: Nenhuma captura foi incluída ainda em `docs/media/`. Assim que houver capturas reais e autorizadas (gameplay, seleção de personagem e multiplayer no Ares ou console real), elas serão exibidas nesta seção com indicação de origem.

---

## Créditos e Licenças

- **Projeto base**: [`cps2recomp`](https://github.com/tmcleroy/cps2recomp) por Tommy McLeroy — licenciado sob **GPL-3.0-only** ([`LICENSE`](LICENSE)).
- **SDK Nintendo 64**: [`libdragon`](https://github.com/DragonMinded/libdragon) (Unlicense).
- **Componentes de terceiros**: Os avisos originais de `MAME` (`cps2crypt.cpp` — BSD-3-Clause por Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald; `qsoundhle` / `Unshuffle` / `DecodeKey` por superctr e Valley Bell), referências de temporização (`jtcps2`), `recomp-common` / `wiiparty-recomp` (GPL-3.0-or-later) e decoders (`stb_vorbis`, `dr_flac`, `dr_mp3`, `dr_wav`) estão preservados integralmente em [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- *Dungeons & Dragons: Shadow over Mystara* é propriedade da Capcom / Wizards of the Coast. Projeto de estudo sem fins comerciais e sem afiliação.

---
---

# 🇺🇸 English

Native **Nintendo 64** port of **Dungeons & Dragons: Shadow over Mystara** (Capcom, 1996 — CPS-2 arcade board), built on top of [`cps2recomp`](https://github.com/tmcleroy/cps2recomp) and compiled with the [`libdragon`](https://github.com/DragonMinded/libdragon) SDK (`preview` branch).

The arcade **Motorola 68000** game logic is statically recompiled to native C++ targeting the N64's **MIPS VR4300** CPU (`tools/recomp.py --native-time`), with no 68000 emulator or interpreter fallback linked into the release ROM. Video rendering runs on the N64 **RDP** (`port_n64/rdp_video.cpp`) using CI4-converted tile graphics and software pixel-ownership masks for irregular sprite priorities. The default ROM (**`port_n64/ddsom_native_playtest.z64`**) plays **WAV64** audio (12,019 Hz mono VADPCM/ULC) through `libdragon`'s 8-channel RSP mixer, while current board compatibility includes **4 KiB serial EEPROM** saves (dual CRC-protected slots) and **legacy CPS-2/CPS-B registers** (`port_n64/board.cpp`). The alternative Z80 v1.15 recompiled audio driver (`NATIVE_AUDIO=1`, `port_n64/audio_native.cpp`) is kept strictly as **experimental** and is not linked into the playtest ROM.

> ⚠️ **Legal Notice**: This repository contains **NO copyrighted ROMs, decryption keys, extracted graphics, audio samples, or game-derived binaries**. Users must supply their own legitimate copy of the `ddsom` ROM set to generate assets locally and build the ROM.

---

## Contents (EN)

- [Architecture & Compatibility](#architecture--compatibility)
- [Requirements & Verified Dependencies](#requirements--verified-dependencies)
- [Required Source Files](#required-source-files)
- [Asset Preparation & Building](#asset-preparation--building)
- [Controls](#controls)
- [Test Status & FPS Measurements](#test-status--fps-measurements)
- [Known Limitations & Pending Work](#known-limitations--pending-work)
- [Gallery (`docs/media/`)](#gallery-docsmedia)
- [Credits & Licenses](#credits--licenses)

---

## Architecture & Compatibility

1. **Static 68000 → MIPS Recompilation (`tools/recomp.py --native-time`)**: The 6,961 entry points in [`port_n64/native_entries.txt`](port_n64/native_entries.txt) are translated to C++ and compiled with `-Os` for the VR4300. 68000 cycle counters are stripped from generated code; frame termination relies on scheduler write fixed-point idleness and block budgets, using hash-table dispatch and vblank poll skipping.
2. **RDP Video Renderer (`port_n64/rdp_video.cpp`)**: CPS-2 tiles are converted offline by [`port_n64/ci4.cpp`](port_n64/ci4.cpp) into CI4 format alongside an empty half-block bitmap (`empty.bin`), kept in cartridge ROM and streamed via DMA into a 4-way associative RDRAM tile cache. Irregular sprite/plane priorities use packed per-pixel ownership masks.
3. **Default WAV64 Audio (`port_n64/audio.cpp`)**: An offline host tool (`port_n64/gen_sound_bank.cpp`) executes all 1,168 sound codes (`0x0000`–`0x048F`) on the reference Z80 v1.15 program and QSound HLE chip, then converts each unique waveform via `audioconv64` into `sXXXX.wav64` (12,019 Hz mono: **compress 1 = VADPCM** for SFX, **compress 2 = ULC** for looping music) indexed by `snd/table.bin`. At runtime, the N64 plays up to 8 channels via `libdragon`'s RSP mixer with a 24-entry LRU file cache.
4. **Alternative Audio Driver (Experimental, `NATIVE_AUDIO=1`)**: `port_n64/audio_native.cpp` and `port_n64/prepare_test_assets.py` prepare and test direct execution of the recompiled Z80 v1.15 driver driving 16 signed 8-bit PCM voices. It remains **disabled in the default ROM** due to high CPU/RDRAM overhead and missing DSP echo effects.
5. **Board Registers & Serial EEPROM (`port_n64/board.cpp`, `port_n64/save.cpp`)**: Implements the required legacy board register map (inputs, CPS-B multiplier, object registers, and `0xF18000` sound queue capture) and serial **4K EEPROM** persistence using versioned, sequence-numbered, CRC-verified dual records.

---

## Requirements & Verified Dependencies

### To Play
- **Real Hardware**: Nintendo 64 console with mandatory **Expansion Pak (8 MiB RDRAM)** + 4K EEPROM-compatible flashcart (SummerCart64, EverDrive-64 X7/v2.5+, etc.).
- **Emulator**: [Ares](https://ares-emulator.github.io/) (tested on **v148** with **paraLLEl-RDP** and **8 MiB Expansion Pak** enabled).

### To Build (Verified Environment)
- **OS**: Linux or Windows 10/11 with **WSL2** (Ubuntu).
- **N64 SDK**: [`libdragon`](https://github.com/DragonMinded/libdragon) (`preview` branch installed in `/opt/libdragon` or `$N64_INST`), providing:
  - `mips64-elf-gcc` **16.2.0**
  - `audioconv64` and `mkdfs` (`n64.mk`)
- **Host Tools** (verified versions):
  - `g++` **15.2.0** (C++20)
  - `GNU Make` **4.4.1**
  - `Python` **3.14.4** (Python 3.10+ compatible)
  - `zlib` development headers (`zlib1g-dev`)

Install host prerequisites on Ubuntu / WSL2 (in addition to `libdragon preview` at `/opt/libdragon`):
```sh
sudo apt-get update
sudo apt-get install -y build-essential git make python3 zlib1g-dev
```

---

## Required Source Files

Place the **21 files** of the European **Dungeons & Dragons: Shadow over Mystara (`ddsom`, Europe 960619)** set inside `rom/ddsom/` (matching [`games/ddsom/game.cfg`](games/ddsom/game.cfg)):

- **68000 Program**: `dd2e.03e`, `dd2e.04e`, `dd2e.05e`, `dd2e.06e`, `dd2e.07`, `dd2e.08`, `dd2e.09`, `dd2e.10`
- **Graphics**: `dd2.13m`, `dd2.15m`, `dd2.17m`, `dd2.19m`, `dd2.14m`, `dd2.16m`, `dd2.18m`, `dd2.20m`
- **Z80 Sound Program**: `dd2.01`, `dd2.02`
- **QSound Samples**: `dd2.11m`, `dd2.12m`
- **CPS-2 Key**: `ddsom.key`

*(Or pass a custom path with `--rom /path/to/ddsom` or `ROM=...`.)*

---

## Asset Preparation & Building

From the repository root in WSL2 / Linux:

```sh
# 1. Prepare all assets from rom/ddsom/:
#    - generates game_info.h
#    - builds and runs port_n64/dump.cpp and port_n64/ci4.cpp
#    - builds and runs port_n64/gen_sound_bank.cpp + audioconv64 (snd/table.bin + WAV64)
#    - translates 68000 code into work/ddsom/n64nt/ via tools/recomp.py --native-time
python3 port_n64/prepare_assets.py --n64-inst /opt/libdragon
# (or: make -C port_n64 prepare N64_INST=/opt/libdragon)

# 2. Build the single output ROM: port_n64/ddsom_native_playtest.z64
make -C port_n64 N64_INST=/opt/libdragon -j4
```

To run asset preparation (if not already present), the entire host test suite, and the ROM build in one command:

```sh
bash port_n64/build_native_test.sh
```

**Single build output:** `port_n64/ddsom_native_playtest.z64`

To launch the ROM in Ares (Windows PowerShell) and record telemetry under `port_n64/build/`:

```powershell
powershell -ExecutionPolicy Bypass -File port_n64\run_native_test.ps1 -ShowWindow -Seconds 120
```

---

## Controls

Input mapping defined in [`port_n64/main.cpp`](port_n64/main.cpp):

| N64 Controller | In-Game Action |
| --- | --- |
| **D-Pad** / **Analog Stick** | Move / Menu navigation |
| **Z** | Insert Coin |
| **Start** | Start |
| **B** | Attack |
| **A** | Jump |
| **C-Right** | Select |
| **C-Down** | Use item / spell |

---

## Test Status & FPS Measurements

The current ROM is a **playtest** build.

### Host tests passing (clean build, current code)
- **Board differential (`board-test`)**: `POLL SKIPPED 249584 exact repeated passes`, `NATIVE FALLBACK instructions=0 entries=0`, **`BOARD PASS 3000 frames`** (CPU, registers, flags, RAM, video, palette, and sound queue identical to interpreter reference).
- **Native-time equivalence (`native-time-test`)**: **`NATIVE TIME PASS: 6000 frames identical`** (`work/ddsom/n64test` timed vs `work/ddsom/n64nt` `--native-time` across 6,000 frames).
- **RDP renderer (`video-test`)**: **`RDP PASS 80`** — 80 scenes across 4 path/cache modes (320 total checks), pixel-identical to the reference CPS-2 renderer (`video/video.cpp`).
- **Sound sequencer (`sound-test`)**: **`SOUND SEQUENCE PASS 30000 IRQ ticks, 1195 commands, 42639 identical register writes; native fallback=0`**.
- **Audio protocol, PCM, and EEPROM**: `AUDIO PROTOCOL PASS`, `PCM PASS`, and `SAVE PASS` (built with `-fsanitize=address,undefined`).
- **Binary & asset audit (`verify_native_test.py`)**: valid N64 header (`80371240`), zero interpreter or experimental audio symbols linked, all 641 `.wav64` files verified against all 912 active references in `snd/table.bin`.

### Identified FPS measurements (Ares)
- **ROM**: `port_n64/ddsom_native_playtest.z64` (`profile=0`, `benchmark_frames=0`, `44,564,480` bytes, SHA-256 `6c6bb4d0a6b7c7674e2e02900dbdd419ec7be66ea798b3318d87de5477406c29`)
- **Environment**: Ares **v148** (paraLLEl-RDP, Expansion Pak enabled), 40s boot/attract run (1,320 frames, `interp_total=0`, audio `missing=0`):
  - **Light / intro / menu scenes** (frames 120–300, 660–840, 1320): **58.50 – 59.87 FPS**
  - **Dense attract scenes** (frames 540–600, 960–1200, 360–440 rects): **25.23 – 31.35 FPS**

---

## Known Limitations & Pending Work

- **Sub-60 FPS in heavy scenes**: Dense tile-map and overlapping sprite scenes remain draw-bound on the N64.
- **Pre-rendered WAV64 audio**: Uses an 8-channel pre-recorded 12,019 Hz bank; live QSound DSP echo/spatial effects are not present in the default ROM.
- **Pending validation**:
  - Full interactive campaign playthrough, character selection, local multiplayer, and physical controller testing in Ares and on **real Nintendo 64 console hardware**.
  - `native_entries.txt` covers boot and explored gameplay; unvisited late-game routines could require additional entry points if reached.
  - Authentic screenshots for `docs/media/`.

---

## Gallery (`docs/media/`)

> **Pending**: No screenshots have been added to `docs/media/` yet. Once authentic Ares or real-hardware captures (gameplay, character select, multiplayer) are provided, they will be linked here with source attribution.

---

## Credits & Licenses

- **Base toolkit**: [`cps2recomp`](https://github.com/tmcleroy/cps2recomp) by Tommy McLeroy — licensed under **GPL-3.0-only** ([`LICENSE`](LICENSE)).
- **Nintendo 64 SDK**: [`libdragon`](https://github.com/DragonMinded/libdragon) (Unlicense).
- **Third-party notices**: Original notices for `MAME` (`cps2crypt.cpp` — BSD-3-Clause by Paul Leaman, Andreas Naive, Nicola Salmoria, Charles MacDonald; `qsoundhle` / `Unshuffle` / `DecodeKey` by superctr and Valley Bell), timing references (`jtcps2`), `recomp-common` / `wiiparty-recomp` (GPL-3.0-or-later), and audio decoders (`stb_vorbis`, `dr_flac`, `dr_mp3`, `dr_wav`) are preserved in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
- *Dungeons & Dragons: Shadow over Mystara* is the property of Capcom / Wizards of the Coast. Non-commercial technical preservation project with no affiliation.
