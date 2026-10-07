# Party Info Debug Window Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a read-only "Party Info" window to the PC port's debug GUI (Tab). It shows each party Pokémon's exact friendship, plus nature, IVs, EVs, and how many steps are left until the next friendship roll.

**Architecture:** A small C file reads the party from the save data and copies the numbers into a plain struct (a "snapshot"). A C++ ImGui window takes a new snapshot every frame and draws it. The window only reads. It never writes to the save. Both files are PC-only (`src/port/`), so the DS ROM build doesn't change.

**Tech Stack:** C (game side), C++ with Dear ImGui (from libntr's `simulator/imgui`), meson.

**Spec:** No separate spec. This came from a chat about Budew → Roselia. The requirements are:
1. Show the exact friendship (0–255) of every party Pokémon.
2. Show the same heart tier the Pokétch Friendship Checker would show.
3. Show how close each one is to 220, the friendship needed for friendship evolutions.
4. Show what boosts friendship gains for that mon: Soothe Bell, Luxury Ball.
5. Show the step counter: steps left until the next walking friendship roll.
6. Also show nature, IVs and EVs.
7. Read only: never change the save.

## Global Constraints

- PC-only code. New files go in `src/port/` and in the `build_target != 'arm'` block of `src/meson.build`. Do **not** add them to `platinum.us/main.lsf`.
- Don't change any shared game file (`src/pokemon.c`, the Pokétch app, etc.). The DS ROM must build the same as before.
- New code follows `CONTRIBUTING.md`: PascalCase functions with a module prefix (`PartyInfo_`), camelCase variables, typedef'd structs. Don't copy the older `src/port/` style (`GUI_CreateMon_...`, 8-space indents).
- Format new files with clang-format 19 (`.clang-format` in the repo root).
- LF line endings.
- Never call `Pokemon_SetValue` or anything else that writes. The window only reads.
- Work on a branch off `colin/main` (for example `feat/party-info-window`). Merge back only when the user approves.

## Review Focus

These are the cases most likely to break for a real player. No test covers them automatically, so check each one by hand in Task 4.

1. **Title screen / no save loaded yet.** The window must not crash. Show the button only once a field system exists (same rule as "Jump to Map"), and have `PartyInfo_Read` return FALSE when there is no save or party.
2. **Eggs in the party.** An egg has a friendship value too (it's the hatch counter), but showing it as friendship is misleading. Show "Egg" and skip the stats.
3. **During a battle.** The battle works on its own copy of the party, and the save party updates when the battle ends. The window will show the values from before the battle. Say so in a small note in the window, so the user doesn't think it's broken.
4. **Party changes while the window is open** (catch a mon, deposit one, swap order). The snapshot is taken fresh every frame, and no `Pokemon *` is kept between frames, so the window just follows the new party.
5. **Friendship 255 and 0.** The heart tier must be 3 hearts at 255 and "no hearts" at 0, the same as the Pokétch (`GetFriendshipLevel` in `src/applications/poketch/friendship_checker/main.c:139`).

---

## Background for the implementer

- **Friendship rules** live in `Pokemon_UpdateFriendship` (`src/pokemon.c:2635`). Every 128 steps, each party mon has a 50% chance to gain friendship. The step counter is saved in the vars: `SystemVars_GetFriendshipStepCount(SaveData_GetVarsFlags(save))` returns 0–127 (`src/overlay005/field_control.c:860`).
- **Soothe Bell** gives ×1.5, rounded down, so it adds nothing to the +1 from walking. That's normal game behavior, not something to fix.
- **Friendship evolution threshold:** `EVOLVE_FRIENDSHIP_THRESHOLD` = 220 (`include/constants/pokemon.h:25`). Roselia also needs a level-up in the daytime.
- **Pokétch heart tiers:** 0–149 no hearts, 150–199 one heart, 200–254 two hearts, 255 three hearts. (Under 70 it shows a "dislike" icon instead. We don't need that.)
- **Threading:** the PC port runs game logic and the GUI on one thread. The GUI draws during the VBlank wait, between game steps, so reading the party there is safe. `Pokemon_GetValue` decrypts and re-encrypts the mon on its own, and leaves it alone if it is already decrypted.
- **Testing:** this repo has no unit tests, and `make check` fails on this fork by design. You verify by building and looking at the window in the running game. Each task says exactly what to check.
- **Building:**
  - Linux compile check (fast): `./drun.sh make linux` in the WSL repo `~/dev/playground/pokeplatinum`.
  - DS ROM still builds: `./drun.sh make`. Only "Filesystem Checksums" is expected to pass in `make check`. Just check that `make` itself doesn't fail.
  - Windows build (the one the user plays): push the branch, `git pull` in `/mnt/c/dev/pokeplatinum`, close the game, then run in the background:
    `cd /mnt/c/dev/pokeplatinum && powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\\Launch_MSys2.ps1 "make win64" > LOG 2>&1`
    The exit code lies. Check LOG for `make: ***`.

## File Structure

| File | What it does |
|---|---|
| Create `src/port/gui_partyinfo_utils.h` | C header: the `PartyInfo` snapshot struct and `PartyInfo_Read`, wrapped in `extern "C"`. |
| Create `src/port/gui_partyinfo_utils.c` | Reads the save party into a `PartyInfo`. Also has the heart tier and nature name helpers. No ImGui here. |
| Create `src/port/gui_partyinfo.cpp` | The ImGui window. Only draws. Caches species names. |
| Modify `src/port/sim_gui_prj.hpp` | Declare `PartyInfoMain`. |
| Modify `src/port/sim_gui_prj.cpp` | Add the "Party Info" button. |
| Modify `src/meson.build:1018-1029` | Add the two new source files to the port-only list. |
| Modify `src/port/gui_createmon.cpp:30` | Side fix: `delete` → `free` (see Task 1). |

The split matches the other debug windows (`gui_createmon.cpp` + `gui_createmon_utils.c`): the C++ file can't easily include the game's C headers, so the C file does the game-side work.

---

### Task 1: Fix the wrong `delete` in the Create Mon window

`GUI_CreateMon_GetSpeciesName` returns memory from `malloc`, but `CreateMonInit` frees it with `delete`. Mixing `malloc` and `delete` is undefined behavior. The Party Info window will use the same helper, so fix it first.

**Files:**
- Modify: `src/port/gui_createmon.cpp:30`

- [ ] **Step 1: Make the change**

In `src/port/gui_createmon.cpp`, add `#include <cstdlib>` next to the other standard includes, and change:

```cpp
        delete pokemonName;
```

to:

```cpp
        free((void *)pokemonName);
```

- [ ] **Step 2: Build**

Run: `./drun.sh make linux`
Expected: build finishes with no errors.

- [ ] **Step 3: Check it by hand**

Start the game (`pokemon` alias in WSL), press Tab, open "Gen Pokemon". The species list should still show all names.

- [ ] **Step 4: Commit**

```bash
git add src/port/gui_createmon.cpp
git commit -m "Free species names with free() in the Gen Pokemon window"
```

---

### Task 2: Read the party into a snapshot (C side)

**Files:**
- Create: `src/port/gui_partyinfo_utils.h`
- Create: `src/port/gui_partyinfo_utils.c`
- Modify: `src/meson.build` (port-only list, after `'port/gui_mapjump_utils.c'`)

**Interfaces:**
- Produces (used by Task 3):
  - `typedef struct PartyInfoMon { ... } PartyInfoMon;`, `typedef struct PartyInfo { ... } PartyInfo;` (fields below)
  - `BOOL PartyInfo_Read(PartyInfo *info);`
  - `int PartyInfo_GetHeartCount(u8 friendship);` returns 0–3
  - `const char *PartyInfo_GetNatureName(u8 nature);`
  - `#define PARTY_INFO_EVOLVE_FRIENDSHIP 220`

- [ ] **Step 1: Write the header**

`src/port/gui_partyinfo_utils.h`:

```c
#ifndef GUI_PARTYINFO_UTILS_H
#define GUI_PARTYINFO_UTILS_H

#include <nitro.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PARTY_INFO_MAX_MONS          6
#define PARTY_INFO_STAT_COUNT        6
#define PARTY_INFO_FRIENDSHIP_STEPS  128
#define PARTY_INFO_EVOLVE_FRIENDSHIP 220

// A plain copy of one party mon's numbers. Safe to keep around after
// the real party changes, because it holds no pointers into the save.
typedef struct PartyInfoMon {
    u16 species;
    u8 level;
    u8 friendship;
    u8 nature;
    u8 ivs[PARTY_INFO_STAT_COUNT]; // HP, Atk, Def, Spe, SpA, SpD
    u8 evs[PARTY_INFO_STAT_COUNT]; // same order
    BOOL isEgg;
    BOOL hasSootheBell;
    BOOL inLuxuryBall;
} PartyInfoMon;

typedef struct PartyInfo {
    int monCount;
    u16 friendshipStepCount; // 0-127, a roll happens when it reaches 128
    PartyInfoMon mons[PARTY_INFO_MAX_MONS];
} PartyInfo;

// Fills info from the current save. Only reads; never changes the save.
// Returns FALSE when no save is loaded yet (for example on the title screen).
BOOL PartyInfo_Read(PartyInfo *info);

// Same tiers as the Pokétch Friendship Checker: 0, 1, 2 or 3 hearts.
int PartyInfo_GetHeartCount(u8 friendship);

const char *PartyInfo_GetNatureName(u8 nature);

#ifdef __cplusplus
}
#endif

#endif // GUI_PARTYINFO_UTILS_H
```

- [ ] **Step 2: Write the C file**

`src/port/gui_partyinfo_utils.c`:

```c
#include "gui_partyinfo_utils.h"

#include "constants/items.h"
#include "constants/pokemon.h"
#include "generated/natures.h"

#include "party.h"
#include "pokemon.h"
#include "savedata.h"
#include "system_vars.h"
#include "vars_flags.h"

static const enum PokemonDataParam sIvParams[PARTY_INFO_STAT_COUNT] = {
    MON_DATA_HP_IV,
    MON_DATA_ATK_IV,
    MON_DATA_DEF_IV,
    MON_DATA_SPEED_IV,
    MON_DATA_SPATK_IV,
    MON_DATA_SPDEF_IV,
};

static const enum PokemonDataParam sEvParams[PARTY_INFO_STAT_COUNT] = {
    MON_DATA_HP_EV,
    MON_DATA_ATK_EV,
    MON_DATA_DEF_EV,
    MON_DATA_SPEED_EV,
    MON_DATA_SPATK_EV,
    MON_DATA_SPDEF_EV,
};

// English names, indexed by enum Nature. Designated initializers keep
// each name tied to its enum value even if the order is wrong here.
static const char *const sNatureNames[] = {
    [NATURE_HARDY] = "Hardy",
    [NATURE_LONELY] = "Lonely",
    [NATURE_BRAVE] = "Brave",
    [NATURE_ADAMANT] = "Adamant",
    [NATURE_NAUGHTY] = "Naughty",
    [NATURE_BOLD] = "Bold",
    [NATURE_DOCILE] = "Docile",
    [NATURE_RELAXED] = "Relaxed",
    [NATURE_IMPISH] = "Impish",
    [NATURE_LAX] = "Lax",
    [NATURE_TIMID] = "Timid",
    [NATURE_HASTY] = "Hasty",
    [NATURE_SERIOUS] = "Serious",
    [NATURE_JOLLY] = "Jolly",
    [NATURE_NAIVE] = "Naive",
    [NATURE_MODEST] = "Modest",
    [NATURE_MILD] = "Mild",
    [NATURE_QUIET] = "Quiet",
    [NATURE_BASHFUL] = "Bashful",
    [NATURE_RASH] = "Rash",
    [NATURE_CALM] = "Calm",
    [NATURE_GENTLE] = "Gentle",
    [NATURE_SASSY] = "Sassy",
    [NATURE_CAREFUL] = "Careful",
    [NATURE_QUIRKY] = "Quirky",
};

static void ReadMon(Pokemon *mon, PartyInfoMon *out)
{
    out->isEgg = Pokemon_GetValue(mon, MON_DATA_IS_EGG, NULL);
    out->species = Pokemon_GetValue(mon, MON_DATA_SPECIES, NULL);
    out->level = Pokemon_GetValue(mon, MON_DATA_LEVEL, NULL);
    out->friendship = Pokemon_GetValue(mon, MON_DATA_FRIENDSHIP, NULL);
    out->nature = Pokemon_GetNature(mon);
    out->hasSootheBell = Pokemon_GetValue(mon, MON_DATA_HELD_ITEM, NULL) == ITEM_SOOTHE_BELL;
    out->inLuxuryBall = Pokemon_GetValue(mon, MON_DATA_POKEBALL, NULL) == ITEM_LUXURY_BALL;

    for (int i = 0; i < PARTY_INFO_STAT_COUNT; i++) {
        out->ivs[i] = Pokemon_GetValue(mon, sIvParams[i], NULL);
        out->evs[i] = Pokemon_GetValue(mon, sEvParams[i], NULL);
    }
}

BOOL PartyInfo_Read(PartyInfo *info)
{
    SaveData *saveData = SaveData_Ptr();

    if (saveData == NULL) {
        return FALSE;
    }

    Party *party = SaveData_GetParty(saveData);

    if (party == NULL) {
        return FALSE;
    }

    info->monCount = Party_GetCurrentCount(party);

    if (info->monCount > PARTY_INFO_MAX_MONS) {
        info->monCount = PARTY_INFO_MAX_MONS;
    }

    for (int i = 0; i < info->monCount; i++) {
        ReadMon(Party_GetPokemonBySlotIndex(party, i), &info->mons[i]);
    }

    info->friendshipStepCount = SystemVars_GetFriendshipStepCount(SaveData_GetVarsFlags(saveData));

    return TRUE;
}

int PartyInfo_GetHeartCount(u8 friendship)
{
    // Matches GetFriendshipLevel in the Pokétch Friendship Checker
    // (src/applications/poketch/friendship_checker/main.c).
    if (friendship == MAX_FRIENDSHIP_VALUE) {
        return 3;
    }

    if (friendship >= 200) {
        return 2;
    }

    if (friendship >= 150) {
        return 1;
    }

    return 0;
}

const char *PartyInfo_GetNatureName(u8 nature)
{
    if (nature >= NELEMS(sNatureNames)) {
        return "???";
    }

    return sNatureNames[nature];
}
```

Note: the nature names above match `generated/natures.txt` (checked on 2026-10-06).

- [ ] **Step 3: Add the file to the build**

In `src/meson.build`, in the port-only list (around line 1029), change:

```meson
    'port/gui_mapjump_utils.c'
```

to:

```meson
    'port/gui_mapjump_utils.c',
    'port/gui_partyinfo_utils.c'
```

- [ ] **Step 4: Format and build**

Run: `clang-format-19 -i src/port/gui_partyinfo_utils.c src/port/gui_partyinfo_utils.h` (or `./drun.sh clang-format-19 -i ...` if it isn't installed locally)
Run: `./drun.sh make linux`
Expected: build finishes with no errors or new warnings in `gui_partyinfo_utils.c`. Nothing calls the code yet; this step only proves it compiles.

- [ ] **Step 5: Commit**

```bash
git add src/port/gui_partyinfo_utils.h src/port/gui_partyinfo_utils.c src/meson.build
git commit -m "Add a read-only party snapshot for the debug GUI"
```

---

### Task 3: The Party Info window (C++ / ImGui side)

**Files:**
- Create: `src/port/gui_partyinfo.cpp`
- Modify: `src/port/sim_gui_prj.hpp`
- Modify: `src/port/sim_gui_prj.cpp`
- Modify: `src/meson.build` (port-only list)

**Interfaces:**
- Consumes: `PartyInfo`, `PartyInfo_Read`, `PartyInfo_GetHeartCount`, `PartyInfo_GetNatureName`, `PARTY_INFO_*` from Task 2. `GUI_CreateMon_GetSpeciesName(int)` from `gui_createmon_utils.h` (returns a `malloc`'d string; free it with `free`).
- Produces: `void SIM::GUI::PartyInfoMain(bool *openState);`

- [ ] **Step 1: Write the window**

`src/port/gui_partyinfo.cpp`:

```cpp
#include <nitro.h>
#include <simulator/imgui/imgui.hpp>
#include <simulator/sim.h>
#include <simulator/sim_gui.hpp>

#include "sim_gui_prj.hpp"

#include "gui_createmon_utils.h"
#include "gui_partyinfo_utils.h"

#include <cstdlib>
#include <map>
#include <string>

namespace SIM::GUI {

static const char *const sStatLabels[PARTY_INFO_STAT_COUNT] = { "HP", "Atk", "Def", "Spe", "SpA", "SpD" };

// Looking up a species name allocates on the game heap, so do it once per
// species instead of every frame.
static const std::string &GetSpeciesName(u16 species)
{
    static std::map<u16, std::string> sCache;

    auto it = sCache.find(species);

    if (it == sCache.end()) {
        const char *name = GUI_CreateMon_GetSpeciesName(species);
        it = sCache.emplace(species, std::string(name)).first;
        free((void *)name);
    }

    return it->second;
}

static void DrawHearts(int heartCount)
{
    if (heartCount == 0) {
        ImGui::TextDisabled("no hearts");
        return;
    }

    std::string hearts;

    for (int i = 0; i < heartCount; i++) {
        hearts += "<3 ";
    }

    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.6f, 1.0f), "%s", hearts.c_str());
}

static void DrawMon(int slot, const PartyInfoMon &mon)
{
    ImGui::PushID(slot);

    if (mon.isEgg) {
        ImGui::Text("%d. Egg", slot + 1);
        ImGui::Separator();
        ImGui::PopID();
        return;
    }

    ImGui::Text("%d. %s  Lv. %d  (%s)", slot + 1, GetSpeciesName(mon.species).c_str(), mon.level, PartyInfo_GetNatureName(mon.nature));

    ImGui::Text("Friendship: %d / 255", mon.friendship);
    ImGui::SameLine();
    DrawHearts(PartyInfo_GetHeartCount(mon.friendship));

    float toEvolve = (float)mon.friendship / PARTY_INFO_EVOLVE_FRIENDSHIP;
    char overlay[32];

    if (mon.friendship >= PARTY_INFO_EVOLVE_FRIENDSHIP) {
        snprintf(overlay, sizeof(overlay), "ready (220+)");
    } else {
        snprintf(overlay, sizeof(overlay), "%d more to 220", PARTY_INFO_EVOLVE_FRIENDSHIP - mon.friendship);
    }

    ImGui::ProgressBar(toEvolve > 1.0f ? 1.0f : toEvolve, ImVec2(-1.0f, 0.0f), overlay);

    if (mon.hasSootheBell || mon.inLuxuryBall) {
        ImGui::Text("Boosts:%s%s", mon.hasSootheBell ? " Soothe Bell" : "", mon.inLuxuryBall ? " Luxury Ball" : "");
    }

    if (ImGui::BeginTable("stats", PARTY_INFO_STAT_COUNT + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("");

        for (int i = 0; i < PARTY_INFO_STAT_COUNT; i++) {
            ImGui::TableSetupColumn(sStatLabels[i]);
        }

        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("IV");

        for (int i = 0; i < PARTY_INFO_STAT_COUNT; i++) {
            ImGui::TableNextColumn();
            ImGui::Text("%d", mon.ivs[i]);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("EV");

        for (int i = 0; i < PARTY_INFO_STAT_COUNT; i++) {
            ImGui::TableNextColumn();
            ImGui::Text("%d", mon.evs[i]);
        }

        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::PopID();
}

void PartyInfoMain(bool *openState)
{
    ImGui::Begin("Party Info", openState);

    PartyInfo info;

    if (!PartyInfo_Read(&info)) {
        ImGui::TextDisabled("No save loaded yet.");
        ImGui::End();
        return;
    }

    ImGui::Text("Steps until next friendship roll: %d", PARTY_INFO_FRIENDSHIP_STEPS - info.friendshipStepCount);
    ImGui::TextDisabled("Each roll: 50%% chance of +1 per mon. Values update after a battle ends.");
    ImGui::Separator();

    for (int i = 0; i < info.monCount; i++) {
        DrawMon(i, info.mons[i]);
    }

    ImGui::End();
}

} // namespace SIM::GUI
```

Note: libntr's ImGui has the Tables API (`BeginTable` in `subprojects/libntr/include/simulator/imgui/imgui.hpp:916`), so the table above works as is.

- [ ] **Step 2: Declare it**

In `src/port/sim_gui_prj.hpp`, after `void MapJumpMain(bool * openState);`, add:

```cpp
void PartyInfoMain(bool * openState);
```

- [ ] **Step 3: Add the button**

In `src/port/sim_gui_prj.cpp`, add the static next to the others:

```cpp
static bool sPartyInfoOpen = false;
```

and put the button inside the existing field-system check, so it only shows once the game is in the overworld:

```cpp
    if(DEBUG_GetFieldSystem() != nullptr) {
        AppButton("Jump to Map", &sMapJumpOpen, MapJumpInit, MapJumpMain);
        AppButton("Party Info", &sPartyInfoOpen, nullptr, PartyInfoMain);
    }
```

(Keep the existing style of this file. It's older port code.)

- [ ] **Step 4: Add the file to the build**

In `src/meson.build`, in the port-only list, change:

```meson
    'port/gui_partyinfo_utils.c'
```

to:

```meson
    'port/gui_partyinfo.cpp',
    'port/gui_partyinfo_utils.c'
```

- [ ] **Step 5: Format and build**

Run: `clang-format-19 -i src/port/gui_partyinfo.cpp`
Run: `./drun.sh make linux`
Expected: build finishes with no errors.

- [ ] **Step 6: Check it by hand (WSL)**

Start the game with the `pokemon` alias, load the save, and press Tab → "Party Info". Check:
- Every party mon is listed with level, nature and friendship.
- The hearts match what the Pokétch Friendship Checker shows for the same mon.
- Budew shows "Boosts: Soothe Bell".
- Walk a few steps: "Steps until next friendship roll" goes down by 1 per step, and goes back to 128 after it reaches the end.

Note: the WSL save is a separate copy from the Windows save, so the numbers may differ from the Windows game.

- [ ] **Step 7: Commit**

```bash
git add src/port/gui_partyinfo.cpp src/port/sim_gui_prj.hpp src/port/sim_gui_prj.cpp src/meson.build
git commit -m "Add a Party Info debug window showing exact friendship"
```

---

### Task 4: Check the edge cases and the DS build

No new code, unless a check fails. Go through the Review Focus list.

- [ ] **Step 1: Fresh mon gives a known value**

Use "Gen Pokemon" to make a level 5 Budew (with a free party slot). Party Info should show friendship **70**, Budew's base friendship, and "no hearts".

- [ ] **Step 2: Title screen**

Restart the game and press Tab on the title screen. The "Party Info" button should not be there, and nothing should crash.

- [ ] **Step 3: Egg**

If you have an egg (or get one from the Day Care), check that it shows "Egg" with no stats.

- [ ] **Step 4: Battle**

Open the window, then get into a wild battle. The window should keep working. Values change only after the battle ends (for example, the level-up friendship gain shows up after the battle).

- [ ] **Step 5: Party changes**

With the window open, swap two party mons in the menu. The window should follow the new order on the next frame.

- [ ] **Step 6: DS ROM still builds**

Run: `./drun.sh make`
Expected: the ROM builds. The new files are port-only and must not be in the ARM build.

- [ ] **Step 7: Windows build**

Push the branch, `git pull` it in `/mnt/c/dev/pokeplatinum`, make sure the game is closed, and run the Windows build command from the Background section. Check `LOG` for `make: ***`. Then the user opens the window in their real save and compares it with the Pokétch.

- [ ] **Step 8: Merge**

Only after the user approves: merge the branch into `colin/main` and push.
