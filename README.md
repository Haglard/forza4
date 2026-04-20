# Vibe Forza 4

Motore per **Forza 4 (Connect Four)** scritto in **C11** con client grafico nativo **macOS (Cocoa/AppKit)**.

Autori: **Sandro Borioni**, **ChatGPT**, **Claude**
Versione: **1.0** -- Licenza: **MIT**

> *not a single line of code is crafted by a human*

---

## Descrizione

Il progetto e' organizzato in tre layer:

- **[engine-core](https://github.com/Haglard/engine-core)** *(submodule in `extern/engine-core`)* -- motore di ricerca negamax generico, transposition table, utility di sistema. Condiviso con chess-engine, dama e tris.
- **connect4** -- regole del Forza 4: board 7x6, generazione mosse, valutazione, zobrist.
- **game_connect4** -- adapter `GameAPI` che connette le regole al motore di ricerca.

Il motore di ricerca non contiene una sola riga specifica per il Forza 4.

---

## Requisiti

- CMake >= 3.16
- GCC o Clang con C11
- Git (per i submodule)
- macOS 12+ per il client Cocoa (target `connect4_client`)

---

## Prima installazione (con submodule)

```bash
git clone https://github.com/Haglard/forza4.git
cd forza4
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
```

---

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j

# Target specifico
cmake --build build --target connect4_client
cmake --build build --target search_cli

# Pulizia
cmake --build build --target clean
```

---

## Target CMake

### Librerie

| Target | Descrizione |
|---|---|
| `engine_core` | Motore negamax + utility *(da submodule engine-core)* |
| `connect4` | Regole Forza 4: board, movegen, eval, zobrist |
| `game_connect4` | Adapter GameAPI per il Forza 4 |

### Eseguibili

| Target | Descrizione |
|---|---|
| `search_cli` | Ricerca best-move da riga di comando |
| `selfplay_cli` | Autogioco motore vs motore |
| `connect4_client` | Client grafico nativo macOS (Cocoa/AppKit) |

---

## Client macOS

```bash
cmake --build build --target connect4_client
./build/connect4_client
```

---

## Struttura directory

```
extern/
  engine-core/        submodule: motore e utility condivise
include/
  connect4/           board, movegen, eval, zobrist
  game/               api.h, connect4_adapter.h
src/
  connect4/           implementazione regole Forza 4
  game/               connect4_adapter.c
tools/
  connect4_client_cocoa.m  client macOS
  search_cli.c
  selfplay_cli.c
build/                out-of-source (non tracciato)
```

---

## Architettura

```
  +---------------------+
  |   connect4_client   |   client macOS
  |   search_cli        |   strumenti CLI
  +----------+----------+
             |
  +----------+----------+
  |    game_connect4    |   GameAPI adapter
  +----------+----------+
             |
  +----------+--------+    +---------------+
  |      connect4     |    |  engine_core  |  submodule
  |   (regole F4)     |    |  (negamax +   |
  |                   |    |   utility)    |
  +-------------------+    +---------------+
```
