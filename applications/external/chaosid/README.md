# ChaosID

![Build](https://github.com/cybersantana83/ChaosID/actions/workflows/release.yml/badge.svg)
![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
![GitHub release](https://img.shields.io/github/v/release/cybersantana83/ChaosID)
![Downloads](https://img.shields.io/github/downloads/cybersantana83/ChaosID/total)

Perito forense de cartoes pro Flipper Zero. Faz scan sequencial LF (125 kHz) -> HF (13.56 MHz), identifica o protocolo e devolve um dossie com tecnologia, criptografia, vetor de ataque conhecido, ano da quebra e nivel de risco.

Parte da serie **Chaos IAM** | feito pela **Cafe com Solda**.

## Download

Baixa o `.fap` pronto na [ultima release](https://github.com/cybersantana83/ChaosID/releases/latest) e arrasta pra `SD Card/apps/NFC/` do seu Flipper via qFlipper ou lab.flipper.net. Depois eh so: **Apps -> NFC -> ChaosID**.

> ⚠️ Compilado contra **Momentum firmware** (API 87.1, target F7). Pra outros firmwares (oficial Flipper Devices, Unleashed, RogueMaster) recompila a partir do source - instrucoes na secao **Build**.

## Status

**v0.3** - leitura real funcionando em LF e HF.

- **LF (125 kHz)** via `lfrfid_worker` - identifica EM4100, HID Prox, Indala, AWID e variantes. Retorna UID raw em hex + dados parseados (Facility Code, Card Number, Customer ID).
- **HF (13.56 MHz)** via `nfc_scanner` + `iso14443_3a_poller_sync` - identifica MIFARE Classic/Ultralight/DESFire/Plus, NTAG, FeliCa, iCLASS, ISO15693. Extrai UID, ATQA e SAK.
- Dossie cruza o protocolo detectado com base interna de 15+ entries, mostrando criptografia usada, vetor de ataque conhecido, ano da quebra publica e nivel de risco (Trivial / Quebrada / Endurecida).

## Estrutura

```
chaos_id/
├── application.fam          manifesto Flipper
├── chaos_id.c               entry point + alloc/free do app
├── chaos_id_app.h           struct de estado + enums (scenes/views/events)
├── scenes/
│   ├── chaos_id_scene.h     declaracoes dos handlers
│   ├── chaos_id_scene.c     tabela de dispatch
│   ├── chaos_id_scene_splash.c     menu principal
│   ├── chaos_id_scene_scanning.c   ponto de integracao com radio
│   ├── chaos_id_scene_result.c     dossie do cartao
│   └── chaos_id_scene_about.c
├── database/
│   ├── cards_db.h           CardProfile struct + helpers
│   └── cards_db.c           15 entries reais (LF + HF)
└── assets/
    └── icon.png             10x10 mono (incluso)
```

## Build (modo isolado, contra SDK do Momentum)

Setup de primeira vez (cria SDK project-local em `.ufbt/` dentro da pasta, em vez do `~/.ufbt/` global):

```bash
# Instala ufbt isolado via pipx (Ubuntu 23.04+ / Debian 12+ exigem por causa do PEP 668)
sudo apt install pipx
pipx ensurepath && exec $SHELL
pipx install ufbt

cd chaos_id
ufbt dotenv_create                              # SDK fica em .ufbt/ DENTRO do projeto
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
ufbt vscode_dist                                # gera .vscode/ com tasks + IntelliSense
code .
```

Ciclo de desenvolvimento:

```bash
ufbt           # build -> dist/chaos_id.fap
ufbt launch    # build + flash USB + executa no Flipper conectado
ufbt cli       # console serial do Flipper (qFlipper precisa estar FECHADO)
ufbt -c        # clean
```

No VS Code: **Ctrl+Shift+B** dispara build. Pra step-debug precisa de probe SWD; sem probe, usa `furi_log_*` no codigo e olha o output no `ufbt cli`.

Antes do `ufbt launch`, fecha qFlipper e a aba do lab.flipper.net se estiverem abertos - seguram a porta USB.

O `assets/icon.png` ja vem no scaffold (10x10 1-bit, cartao com chip EMV). Pra trocar o desenho, qualquer PNG 10x10 monocromatico serve.

## Como funciona

`scenes/chaos_id_scene_scanning.c` orquestra duas fases sequenciais:

### Fase LF (125 kHz) - ate 4s

1. Aloca `ProtocolDict` com todos os protocolos LF disponiveis (`lfrfid_protocols`)
2. Inicia `LFRFIDWorker` em modo `LFRFIDWorkerReadTypeAuto` (cicla ASK e PSK)
3. Callback `lf_read_callback` dispara em `LFRFIDWorkerReadDone`:
   - `protocol_dict_get_data()` retorna os bytes raw -> formatados como hex no campo UID
   - `protocol_dict_render_brief_data()` retorna campos parseados (FC, Card, CL, decimal)
   - `protocol_dict_get_name()` retorna o nome -> `cards_db_find_by_lfrfid_name()` mapeia pro DB
4. Se nada detectado em 4s, transita pra fase HF

### Fase HF (13.56 MHz) - ate 2.5s + ~300ms de activation

1. Aloca `Nfc` e `NfcScanner` (greedy, varre todos os protocolos em paralelo)
2. Callback `hf_scanner_callback` dispara em `NfcScannerEventTypeDetected`:
   - Salva o protocolo (mais especifico primeiro) e delega pro UI thread via custom event
   - Nao para o scanner daqui (deadlock - somos a worker thread dele)
3. UI thread recebe `HfDetected`, para o scanner, chama `iso14443_3a_poller_sync_read()`:
   - Bloqueia ~200-300ms refazendo activation ISO14443-3A (sem crypto, sem dictionary attack)
   - Retorna struct com UID + ATQA + SAK
4. `nfc_device_get_protocol_name()` + `cards_db_find_by_nfc_name()` mapeiam pro DB

### Threading

LFRFID worker e NfcScanner rodam em threads proprias. Todo callback de RF fala com a UI thread via `view_dispatcher_send_custom_event()` - nunca chame APIs de view/scene_manager direto da worker thread (deadlock).

## Build multi-firmware (release)

Pra gerar `.fap` simultaneamente pra Momentum + firmware oficial da Flipper Devices:

\`\`\`bash
./scripts/build_multi.sh
\`\`\`

Saída em `dist/multi/`:

- `chaos_id-momentum.fap` — Momentum firmware
- `chaos_id-official.fap` — Flipper Devices oficial

O script chaveia o canal SDK do `ufbt`, faz clean + build, e no fim restaura o SDK default (Momentum). Adicionar outros firmwares (Unleashed, RogueMaster) é só descomentar/adicionar linha no array `FIRMWARES=(...)`.

## Roadmap

- [x] **v0.1** - UI + scene manager + base de cartoes + scan simulado
- [x] **v0.2** - integracao real com `lfrfid_worker` (LF) e `nfc_scanner` (HF)
- [x] **v0.3** - extracao de UID/ATQA/SAK via `iso14443_3a_poller_sync`
- [ ] **v0.4** - persistencia: salva cada scan em `/ext/apps_data/chaos_id/log.csv` (timestamp, protocol, UID, risk)
- [ ] **v0.5** - botao "Investigar" no dossie de cartoes broken: pra MIFARE Classic chama nested attack direto
- [ ] **v0.6** - i18n EN/PT, splash com identidade visual Cafe com Solda, submit pro app catalog oficial

## Licenca

**GPLv3 ou posterior** (GNU General Public License v3.0-or-later).

- Texto completo em `LICENSE` (canonico, SHA-256 `3972dc97...86`).
- Cada arquivo fonte tem header SPDX `GPL-3.0-or-later`.
- Forks e derivados devem ser redistribuidos sob a mesma licenca.

Escolha alinhada com a cultura do firmware Flipper e de ferramentas de pesquisa em seguranca (Proxmark, Chameleon, etc).
