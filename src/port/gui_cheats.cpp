#include <nitro.h>
#include <simulator/sim.h>
#include <simulator/sim_gui.hpp>
#include <simulator/imgui/imgui.hpp>

#include "sim_gui_prj.hpp"

#include "port/sim_config_prj.h"

namespace SIM::GUI {

static void ConfigCheckBox(const char * label, bool * configVar, bool& configChanged) {
    if(ImGui::Checkbox(label, configVar)) {
        configChanged = true;
    }
}

void CheatsMain(bool * openState) {
    ImGui::Begin("Cheats", openState);
    SIM_Config_prj_type * myConfig = SIM_Config_prj_GetConfig();
    bool configChanged = false;

    ConfigCheckBox("Walk Through Walls", (bool*)&myConfig->walkThroughWalls, configChanged);
    ConfigCheckBox("Disable Random Encounters", (bool*)&myConfig->disableRandomEncounters, configChanged);
    ConfigCheckBox("Run from Trainer Battles", (bool*)&myConfig->runFromTrainerBattles, configChanged);

    // Order must match SIM_PlayerWalkSpeed
    const char * walkSpeedNames[SIM_PLAYER_WALK_SPEED_COUNT] = { "Normal", "1.14x", "1.33x" };
    if(ImGui::Combo("Walk Speed", &myConfig->playerWalkSpeed, walkSpeedNames, SIM_PLAYER_WALK_SPEED_COUNT)) {
        configChanged = true;
    }


    if(configChanged) {
        SIM_Config_prj_SaveConfigFile(myConfig);
    }


    ImGui::End();
}
}