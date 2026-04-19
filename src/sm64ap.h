#ifndef SM64AP_H
#define SM64AP_H

#include <stdbool.h>
#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
#define AP_EXTERN_C extern "C"
#else
#define AP_EXTERN_C extern
#endif
#ifdef __cplusplus
extern "C" {
#include "types.h"
}
#else
#include "types.h"
#endif

#define SM64AP_ID_OFFSET 3626000
#define SM64AP_SECOND_CHECK_OFFSET 1000
#define SM64AP_ITEMID_STAR SM64AP_ID_OFFSET
#define SM64AP_ID_KEY1 (SM64AP_ID_OFFSET + 178)
#define SM64AP_ID_KEY2 (SM64AP_ID_KEY1 + 1)
#define SM64AP_ID_KEYPROG (SM64AP_ID_KEY2 + 1)
#define SM64AP_ID_WINGCAP (SM64AP_ID_KEYPROG + 1)
#define SM64AP_ID_METALCAP (SM64AP_ID_WINGCAP + 1)
#define SM64AP_ID_VANISHCAP (SM64AP_ID_METALCAP + 1)
#define SM64AP_ITEMID_1UP (SM64AP_ID_VANISHCAP + 1)
#define SM64AP_ID_KOOPA_SHELL (SM64AP_ID_OFFSET + 1768)
#define SM64AP_ID_TOAD_133_UNLOCK (SM64AP_ID_OFFSET + 1769)
#define SM64AP_ID_TOAD_134_UNLOCK (SM64AP_ID_OFFSET + 1770)
#define SM64AP_ID_TOAD_135_UNLOCK (SM64AP_ID_OFFSET + 1771)
#define SM64AP_ID_TOAD_076_UNLOCK (SM64AP_ID_OFFSET + 1772)
#define SM64AP_ID_TOAD_083_UNLOCK (SM64AP_ID_OFFSET + 1773)
#define SM64AP_ID_TOAD_137_UNLOCK (SM64AP_ID_OFFSET + 1774)
#define SM64AP_ID_TOAD_082_UNLOCK (SM64AP_ID_OFFSET + 1775)
#define SM64AP_ID_TOAD_136_UNLOCK (SM64AP_ID_OFFSET + 1776)

#define SM64AP_ABILITY_OFFSET (SM64AP_ITEMID_1UP + 1)
#define SM64AP_ID_DOUBLEJUMP (SM64AP_ABILITY_OFFSET)
#define SM64AP_ID_TRIPLEJUMP (SM64AP_ABILITY_OFFSET + 1)
#define SM64AP_ID_LONGJUMP (SM64AP_ABILITY_OFFSET + 2)
#define SM64AP_ID_BACKFLIP (SM64AP_ABILITY_OFFSET + 3)
#define SM64AP_ID_SIDEFLIP (SM64AP_ABILITY_OFFSET + 4)
#define SM64AP_ID_WALLKICK (SM64AP_ABILITY_OFFSET + 5)
#define SM64AP_ID_DIVE (SM64AP_ABILITY_OFFSET + 6)
#define SM64AP_ID_GROUNDPOUND (SM64AP_ABILITY_OFFSET + 7)
#define SM64AP_ID_KICK (SM64AP_ABILITY_OFFSET + 8)
#define SM64AP_ID_CLIMB (SM64AP_ABILITY_OFFSET + 9)
#define SM64AP_ID_LEDGEGRAB (SM64AP_ABILITY_OFFSET + 10)
#define SM64AP_ID_GRAB (SM64AP_ABILITY_OFFSET + 197)
#define SM64AP_ID_PUNCH (SM64AP_ABILITY_OFFSET + 196)
#define SM64AP_ID_SWIM (SM64AP_ABILITY_OFFSET + 198)

#define SM64AP_ID_CANNONUNLOCK(x) (SM64AP_ID_OFFSET + 200 + x)
#define SM64AP_ID_PAINTINGUNLOCK(x) (SM64AP_ID_OFFSET + 230 + x)
#define SM64AP_ID_ABILITY(x) (SM64AP_ABILITY_OFFSET + x)
// Reserving some room for coins, if needed
// Nice filler
#define SM64AP_ID_1_HEALTH_PIP (SM64AP_ID_OFFSET + 1750)
#define SM64AP_ID_2_HEALTH_PIP (SM64AP_ID_OFFSET + 1751)
#define SM64AP_ID_3_HEALTH_PIP (SM64AP_ID_OFFSET + 1752)
#define SM64AP_ID_4_HEALTH_PIP (SM64AP_ID_OFFSET + 1753)
#define SM64AP_ID_FULL_REFILL (SM64AP_ID_OFFSET + 1754)
// Traps
#define SM64AP_ID_BONK_TRAP (SM64AP_ID_OFFSET + 1760)
#define SM64AP_ID_FIRE_TRAP (SM64AP_ID_OFFSET + 1761)
#define SM64AP_ID_ELEC_TRAP (SM64AP_ID_OFFSET + 1762)
#define SM64AP_ID_CHUCK_TRAP (SM64AP_ID_OFFSET + 1763)
#define SM64AP_ID_SPIN_TRAP (SM64AP_ID_OFFSET + 1764)
#define SM64AP_ID_GUST_TRAP (SM64AP_ID_OFFSET + 1765)
#define SM64AP_ID_LITERATURE_TRAP (SM64AP_ID_OFFSET + 1766)
#define SM64AP_ID_RR_TRAP (SM64AP_ID_OFFSET + 1767)

// Toads
#define SM64AP_TOAD_CHECK_OFFSET 2000

#define SM64AP_LOCATIONID_BOARDBOWSERSSUB (SM64AP_ID_OFFSET + 56)
#define SM64AP_LOCATIONID_BASEMENTTOAD (SM64AP_ID_OFFSET + 168)
#define SM64AP_LOCATIONID_SECONDFLOORTOAD (SM64AP_LOCATIONID_BASEMENTTOAD + 1)
#define SM64AP_LOCATIONID_THIRDFLOORTOAD (SM64AP_LOCATIONID_SECONDFLOORTOAD + 1)
#define SM64AP_LOCATIONID_MIPS1 (SM64AP_LOCATIONID_THIRDFLOORTOAD + 1)
#define SM64AP_LOCATIONID_MIPS2 (SM64AP_LOCATIONID_MIPS1 + 1)

#define SM64AP_NUM_LOCS 1200

#define SM64AP_NUM_ABILITIES 200

// Init
AP_EXTERN_C void SM64AP_InitMW(const char *, const char *, const char *);
AP_EXTERN_C void SM64AP_InitSP(const char *);

// Local Stars, Keys and Caps
AP_EXTERN_C int SM64AP_GetStars();
AP_EXTERN_C int SM64AP_GetRequiredStars(int);
AP_EXTERN_C u32 SM64AP_CourseStarFlags(s32);
AP_EXTERN_C void SM64AP_RedirectWarp(s16 *, s16 *, s8 *, s16 *, s16 *, bool, int);
AP_EXTERN_C int SM64AP_EntranceToTTC();
AP_EXTERN_C void SM64AP_SetClockToTTCAction(int *action);
AP_EXTERN_C void SM64AP_SetClockToTTCState();
AP_EXTERN_C bool SM64AP_CheckedLoc(int);
AP_EXTERN_C bool SM64AP_HaveKey1();
AP_EXTERN_C bool SM64AP_HaveKey2();
AP_EXTERN_C bool SM64AP_HaveCap(int);
AP_EXTERN_C bool SM64AP_HaveCannon(int);
AP_EXTERN_C bool SM64AP_HavePainting(int);
AP_EXTERN_C bool SM64AP_DeathLinkPending();
AP_EXTERN_C void SM64AP_DeathLinkClear();
AP_EXTERN_C void SM64AP_DeathLinkSend();

// Local Moves
AP_EXTERN_C bool SM64AP_CanDoubleJump();
AP_EXTERN_C bool SM64AP_CanTripleJump();
AP_EXTERN_C bool SM64AP_CanLongJump();
AP_EXTERN_C bool SM64AP_CanBackflip();
AP_EXTERN_C bool SM64AP_CanSideFlip();
AP_EXTERN_C bool SM64AP_CanWallKick();
AP_EXTERN_C bool SM64AP_CanDive();
AP_EXTERN_C bool SM64AP_CanGroundPound();
AP_EXTERN_C bool SM64AP_CanKick();
AP_EXTERN_C bool SM64AP_CanClimb();
AP_EXTERN_C bool SM64AP_CanLedgeGrab();
AP_EXTERN_C bool SM64AP_CanGrab();
AP_EXTERN_C bool SM64AP_CanPunch();
AP_EXTERN_C bool SM64AP_CanSwim();
AP_EXTERN_C bool sm64_have_toad_133;
AP_EXTERN_C bool sm64_have_toad_134;
AP_EXTERN_C bool sm64_have_toad_135;
AP_EXTERN_C bool sm64_have_toad_076;
AP_EXTERN_C bool sm64_have_toad_083;
AP_EXTERN_C bool sm64_have_toad_137;
AP_EXTERN_C bool sm64_have_toad_082;
AP_EXTERN_C bool sm64_have_toad_136;
AP_EXTERN_C void SM64AP_CheckWFPiranhaPlant(struct Object *);

// Send Item
AP_EXTERN_C void SM64AP_SendByBoxID(int);
AP_EXTERN_C void SM64AP_SendItem(int);
AP_EXTERN_C void SM64AP_CheckEnemyDeath(struct Object *);
AP_EXTERN_C void SM64AP_SpindriftStolen(struct Object *o);
AP_EXTERN_C void SM64AP_UpdateRRTrapTimer(struct MarioState *);
AP_EXTERN_C bool gFreezeTrapped;
AP_EXTERN_C s32 gFreezeTrapTimer;
AP_EXTERN_C Vec3f gFreezePos;
AP_EXTERN_C Vec3s gFreezeFaceAngle;
AP_EXTERN_C s32 gFreezeAction;
AP_EXTERN_C u32 gFreezeActionArg;
AP_EXTERN_C u16 gFreezeAnimFrame;

AP_EXTERN_C void SM64AP_BeginFreezeTrap(struct MarioState *m);
AP_EXTERN_C bool SM64AP_UpdateFreezeTrap(struct MarioState *m);

// Print Next Message to Screen
AP_EXTERN_C void SM64AP_PrintNext();

// If an item exists on the stack, return it, otherwise 0
AP_EXTERN_C int64_t SM64AP_PopDelayedStack();

// Called on each Bowser stage completion, i is bowser index. Will send StoryComplete depending on
// completion option.
AP_EXTERN_C void SM64AP_FinishBowser(int i);

// Used to send and receive moat state
AP_EXTERN_C void SM64AP_SetMoatDrained();
AP_EXTERN_C bool SM64AP_MoatDrained();

// Check for switch state (used for initial switch state on level load)
AP_EXTERN_C bool SM64AP_PressedSwitch(int);
AP_EXTERN_C bool gRRTrapped;
AP_EXTERN_C bool gRRReturning;
AP_EXTERN_C s16 gRRReturnLevel;
AP_EXTERN_C s16 gRRReturnArea;
AP_EXTERN_C f32 gRRReturnPos[3];
AP_EXTERN_C f32 gRRReturnAngle;
AP_EXTERN_C s32 gRRTrapTimer;

#define RR_TRAP_STAR_BIT 0x80000000


#undef AP_EXTERN_C

#ifdef __cplusplus
}
#endif

#endif
