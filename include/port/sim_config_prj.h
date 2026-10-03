#ifndef SIM_CONFIG_PRJ_H
#define SIM_CONFIG_PRJ_H

#include <nitro/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// Walking speed of the player on foot, when not running
typedef enum {
    SIM_PLAYER_WALK_SPEED_NORMAL = 0, // 8 frames per tile, like the original game
    SIM_PLAYER_WALK_SPEED_1_14X,      // 7 frames per tile
    SIM_PLAYER_WALK_SPEED_1_33X,      // 6 frames per tile
    SIM_PLAYER_WALK_SPEED_COUNT
} SIM_PlayerWalkSpeed;

typedef struct {
    BOOL enable60fps; // When true, unlock the framerate to 60fps
    BOOL enable60fpsSpeedFix; // When enabled, fixes things in the game that run at double speed due to being at 60fps
	BOOL enableAsserts; //Should asserts cause a communication error?
    BOOL breakDebuggerOnGfAssert; // When true, break the debugger when a GF_ASSERT fails

	// Cheats section
	BOOL walkThroughWalls; // When enabled, allows the player to walk through walls
    BOOL disableRandomEncounters; // When enabled, all random encounters are turned off
    BOOL runFromTrainerBattles; // When enabled, allows running away from trainer battles
    int playerWalkSpeed; // A SIM_PlayerWalkSpeed value
} SIM_Config_prj_type;

void SIM_Config_prj_LoadDefaults(SIM_Config_prj_type * aConfig);
BOOL SIM_Config_prj_LoadConfigFile(SIM_Config_prj_type * aConfig);
void SIM_Config_prj_SaveConfigFile(SIM_Config_prj_type * aConfig);
void SIM_Config_prj_init();
SIM_Config_prj_type * SIM_Config_prj_GetConfig();

#ifdef __cplusplus
}
#endif

#endif