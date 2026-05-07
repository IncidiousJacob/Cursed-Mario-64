#include "sm64ap.h"
#include "Archipelago.h"
#include "sm64ap_coinsanity.h"

extern "C" {
#include "game/print.h"
#include "gfx_dimensions.h"
#include "level_table.h"
#include "game/level_update.h"
#include "game/area.h"
#include "game/mario.h"
#include "game/object_list_processor.h"
#include "object_fields.h"
#include "object_constants.h"
#include "behavior_data.h"
#include "game/object_helpers.h"
#include "model_ids.h"
}

#include <string>
#include <vector>
#include <cmath>
#include <map>
#include <cstdio>
#include <bitset>
#include <set>
#include <queue>

#define WARP_NODE_CREDITS_MIN 0xF8 // level_update.c
#define NUM_PAINTING_LOCKS 15

// Set to false on some branch for compat with patches
static constexpr bool SM64AP_SUPPORT_MOVE_RANDO = true;

int starsCollected = 0;
bool sm64_locations[SM64AP_NUM_LOCS];
bool sm64_have_key1 = false;
bool sm64_have_key2 = false;
bool sm64_have_wingcap = false;
bool sm64_have_metalcap = false;
bool sm64_have_vanishcap = false;
bool sm64_have_toad_133 = false;
bool sm64_have_toad_134 = false;
bool sm64_have_toad_135 = false;
bool sm64_have_toad_076 = false;
bool sm64_have_toad_083 = false;
bool sm64_have_toad_137 = false;
bool sm64_have_toad_082 = false;
bool sm64_have_toad_136 = false;
bool sm64_have_bitdw_bowser = false;
bool sm64_have_bitfs_bowser = false;
bool sm64_have_bits_bowser = false;
bool sm64_have_bitdw_bombs = false;
bool sm64_have_bitfs_bombs = false;
bool sm64_have_bits_bombs = false;
int sm64_moat_state = 0;
bool sm64_have_cannon[15];
bool sm64_have_painting[NUM_PAINTING_LOCKS];
int sm64_completion_type = 0;
std::bitset<SM64AP_NUM_ABILITIES> sm64_have_abilities;
int *sm64_clockaction = nullptr;
int sm64_cost_firstbowserdoor = 8;
int sm64_cost_basementdoor = 30;
int sm64_cost_secondfloordoor = 50;
int sm64_cost_endlessstairs = 70;
int sm64_cost_mips1 = 15;
int sm64_cost_mips2 = 50;
int msg_frame_duration = 90; // 3 Secounds at 30F/s
int cur_msg_frame_duration = msg_frame_duration;
std::queue<int64_t> delayed_queue;
bool gRRTrapped = false;
u8 gRRTrapShowCutscene = 0;
bool gRRReturning = false;
s16 gRRReturnLevel = 0;
s16 gRRReturnArea = 0;
f32 gRRReturnPos[3] = { 0, 0, 0 };
f32 gRRReturnAngle = 0;
s32 gRRTrapTimer = 0;
static bool sm64_received_move_rando_high = false;
char gPlantDebugText[64];
s32 gPlantDebugTimer = 0;


#define SM64AP_COURSE_MIN 1
#define SM64AP_COURSE_MAX 15

static s16 sAPLevelCoins[SM64AP_COURSE_MAX + 1];

std::map<int, int> map_entrances;
std::set<int> course_dest_supported;

std::map<int, int> map_boxid_locid;

int sm64_exit_return_to;
int sm64_exit_orig_entrancelvl;

SM64AP_RGB8 gMarioHatShirtColor;
SM64AP_RGB8 gMarioSkinColor;
SM64AP_RGB8 gMarioHairColor;
SM64AP_RGB8 gMarioOverallsColor;
SM64AP_RGB8 gMarioShoesColor;
SM64AP_RGB8 gMarioGlovesColor;
SM64AP_RGB8 gStarColor;
SM64AP_RGB8 gToadBodyColor;
SM64AP_RGB8 gToadSpotColor;
SM64AP_RGB8 gToadSkinColor;
SM64AP_RGB8 gToadShoeColor;
SM64AP_RGB8 gGoombaColor;
SM64AP_RGB8 gPiranhaHeadColor;
SM64AP_RGB8 gPiranhaStemColor;
SM64AP_RGB8 gPiranhaLeafColor;
SM64AP_RGB8 gBowserBodyColor;
SM64AP_RGB8 gBobombColor;
SM64AP_RGB8 gBobombMetalColor;
SM64AP_RGB8 gPenguinBodyColor;
SM64AP_RGB8 gPenguinBellyColor;
SM64AP_RGB8 gPenguinBeakColor;
SM64AP_RGB8 gBooColor;
SM64AP_RGB8 gBowserFlameColor;
SM64AP_RGB8 gPeachColor050009F8;
SM64AP_RGB8 gPeachColor05000A10;
SM64AP_RGB8 gPeachColor05005FA0;
SM64AP_RGB8 gPeachColor05006138;
SM64AP_RGB8 gPeachColor05006150;
SM64AP_RGB8 gPeachColor05006A90;
SM64AP_RGB8 gFlyGuyPropellerColor;
SM64AP_RGB8 gFlyGuyFeetColor;
SM64AP_RGB8 gFlyGuyBodyColor;
SM64AP_RGB8 gFlyGuyFaceColor;
SM64AP_RGB8 gFlyGuyShadowColor;
SM64AP_RGB8 gSignPostColor;
SM64AP_RGB8 gSignBoardColor;


int sm64_ap_health_items_received = 0;

#define SM64AP_MIN_MAX_HEALTH 0x0300
#define SM64AP_FULL_MAX_HEALTH 0x0880

static s16 SM64AP_GetMaxHealth(void) {
    s16 maxHealth = SM64AP_MIN_MAX_HEALTH + (sm64_ap_health_items_received * 0x0100);

    if (maxHealth > SM64AP_FULL_MAX_HEALTH) {
        maxHealth = SM64AP_FULL_MAX_HEALTH;
    }

    return maxHealth;
}

void SM64AP_ApplyProgressiveHealth(void) {
    if (gMarioState == NULL) {
        return;
    }

    s16 maxHealth = SM64AP_GetMaxHealth();

    if (gMarioState->health > maxHealth || gMarioState->health == 0x0880) {
        gMarioState->health = maxHealth;
    }
}


static bool SM64AP_CanSpawnFieldItem(void) {
    if (gMarioObject == NULL || gMarioState == NULL || gCurrentArea == NULL) {
        return false;
    }

    // Don't spawn field items in hub / non-course maps
    switch (gCurrLevelNum) {
        case LEVEL_CASTLE:
        case LEVEL_CASTLE_GROUNDS:
        case LEVEL_CASTLE_COURTYARD:
            return false;
    }

    return true;
}

static void SM64AP_SpawnKoopaShellInFrontOfMario(void) {
    if (!SM64AP_CanSpawnFieldItem()) {
        return;
    }

    struct Object *shell =
        spawn_object_relative(0, 0, 60, 220, gMarioObject, MODEL_KOOPA_SHELL, bhvKoopaShell);

    if (shell != NULL) {
        shell->oForwardVel = 0.0f;
        shell->oVelY = 0.0f;
    }
}

static uint32_t sm64ap_splitmix32(uint32_t &x) {
    x += 0x9E3779B9u;
    uint32_t z = x;
    z ^= z >> 16;
    z *= 0x85EBCA6Bu;
    z ^= z >> 13;
    z *= 0xC2B2AE35u;
    z ^= z >> 16;
    return z;
}

static u8 sm64ap_palette_byte(uint32_t &x, int minv, int maxv) {
    return (u8)(minv + (sm64ap_splitmix32(x) % (maxv - minv + 1)));
}


// NEW SYSTEM (used instead of rolling x)
static uint32_t sm64ap_mix_seed(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

static u8 sm64ap_color_channel(uint32_t seed, uint32_t salt, int minv, int maxv) {
    uint32_t x = sm64ap_mix_seed(seed ^ salt);
    return (u8)(minv + (x % (maxv - minv + 1)));
}

static SM64AP_RGB8 sm64ap_make_color(uint32_t seed, uint32_t salt, int minv, int maxv) {
    SM64AP_RGB8 c;

    c.r = sm64ap_color_channel(seed, salt ^ 0xA1B2C3D4u, minv, maxv);
    c.g = sm64ap_color_channel(seed, salt ^ 0xB2C3D4E5u, minv, maxv);
    c.b = sm64ap_color_channel(seed, salt ^ 0xC3D4E5F6u, minv, maxv);

    return c;
}


// UPDATED PALETTE SEED FUNCTION
void SM64AP_SetMarioPaletteSeed(int seed) {
    uint32_t s = (uint32_t)(seed ? seed : 1);

    gMarioHatShirtColor  = sm64ap_make_color(s, 0x1001u, 80, 255);
    gMarioSkinColor      = sm64ap_make_color(s, 0x1002u, 80, 255);
    gMarioHairColor      = sm64ap_make_color(s, 0x1003u, 85, 255);
    gMarioOverallsColor  = sm64ap_make_color(s, 0x1004u, 64, 255);
    gMarioShoesColor     = sm64ap_make_color(s, 0x1005u, 70, 255);
    gMarioGlovesColor    = sm64ap_make_color(s, 0x1006u, 75, 255);
    gStarColor           = sm64ap_make_color(s, 0x1007u, 65, 255);

    gToadBodyColor       = sm64ap_make_color(s, 0x2001u, 48, 255);
    gToadSpotColor       = sm64ap_make_color(s, 0x2002u, 96, 255);
    gToadSkinColor       = sm64ap_make_color(s, 0x2003u, 80, 240);
    gToadShoeColor       = sm64ap_make_color(s, 0x2004u, 16, 180);

    gGoombaColor         = sm64ap_make_color(s, 0x3001u, 96, 255);

    gPiranhaHeadColor    = sm64ap_make_color(s, 0x4001u, 60, 240);
    gPiranhaStemColor    = sm64ap_make_color(s, 0x4002u, 75, 245);
    gPiranhaLeafColor    = sm64ap_make_color(s, 0x4003u, 85, 255);

    gBowserBodyColor     = sm64ap_make_color(s, 0x5001u, 64, 220);
    gBowserFlameColor    = sm64ap_make_color(s, 0x5002u, 75, 255);

    gBobombColor         = sm64ap_make_color(s, 0x6001u, 96, 255);
    gBobombMetalColor    = sm64ap_make_color(s, 0x6002u, 32, 200);

    gPenguinBodyColor    = sm64ap_make_color(s, 0x7001u, 64, 255);
    gPenguinBellyColor   = sm64ap_make_color(s, 0x7002u, 96, 255);
    gPenguinBeakColor    = sm64ap_make_color(s, 0x7003u, 80, 255);

    gBooColor            = sm64ap_make_color(s, 0x8001u, 75, 255);

    gPeachColor050009F8  = sm64ap_make_color(s, 0x9001u, 80, 255);
    gPeachColor05000A10  = sm64ap_make_color(s, 0x9002u, 80, 255);
    gPeachColor05005FA0  = sm64ap_make_color(s, 0x9003u, 80, 255);
    gPeachColor05006138  = sm64ap_make_color(s, 0x9004u, 80, 255);
    gPeachColor05006150  = sm64ap_make_color(s, 0x9005u, 80, 255);
    gPeachColor05006A90  = sm64ap_make_color(s, 0x9006u, 80, 255);

    gFlyGuyPropellerColor = sm64ap_make_color(s, 0xA001u, 80, 255);
    gFlyGuyFeetColor      = sm64ap_make_color(s, 0xA002u, 80, 255);
    gFlyGuyBodyColor      = sm64ap_make_color(s, 0xA003u, 80, 255);
    gFlyGuyFaceColor      = sm64ap_make_color(s, 0xA004u, 90, 255);
    gFlyGuyShadowColor    = sm64ap_make_color(s, 0xA005u, 40, 255);

    gSignPostColor  = sm64ap_make_color(s, 0xB001u, 48, 220);
    gSignBoardColor = sm64ap_make_color(s, 0xB002u, 64, 255);

    SM64AP_ApplyMarioPalette();
    SM64AP_ApplyStarPalette();
    SM64AP_ApplyToadPalette();
    SM64AP_ApplyGoombaPalette();
    SM64AP_ApplyPiranhaPalette();
    SM64AP_ApplyBowserPalette();
    SM64AP_ApplyBobombPalette();
    SM64AP_ApplyPenguinPalette();
    SM64AP_ApplyBooPalette();
    SM64AP_ApplyBowserFlamePalette();
    SM64AP_ApplyPeachPalette();
    SM64AP_ApplyFlyGuyPalette();
    SM64AP_ApplySignPalette();
}


void SM64AP_CheckWFPiranhaPlant(struct Object *o) {
    if (o == NULL || gCurrLevelNum != LEVEL_WF) {
        return;
    }

    int hX = (int) roundf(o->oHomeX);
    int hY = (int) roundf(o->oHomeY);
    int hZ = (int) roundf(o->oHomeZ);

    if (hX == 4625 && hY == 256 && hZ == 5017) {
        if (!SM64AP_CheckedLoc(2400)) {
            SM64AP_SendItem(2400);
        }
    } else if (hX == 1822 && hY == 2560 && hZ == -101) {
        if (!SM64AP_CheckedLoc(2401)) {
            SM64AP_SendItem(2401);
        }
    } else if (hX == 689 && hY == 2560 && hZ == 1845) {
        if (!SM64AP_CheckedLoc(2402)) {
            SM64AP_SendItem(2402);
        }
    }
}

void SM64AP_CheckCCMSpindrift(struct Object *o) {
    if (o == NULL || gCurrLevelNum != LEVEL_CCM) {
        return;
    }

    int hX = (int) roundf(o->oHomeX);
    int hZ = (int) roundf(o->oHomeZ);

    if (hX == 2542 && hZ == -1714) {
        if (!SM64AP_CheckedLoc(3626403)) SM64AP_SendItem(3626403);
    } else if (hX == -6090 && hZ == 1936) {
        if (!SM64AP_CheckedLoc(3626404)) SM64AP_SendItem(3626404);
    } else if (hX == 4346 && hZ == 400) {
        if (!SM64AP_CheckedLoc(3626405)) SM64AP_SendItem(3626405);
    } else if (hX == -5054 && hZ == -1054) {
        if (!SM64AP_CheckedLoc(3626406)) SM64AP_SendItem(3626406);
    } else if (hX == -5033 && hZ == -2666) {
        if (!SM64AP_CheckedLoc(3626407)) SM64AP_SendItem(3626407);
    } else if (hX == -488 && hZ == -2305) {
        if (!SM64AP_CheckedLoc(3626408)) SM64AP_SendItem(3626408);
    } else if (hX == -1768 && hZ == -1793) {
        if (!SM64AP_CheckedLoc(3626409)) SM64AP_SendItem(3626409);
    }
}

void SM64AP_Boosanity(struct Object *o) {
    int64_t loc_id = 0;

    if (o == NULL) {
        return;
    }

    int hX = (int) roundf(o->oHomeX);
    int hZ = (int) roundf(o->oHomeZ);

    if (gCurrLevelNum == LEVEL_BBH) {
        // Ghost Hunt Boos (5 total)
        if (o->behavior == bhvGhostHuntBoo) {
            if (hX == 20 && hZ == -908) loc_id = 2500;
            else if (hX == 3150 && hZ == 398) loc_id = 2501;
            else if (hX == -2000 && hZ == -800) loc_id = 2502;
            else if (hX == 2851 && hZ == 2289) loc_id = 2503;
            else if (hX == -1551 && hZ == -1018) loc_id = 2504;
        }
        // Lone Boo
        else if (o->behavior == bhvBoo) {
            if (hX == 581 && hZ == -206) loc_id = 2505;
        }
        // Merry-Go-Round small boos (5 total)
        else if (o->behavior == bhvMerryGoRoundBoo
              && obj_has_behavior(o->parentObj, bhvMerryGoRoundBooManager)) {
            loc_id = 2506 + o->parentObj->oMerryGoRoundBooManagerNumBoosKilled;
        }
    }
    else if (gCurrLevelNum == LEVEL_CASTLE_COURTYARD) {
        // Courtyard boos (9 total)
        if (o->behavior == bhvGhostHuntBoo) {
            if (hX == -3217 && hZ == -101) loc_id = 2511;
            else if (hX == -3007 && hZ == 109) loc_id = 2512;
            else if (hX == -3427 && hZ == -311) loc_id = 2513;

            else if (hX == 3317 && hZ == -1701) loc_id = 2514;
            else if (hX == 3527 && hZ == -1491) loc_id = 2515;
            else if (hX == 3107 && hZ == -1911) loc_id = 2516;

            else if (hX == -71 && hZ == -1387) loc_id = 2517;
            else if (hX == 139 && hZ == -1177) loc_id = 2518;
            else if (hX == -281 && hZ == -1597) loc_id = 2519;
        }
    }

    if (loc_id != 0 && !SM64AP_CheckedLoc(loc_id)) {
        SM64AP_SendItem(loc_id);
    }
}

void SM64AP_Scuttlesanity(struct Object *o) {
    int64_t loc_id = 0;

    if (o == NULL || gCurrLevelNum != LEVEL_BBH)
        return;

    switch (o->oBehParams2ndByte) {
        case 1:
            loc_id = 2600;
            break;
        case 2:
            loc_id = 2601;
            break;
        case 3:
            loc_id = 2602;
            break;
    }

    if (loc_id != 0 && !SM64AP_CheckedLoc(loc_id)) {
        SM64AP_SendItem(loc_id);
    }
}

typedef struct {
    int level;
    int x;
    int y;
    int z;
    int range;
    int locId;
    int itemId;
} SM64APLockedCoin;

static SM64APLockedCoin sLockedCoins[] = {
    // level,        x,    y,    z,    range, locId, itemId
    { LEVEL_CASTLE, -724, 388, -324, 50,   6000,  6000 },
    { LEVEL_CASTLE, -618, 388, -324, 50,   6001,  6001 },
    { LEVEL_CASTLE, -1430, 388, -324, 50,   6002,  6002 },
    { LEVEL_CASTLE, -1324, 388, -324, 50,   6003,  6003 },
    { LEVEL_BOB, -449, 0, 5668, 50,   6004,  6004 },
    { LEVEL_BOB, -289, 0, 5668, 50,   6005,  6005 },
    { LEVEL_BOB, -129, 0, 5668, 50,   6006,  6006 },
    { LEVEL_BOB, 31, 0, 5668, 50,   6007,  6007 },
    { LEVEL_BOB, 191, 0, 5668, 50,   6008,  6008 },
    { LEVEL_BOB, -6060, 1024, -5040, 50,   6009,  6009 },
    { LEVEL_BOB, -6272, 1024, -5128, 50,   6010,  6010 },
    { LEVEL_BOB, -6360, 1024, -5340, 50,   6011,  6011 },
    { LEVEL_BOB, -6060, 1024, -5640, 50,   6012,  6012 },
    { LEVEL_BOB, -5848, 1024, -5552, 50,   6013,  6013 },
    { LEVEL_BOB, -5760, 1024, -5340, 50,   6014,  6014 },
    { LEVEL_BOB, -5848, 1024, -5124, 50,   6015,  6015 },
    { LEVEL_BOB, -6272, 1024, -5552, 50,   6016,  6016 },
    { LEVEL_BOB, 1544, 2917, -2326, 50,   6017,  6017 },
    { LEVEL_BOB, 1697, 2917, -2280, 50,   6018,  6018 },
    { LEVEL_BOB, 1851, 2917, -2234, 50,   6019,  6019 },
    { LEVEL_BOB, 2004, 2917, -2187, 50,   6020,  6020 },
    { LEVEL_BOB, 2157, 2917, -2141, 50,   6021,  6021 },
    { LEVEL_BOB, 4553, 3075, -2556, 50,   6022,  6022 },
    { LEVEL_BOB, 4694, 3077, -2631, 50,   6023,  6023 },
    { LEVEL_BOB, 4836, 3079, -2707, 50,   6024,  6024 },
    { LEVEL_BOB, 4977, 3081, -2782, 50,   6025,  6025 },
    { LEVEL_BOB, 5118, 3083, -2857, 50,   6026,  6026 },
    { LEVEL_WF, 4611, 256, 441, 50,   6027,  6027 },
    { LEVEL_WF, 4823, 256, 353, 50,   6028,  6028 },
    { LEVEL_WF, 4911, 256, 141, 50,   6029,  6029 },
    { LEVEL_WF, 4823, 256, -71, 50,   6030,  6030 },
    { LEVEL_WF, 4611, 256, -159, 50,   6031,  6031 },
    { LEVEL_WF, 4399, 256, -71, 50,   6032,  6032 },
    { LEVEL_WF, 4311, 256, 141, 50,   6033,  6033 },
    { LEVEL_WF, 4399, 256, 353, 50,   6034,  6034 },
    { LEVEL_WF, 3760, 706, 3060, 50,   6035,  6035 },
    { LEVEL_WF, 3760, 749, 2900, 50,   6036,  6036 },
    { LEVEL_WF, 3760, 790, 2740, 50,   6037,  6037 },
    { LEVEL_WF, 3760, 823, 2580, 50,   6038,  6038 },
    { LEVEL_WF, 3760, 856, 2420, 50,   6039,  6039 },
    { LEVEL_WF, 3396, 1036, 3600, 50,   6040,  6040 },
    { LEVEL_WF, 3396, 1092, 3440, 50,   6041,  6041 },
    { LEVEL_WF, 3396, 1149, 3280, 50,   6042,  6042 },
    { LEVEL_WF, 3396, 1205, 3120, 50,   6043,  6043 },
    { LEVEL_WF, 3396, 1261, 2960, 50,   6044,  6044 },
    { LEVEL_WF, 1770, 922, 2541, 50,   6045,  6045 },
    { LEVEL_WF, 1858, 922, 2329, 50,   6046,  6046 },
    { LEVEL_WF, 1770, 922, 2117, 50,   6047,  6047 },
    { LEVEL_WF, 1558, 922, 2029, 50,   6048,  6048 },
    { LEVEL_WF, 1346, 922, 2117, 50,   6049,  6049 },
    { LEVEL_WF, 1258, 922, 2329, 50,   6050,  6050 },
    { LEVEL_WF, 1346, 922, 2541, 50,   6051,  6051 },
    { LEVEL_WF, 1558, 922, 2629, 50,   6052,  6052 },
    { LEVEL_WF, -1080, 1024, 3900, 50,   6053,  6053 },
    { LEVEL_WF, -1240, 1024, 3900, 50,   6054,  6054 },
    { LEVEL_WF, -1400, 1024, 3900, 50,   6055,  6055 },
    { LEVEL_WF, -1560, 1024, 3900, 50,   6056,  6056 },
    { LEVEL_WF, -1720, 1024, 3900, 50,   6057,  6057 },
    { LEVEL_WF, -2288, 1792, -48, 50,   6058,  6058 },
    { LEVEL_WF, -2500, 1792, 40, 50,   6059,  6059 },
    { LEVEL_WF, -2712, 1792, -48, 50,   6060,  6060 },
    { LEVEL_WF, -2800, 1792, -260, 50,   6061,  6061 },
    { LEVEL_WF, -2712, 1792, -472, 50,   6062,  6062 },
    { LEVEL_WF, -2500, 1792, -560, 50,   6063,  6063 },
    { LEVEL_WF, -2288, 1792, -472, 50,   6064,  6064 },
    { LEVEL_WF, -2200, 1792, -260, 50,   6065,  6065 },
    { LEVEL_WF, 1574, 2586, 2299, 50,   6066,  6066 },
    { LEVEL_WF, 1414, 2586, 2299, 50,   6067,  6067 },
    { LEVEL_WF, 1254, 2586, 2299, 50,   6068,  6068 },
    { LEVEL_WF, 1094, 2586, 2299, 50,   6069,  6069 },
    { LEVEL_WF, 934, 2586, 2299, 50,   6070,  6070 },
    { LEVEL_WF, -750, 2650, 2800, 50,   6071,  6071 },
    { LEVEL_WF, -500, 2650, 2900, 50,   6072,  6072 },
    { LEVEL_WF, 0, 2650, 2900, 50,   6073,  6073 },
    { LEVEL_WF, 250, 2650, 2800, 50,   6074,  6074 },
    { LEVEL_WF, 3234, 3345, -2087, 50,   6075,  6075 },
    { LEVEL_WF, 3022, 3345, -1999, 50,   6076,  6076 },
    { LEVEL_WF, 2934, 3345, -1787, 50,   6077,  6077 },
    { LEVEL_WF, 3022, 3345, -1575, 50,   6078,  6078 },
    { LEVEL_WF, 3234, 3345, -1487, 50,   6079,  6079 },
    { LEVEL_WF, 3446, 3345, -1575, 50,   6080,  6080 },
    { LEVEL_WF, 3534, 3345, -1787, 50,   6081,  6081 },
    { LEVEL_WF, 3446, 3345, -1999, 50,   6082,  6082 },
    { LEVEL_WF, 1108, 3584, -2502, 50,   6083,  6083 },
    { LEVEL_WF, 1179, 3584, -2573, 50,   6084,  6084 },
    { LEVEL_WF, 1250, 3584, -2644, 50,   6085,  6085 },
    { LEVEL_WF, 1321, 3584, -2573, 50,   6086,  6086 },
    { LEVEL_WF, 1321, 3584, -2644, 50,   6087,  6087 },
    { LEVEL_WF, 1179, 3584, -2715, 50,   6088,  6088 },
    { LEVEL_WF, 1250, 3584, -2715, 50,   6089,  6089 },
    { LEVEL_WF, 1321, 3584, -2715, 50,   6090,  6090 },
    { LEVEL_JRB, -1992, -650, 3988, 50,   6091,  6091 },
    { LEVEL_JRB, -1780, -650, 3900, 50,   6092,  6092 },
    { LEVEL_JRB, -1568, -650, 3988, 50,   6093,  6093 },
    { LEVEL_JRB, -1480, -650, 4200, 50,   6094,  6094 },
    { LEVEL_JRB, -1568, -650, 4412, 50,   6095,  6095 },
    { LEVEL_JRB, -1780, -650, 4500, 50,   6096,  6096 },
    { LEVEL_JRB, -1992, -650, 4412, 50,   6097,  6097 },
    { LEVEL_JRB, -2080, -650, 4200, 50,   6098,  6098 },
    { LEVEL_JRB, 51, 960, 3044, 50,   6099,  6099 },
    { LEVEL_JRB, -161, 960, 2956, 50,   6100,  6100 },
    { LEVEL_JRB, -249, 960, 2744, 50,   6101,  6101 },
    { LEVEL_JRB, -161, 960, 2532, 50,   6102,  6102 },
    { LEVEL_JRB, 51, 960, 2444, 50,   6103,  6103 },
    { LEVEL_JRB, 263, 960, 2532, 50,   6104,  6104 },
    { LEVEL_JRB, 351, 960, 2744, 50,   6105,  6105 },
    { LEVEL_JRB, 263, 960, 2956, 50,   6106,  6106 },
    { LEVEL_JRB, 255, 1160, 7633, 50,   6107,  6107 },
    { LEVEL_JRB, 255, 1288, 7633, 50,   6108,  6108 },
    { LEVEL_JRB, 255, 1416, 7633, 50,   6109,  6109 },
    { LEVEL_JRB, 255, 1544, 7633, 50,   6110,  6110 },
    { LEVEL_JRB, 255, 1672, 7633, 50,   6111,  6111 },
    { LEVEL_JRB, 2973, 1536, 6493, 50,   6112,  6112 },
    { LEVEL_JRB, 3086, 1536, 6606, 50,   6113,  6113 },
    { LEVEL_JRB, 3200, 1536, 6720, 50,   6114,  6114 },
    { LEVEL_JRB, 3313, 1536, 6833, 50,   6115,  6115 },
    { LEVEL_JRB, 3426, 1536, 6946, 50,   6116,  6116 },
    { LEVEL_JRB, 3506, 1536, 6166, 50,   6117,  6117 },
    { LEVEL_JRB, 3393, 1536, 6053, 50,   6118,  6118 },
    { LEVEL_JRB, 3280, 1536, 5940, 50,   6119,  6119 },
    { LEVEL_JRB, 3166, 1536, 5826, 50,   6120,  6120 },
    { LEVEL_JRB, 3053, 1536, 5713, 50,   6121,  6121 },
    { LEVEL_JRB, 3993, 1536, 5473, 50,   6122,  6122 },
    { LEVEL_JRB, 4106, 1536, 5586, 50,   6123,  6123 },
    { LEVEL_JRB, 4220, 1536, 5700, 50,   6124,  6124 },
    { LEVEL_JRB, 4333, 1536, 5813, 50,   6125,  6125 },
    { LEVEL_JRB, 4446, 1536, 5926, 50,   6126,  6126 },
    { LEVEL_JRB, 5060, -4420, 720, 50,   6127,  6127 },
    { LEVEL_JRB, 5201, -4362, 720, 50,   6128,  6128 },
    { LEVEL_JRB, 5260, -4220, 720, 50,   6129,  6129 },
    { LEVEL_JRB, 5201, -4079, 720, 50,   6130,  6130 },
    { LEVEL_JRB, 5060, -4020, 720, 50,   6131,  6131 },
    { LEVEL_JRB, 4919, -4079, 720, 50,   6132,  6132 },
    { LEVEL_JRB, 4860, -4220, 720, 50,   6133,  6133 },
    { LEVEL_JRB, 4919, -4362, 720, 50,   6134,  6134 },
    { LEVEL_JRB, -2228, -2966, -3908, 50,   6135,  6135 },
    { LEVEL_JRB, -2440, -2966, -3820, 50,   6136,  6136 },
    { LEVEL_JRB, -2652, -2966, -3908, 50,   6137,  6137 },
    { LEVEL_JRB, -2740, -2966, -4120, 50,   6138,  6138 },
    { LEVEL_JRB, -2652, -2966, -4332, 50,   6139,  6139 },
    { LEVEL_JRB, -2440, -2966, -4420, 50,   6140,  6140 },
    { LEVEL_JRB, -2228, -2966, -4332, 50,   6141,  6141 },
    { LEVEL_JRB, -2140, -2966, -4120, 50,   6142,  6142 },
    { LEVEL_PSS, 3320, 6144, -5640, 50,   6143,  6143 },
    { LEVEL_PSS, 3160, 6144, -5640, 50,   6144,  6144 },
    { LEVEL_PSS, 3000, 6127, -5640, 50,   6145,  6145 },
    { LEVEL_PSS, 2840, 6090, -5640, 50,   6146,  6146 },
    { LEVEL_PSS, 2680, 6053, -5640, 50,   6147,  6147 },
    { LEVEL_PSS, -3233, 4688, -5600, 50,   6148,  6148 },
    { LEVEL_PSS, -5636, 4331, -5054, 50,   6149,  6149 },
    { LEVEL_PSS, -5980, 3453, 945, 50,   6150,  6150 },
    { LEVEL_PSS, -2870, 2837, 2300, 50,   6151,  6151 },
    { LEVEL_PSS, -1200, 2771, 2280, 50,   6152,  6152 },
    { LEVEL_PSS, -1040, 2767, 2280, 50,   6153,  6153 },
    { LEVEL_PSS, -880, 2724, 2280, 50,   6154,  6154 },
    { LEVEL_PSS, -720, 2639, 2280, 50,   6155,  6155 },
    { LEVEL_PSS, -560, 2540, 2280, 50,   6156,  6156 },
    { LEVEL_PSS, -60, 2230, 2280, 50,   6157,  6157 },
    { LEVEL_PSS, 100, 2130, 2280, 50,   6158,  6158 },
    { LEVEL_PSS, 260, 2031, 2280, 50,   6159,  6159 },
    { LEVEL_PSS, 420, 1932, 2280, 50,   6160,  6160 },
    { LEVEL_PSS, 580, 1833, 2280, 50,   6161,  6161 },
    { LEVEL_PSS, 3930, 274, 2425, 50,   6162,  6162 },
    { LEVEL_PSS, 5174, 61, 3200, 50,   6163,  6163 },
    { LEVEL_PSS, 5707, -128, 4565, 50,   6164,  6164 },
    { LEVEL_PSS, 5194, -310, 5845, 50,   6165,  6165 },
    { LEVEL_PSS, 3640, -543, 6450, 50,   6166,  6166 },
    { LEVEL_PSS, 2550, -700, 6050, 50,   6167,  6167 },
    { LEVEL_PSS, 1821, -921, 4616, 50,   6168,  6168 },
    { LEVEL_PSS, 1860, -1203, 2000, 50,   6169,  6169 },
    { LEVEL_PSS, 1860, -1223, 1840, 50,   6170,  6170 },
    { LEVEL_PSS, 1860, -1223, 1680, 50,   6171,  6171 },
    { LEVEL_PSS, 1860, -1224, 1520, 50,   6172,  6172 },
    { LEVEL_PSS, 1860, -1225, 1360, 50,   6173,  6173 },
    { LEVEL_PSS, 1880, -1481, -260, 50,   6174,  6174 },
    { LEVEL_PSS, 1880, -1480, -100, 50,   6175,  6175 },
    { LEVEL_PSS, 1880, -1479, 60, 50,   6176,  6176 },
    { LEVEL_PSS, 1880, -1477, 220, 50,   6177,  6177 },
    { LEVEL_PSS, 1880, -1426, 380, 50,   6178,  6178 },
    { LEVEL_PSS, 1860, -1774, -1400, 50,   6179,  6179 },
    { LEVEL_PSS, 1860, -1820, -1560, 50,   6180,  6180 },
    { LEVEL_PSS, 1860, -1840, -1720, 50,   6181,  6181 },
    { LEVEL_PSS, 1860, -1852, -1880, 50,   6182,  6182 },
    { LEVEL_PSS, 1860, -1868, -2040, 50,   6183,  6183 },
    { LEVEL_PSS, 1854, -2132, -4290, 50,   6184,  6184 },
    { LEVEL_PSS, -20, -2542, -6304, 50,   6185,  6185 },
    { LEVEL_PSS, -1197, -2929, -4692, 50,   6186,  6186 },
    { LEVEL_PSS, -2565, -3268, -3525, 50,   6187,  6187 },
    { LEVEL_PSS, -4909, -3633, -4218, 50,   6188,  6188 },
    { LEVEL_PSS, -6290, -3937, -2545, 50,   6189,  6189 },
    { LEVEL_PSS, -6400, -4146, -590, 50,   6190,  6190 },
    { LEVEL_PSS, -6400, -4335, 1409, 50,   6191,  6191 },
    { LEVEL_PSS, -6400, -4530, 3481, 50,   6192,  6192 },
    
    
    

    // Add future locked coins here:\\
    // { LEVEL_BOB, 1234, 200, -900, 150, 6001, 6001 },
};

static int SM64AP_MatchesLockedCoin(struct Object *o) {
    if (o == NULL) {
        return -1;
    }

    for (int i = 0; i < (int)(sizeof(sLockedCoins) / sizeof(sLockedCoins[0])); i++) {
        SM64APLockedCoin *coin = &sLockedCoins[i];

        if (gCurrLevelNum != coin->level) {
            continue;
        }

        int dx = (int)roundf(o->oPosX) - coin->x;
        int dy = (int)roundf(o->oPosY) - coin->y;
        int dz = (int)roundf(o->oPosZ) - coin->z;

        if (dx > -coin->range && dx < coin->range &&
            dy > -coin->range && dy < coin->range &&
            dz > -coin->range && dz < coin->range) {
            return i;
        }
    }

    return -1;
}

bool SM64AP_CanCollectLockedCoin(struct Object *o) {
    int coinIndex = SM64AP_MatchesLockedCoin(o);

    if (coinIndex < 0) {
        return true;
    }

    int locId = sLockedCoins[coinIndex].locId;
    int itemId = sLockedCoins[coinIndex].itemId;

    if (SM64AP_CoinsanityCheckedLoc(locId)) {
        return false;
    }

    if (!SM64AP_HaveCoinsanityReturnItemFlag(itemId)) {
        return false;
    }

    return true;
}

int SM64AP_GetLockedCoinLoc(struct Object *o) {
    int coinIndex = SM64AP_MatchesLockedCoin(o);

    if (coinIndex < 0) {
        return 0;
    }

    return sLockedCoins[coinIndex].locId;
}

void SM64AP_CheckLockedCoin(struct Object *o) {
    int locId = SM64AP_GetLockedCoinLoc(o);

    if (locId != 0 && !SM64AP_CoinsanityCheckedLoc(locId)) {
        SM64AP_SendItem(locId);
        SM64AP_CoinsanitySetCheckedLoc(locId);
    }
}

void SM64AP_AddCoinsToCourse(s16 course, s16 amount) {
    if (gMarioState == NULL) {
        return;
    }

    if (gCurrCourseNum != course) {
        return;
    }

    gMarioState->numCoins += amount;

    if (gMarioState->numCoins > 999) {
        gMarioState->numCoins = 999;
    }
}

void SM64AP_HandleLevelCoinItem(s32 itemId) {
    if (itemId < 3627010 || itemId > 3627154) {
        return;
    }

    s16 coinType = itemId % 10;

    if (coinType > 4) {
        return;
    }

    s16 course = (itemId - 3627000) / 10;
    s16 amount = 0;

    switch (coinType) {
        case 0: amount = 1; break;
        case 1: amount = 2; break;
        case 2: amount = 5; break;
        case 3: amount = 10; break;
        case 4: amount = 20; break;
    }

    sAPLevelCoins[course] += amount;

    if (sAPLevelCoins[course] > 999) {
        sAPLevelCoins[course] = 999;
    }

    if (gCurrCourseNum == course && gMarioState != NULL) {
        gMarioState->numCoins = sAPLevelCoins[course];
        gHudDisplay.coins = gMarioState->numCoins;
    }
}

void SM64AP_UpdateLevelCoinCount(void) {
    s16 course = gCurrCourseNum;

    if (gMarioState == NULL) {
        return;
    }

    if (course < SM64AP_COURSE_MIN || course > SM64AP_COURSE_MAX) {
        gMarioState->numCoins = 0;
        gHudDisplay.coins = 0;
        return;
    }

    gMarioState->numCoins = sAPLevelCoins[course];
    gHudDisplay.coins = gMarioState->numCoins;
}

  void SM64AP_RecvItem(int64_t idx, bool notify) {
    if (idx == SM64AP_ID_DEATH_TRAP) {
        if (gMarioState != NULL) {
            gMarioState->health = 0;
        }
        return;
    }

    if (idx >= 3627010 && idx <= 3627154) {
        SM64AP_HandleLevelCoinItem((s32)idx);
        return;
    }

    if (idx >= SM64AP_ID_1_HEALTH_PIP && idx < SM64AP_ID_RR_TRAP) {
        sm64_ap_health_items_received++;
        SM64AP_ApplyProgressiveHealth();
    }

    if (SM64AP_SetCoinsanityReturnItemFlag(idx)) {
        return;
    }    

    if (idx == SM64AP_ID_PUNCH) {
        sm64_have_abilities[11] = true;
        return;

    } else if (idx == SM64AP_ID_GRAB) {
        sm64_have_abilities[12] = true;
        return;

    } else if (idx == SM64AP_ID_SWIM) {
        sm64_have_abilities[13] = true;
        return;

    } else if (idx >= SM64AP_ID_CANNONUNLOCK(0) && idx <= SM64AP_ID_CANNONUNLOCK(15 - 1)) {
        sm64_have_cannon[idx - SM64AP_ID_CANNONUNLOCK(0)] = true;
        return;

    } else if (idx >= SM64AP_ID_PAINTINGUNLOCK(0)
        && idx <= SM64AP_ID_PAINTINGUNLOCK(NUM_PAINTING_LOCKS - 1)) {
        switch (idx) {
            case SM64AP_ID_PAINTINGUNLOCK(1):  // WF
                sm64_have_painting[1] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(2):  // KRB / JRB
                sm64_have_painting[2] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(3):  // CCM
                sm64_have_painting[3] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(6):  // LLL
                sm64_have_painting[6] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(7):  // SSL
                sm64_have_painting[7] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(8):  // DDD
                sm64_have_painting[8] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(9):  // SL
                sm64_have_painting[9] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(10): // WDW
                sm64_have_painting[10] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(11): // TTM
                sm64_have_painting[11] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(12): // THI
                sm64_have_painting[12] = true;
                break;
            case SM64AP_ID_PAINTINGUNLOCK(13): // TTC
                sm64_have_painting[13] = true;
                break;
        }
        return;

    } else if (idx >= SM64AP_ID_ABILITY(0) && idx <= SM64AP_ID_ABILITY(SM64AP_NUM_ABILITIES - 1)) {
        int slot = idx - SM64AP_ABILITY_OFFSET;
        sm64_have_abilities[slot] = true;
        return;

    } else if (idx == SM64AP_ID_KOOPA_SHELL) {
        if (notify || !SM64AP_CanSpawnFieldItem()) {
            delayed_queue.push(idx);
        } else {
            SM64AP_SpawnKoopaShellInFrontOfMario();
        }
        return;

    } else if (idx >= SM64AP_ID_1_HEALTH_PIP && idx <= SM64AP_ID_RR_TRAP) {
        if (notify) {
            if (idx == SM64AP_ID_RR_TRAP) {
                gRRTrapTimer = 4 * 60 * 30;
            }
            delayed_queue.push(idx);
        }
        return;

    } else {
        switch (idx) {
            case SM64AP_ITEMID_STAR:
                starsCollected++;
                break;

            case SM64AP_ID_KEY1:
                sm64_have_key1 = true;
                break;

            case SM64AP_ID_KEY2:
                sm64_have_key2 = true;
                break;

            case SM64AP_ID_KEYPROG:
                sm64_have_key2 = sm64_have_key1;
                sm64_have_key1 = true;
                break;

            case SM64AP_ID_WINGCAP:
                sm64_have_wingcap = true;
                break;

            case SM64AP_ID_METALCAP:
                sm64_have_metalcap = true;
                break;

            case SM64AP_ID_VANISHCAP:
                sm64_have_vanishcap = true;
                break;

            case SM64AP_ITEMID_1UP:
                gMarioState->numLives++;
                break;

            case SM64AP_ID_TOAD_133_UNLOCK:
                sm64_have_toad_133 = true;
                break;

            case SM64AP_ID_TOAD_134_UNLOCK:
                sm64_have_toad_134 = true;
                break;

            case SM64AP_ID_TOAD_135_UNLOCK:
                sm64_have_toad_135 = true;
                break;

            case SM64AP_ID_TOAD_076_UNLOCK:
                sm64_have_toad_076 = true;
                break;

            case SM64AP_ID_TOAD_083_UNLOCK:
                sm64_have_toad_083 = true;
                break;

            case SM64AP_ID_TOAD_137_UNLOCK:
                sm64_have_toad_137 = true;
                break;

            case SM64AP_ID_TOAD_082_UNLOCK:
                sm64_have_toad_082 = true;
                break;

            case SM64AP_ID_TOAD_136_UNLOCK:
                sm64_have_toad_136 = true;
                break;

            case SM64AP_ID_BSBITDW_UNLOCK:
                sm64_have_bitdw_bowser = true;
                break;

            case SM64AP_ID_BSBITFS_UNLOCK:
                sm64_have_bitfs_bowser = true;
                break;

            case SM64AP_ID_BSBITS_UNLOCK:
                sm64_have_bits_bowser = true;
                break;

            case SM64AP_ID_BBBITDW_UNLOCK:
                sm64_have_bitdw_bombs = true;
                break;

            case SM64AP_ID_BBBITFS_UNLOCK:
                sm64_have_bitfs_bombs = true;
                break;

            case SM64AP_ID_BBBITS_UNLOCK:
                sm64_have_bits_bombs = true;
                break;
        }
    }
}
void SM64AP_CheckLocation(int64_t loc_id) {
    if (loc_id < SM64AP_ID_OFFSET) {
        return;
    }

    int index = loc_id - SM64AP_ID_OFFSET;

    if (index < 0 || index >= SM64AP_NUM_LOCS) {
        return;
    }

    sm64_locations[index] = true;
}

u32 SM64AP_CourseStarFlags(s32 courseIdx) {
    u32 starflags = 0;
    s32 courseIndex = courseIdx;
    if (courseIdx == -1) {
        courseIndex = 24;
    }
    for (int i = 0; i < 7; i++) {
        if (sm64_locations[i + (courseIndex * 7)]) {
            starflags |= (1 << i);
        }
    }
    return starflags;
}

void setCourseNodeAndArea(int coursenum, s16 *oldnode, bool isDeathWarp, int warpOp) {
    switch (coursenum) {
        case LEVEL_BOB:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_CCM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_WF:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_JRB:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_BBH:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x0B : 0x0A;
            return;
        case LEVEL_LLL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_SSL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_HMC:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_DDD:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_WDW:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x64 : 0x32;
            return;
        case LEVEL_THI:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x65 : 0x33;
            return;
        case LEVEL_TTM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x66 : 0x34;
            return;
        case LEVEL_TTC:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x67 : 0x35;
            return;
        case LEVEL_SL:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x68 : 0x36;
            return;
        case LEVEL_RR:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x6C : 0x3A;
            return;
        case LEVEL_PSS:
        case LEVEL_TOTWC:
            *oldnode = isDeathWarp ? 0x21 : (warpOp == WARP_OP_STAR_EXIT ? 0x26 : 0x20);
            return;
        case LEVEL_SA:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x28 : 0x27;
            return;
        case LEVEL_BITDW:
        case LEVEL_BOWSER_1:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x25 : 0x24;
            return;
        case LEVEL_VCUTM:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x06 : 0x07;
            return;
        case LEVEL_BITFS:
        case LEVEL_BOWSER_2:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x68 : 0x36;
            return;
        case LEVEL_WMOTR:
            *oldnode = (isDeathWarp || warpOp != WARP_OP_STAR_EXIT) ? 0x6D : 0x38;
        default:
            return;
    }
}

void SM64AP_RedirectWarp(s16 *curLevel, s16 *destLevel, s8 *curArea, s16 *destArea, s16 *destWarpNode,
                         bool isDeathWarp, int warpOp) {
    // When warping, always lock the clock and reset var to avoid segfault if old clock val is not in
    // new area
    SM64AP_SetClockToTTCState();
    if (*destLevel == LEVEL_BOWSER_3 || *curLevel == LEVEL_BOWSER_3 || *destLevel == LEVEL_BITS
        || *curLevel == LEVEL_BITS)
        return; // Dont play around with this one
    if (*destWarpNode >= WARP_NODE_CREDITS_MIN)
        return; // Credit Warps
    if ((*curLevel == LEVEL_CASTLE || *curLevel == LEVEL_CASTLE_COURTYARD
         || *curLevel == LEVEL_CASTLE_GROUNDS || *curLevel == LEVEL_HMC)
        && *destLevel != LEVEL_CASTLE && *destLevel != LEVEL_CASTLE_COURTYARD
        && *destLevel != LEVEL_CASTLE_GROUNDS) {
        int destination;
        switch (*destLevel) {
            case LEVEL_LLL:
            case LEVEL_SSL:
            case LEVEL_TTM:
            case LEVEL_COTMC:
                destination = map_entrances[*destLevel * 10 + 1];
                break;
            default:
                if (*curLevel == LEVEL_HMC)
                    return; // Safety Check: If in HMC only relevant warp is to COTMC
                destination = map_entrances[*destLevel * 10 + *destArea];
                break;
        }
        if (*curLevel != LEVEL_HMC) { // HMC -> COTMC transition should not set new return point
            sm64_exit_return_to = *curLevel * 10 + *curArea;
            sm64_exit_orig_entrancelvl = *destLevel;
        }
        *destLevel = destination / 10; // Cuts off Area Info
        *destArea = destination % 10;  // Cuts off Level Info
        *destWarpNode = 0x0A;
        return;
    }

    if ((*destLevel == LEVEL_CASTLE || *destLevel == LEVEL_CASTLE_COURTYARD
         || *destLevel == LEVEL_CASTLE_GROUNDS)
        && course_dest_supported.find(*curLevel) != course_dest_supported.end()) {
        if (*destLevel == LEVEL_CASTLE && (*destWarpNode == 0x1F || *destWarpNode == 0x00))
            return; // Exit Course or Inter-Castle warp
        *destLevel = sm64_exit_return_to / 10;
        *destArea = sm64_exit_return_to % 10;
        setCourseNodeAndArea(sm64_exit_orig_entrancelvl, destWarpNode, isDeathWarp, warpOp);
        return;
    }
}

int SM64AP_EntranceToTTC() {
    int level = 0;
    for (auto itr : map_entrances) {
        if (itr.second / 10 == LEVEL_TTC) {
            return itr.first;
        }
    }
    return -1; // Error Cond
}

void SM64AP_SetClockToTTCAction(int *action) {
    sm64_clockaction = action;
}

void SM64AP_SetClockToTTCState() {
    if (sm64_clockaction)
        *sm64_clockaction = 5;
    sm64_clockaction = nullptr;
}

void SM64AP_SetFirstBowserDoorCost(int amount) {
    sm64_cost_firstbowserdoor = amount;
}

void SM64AP_SetBasementDoorCost(int amount) {
    sm64_cost_basementdoor = amount;
}

void SM64AP_SetSecondFloorDoorCost(int amount) {
    sm64_cost_secondfloordoor = amount;
}

void SM64AP_SetMIPS1Cost(int amount) {
    sm64_cost_mips1 = amount;
}

void SM64AP_SetMIPS2Cost(int amount) {
    sm64_cost_mips2 = amount;
}

void SM64AP_SetStarsToFinish(int amount) {
    sm64_cost_endlessstairs = amount;
}

void SM64AP_SetCompletionType(int type) {
    sm64_completion_type = type;
}

void SM64AP_SetCourseMap(std::map<int, int> map) {
    map_entrances = map;
}

void SM64AP_SetMoveRandoVec(int vec) {
    int limit = (SM64AP_NUM_ABILITIES < 32) ? SM64AP_NUM_ABILITIES : 32;
    for (int i = 1; i < limit; i++) {
        sm64_have_abilities[i] = !std::bitset<32>(vec).test(i) || sm64_have_abilities[i];
    }
}

// Separate bitmask for high-index abilities (Punch=bit0, Grab=bit1, Swim=bit2).
// A 0 bit means NOT randomized (auto-unlock). A 1 bit means RANDOMIZED (wait for RecvItem).
// If the AP world never sends MoveRandoVecHigh we assume these moves are not randomized
// and auto-unlock them for backward compatibility.


void SM64AP_SetMoveRandoVecHigh(int vec) {
    sm64_received_move_rando_high = true;

    sm64_have_abilities[11] = !(vec & (1 << 0)); // Punch
    sm64_have_abilities[12] = !(vec & (1 << 1)); // Grab
    sm64_have_abilities[13] = !(vec & (1 << 2)); // Swim

    printf("MoveRandoVecHigh=%d | punch=%d grab=%d swim=%d\n",
        vec,
        (int)sm64_have_abilities[11],
        (int)sm64_have_abilities[12],
        (int)sm64_have_abilities[13]);
}
void SM64AP_SetPaintingRando(int enabled) {
    if (!enabled) {
        // Not enabled, so unlock all paintings
        for (int i = 0; i < NUM_PAINTING_LOCKS; i++)
            sm64_have_painting[i] = true;
    }
}

void SM64AP_ResetItems() {
    for (int i = 0; i < SM64AP_NUM_LOCS; i++) {
        sm64_locations[i] = false;
    }
    for (int i = 0; i < 15; i++) {
        sm64_have_cannon[i] = false;
    }
    for (int i = 0; i < NUM_PAINTING_LOCKS; i++) {
        sm64_have_painting[i] = false;
    }
    sm64_have_abilities.reset();
    sm64_received_move_rando_high = false;
    sm64_have_key1 = false;
    sm64_have_key2 = false;
    sm64_have_wingcap = false;
    sm64_have_metalcap = false;
    sm64_have_vanishcap = false;
    SM64AP_ResetCoinsanityFlags();
    starsCollected = 0;

    for (int i = 0; i <= SM64AP_COURSE_MAX; i++) {
        sAPLevelCoins[i] = 0;
    }

    AP_SetServerDataRequest moat_request;
    moat_request.key = AP_GetPrivateServerDataPrefix() + "MoatDrained";
    moat_request.type = AP_DataType::Int;
    int def_val = 0;
    moat_request.operations = { { "default", &def_val } };
    moat_request.default_value = &def_val;
    moat_request.want_reply = true;
    AP_SetServerData(&moat_request);
}

void SM64AP_SetReplyHandler(AP_SetReply reply) {
    if (reply.key == AP_GetPrivateServerDataPrefix() + "FinishedBowser") {
        switch (sm64_completion_type) {
            case 0: // Only BitS
                if ((*(int *) (reply.value) & 0b100) > 0)
                    AP_StoryComplete();
                break;
            case 1: // All Bowser Stages
                if (*(int *) (reply.value) == 0b111)
                    AP_StoryComplete();
                break;
        }
    } else if (reply.key == AP_GetPrivateServerDataPrefix() + "MoatDrained") {
        sm64_moat_state = *(int *) (reply.value);
    }
}

void SM64AP_GenericInit() {
    AP_SetDeathLinkSupported(true);
    AP_SetItemClearCallback(&SM64AP_ResetItems);
    AP_SetLocationCheckedCallback(&SM64AP_CheckLocation);
    AP_SetItemRecvCallback(&SM64AP_RecvItem);
    AP_RegisterSetReplyCallback(&SM64AP_SetReplyHandler);
    AP_SetNotify(AP_GetPrivateServerDataPrefix() + "FinishedBowser", AP_DataType::Int);
    AP_SetNotify(AP_GetPrivateServerDataPrefix() + "MoatDrained", AP_DataType::Int);

    AP_RegisterSlotDataIntCallback("FirstBowserDoorCost", &SM64AP_SetFirstBowserDoorCost);
    AP_RegisterSlotDataIntCallback("BasementDoorCost", &SM64AP_SetBasementDoorCost);
    AP_RegisterSlotDataIntCallback("SecondFloorDoorCost", &SM64AP_SetSecondFloorDoorCost);
    AP_RegisterSlotDataIntCallback("MIPS1Cost", &SM64AP_SetMIPS1Cost);
    AP_RegisterSlotDataIntCallback("MIPS2Cost", &SM64AP_SetMIPS2Cost);
    AP_RegisterSlotDataIntCallback("StarsToFinish", &SM64AP_SetStarsToFinish);
    AP_RegisterSlotDataIntCallback("CompletionType", &SM64AP_SetCompletionType);
    AP_RegisterSlotDataIntCallback("MoveRandoVec", &SM64AP_SetMoveRandoVec);
    AP_RegisterSlotDataIntCallback("MoveRandoVecHigh", &SM64AP_SetMoveRandoVecHigh);
    AP_RegisterSlotDataIntCallback("PaintingRando", &SM64AP_SetPaintingRando);
    AP_RegisterSlotDataMapIntIntCallback("AreaRando", &SM64AP_SetCourseMap);
    AP_RegisterSlotDataIntCallback("MarioPaletteSeed", &SM64AP_SetMarioPaletteSeed);

    course_dest_supported = { LEVEL_BOB,     LEVEL_WF,    LEVEL_JRB,   LEVEL_CCM,      LEVEL_BBH,
                              LEVEL_HMC,     LEVEL_LLL,   LEVEL_SSL,   LEVEL_DDD,      LEVEL_SL,
                              LEVEL_WDW,     LEVEL_TTM,   LEVEL_THI,   LEVEL_TTC,      LEVEL_RR,
                              LEVEL_PSS,     LEVEL_SA,    LEVEL_BITDW, LEVEL_TOTWC,    LEVEL_COTMC,
                              LEVEL_VCUTM,   LEVEL_BITFS, LEVEL_WMOTR, LEVEL_BOWSER_1, LEVEL_BOWSER_2,
                              LEVEL_BOWSER_3 };

    map_boxid_locid[LEVEL_CCM * 10 + 1] = 3626215;
    map_boxid_locid[LEVEL_CCM * 10 + 2] = 3626216;
    map_boxid_locid[LEVEL_CCM * 10 + 3] = 3626217;
    map_boxid_locid[LEVEL_BBH * 10 + 1] = 3626218;
    map_boxid_locid[LEVEL_HMC * 10 + 1] = 3626219;
    map_boxid_locid[LEVEL_HMC * 10 + 2] = 3626220;
    map_boxid_locid[LEVEL_SSL * 10 + 1] = 3626221;
    map_boxid_locid[LEVEL_SSL * 10 + 2] = 3626222;
    map_boxid_locid[LEVEL_SSL * 10 + 3] = 3626223;
    map_boxid_locid[LEVEL_SL * 10 + 1] = 3626224;
    map_boxid_locid[LEVEL_SL * 10 + 2] = 3626225;
    map_boxid_locid[LEVEL_WDW * 10 + 2] =
        3626226; // Uses first bit as flag for something, makes mario invisible :/
    map_boxid_locid[LEVEL_TTM * 10 + 1] = 3626227;
    map_boxid_locid[LEVEL_THI * 10 + 1] = 3626228;
    map_boxid_locid[LEVEL_THI * 10 + 2] = 3626229;
    map_boxid_locid[LEVEL_THI * 10 + 3] = 3626230;
    map_boxid_locid[LEVEL_TTC * 10 + 1] = 3626231;
    map_boxid_locid[LEVEL_TTC * 10 + 2] = 3626232;
    map_boxid_locid[LEVEL_RR * 10 + 1] = 3626233;
    map_boxid_locid[LEVEL_RR * 10 + 2] = 3626234;
    map_boxid_locid[LEVEL_RR * 10 + 3] = 3626235;
    map_boxid_locid[LEVEL_BITDW * 10 + 1] = 3626236;
    map_boxid_locid[LEVEL_BITDW * 10 + 2] = 3626237;
    map_boxid_locid[LEVEL_BITFS * 10 + 1] = 3626238;
    map_boxid_locid[LEVEL_BITFS * 10 + 2] = 3626239;
    map_boxid_locid[LEVEL_BITS * 10 + 1] = 3626240;
    map_boxid_locid[LEVEL_COTMC * 10 + 1] = 3626241;
    map_boxid_locid[LEVEL_VCUTM * 10 + 1] = 3626242;
    map_boxid_locid[LEVEL_WMOTR * 10 + 1] = 3626243;
}

void SM64AP_InitMW(const char *ip, const char *player_name, const char *passwd) {
    AP_Init(ip, "Cursed Mario 64", player_name, passwd);
    SM64AP_GenericInit();
    AP_Start();
}

void SM64AP_InitSP(const char *filename) {
    AP_Init(filename);
    SM64AP_GenericInit();
    AP_Start();
}

void SM64AP_SendByBoxID(int id) {
    SM64AP_SendItem(map_boxid_locid[id]);
}

void SM64AP_SendItem(int idx) {
    AP_SendItem(idx);
}

void SM64AP_CheckEnemyDeath(struct Object *o) {
    int64_t loc_id = 0;
    int hX = (int) roundf(o->oHomeX);
    int hZ = (int) roundf(o->oHomeZ);

    if (gCurrLevelNum == LEVEL_BOB) {
        if (o->behavior == bhvGoomba) {
            if (hX == -2713 && hZ == 5778)
                loc_id = 3626300;
            else if (hX == -342 && hZ == 5433)
                loc_id = 3626301;
            else if (o->parentObj != o) {
                int pHX = (int) roundf(o->parentObj->oPosX);
                int pHZ = (int) roundf(o->parentObj->oPosZ);
                int raw_idx = (o->oBehParams2ndByte & 0xFC);
                int tri_idx = -1;

                if (raw_idx & 0x04)
                    tri_idx = 0;
                else if (raw_idx & 0x08)
                    tri_idx = 1;
                else if (raw_idx & 0x10)
                    tri_idx = 2;

                if (tri_idx != -1) {
                    if (pHX == 3640 && pHZ == 6280)
                        loc_id = 3626302 + tri_idx;
                    else if (pHX == 6060 && pHZ == 2000)
                        loc_id = 3626305 + tri_idx;
                    else if (pHX == -6050 && pHZ == 1250)
                        loc_id = 3626308 + tri_idx;
                }
            }
        } else if (o->behavior == bhvBobomb) {
            if (hX == -3080 && hZ == -5200)
                loc_id = 3626311;
            else if (hX == -3688 && hZ == -3813)
                loc_id = 3626312;
            else if (hX == -4629 && hZ == -1772)
                loc_id = 3626313;
            else if (hX == -3480 && hZ == -2120)
                loc_id = 3626314;
            else if (hX == -3800 && hZ == -460)
                loc_id = 3626315;
            else if (hX == 6888 && hZ == -5608)
                loc_id = 3626316;
            else if (hX == 2350 && hZ == 3700)
                loc_id = 3626317;
            else if (hX == -1750 && hZ == -2800)
                loc_id = 3626318;
            else if (hX == -1400 && hZ == -950)
                loc_id = 3626319;
            else if (hX == -2650 && hZ == 1750)
                loc_id = 3626320;
            else if (hX == -1900 && hZ == 3450)
                loc_id = 3626321;
            else if (hX == 1127 && hZ == -2495)
                loc_id = 3626322;
        } else if (o->behavior == bhvKoopa) {
            if (hX == 3400 && hZ == 6500)
                loc_id = 3626323;
        }
    } else if (gCurrLevelNum == LEVEL_CCM) {
        if (o->behavior == bhvMrBlizzard) {
            if (hX == -2376 && hZ == 4256)
                loc_id = 3626400;
            else if (hX == -394 && hZ == 4878)
                loc_id = 3626401;
            else if (hX == 3054 && hZ == 2072)
                loc_id = 3626402;
        } else if (o->behavior == bhvSpindrift) {
             if (hX == 2542 && hZ == -1714)
                loc_id = 3626403;
            else if (hX == -6090 && hZ == 1936)
                loc_id = 3626404;
            else if (hX == 4346 && hZ == 400)
                loc_id = 3626405;
            else if (hX == -5054 && hZ == -1054)
                loc_id = 3626406;
            else if (hX == -5033 && hZ == -2666)
                loc_id = 3626407;
            else if (hX == -488 && hZ == -2305)
                loc_id = 3626408;
            else if (hX == -1768 && hZ == -1793)
                loc_id = 3626409;
        }
    } else if (gCurrLevelNum == LEVEL_LLL) {
        if (o->behavior == bhvMrI) {
            if (hX == -3199 && hZ == 3456)
                loc_id = 3626500;
            else if (hX == 6673 && hZ == -3060)
                loc_id = 3626508;
        } else if (o->behavior == bhvBigBully || o->behavior == bhvBigBullyWithMinions) {
            if (hX == 0 && hZ == -4385)
                loc_id = 3626501;
            else if (hX == 4046 && hZ == -5521)
                loc_id = 3626502;
        } else if (o->behavior == bhvSmallBully) {
            if (hX == -5119 && hZ == -2482)
                loc_id = 3626503;
            else if (hX == 0 && hZ == 3712)
                loc_id = 3626504;
            else if (hX == 6813 && hZ == 1613)
                loc_id = 3626505;
            else if (hX == 7168 && hZ == 998)
                loc_id = 3626506;
            else if (hX == -5130 && hZ == -1663)
                loc_id = 3626507;
            else if (hX == 1300 && hZ == 2300)
                loc_id = 3626509;
            else if (hX == -960 && hZ == -2610)
                loc_id = 3626510;
            else if (hX == 4454 && hZ == -5426)
                loc_id = 3626511;
            else if (hX == 3840 && hZ == -6041)
                loc_id = 3626512;
            else if (hX == 3226 && hZ == -5426)
                loc_id = 3626513;
        }
    }

    if (loc_id != 0 && !SM64AP_CheckedLoc(loc_id)) {
        SM64AP_SendItem(loc_id);
    }
}

void SM64AP_UpdateRRTrapTimer(struct MarioState *m) {
    if (!gRRTrapped || gCurrLevelNum != LEVEL_RR)
        return;
    if (gRRTrapTimer > 0) {
        gRRTrapTimer--;
        if (gRRTrapTimer == 0) {
            SM64AP_DeathLinkSend();
            gRRTrapped = false;
            gRRReturning = true;
            initiate_warp(gRRReturnLevel, gRRReturnArea, 0x0A, 0);
            fade_into_special_warp(0, 0);
        }
    }
}

int64_t SM64AP_PopDelayedStack(void) {
    if (delayed_queue.empty())
        return 0;

    int64_t item = delayed_queue.front();
    delayed_queue.pop();
    return item;
}

void SM64AP_ProcessDelayedItems(void) {
    size_t count = delayed_queue.size();

    for (size_t i = 0; i < count; i++) {
        int64_t item = SM64AP_PopDelayedStack();

        if (item == SM64AP_ID_KOOPA_SHELL) {
            if (SM64AP_CanSpawnFieldItem()) {
                SM64AP_SpawnKoopaShellInFrontOfMario();
            } else {
                delayed_queue.push(item);
            }
        }
    }
}
void SM64AP_FinishBowser(int i) {
    AP_SetServerDataRequest req;
    req.key = AP_GetPrivateServerDataPrefix() + "FinishedBowser";
    int def_val = 0;
    req.default_value = &def_val;
    req.type = AP_DataType::Int;
    req.want_reply = true;
    int flag = 0b001 << i;
    req.operations = std::vector<AP_DataStorageOperation>{ { { "or", &flag } } };
    AP_SetServerData(&req);
}

void SM64AP_SetMoatDrained() {
    AP_SetServerDataRequest req;
    req.key = AP_GetPrivateServerDataPrefix() + "MoatDrained";
    req.type = AP_DataType::Int;
    req.want_reply = true;
    int new_val = 1;
    req.operations = std::vector<AP_DataStorageOperation>{ { { "replace", &new_val } } };
    AP_SetServerData(&req);
}

int SM64AP_GetStars() {
    return starsCollected;
}

int SM64AP_GetRequiredStars(int idprx) {
    switch (idprx) {
        case 8: // Star Door 8
            return sm64_cost_firstbowserdoor;
        case 30: // Star Door 30
            return sm64_cost_basementdoor;
        case 50: // Star Door 50
            return sm64_cost_secondfloordoor;
        case 70: // Star Door 70
            return sm64_cost_endlessstairs;
        case SM64AP_LOCATIONID_MIPS1: // MIPS 1
            return sm64_cost_mips1;
        case SM64AP_LOCATIONID_MIPS2: // MIPS 2
            return sm64_cost_mips2;
        default:
            return idprx;
    }
}

bool SM64AP_CheckedLoc(int x) {
    if (x < SM64AP_ID_OFFSET) {
        return false;
    }

    int index = x - SM64AP_ID_OFFSET;

    if (index < 0 || index >= SM64AP_NUM_LOCS) {
        return false;
    }

    return sm64_locations[index];
}

bool SM64AP_HaveKey1() {
    return sm64_have_key1;
}

bool SM64AP_HaveKey2() {
    return sm64_have_key2;
}

bool SM64AP_HaveCap(int flag) {
    switch (flag) {
        case 2:
            return sm64_have_wingcap;
            break;
        case 4:
            return sm64_have_metalcap;
            break;
        case 8:
            return sm64_have_vanishcap;
            break;
        default:
            // Probably coin/1up or something
            return true;
    }
}

bool SM64AP_PressedSwitch(int flag) {
    switch (flag) {
        case 2:
            return SM64AP_CheckedLoc(SM64AP_ID_WINGCAP);
        case 4:
            return SM64AP_CheckedLoc(SM64AP_ID_METALCAP);
        case 8:
            return SM64AP_CheckedLoc(SM64AP_ID_VANISHCAP);
        default:
            // Shouldn't happen, but just in case, this shouldn't be pressed
            return false;
    }
}

bool SM64AP_HaveCannon(int courseIdx) {
    if (courseIdx < 15)
        return sm64_have_cannon[courseIdx];
    return true;
}

bool SM64AP_HavePainting(int courseIdx) {
    switch (courseIdx) {
        case 1:  // BOB painting is always unlocked
        case 5:  // BBH doesn't have a painting
        case 6:  // HMC has a painting but you get stuck in an infinite loop of falling in and getting
                 // pushed out, so let's not do that :)
        case 15: // RR doesn't have a painting
            return true;
        default:
            // courses are 1-indexed, the items are 0-indexed
            return sm64_have_painting[courseIdx - 1];
    }
}

bool SM64AP_MoatDrained() {
    return sm64_moat_state != 0;
}

bool SM64AP_DeathLinkPending() {
    return AP_DeathLinkPending();
}

void SM64AP_DeathLinkClear() {
    AP_DeathLinkClear();
}

void SM64AP_DeathLinkSend() {
    if (!SM64AP_DeathLinkPending()) {
        return AP_DeathLinkSend();
    } else {
        SM64AP_DeathLinkClear();
    }
}

bool SM64AP_CanDoubleJump() {
    return sm64_have_abilities[SM64AP_ID_DOUBLEJUMP - SM64AP_ABILITY_OFFSET]
           || sm64_have_abilities[SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanTripleJump() {
    return sm64_have_abilities[SM64AP_ID_TRIPLEJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanLongJump() {
    return sm64_have_abilities[SM64AP_ID_LONGJUMP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanBackflip() {
    return sm64_have_abilities[SM64AP_ID_BACKFLIP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanSideFlip() {
    return sm64_have_abilities[SM64AP_ID_SIDEFLIP - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanWallKick() {
    return sm64_have_abilities[SM64AP_ID_WALLKICK - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanDive() {
    return sm64_have_abilities[SM64AP_ID_DIVE - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanGroundPound() {
    return sm64_have_abilities[SM64AP_ID_GROUNDPOUND - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanKick() {
    return sm64_have_abilities[SM64AP_ID_KICK - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanClimb() {
    return sm64_have_abilities[SM64AP_ID_CLIMB - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanLedgeGrab() {
    return sm64_have_abilities[SM64AP_ID_LEDGEGRAB - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanPunch() {
    return sm64_have_abilities[11];
}

bool SM64AP_CanGrab() {
    return sm64_have_abilities[12];
}

bool SM64AP_CanSwim() {
    return sm64_have_abilities[13];
}


void SM64AP_PrintNext() {

    SM64AP_ProcessDelayedItems();

    if (AP_GetConnectionStatus() == AP_ConnectionStatus::Disconnected) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 7, SCREEN_HEIGHT / 2,
                   "Connecting");
    }

    if (AP_GetConnectionStatus() == AP_ConnectionStatus::ConnectionRefused) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2,
                   "CONNECTION REFUSED");
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2 - 20,
                   "CHECK ARGS");
    }

    static int auth_timer = 0;
    if (AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        if (!sm64_received_move_rando_high) {
            if (auth_timer < 90) {
                auth_timer++;
            } else {
                sm64_received_move_rando_high = true;
                sm64_have_abilities[11] = true;
                sm64_have_abilities[12] = true;
                sm64_have_abilities[13] = true;
            }
        }
    } else {
        auth_timer = 0;
    }

    if (!sm64_have_abilities.all() && !SM64AP_SUPPORT_MOVE_RANDO) {
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2,
                   "INCOMPATIBLE WITH");
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 2 - 20,
                   "MOUE RANDO");
    }

    if (!AP_IsMessagePending())
        return;

    AP_Message *msg = AP_GetLatestMessage();

    if (msg->type == AP_MessageType::ItemSend) {
        AP_ItemSendMessage *o_msg = static_cast<AP_ItemSendMessage *>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1 - 0) * 20,
                   (o_msg->item + std::string(" was sent")).c_str());
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1 - 1) * 20,
                   (std::string("to ") + o_msg->recvPlayer).c_str());

    } else if (msg->type == AP_MessageType::ItemRecv) {
        AP_ItemRecvMessage *o_msg = static_cast<AP_ItemRecvMessage *>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1 - 0) * 20,
                   (std::string("Got ") + o_msg->item).c_str());
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1 - 1) * 20,
                   (std::string("From ") + o_msg->sendPlayer).c_str());

    } else if (msg->type == AP_MessageType::Countdown) {
        cur_msg_frame_duration = std::min(cur_msg_frame_duration, 30);
        AP_CountdownMessage *o_msg = static_cast<AP_CountdownMessage *>(msg);
        print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0) + SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2,
                   std::to_string(o_msg->timer).c_str());
    }

    if (cur_msg_frame_duration > 0) {
        cur_msg_frame_duration--;
    } else {
        AP_ClearLatestMessage();
        cur_msg_frame_duration = msg_frame_duration;
    }
}
