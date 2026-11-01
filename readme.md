# HERO: Last Stand — Definitive Edition

A code-drawn 2D dark-fantasy boss battle written in modern C++ using **SFML 3**. 

All visual assets—including player animations, character models, glowing runes, dynamic particle systems, screen shakes, and gothic environmental backdrops—are rendered procedurally via code primitives (`sf::RectangleShape`, `sf::CircleShape`, and `sf::ConvexShape`). **No external sprite sheets or texture packs are required.**

---

## Table of Contents

- [Features](#features)
- [Requirements & Prerequisites](#requirements--prerequisites)
- [Controls](#controls)
- [Gameplay Mechanics](#gameplay-mechanics)
- [Building & Running](#building--running)
  - [Windows (MSYS2 / UCRT64)](#windows-msys2--ucrt64)
  - [Linux (Ubuntu / Debian / Arch)](#linux-ubuntu--debian--arch)
  - [macOS (Homebrew)](#macos-homebrew)
  - [Font Dependency Note](#font-dependency-note)
- [Project Architecture](#project-architecture)

---

## Features

- **Procedural Graphics Engine:** Zero external images or spritesheets; all backgrounds, characters, animations, and atmospheric effects are generated mathematically in real time.
- **Souls-like Combat Mechanics:**
  - 3-hit grounded combo chain ending in a high-damage crimson finisher.
  - Plunging aerial attack.
  - Precision **Runic Parry** window with stagger reward.
  - **Shadow Dash** with afterimage ghost trails and invulnerability frames ($i$-frames).
  - Flask healing system with limited charges.
  - Stamina management balancing offensive and evasive maneuvers.
- **Dynamic Boss AI ("The Ashen Colossus"):**
  - Multi-pattern attack sets: *Abyssal Cleave*, *Ground Spike Eruptions*, and *Meteor Barrages*.
  - **Phase II: Ashen Overdrive** triggered dynamically at $\le 50\%$ health (enhanced attack frequency, blood-moon atmosphere, and dual-direction hazards).
  - Stagger system rewarding successful parries with high critical damage windows.
- **Polished Game Feel:** Dynamic screen shake, hitstop freeze frames, damage popups, delayed health damage bars, and custom particle dynamics (sparks, shockwaves, smoke, and motes).

---

## Requirements & Prerequisites

### 1. Compiler
- A modern C++ compiler supporting at least **C++17** (C++20 recommended):
  - GCC 11+ / Clang 13+ / MSVC 19.29+ (Visual Studio 2019 or newer).

### 2. Dependencies
- **SFML 3.x** (`sfml-graphics`, `sfml-window`, `sfml-system`).
  > **Note:** This codebase targets **SFML 3**, utilizing updated API conventions (e.g., `event->getIf<sf::Event::KeyPressed>()`, `sf::Vector2f` vector structures, and `sf::degrees()`). It will not compile against SFML 2.x without backport modifications.
- **`pkg-config`** (optional, recommended for build automation on Unix/MinGW).

---

## Controls

The game supports two control schemes (Primary and Alternative arcade layout):

| Action | Primary Key | Alternative Key | Notes |
| :--- | :---: | :---: | :--- |
| **Move Left** | `A` | `Left Arrow` | Smooth inertia acceleration |
| **Move Right** | `D` | `Right Arrow` | Smooth inertia acceleration |
| **Jump** | `Space` | — | Variable jump height (hold to ascend higher) |
| **Sword Attack** | `Z` | `J` | 3-hit combo grounded; plunging slash mid-air |
| **Shadow Dash** | `Left Shift` | `K` | Grants temporary $i$-frames; consumes stamina |
| **Runic Parry** | `X` | `L` | Timed guard; staggers boss and restores stamina |
| **Ember Flask** | `C` | `H` | Restores $+45\text{ HP}$ (3 charges total) |
| **Restart Match** | `R` | — | Usable after victory or defeat |
| **Exit** | `Escape` | — | Closes the window |

---

## Gameplay Mechanics

### Combat Breakdown

- **Grounded Combo:**
  - **Hit 1 (Slash):** $14\text{ DMG}$
  - **Hit 2 (Rising):** $17\text{ DMG}$
  - **Hit 3 (Crimson Finisher):** $28\text{ DMG}$ (Extended range and lunge)
- **Aerial Plunge:** $24\text{ DMG}$ with impact shockwave on landing.
- **Perfect Parry:** Parrying an incoming boss strike reflects the blow, inflicts $-35\text{ DMG}$, triggers hitstop, restores $+40\text{ Stamina}$, and sends the boss into a **Staggered** state.
- **Stagger Bonus:** Attacks dealt while the boss is staggered receive a **$1.6\times$ damage multiplier**.

### Boss Phases

- **Phase I:** The Ashen Colossus cycles through standard lunging strikes, shockwave spikes, and falling meteors.
- **Phase II ($\le 50\%\text{ HP}$):** The moon shifts to a Blood Moon, the sky darkens, and the boss gains faster telegraphs, dual-direction spikes, and denser meteor showers.

---

## Building & Running

### Windows (MSYS2 / UCRT64)

1. Open the **MSYS2 UCRT64** terminal.
2. Install dependencies:
   ```bash
   pacman -Syu
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-sfml mingw-w64-ucrt-x86_64-pkg-config
   ```
3. Compile and link:
   ```bash
   g++ hero_last_stand.cpp -o LastStand.exe -std=c++17 $(pkg-config --cflags --libs sfml-graphics)
   ```
4. Run the executable:
   ```bash
   ./LastStand.exe
   ```

---

### Linux (Ubuntu / Debian / Arch)

Ensure your package manager provides **SFML 3**:

```bash
# Ubuntu / Debian (if SFML 3 is available in your distribution channels)
sudo apt update
sudo apt install g++ libsfml-dev pkg-config

# Arch Linux
sudo pacman -S gcc sfml pkgconf
```

Compile:
```bash
g++ hero_last_stand.cpp -o LastStand -std=c++17 $(pkg-config --cflags --libs sfml-graphics)
./LastStand
```

---

### macOS (Homebrew)

```bash
brew install sfml pkg-config
clang++ hero_last_stand.cpp -o LastStand -std=c++17 $(pkg-config --cflags --libs sfml-graphics)
./LastStand
```

---

### Font Dependency Note

The source code loads a default font from `C:/Windows/Fonts/arial.ttf`:

```cpp
bool fontLoaded = font.openFromFile("C:/Windows/Fonts/arial.ttf");
```

- **For non-Windows platforms (Linux / macOS):** Place a TrueType font (such as `arial.ttf` or `DejaVuSans.ttf`) in your executable directory and update the path:
  ```cpp
  bool fontLoaded = font.openFromFile("arial.ttf");
  ```
- If the font fails to load, the game will run smoothly without text, but HUD text and combat numbers will not be displayed.

---

## Project Architecture

```
hero_last_stand.cpp
├── Helpers / Geometry        (Color clamping, circle, line, polygon primitives)
├── VFX Subsystem             (Particle, Shockwave, FloatingText, GroundSpike, Meteor)
├── World & Environment       (Atmosphere starry sky, Blood moon, Gothic Town rubble)
├── Player Class              (Kinematics, combos, dash afterimages, parry shield, HUD stats)
├── Monster Class             (FSM states: Idle, Walk, Windup, Strike, Staggered, Phase II)
└── main() Loop               (Input polling, hit detection, hitstop, screenshake, render pipelines)
```