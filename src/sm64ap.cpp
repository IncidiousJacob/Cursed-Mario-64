#include "sm64ap.h"
#include "Archipelago.h"

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
char gPlantDebugText[64];
s32 gPlantDebugTimer = 0;

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

static SM64AP_RGB8 sm64ap_make_color(uint32_t &x, int minv, int maxv) {
    SM64AP_RGB8 c;
    c.r = sm64ap_palette_byte(x, minv, maxv);
    c.g = sm64ap_palette_byte(x, minv, maxv);
    c.b = sm64ap_palette_byte(x, minv, maxv);
    return c;
}

void SM64AP_SetMarioPaletteSeed(int seed) {

    uint32_t x = (uint32_t)(seed ? seed : 1);

    gMarioHatShirtColor = sm64ap_make_color(x, 80, 255);
    gMarioSkinColor     = sm64ap_make_color(x, 80, 255);
    gMarioHairColor     = sm64ap_make_color(x, 85, 255);
    gMarioOverallsColor = sm64ap_make_color(x, 64, 255);
    gMarioShoesColor    = sm64ap_make_color(x, 70, 255);
    gMarioGlovesColor   = sm64ap_make_color(x, 75, 255);
    gStarColor          = sm64ap_make_color(x, 69, 255);

    gToadBodyColor = sm64ap_make_color(x, 48, 255);
    gToadSpotColor = sm64ap_make_color(x, 96, 255);
    gToadSkinColor = sm64ap_make_color(x, 80, 240);
    gToadShoeColor = sm64ap_make_color(x, 16, 180);

    gGoombaColor = sm64ap_make_color(x, 96, 255);

    gPiranhaHeadColor = sm64ap_make_color(x, 60, 240);
    gPiranhaStemColor = sm64ap_make_color(x, 75, 245);
    gPiranhaLeafColor = sm64ap_make_color(x, 85, 255);

    gBowserBodyColor  = sm64ap_make_color(x, 64, 220);

    gBobombColor = sm64ap_make_color(x, 64, 255);

    SM64AP_ApplyMarioPalette();
    SM64AP_ApplyStarPalette();
    SM64AP_ApplyToadPalette();
    SM64AP_ApplyGoombaPalette();
    SM64AP_ApplyPiranhaPalette();
    SM64AP_ApplyBowserPalette();
    SM64AP_ApplyBobombPalette();
}

static void SM64AP_SpawnKoopaShellInFrontOfMario(void) {
    if (gMarioObject == NULL || gMarioState == NULL || gCurrentArea == NULL) {
        return;
    }

    struct Object *shell =
        spawn_object_relative(0, 0, 60, 220, gMarioObject, MODEL_KOOPA_SHELL, bhvKoopaShell);

    if (shell != NULL) {
        shell->oForwardVel = 0.0f;
        shell->oVelY = 0.0f;
    }
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
            // results:
            // 2506, 2507, 2508, 2509, 2510
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

void SM64AP_RecvItem(int64_t idx, bool notify) {
    if (idx >= SM64AP_ID_CANNONUNLOCK(0) && idx <= SM64AP_ID_CANNONUNLOCK(15 - 1)) {
        sm64_have_cannon[idx - (SM64AP_ID_CANNONUNLOCK(0))] = true;
    } else if (idx >= SM64AP_ID_PAINTINGUNLOCK(0)
               && idx <= SM64AP_ID_PAINTINGUNLOCK(NUM_PAINTING_LOCKS - 1)) {
        sm64_have_painting[idx - (SM64AP_ID_PAINTINGUNLOCK(0))] = true;
    } else if (idx == SM64AP_ID_ABILITY(0)) {
        sm64_have_abilities[idx - SM64AP_ABILITY_OFFSET + 1] =
            sm64_have_abilities[idx - SM64AP_ABILITY_OFFSET];
        sm64_have_abilities[idx - SM64AP_ABILITY_OFFSET] = true;
    } else if (idx >= SM64AP_ID_ABILITY(1) && idx <= SM64AP_ID_ABILITY(SM64AP_NUM_ABILITIES - 1)) {
        sm64_have_abilities[idx - SM64AP_ABILITY_OFFSET] = true;
    } else if (idx >= SM64AP_ID_1_HEALTH_PIP && idx <= SM64AP_ID_RR_TRAP) {
        if (notify) {
            if (idx == SM64AP_ID_RR_TRAP) {
                gRRTrapTimer = 4 * 60 * 30;
            }
            delayed_queue.push(idx);
        }
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
            case SM64AP_ID_KOOPA_SHELL:
                SM64AP_SpawnKoopaShellInFrontOfMario();
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
    sm64_locations[loc_id - SM64AP_ID_OFFSET] = true;
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
    // Standard moves are in the first 32 bits. High IDs (like 196-198) are purely randomized.
    int limit = (SM64AP_NUM_ABILITIES < 32) ? SM64AP_NUM_ABILITIES : 32;
    for (int i = 1; i < limit; i++) {
        sm64_have_abilities[i] = !std::bitset<32>(vec).test(i) || sm64_have_abilities[i];
    }
}

// Separate bitmask for high-index abilities (Punch=bit0, Grab=bit1, Swim=bit2).
// A 0 bit means NOT randomized (auto-unlock). A 1 bit means RANDOMIZED (wait for RecvItem).
// If the AP world never sends MoveRandoVecHigh we assume these moves are not randomized
// and auto-unlock them for backward compatibility.
static bool sm64_received_move_rando_high = false;

void SM64AP_SetMoveRandoVecHigh(int vec) {
    sm64_received_move_rando_high = true;
    // bit 0 = Punch, bit 1 = Grab, bit 2 = Swim
    if (!std::bitset<32>(vec).test(0))
        sm64_have_abilities[SM64AP_ID_PUNCH - SM64AP_ABILITY_OFFSET] = true;
    if (!std::bitset<32>(vec).test(1))
        sm64_have_abilities[SM64AP_ID_GRAB - SM64AP_ABILITY_OFFSET] = true;
    if (!std::bitset<32>(vec).test(2))
        sm64_have_abilities[SM64AP_ID_SWIM - SM64AP_ABILITY_OFFSET] = true;
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
    starsCollected = 0;

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

// If an item exists on the stack, return it, otherwise 0
int64_t SM64AP_PopDelayedStack() {
    if (delayed_queue.empty())
        return 0;
    int64_t item = delayed_queue.front();
    delayed_queue.pop();
    return item;
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
    return sm64_locations[x - SM64AP_ID_OFFSET];
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

bool SM64AP_CanGrab() {
    return sm64_have_abilities[SM64AP_ID_GRAB - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanPunch() {
    return sm64_have_abilities[SM64AP_ID_PUNCH - SM64AP_ABILITY_OFFSET];
}

bool SM64AP_CanSwim() {
    return sm64_have_abilities[SM64AP_ID_SWIM - SM64AP_ABILITY_OFFSET];
}

void SM64AP_PrintNext() {
    
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

    // Backward compatibility: if the AP world never sent MoveRandoVecHigh (old worlds),
    // auto-unlock Punch/Grab/Swim once we are connected and slot data has been processed.
    static int auth_timer = 0;
    if (AP_GetConnectionStatus() == AP_ConnectionStatus::Authenticated) {
        if (!sm64_received_move_rando_high) {
            if (auth_timer < 90) {
                auth_timer++;
            } else {
                sm64_received_move_rando_high = true; // Run only once
                sm64_have_abilities[SM64AP_ID_PUNCH - SM64AP_ABILITY_OFFSET] = true;
                sm64_have_abilities[SM64AP_ID_GRAB - SM64AP_ABILITY_OFFSET] = true;
                sm64_have_abilities[SM64AP_ID_SWIM - SM64AP_ABILITY_OFFSET] = true;
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
    } else {
        // print_text(GFX_DIMENSIONS_FROM_LEFT_EDGE(0), (1-0)*20, msg->text.c_str());
    }
    if (cur_msg_frame_duration > 0) {
        cur_msg_frame_duration--;
    } else {
        AP_ClearLatestMessage();
        cur_msg_frame_duration = msg_frame_duration;
    }
}
