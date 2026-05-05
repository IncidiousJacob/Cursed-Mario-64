

#include <PR/ultratypes.h>

#include "sm64ap.h"
#include <stdio.h>
#include "sm64.h"
#include "area.h"
#include "audio/data.h"
#include "audio/external.h"
#include "behavior_actions.h"
#include "behavior_data.h"
#include "camera.h"
#include "engine/graph_node.h"
#include "engine/math_util.h"
#include "engine/surface_collision.h"
#include "game_init.h"
#include "interaction.h"
#include "level_table.h"
#include "level_update.h"
#include "main.h"
#include "mario.h"
#include "mario_actions_airborne.h"
#include "mario_actions_automatic.h"
#include "mario_actions_cutscene.h"
#include "mario_actions_moving.h"
#include "mario_actions_object.h"
#include "mario_actions_stationary.h"
#include "mario_actions_submerged.h"
#include "mario_misc.h"
#include "mario_step.h"
#include "memory.h"
#include "object_fields.h"
#include "object_helpers.h"
#include "object_list_processor.h"
#include "print.h"
#include "save_file.h"
#include "sound_init.h"
#include "thread6.h"
#include "pc/configfile.h"
#include "pc/cheats.h"
#ifdef BETTERCAMERA
#include "bettercamera.h"
#endif

u32 unused80339F10;
s8 filler80339F1C[20];

/**************************************************
 *                    ANIMATIONS                  *
 **************************************************/

/**
 * Checks if Mario's animation has reached its end point.
 */
s32 is_anim_at_end(struct MarioState *m) {
    struct Object *o = m->marioObj;

    return (o->header.gfx.unk38.animFrame + 1) == o->header.gfx.unk38.curAnim->unk08;
}

/**
 * Checks if Mario's animation has surpassed 2 frames before its end point.
 */
s32 is_anim_past_end(struct MarioState *m) {
    struct Object *o = m->marioObj;

    return o->header.gfx.unk38.animFrame >= (o->header.gfx.unk38.curAnim->unk08 - 2);
}

/**
 * Sets Mario's animation without any acceleration, running at its default rate.
 */
s16 set_mario_animation(struct MarioState *m, s32 targetAnimID) {
    struct Object *o = m->marioObj;
    struct Animation *targetAnim = m->animation->targetAnim;

    if (load_patchable_table(m->animation, targetAnimID)) {
        targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
        targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
    }

    if (o->header.gfx.unk38.animID != targetAnimID) {
        o->header.gfx.unk38.animID = targetAnimID;
        o->header.gfx.unk38.curAnim = targetAnim;
        o->header.gfx.unk38.animAccel = 0;
        o->header.gfx.unk38.animYTrans = m->unkB0;

        if (targetAnim->flags & ANIM_FLAG_2) {
            o->header.gfx.unk38.animFrame = targetAnim->unk04;
        } else {
            if (targetAnim->flags & ANIM_FLAG_FORWARD) {
                o->header.gfx.unk38.animFrame = targetAnim->unk04 + 1;
            } else {
                o->header.gfx.unk38.animFrame = targetAnim->unk04 - 1;
            }
        }
    }

    return o->header.gfx.unk38.animFrame;
}

/**
 * Sets Mario's animation where the animation is sped up or
 * slowed down via acceleration.
 */
s16 set_mario_anim_with_accel(struct MarioState *m, s32 targetAnimID, s32 accel) {
    struct Object *o = m->marioObj;
    struct Animation *targetAnim = m->animation->targetAnim;

    if (load_patchable_table(m->animation, targetAnimID)) {
        targetAnim->values = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->values);
        targetAnim->index = (void *) VIRTUAL_TO_PHYSICAL((u8 *) targetAnim + (uintptr_t) targetAnim->index);
    }

    if (o->header.gfx.unk38.animID != targetAnimID) {
        o->header.gfx.unk38.animID = targetAnimID;
        o->header.gfx.unk38.curAnim = targetAnim;
        o->header.gfx.unk38.animYTrans = m->unkB0;

        if (targetAnim->flags & ANIM_FLAG_2) {
            o->header.gfx.unk38.animFrameAccelAssist = (targetAnim->unk04 << 0x10);
        } else {
            if (targetAnim->flags & ANIM_FLAG_FORWARD) {
                o->header.gfx.unk38.animFrameAccelAssist = (targetAnim->unk04 << 0x10) + accel;
            } else {
                o->header.gfx.unk38.animFrameAccelAssist = (targetAnim->unk04 << 0x10) - accel;
            }
        }

        o->header.gfx.unk38.animFrame = (o->header.gfx.unk38.animFrameAccelAssist >> 0x10);
    }

    o->header.gfx.unk38.animAccel = accel;

    return o->header.gfx.unk38.animFrame;
}

/**
 * Sets the animation to a specific "next" frame from the frame given.
 */
void set_anim_to_frame(struct MarioState *m, s16 animFrame) {
    struct GraphNodeObject_sub *animInfo = &m->marioObj->header.gfx.unk38;
    struct Animation *curAnim = animInfo->curAnim;

    if (animInfo->animAccel) {
        if (curAnim->flags & ANIM_FLAG_FORWARD) {
            animInfo->animFrameAccelAssist = (animFrame << 0x10) + animInfo->animAccel;
        } else {
            animInfo->animFrameAccelAssist = (animFrame << 0x10) - animInfo->animAccel;
        }
    } else {
        if (curAnim->flags & ANIM_FLAG_FORWARD) {
            animInfo->animFrame = animFrame + 1;
        } else {
            animInfo->animFrame = animFrame - 1;
        }
    }
}

s32 is_anim_past_frame(struct MarioState *m, s16 animFrame) {
    s32 isPastFrame;
    s32 acceleratedFrame = animFrame << 0x10;
    struct GraphNodeObject_sub *animInfo = &m->marioObj->header.gfx.unk38;
    struct Animation *curAnim = animInfo->curAnim;

    if (animInfo->animAccel) {
        if (curAnim->flags & ANIM_FLAG_FORWARD) {
            isPastFrame =
                (animInfo->animFrameAccelAssist > acceleratedFrame)
                && (acceleratedFrame >= (animInfo->animFrameAccelAssist - animInfo->animAccel));
        } else {
            isPastFrame =
                (animInfo->animFrameAccelAssist < acceleratedFrame)
                && (acceleratedFrame <= (animInfo->animFrameAccelAssist + animInfo->animAccel));
        }
    } else {
        if (curAnim->flags & ANIM_FLAG_FORWARD) {
            isPastFrame = (animInfo->animFrame == (animFrame + 1));
        } else {
            isPastFrame = ((animInfo->animFrame + 1) == animFrame);
        }
    }

    return isPastFrame;
}

/**
 * Rotates the animation's translation into the global coordinate system
 * and returns the animation's flags.
 */
s16 find_mario_anim_flags_and_translation(struct Object *obj, s32 yaw, Vec3s translation) {
    f32 dx;
    f32 dz;

    struct Animation *curAnim = (void *) obj->header.gfx.unk38.curAnim;
    s16 animFrame = geo_update_animation_frame(&obj->header.gfx.unk38, NULL);
    u16 *animIndex = segmented_to_virtual((void *) curAnim->index);
    s16 *animValues = segmented_to_virtual((void *) curAnim->values);

    f32 s = (f32) sins(yaw);
    f32 c = (f32) coss(yaw);

    dx = *(animValues + (retrieve_animation_index(animFrame, &animIndex))) / 4.0f;
    translation[1] = *(animValues + (retrieve_animation_index(animFrame, &animIndex))) / 4.0f;
    dz = *(animValues + (retrieve_animation_index(animFrame, &animIndex))) / 4.0f;

    translation[0] = (dx * c) + (dz * s);
    translation[2] = (-dx * s) + (dz * c);

    return curAnim->flags;
}

/**
 * Updates Mario's position from his animation's translation.
 */
void update_mario_pos_for_anim(struct MarioState *m) {
    Vec3s translation;
    s16 flags;

    flags = find_mario_anim_flags_and_translation(m->marioObj, m->faceAngle[1], translation);

    if (flags & (ANIM_FLAG_HOR_TRANS | ANIM_FLAG_6)) {
        m->pos[0] += (f32) translation[0];
        m->pos[2] += (f32) translation[2];
    }

    if (flags & (ANIM_FLAG_VERT_TRANS | ANIM_FLAG_6)) {
        m->pos[1] += (f32) translation[1];
    }
}

/**
 * Finds the vertical translation from Mario's animation.
 */
s16 return_mario_anim_y_translation(struct MarioState *m) {
    Vec3s translation;
    find_mario_anim_flags_and_translation(m->marioObj, 0, translation);

    return translation[1];
}

/**************************************************
 *                      AUDIO                     *
 **************************************************/

/**
 * Plays a sound if if Mario doesn't have the flag being checked.
 */
void play_sound_if_no_flag(struct MarioState *m, u32 soundBits, u32 flags) {
    if ((m->flags & flags) == 0) {
        play_sound(soundBits, m->marioObj->header.gfx.cameraToObject);
        m->flags |= flags;
    }
}

/**
 * Plays a jump sound if one has not been played since the last action change.
 */
void play_mario_jump_sound(struct MarioState *m) {
    if (!(m->flags & MARIO_MARIO_SOUND_PLAYED)) {
#ifndef VERSION_JP
        if (m->action == ACT_TRIPLE_JUMP) {
            play_sound(SOUND_MARIO_YAHOO_WAHA_YIPPEE + ((gAudioRandom % 5) << 16),
                       m->marioObj->header.gfx.cameraToObject);
        } else {
#endif
            play_sound(SOUND_MARIO_YAH_WAH_HOO + ((gAudioRandom % 3) << 16),
                       m->marioObj->header.gfx.cameraToObject);
#ifndef VERSION_JP
        }
#endif

        m->flags |= MARIO_MARIO_SOUND_PLAYED;
    }
}

/**
 * Adjusts the volume/pitch of sounds from Mario's speed.
 */
void adjust_sound_for_speed(struct MarioState *m) {
    s32 absForwardVel = (m->forwardVel > 0.0f) ? m->forwardVel : -m->forwardVel;
    func_80320A4C(1, (absForwardVel > 100) ? 100 : absForwardVel);
}

/**
 * Spawns particles if the step sound says to, then either plays a step sound or relevant other sound.
 */
void play_sound_and_spawn_particles(struct MarioState *m, u32 soundBits, u32 waveParticleType) {
    if (m->terrainSoundAddend == (SOUND_TERRAIN_WATER << 16)) {
        if (waveParticleType != 0) {
            m->particleFlags |= PARTICLE_SHALLOW_WATER_SPLASH;
        } else {
            m->particleFlags |= PARTICLE_SHALLOW_WATER_WAVE;
        }
    } else {
        if (m->terrainSoundAddend == (SOUND_TERRAIN_SAND << 16)) {
            m->particleFlags |= PARTICLE_DIRT;
        } else if (m->terrainSoundAddend == (SOUND_TERRAIN_SNOW << 16)) {
            m->particleFlags |= PARTICLE_SNOW;
        }
    }

    if ((m->flags & MARIO_METAL_CAP) || soundBits == SOUND_ACTION_UNSTUCK_FROM_GROUND
        || soundBits == SOUND_MARIO_PUNCH_HOO) {
        play_sound(soundBits, m->marioObj->header.gfx.cameraToObject);
    } else {
        play_sound(m->terrainSoundAddend + soundBits, m->marioObj->header.gfx.cameraToObject);
    }
}

/**
 * Plays an environmental sound if one has not been played since the last action change.
 */
void play_mario_action_sound(struct MarioState *m, u32 soundBits, u32 waveParticleType) {
    if ((m->flags & MARIO_ACTION_SOUND_PLAYED) == 0) {
        play_sound_and_spawn_particles(m, soundBits, waveParticleType);
        m->flags |= MARIO_ACTION_SOUND_PLAYED;
    }
}

/**
 * Plays a landing sound, accounting for metal cap.
 */
void play_mario_landing_sound(struct MarioState *m, u32 soundBits) {
    play_sound_and_spawn_particles(
        m, (m->flags & MARIO_METAL_CAP) ? SOUND_ACTION_METAL_LANDING : soundBits, 1);
}

/**
 * Plays a landing sound, accounting for metal cap. Unlike play_mario_landing_sound,
 * this function uses play_mario_action_sound, making sure the sound is only
 * played once per action.
 */
void play_mario_landing_sound_once(struct MarioState *m, u32 soundBits) {
    play_mario_action_sound(m, (m->flags & MARIO_METAL_CAP) ? SOUND_ACTION_METAL_LANDING : soundBits,
                            1);
}

/**
 * Plays a heavy landing (ground pound, etc.) sound, accounting for metal cap.
 */
void play_mario_heavy_landing_sound(struct MarioState *m, u32 soundBits) {
    play_sound_and_spawn_particles(
        m, (m->flags & MARIO_METAL_CAP) ? SOUND_ACTION_METAL_HEAVY_LANDING : soundBits, 1);
}

/**
 * Plays a heavy landing (ground pound, etc.) sound, accounting for metal cap.
 * Unlike play_mario_heavy_landing_sound, this function uses play_mario_action_sound,
 * making sure the sound is only played once per action.
 */
void play_mario_heavy_landing_sound_once(struct MarioState *m, u32 soundBits) {
    play_mario_action_sound(
        m, (m->flags & MARIO_METAL_CAP) ? SOUND_ACTION_METAL_HEAVY_LANDING : soundBits, 1);
}

/**
 * Plays action and Mario sounds relevant to what was passed into the function.
 */
void play_mario_sound(struct MarioState *m, s32 actionSound, s32 marioSound) {
    if (actionSound == SOUND_ACTION_TERRAIN_JUMP) {
        play_mario_action_sound(
                m, (m->flags & MARIO_METAL_CAP) ? (s32)SOUND_ACTION_METAL_JUMP
                                                : (s32)SOUND_ACTION_TERRAIN_JUMP, 1);
    } else {
        play_sound_if_no_flag(m, actionSound, MARIO_ACTION_SOUND_PLAYED);
    }

    if (marioSound == 0) {
        play_mario_jump_sound(m);
    }

    if (marioSound != -1) {
        play_sound_if_no_flag(m, marioSound, MARIO_MARIO_SOUND_PLAYED);
    }
}

/**************************************************
 *                     ACTIONS                    *
 **************************************************/

/**
 * Sets Mario's other velocities from his forward speed.
 */
void mario_set_forward_vel(struct MarioState *m, f32 forwardVel) {
    m->forwardVel = forwardVel;

    m->slideVelX = sins(m->faceAngle[1]) * m->forwardVel;
    m->slideVelZ = coss(m->faceAngle[1]) * m->forwardVel;

    m->vel[0] = (f32) m->slideVelX;
    m->vel[2] = (f32) m->slideVelZ;
}

/**
 * Returns the slipperiness class of Mario's floor.
 */
s32 mario_get_floor_class(struct MarioState *m) {
    s32 floorClass;

    // The slide terrain type defaults to slide slipperiness.
    // This doesn't matter too much since normally the slide terrain
    // is checked for anyways.
    if ((m->area->terrainType & TERRAIN_MASK) == TERRAIN_SLIDE) {
        floorClass = SURFACE_CLASS_VERY_SLIPPERY;
    } else {
        floorClass = SURFACE_CLASS_DEFAULT;
    }

    if (m->floor) {
        switch (m->floor->type) {
            case SURFACE_NOT_SLIPPERY:
            case SURFACE_HARD_NOT_SLIPPERY:
            case SURFACE_SWITCH:
                floorClass = SURFACE_CLASS_NOT_SLIPPERY;
                break;

            case SURFACE_SLIPPERY:
            case SURFACE_NOISE_SLIPPERY:
            case SURFACE_HARD_SLIPPERY:
            case SURFACE_NO_CAM_COL_SLIPPERY:
                floorClass = SURFACE_CLASS_SLIPPERY;
                break;

            case SURFACE_VERY_SLIPPERY:
            case SURFACE_ICE:
            case SURFACE_HARD_VERY_SLIPPERY:
            case SURFACE_NOISE_VERY_SLIPPERY_73:
            case SURFACE_NOISE_VERY_SLIPPERY_74:
            case SURFACE_NOISE_VERY_SLIPPERY:
            case SURFACE_NO_CAM_COL_VERY_SLIPPERY:
                floorClass = SURFACE_CLASS_VERY_SLIPPERY;
                break;
        }
    }

    // Crawling allows Mario to not slide on certain steeper surfaces.
    if (m->action == ACT_CRAWLING && m->floor->normal.y > 0.5f && floorClass == SURFACE_CLASS_DEFAULT) {
        floorClass = SURFACE_CLASS_NOT_SLIPPERY;
    }

    return floorClass;
}

// clang-format off
s8 sTerrainSounds[7][6] = {
    // default,              hard,                 slippery,
    // very slippery,        noisy default,        noisy slippery
    { SOUND_TERRAIN_DEFAULT, SOUND_TERRAIN_STONE,  SOUND_TERRAIN_GRASS,
      SOUND_TERRAIN_GRASS,   SOUND_TERRAIN_GRASS,  SOUND_TERRAIN_DEFAULT }, // TERRAIN_GRASS
    { SOUND_TERRAIN_STONE,   SOUND_TERRAIN_STONE,  SOUND_TERRAIN_STONE,
      SOUND_TERRAIN_STONE,   SOUND_TERRAIN_GRASS,  SOUND_TERRAIN_GRASS }, // TERRAIN_STONE
    { SOUND_TERRAIN_SNOW,    SOUND_TERRAIN_ICE,    SOUND_TERRAIN_SNOW,
      SOUND_TERRAIN_ICE,     SOUND_TERRAIN_STONE,  SOUND_TERRAIN_STONE }, // TERRAIN_SNOW
    { SOUND_TERRAIN_SAND,    SOUND_TERRAIN_STONE,  SOUND_TERRAIN_SAND,
      SOUND_TERRAIN_SAND,    SOUND_TERRAIN_STONE,  SOUND_TERRAIN_STONE }, // TERRAIN_SAND
    { SOUND_TERRAIN_SPOOKY,  SOUND_TERRAIN_SPOOKY, SOUND_TERRAIN_SPOOKY,
      SOUND_TERRAIN_SPOOKY,  SOUND_TERRAIN_STONE,  SOUND_TERRAIN_STONE }, // TERRAIN_SPOOKY
    { SOUND_TERRAIN_DEFAULT, SOUND_TERRAIN_STONE,  SOUND_TERRAIN_GRASS,
      SOUND_TERRAIN_ICE,     SOUND_TERRAIN_STONE,  SOUND_TERRAIN_ICE }, // TERRAIN_WATER
    { SOUND_TERRAIN_STONE,   SOUND_TERRAIN_STONE,  SOUND_TERRAIN_STONE,
      SOUND_TERRAIN_STONE,   SOUND_TERRAIN_ICE,    SOUND_TERRAIN_ICE }, // TERRAIN_SLIDE
};
// clang-format on

/**
 * Computes a value that should be added to terrain sounds before playing them.
 * This depends on surfaces and terrain.
 */
u32 mario_get_terrain_sound_addend(struct MarioState *m) {
    s16 floorSoundType;
    s16 terrainType = m->area->terrainType & TERRAIN_MASK;
    s32 ret = SOUND_TERRAIN_DEFAULT << 16;
    s32 floorType;

    if (m->floor) {
        floorType = m->floor->type;

        if ((gCurrLevelNum != LEVEL_LLL) && (m->floorHeight < (m->waterLevel - 10))) {
            // Water terrain sound, excluding LLL since it uses water in the volcano.
            ret = SOUND_TERRAIN_WATER << 16;
        } else if (SURFACE_IS_QUICKSAND(floorType)) {
            ret = SOUND_TERRAIN_SAND << 16;
        } else {
            switch (floorType) {
                default:
                    floorSoundType = 0;
                    break;

                case SURFACE_NOT_SLIPPERY:
                case SURFACE_HARD:
                case SURFACE_HARD_NOT_SLIPPERY:
                case SURFACE_SWITCH:
                    floorSoundType = 1;
                    break;

                case SURFACE_SLIPPERY:
                case SURFACE_HARD_SLIPPERY:
                case SURFACE_NO_CAM_COL_SLIPPERY:
                    floorSoundType = 2;
                    break;

                case SURFACE_VERY_SLIPPERY:
                case SURFACE_ICE:
                case SURFACE_HARD_VERY_SLIPPERY:
                case SURFACE_NOISE_VERY_SLIPPERY_73:
                case SURFACE_NOISE_VERY_SLIPPERY_74:
                case SURFACE_NOISE_VERY_SLIPPERY:
                case SURFACE_NO_CAM_COL_VERY_SLIPPERY:
                    floorSoundType = 3;
                    break;

                case SURFACE_NOISE_DEFAULT:
                    floorSoundType = 4;
                    break;

                case SURFACE_NOISE_SLIPPERY:
                    floorSoundType = 5;
                    break;
            }

            ret = sTerrainSounds[terrainType][floorSoundType] << 16;
        }
    }

    return ret;
}

/**
 * Collides with walls and returns the most recent wall.
 */
struct Surface *resolve_and_return_wall_collisions(Vec3f pos, f32 offset, f32 radius) {
    struct WallCollisionData collisionData;
    struct Surface *wall = NULL;

    collisionData.x = pos[0];
    collisionData.y = pos[1];
    collisionData.z = pos[2];
    collisionData.radius = radius;
    collisionData.offsetY = offset;

    if (find_wall_collisions(&collisionData)) {
        wall = collisionData.walls[collisionData.numWalls - 1];
    }

    pos[0] = collisionData.x;
    pos[1] = collisionData.y;
    pos[2] = collisionData.z;

    // This only returns the most recent wall and can also return NULL
    // there are no wall collisions.
    return wall;
}

/**
 * Finds the ceiling from a vec3f horizontally and a height (with 80 vertical buffer).
 */
f32 vec3f_find_ceil(Vec3f pos, f32 height, struct Surface **ceil) {
    UNUSED f32 unused;

    return find_ceil(pos[0], height + 80.0f, pos[2], ceil);
}

/**
 * Determines if Mario is facing "downhill."
 */
s32 mario_facing_downhill(struct MarioState *m, s32 turnYaw) {
    s16 faceAngleYaw = m->faceAngle[1];

    // This is never used in practice, as turnYaw is
    // always passed as zero.
    if (turnYaw && m->forwardVel < 0.0f) {
        faceAngleYaw += 0x8000;
    }

    faceAngleYaw = m->floorAngle - faceAngleYaw;

    return (-0x4000 < faceAngleYaw) && (faceAngleYaw < 0x4000);
}

/**
 * Determines if a surface is slippery based on the surface class.
 */
u32 mario_floor_is_slippery(struct MarioState *m) {
    f32 normY;

    if ((m->area->terrainType & TERRAIN_MASK) == TERRAIN_SLIDE
        && m->floor->normal.y < 0.9998477f //~cos(1 deg)
    ) {
        return TRUE;
    }

    switch (mario_get_floor_class(m)) {
        case SURFACE_VERY_SLIPPERY:
            normY = 0.9848077f; //~cos(10 deg)
            break;

        case SURFACE_SLIPPERY:
            normY = 0.9396926f; //~cos(20 deg)
            break;

        default:
            normY = 0.7880108f; //~cos(38 deg)
            break;

        case SURFACE_NOT_SLIPPERY:
            normY = 0.0f;
            break;
    }

    return m->floor->normal.y <= normY;
}

/**
 * Determines if a surface is a slope based on the surface class.
 */
s32 mario_floor_is_slope(struct MarioState *m) {
    f32 normY;

    if ((m->area->terrainType & TERRAIN_MASK) == TERRAIN_SLIDE
        && m->floor->normal.y < 0.9998477f //~cos(1 deg)
    ) {
        return TRUE;
    }

    switch (mario_get_floor_class(m)) {
        case SURFACE_VERY_SLIPPERY:
            normY = 0.9961947f; //~cos(5 deg)
            break;

        case SURFACE_SLIPPERY:
            normY = 0.9848077f; //~cos(10 deg)
            break;

        default:
            normY = 0.9659258f; //~cos(15 deg)
            break;

        case SURFACE_NOT_SLIPPERY:
            normY = 0.9396926f; //~cos(20 deg)
            break;
    }

    return m->floor->normal.y <= normY;
}

/**
 * Determines if a surface is steep based on the surface class.
 */
s32 mario_floor_is_steep(struct MarioState *m) {
    f32 normY;
    s32 result = FALSE;

    // Interestingly, this function does not check for the
    // slide terrain type. This means that steep behavior persists for
    // non-slippery and slippery surfaces.
    // This does not matter in vanilla game practice.
    if (!mario_facing_downhill(m, FALSE)) {
        switch (mario_get_floor_class(m)) {
            case SURFACE_VERY_SLIPPERY:
                normY = 0.9659258f; //~cos(15 deg)
                break;

            case SURFACE_SLIPPERY:
                normY = 0.9396926f; //~cos(20 deg)
                break;

            default:
                normY = 0.8660254f; //~cos(30 deg)
                break;

            case SURFACE_NOT_SLIPPERY:
                normY = 0.8660254f; //~cos(30 deg)
                break;
        }

        result = m->floor->normal.y <= normY;
    }

    return result;
}

/**
 * Finds the floor height relative from Mario given polar displacement.
 */
f32 find_floor_height_relative_polar(struct MarioState *m, s16 angleFromMario, f32 distFromMario) {
    struct Surface *floor;
    f32 floorY;

    f32 y = sins(m->faceAngle[1] + angleFromMario) * distFromMario;
    f32 x = coss(m->faceAngle[1] + angleFromMario) * distFromMario;

    floorY = find_floor(m->pos[0] + y, m->pos[1] + 100.0f, m->pos[2] + x, &floor);

    return floorY;
}

/**
 * Returns the slope of the floor based off points around Mario.
 */
s16 find_floor_slope(struct MarioState *m, s16 yawOffset) {
    struct Surface *floor;
    f32 forwardFloorY, backwardFloorY;
    f32 forwardYDelta, backwardYDelta;
    s16 result;

    f32 x = sins(m->faceAngle[1] + yawOffset) * 5.0f;
    f32 z = coss(m->faceAngle[1] + yawOffset) * 5.0f;

    forwardFloorY = find_floor(m->pos[0] + x, m->pos[1] + 100.0f, m->pos[2] + z, &floor);
    backwardFloorY = find_floor(m->pos[0] - x, m->pos[1] + 100.0f, m->pos[2] - z, &floor);

    //! If Mario is near OOB, these floorY's can sometimes be -11000.
    //  This will cause these to be off and give improper slopes.
    forwardYDelta = forwardFloorY - m->pos[1];
    backwardYDelta = m->pos[1] - backwardFloorY;

    if (forwardYDelta * forwardYDelta < backwardYDelta * backwardYDelta) {
        result = atan2s(5.0f, forwardYDelta);
    } else {
        result = atan2s(5.0f, backwardYDelta);
    }

    return result;
}

/**
 * Adjusts Mario's camera and sound based on his action status.
 */
void update_mario_sound_and_camera(struct MarioState *m) {
    u32 action = m->action;
    s32 camPreset = m->area->camera->mode;

    if (action == ACT_FIRST_PERSON) {
        raise_background_noise(2);
        gCameraMovementFlags &= ~CAM_MOVE_C_UP_MODE;
        // Go back to the last camera mode
        set_camera_mode(m->area->camera, -1, 1);
    } else if (action == ACT_SLEEPING) {
        raise_background_noise(2);
    }

    if (!(action & (ACT_FLAG_SWIMMING | ACT_FLAG_METAL_WATER))) {
        if (camPreset == CAMERA_MODE_BEHIND_MARIO || camPreset == CAMERA_MODE_WATER_SURFACE) {
            set_camera_mode(m->area->camera, m->area->camera->defMode, 1);
        }
    }
}

/**
 * Transitions Mario to a steep jump action.
 */
void set_steep_jump_action(struct MarioState *m) {
    m->marioObj->oMarioSteepJumpYaw = m->faceAngle[1];

    if (m->forwardVel > 0.0f) {
        //! ((s16)0x8000) has undefined behavior. Therefore, this downcast has
        // undefined behavior if m->floorAngle >= 0.
        s16 angleTemp = m->floorAngle + 0x8000;
        s16 faceAngleTemp = m->faceAngle[1] - angleTemp;

        f32 y = sins(faceAngleTemp) * m->forwardVel;
        f32 x = coss(faceAngleTemp) * m->forwardVel * 0.75f;

        m->forwardVel = sqrtf(y * y + x * x);
        m->faceAngle[1] = atan2s(x, y) + angleTemp;
    }

    drop_and_set_mario_action(m, ACT_STEEP_JUMP, 0);
}

/**
 * Sets Mario's vertical speed from his forward speed.
 */
static void set_mario_y_vel_based_on_fspeed(struct MarioState *m, f32 initialVelY, f32 multiplier) {
    // get_additive_y_vel_for_jumps is always 0 and a stubbed function.
    // It was likely trampoline related based on code location.
    m->vel[1] = initialVelY + get_additive_y_vel_for_jumps() + m->forwardVel * multiplier;

    if (m->squishTimer != 0 || m->quicksandDepth > 1.0f) {
        m->vel[1] *= 0.5f;
    }
}

/**
 * Transitions for a variety of airborne actions.
 */
static u32 set_mario_action_airborne(struct MarioState *m, u32 action, u32 actionArg) {
    f32 fowardVel;

    if (m->squishTimer != 0 || m->quicksandDepth >= 1.0f) {
        if (action == ACT_DOUBLE_JUMP || action == ACT_TWIRLING) {
            action = ACT_JUMP;
        }
    }

    switch (action) {
        case ACT_DOUBLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 52.0f, 0.25f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_BACKFLIP:
            m->marioObj->header.gfx.unk38.animID = -1;
            m->forwardVel = -16.0f;
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            break;

        case ACT_TRIPLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 69.0f, 0.0f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_FLYING_TRIPLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 82.0f, 0.0f);
            break;

        case ACT_WATER_JUMP:
        case ACT_HOLD_WATER_JUMP:
            if (actionArg == 0) {
                set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.0f);
            }
            break;

        case ACT_BURNING_JUMP:
            m->vel[1] = 31.5f;
            m->forwardVel = 8.0f;
            break;

        case ACT_RIDING_SHELL_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            break;

        case ACT_JUMP:
        case ACT_HOLD_JUMP:
            m->marioObj->header.gfx.unk38.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            m->forwardVel *= 0.8f;
            break;

        case ACT_WALL_KICK_AIR:
        case ACT_TOP_OF_POLE_JUMP:
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            if (m->forwardVel < 24.0f) {
                m->forwardVel = 24.0f;
            }
            m->wallKickTimer = 0;
            break;

        case ACT_SIDE_FLIP:
            set_mario_y_vel_based_on_fspeed(m, 62.0f, 0.0f);
            m->forwardVel = 8.0f;
            m->faceAngle[1] = m->intendedYaw;
            break;

        case ACT_STEEP_JUMP:
            m->marioObj->header.gfx.unk38.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 42.0f, 0.25f);
            m->faceAngle[0] = -0x2000;
            break;

        case ACT_LAVA_BOOST:
            m->vel[1] = 84.0f;
            if (actionArg == 0) {
                m->forwardVel = 0.0f;
            }
            break;

        case ACT_DIVE:
            if ((fowardVel = m->forwardVel + 15.0f) > 48.0f) {
                fowardVel = 48.0f;
            }
            mario_set_forward_vel(m, fowardVel);
            break;

        case ACT_LONG_JUMP:
            m->marioObj->header.gfx.unk38.animID = -1;
            set_mario_y_vel_based_on_fspeed(m, 30.0f, 0.0f);
            m->marioObj->oMarioLongJumpIsSlow = m->forwardVel > 16.0f ? FALSE : TRUE;

            //! (BLJ's) This properly handles long jumps from getting forward speed with
            //  too much velocity, but misses backwards longs allowing high negative speeds.
            if ((m->forwardVel *= 1.5f) > 48.0f) {
                m->forwardVel = 48.0f;
            }
            break;

        case ACT_SLIDE_KICK:
            m->vel[1] = 12.0f;
            if (m->forwardVel < 32.0f) {
                m->forwardVel = 32.0f;
            }
            break;

        case ACT_JUMP_KICK:
            m->vel[1] = 20.0f;
            break;
    }

    m->peakHeight = m->pos[1];
    m->flags |= MARIO_UNKNOWN_08;

    return action;
}

/**
 * Transitions for a variety of moving actions.
 */
static u32 set_mario_action_moving(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    s16 floorClass = mario_get_floor_class(m);
    f32 forwardVel = m->forwardVel;
    f32 mag = min(m->intendedMag, 8.0f);

    switch (action) {
        case ACT_WALKING:
            if (floorClass != SURFACE_CLASS_VERY_SLIPPERY) {
                if (0.0f <= forwardVel && forwardVel < mag) {
                    m->forwardVel = mag;
                }
            }

            m->marioObj->oMarioWalkingPitch = 0;
            break;

        case ACT_HOLD_WALKING:
            if (0.0f <= forwardVel && forwardVel < mag / 2.0f) {
                m->forwardVel = mag / 2.0f;
            }
            break;

        case ACT_BEGIN_SLIDING:
            if (mario_facing_downhill(m, FALSE)) {
                action = ACT_BUTT_SLIDE;
            } else {
                action = ACT_STOMACH_SLIDE;
            }
            break;

        case ACT_HOLD_BEGIN_SLIDING:
            if (mario_facing_downhill(m, FALSE)) {
                action = ACT_HOLD_BUTT_SLIDE;
            } else {
                action = ACT_HOLD_STOMACH_SLIDE;
            }
            break;
    }

    return action;
}

/**
 * Transition for certain submerged actions, which is actually just the metal jump actions.
 */
static u32 set_mario_action_submerged(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    if (action == ACT_METAL_WATER_JUMP || action == ACT_HOLD_METAL_WATER_JUMP) {
        m->vel[1] = 32.0f;
    }

    return action;
}

/**
 * Transitions for a variety of cutscene actions.
 */
static u32 set_mario_action_cutscene(struct MarioState *m, u32 action, UNUSED u32 actionArg) {
    switch (action) {
        case ACT_EMERGE_FROM_PIPE:
            m->vel[1] = 52.0f;
            break;

        case ACT_FALL_AFTER_STAR_GRAB:
            mario_set_forward_vel(m, 0.0f);
            break;

        case ACT_SPAWN_SPIN_AIRBORNE:
            mario_set_forward_vel(m, 2.0f);
            break;

        case ACT_SPECIAL_EXIT_AIRBORNE:
        case ACT_SPECIAL_DEATH_EXIT:
            m->vel[1] = 64.0f;
            break;
    }

    return action;
}

/**
 * Puts Mario into a given action, putting Mario through the appropriate
 * specific function if needed.
 */
u32 set_mario_action(struct MarioState *m, u32 action, u32 actionArg) {
    // Intercepting and replacing Mario's jumps if not yet unlocked.
    if (   (action == ACT_DOUBLE_JUMP && !SM64AP_CanDoubleJump())
        || (action == ACT_TRIPLE_JUMP && !SM64AP_CanTripleJump())
        || (action == ACT_FLYING_TRIPLE_JUMP && !SM64AP_CanTripleJump())
        || (action == ACT_BACKFLIP && !SM64AP_CanBackflip())
        || (action == ACT_LONG_JUMP && !SM64AP_CanLongJump())
        || (action == ACT_SIDE_FLIP && !SM64AP_CanSideFlip())
    ) {
        action = ACT_JUMP;
    }
    switch (action & ACT_GROUP_MASK) {
        case ACT_GROUP_MOVING:
            action = set_mario_action_moving(m, action, actionArg);
            break;

        case ACT_GROUP_AIRBORNE:
            action = set_mario_action_airborne(m, action, actionArg);
            break;

        case ACT_GROUP_SUBMERGED:
            if (!SM64AP_CanSwim() && !(m->flags & MARIO_METAL_CAP) && action != ACT_WATER_DEATH && action != ACT_DROWNING) {
                action = ACT_WATER_DEATH;
            } else {
                action = set_mario_action_submerged(m, action, actionArg);
            }
            break;

        case ACT_GROUP_CUTSCENE:
            action = set_mario_action_cutscene(m, action, actionArg);
            break;
    }

    // Resets the sound played flags, meaning Mario can play those sound types again.
    m->flags &= ~(MARIO_ACTION_SOUND_PLAYED | MARIO_MARIO_SOUND_PLAYED);

    if (!(m->action & ACT_FLAG_AIR)) {
        m->flags &= ~MARIO_UNKNOWN_18;
    }

    // Initialize the action information.
    m->prevAction = m->action;
    m->action = action;
    m->actionArg = actionArg;
    m->actionState = 0;
    m->actionTimer = 0;

    return TRUE;
}

/**
 * Puts Mario into a specific jumping action from a landing action.
 */
s32 set_jump_from_landing(struct MarioState *m) {
    if (m->quicksandDepth >= 11.0f) {
        if (m->heldObj == NULL) {
            return set_mario_action(m, ACT_QUICKSAND_JUMP_LAND, 0);
        } else {
            return set_mario_action(m, ACT_HOLD_QUICKSAND_JUMP_LAND, 0);
        }
    }

    if (mario_floor_is_steep(m)) {
        set_steep_jump_action(m);
    } else {
        if ((m->doubleJumpTimer == 0) || (m->squishTimer != 0)) {
            set_mario_action(m, ACT_JUMP, 0);
        } else {
            switch (m->prevAction) {
                case ACT_JUMP_LAND:
                    set_mario_action(m, ACT_DOUBLE_JUMP, 0);
                    break;

                case ACT_FREEFALL_LAND:
                    set_mario_action(m, ACT_DOUBLE_JUMP, 0);
                    break;

                case ACT_SIDE_FLIP_LAND_STOP:
                    set_mario_action(m, ACT_DOUBLE_JUMP, 0);
                    break;

                case ACT_DOUBLE_JUMP_LAND:
                    // If Mario has a wing cap, he ignores the typical speed
                    // requirement for a triple jump.
                    if (m->flags & MARIO_WING_CAP) {
                        set_mario_action(m, ACT_FLYING_TRIPLE_JUMP, 0);
                    } else if (m->forwardVel > 20.0f) {
                        set_mario_action(m, ACT_TRIPLE_JUMP, 0);
                    } else {
                        set_mario_action(m, ACT_JUMP, 0);
                    }
                    break;

                default:
                    set_mario_action(m, ACT_JUMP, 0);
                    break;
            }
        }
    }

    m->doubleJumpTimer = 0;

    return TRUE;
}

/**
 * Puts Mario in a given action, as long as it is not overruled by
 * either a quicksand or steep jump.
 */
s32 set_jumping_action(struct MarioState *m, u32 action, u32 actionArg) {
    UNUSED u32 currAction = m->action;

    if (m->quicksandDepth >= 11.0f) {
        // Checks whether Mario is holding an object or not.
        if (m->heldObj == NULL) {
            return set_mario_action(m, ACT_QUICKSAND_JUMP_LAND, 0);
        } else {
            return set_mario_action(m, ACT_HOLD_QUICKSAND_JUMP_LAND, 0);
        }
    }

    if (mario_floor_is_steep(m)) {
        set_steep_jump_action(m);
    } else {
        set_mario_action(m, action, actionArg);
    }

    return TRUE;
}

/**
 * Drop anything Mario is holding and set a new action.
 */
s32 drop_and_set_mario_action(struct MarioState *m, u32 action, u32 actionArg) {
    mario_stop_riding_and_holding(m);

    return set_mario_action(m, action, actionArg);
}

/**
 * Increment Mario's hurt counter and set a new action.
 */
s32 hurt_and_set_mario_action(struct MarioState *m, u32 action, u32 actionArg, s16 hurtCounter) {
    m->hurtCounter = hurtCounter;

    return set_mario_action(m, action, actionArg);
}

/**
 * Checks a variety of inputs for common transitions between many different
 * actions. A common variant of the below function.
 */
s32 check_common_action_exits(struct MarioState *m) {
    if (m->input & INPUT_A_PRESSED) {
        return set_mario_action(m, ACT_JUMP, 0);
    }
    if (m->input & INPUT_OFF_FLOOR) {
        return set_mario_action(m, ACT_FREEFALL, 0);
    }
    if (m->input & INPUT_NONZERO_ANALOG) {
        return set_mario_action(m, ACT_WALKING, 0);
    }
    if (m->input & INPUT_ABOVE_SLIDE) {
        return set_mario_action(m, ACT_BEGIN_SLIDING, 0);
    }

    return FALSE;
}

/**
 * Checks a variety of inputs for common transitions between many different
 * object holding actions. A holding variant of the above function.
 */
s32 check_common_hold_action_exits(struct MarioState *m) {
    if (m->input & INPUT_A_PRESSED) {
        return set_mario_action(m, ACT_HOLD_JUMP, 0);
    }
    if (m->input & INPUT_OFF_FLOOR) {
        return set_mario_action(m, ACT_HOLD_FREEFALL, 0);
    }
    if (m->input & INPUT_NONZERO_ANALOG) {
        return set_mario_action(m, ACT_HOLD_WALKING, 0);
    }
    if (m->input & INPUT_ABOVE_SLIDE) {
        return set_mario_action(m, ACT_HOLD_BEGIN_SLIDING, 0);
    }

    return FALSE;
}

/**
 * Transitions Mario from a submerged action to a walking action.
 */
s32 transition_submerged_to_walking(struct MarioState *m) {
    set_camera_mode(m->area->camera, m->area->camera->defMode, 1);

    vec3s_set(m->angleVel, 0, 0, 0);

    if (m->heldObj == NULL) {
        return set_mario_action(m, ACT_WALKING, 0);
    } else {
        return set_mario_action(m, ACT_HOLD_WALKING, 0);
    }
}

/**
 * This is the transition function typically for entering a submerged action for a
 * non-submerged action. This also applies the water surface camera preset.
 */
s32 set_water_plunge_action(struct MarioState *m) {
    m->forwardVel = m->forwardVel / 4.0f;
    m->vel[1] = m->vel[1] / 2.0f;

    m->pos[1] = m->waterLevel - 100;

    m->faceAngle[2] = 0;

    vec3s_set(m->angleVel, 0, 0, 0);

    if ((m->action & ACT_FLAG_DIVING) == 0) {
        m->faceAngle[0] = 0;
    }

    if (m->area->camera->mode != CAMERA_MODE_WATER_SURFACE) {
        set_camera_mode(m->area->camera, CAMERA_MODE_WATER_SURFACE, 1);
    }

    return set_mario_action(m, ACT_WATER_PLUNGE, 0);
}

/**
 * These are the scaling values for the x and z axis for Mario
 * when he is close to unsquishing.
 */
u8 sSquishScaleOverTime[16] = { 0x46, 0x32, 0x32, 0x3C, 0x46, 0x50, 0x50, 0x3C,
                                0x28, 0x14, 0x14, 0x1E, 0x32, 0x3C, 0x3C, 0x28 };

/**
 * Applies the squish to Mario's model via scaling.
 */
void squish_mario_model(struct MarioState *m) {
    if (m->squishTimer != 0xFF) {
        // If no longer squished, scale back to default.
        // Also handles the Tiny Mario and Huge Mario cheats.
        if (m->squishTimer == 0) {
            if (Cheats.EnableCheats) {
                if (Cheats.HugeMario) {
                    vec3f_set(m->marioObj->header.gfx.scale, 2.5f, 2.5f, 2.5f);
                }
                else if (Cheats.TinyMario) {
                    vec3f_set(m->marioObj->header.gfx.scale, 0.2f, 0.2f, 0.2f);
                }
                else {
                    vec3f_set(m->marioObj->header.gfx.scale, 1.0f, 1.0f, 1.0f);
                }
            }
            else {
                vec3f_set(m->marioObj->header.gfx.scale, 1.0f, 1.0f, 1.0f);
            }
            
        }
        // If timer is less than 16, rubber-band Mario's size scale up and down.
        else if (m->squishTimer <= 16) {
            m->squishTimer -= 1;

            m->marioObj->header.gfx.scale[1] =
                1.0f - ((sSquishScaleOverTime[15 - m->squishTimer] * 0.6f) / 100.0f);
            m->marioObj->header.gfx.scale[0] =
                ((sSquishScaleOverTime[15 - m->squishTimer] * 0.4f) / 100.0f) + 1.0f;

            m->marioObj->header.gfx.scale[2] = m->marioObj->header.gfx.scale[0];
        } else {
            m->squishTimer -= 1;

            vec3f_set(m->marioObj->header.gfx.scale, 1.4f, 0.4f, 1.4f);
        }
    }
}

/**
 * Debug function that prints floor normal, velocity, and action information.
 */
void debug_print_speed_action_normal(struct MarioState *m) {
    f32 steepness;
    f32 floor_nY;

    if (gShowDebugText) {
        steepness = sqrtf(
            ((m->floor->normal.x * m->floor->normal.x) + (m->floor->normal.z * m->floor->normal.z)));
        floor_nY = m->floor->normal.y;

        print_text_fmt_int(210, 88, "ANG %d", (atan2s(floor_nY, steepness) * 180.0f) / 32768.0f);

        print_text_fmt_int(210, 72, "SPD %d", m->forwardVel);

        // STA short for "status," the official action name via SMS map.
        print_text_fmt_int(210, 56, "STA %x", (m->action & ACT_ID_MASK));
    }
}

/**
 * Update the button inputs for Mario.
 */
void update_mario_button_inputs(struct MarioState *m) {
    if (m->controller->buttonPressed & A_BUTTON) {
        m->input |= INPUT_A_PRESSED;
    }

    if (m->controller->buttonDown & A_BUTTON) {
        m->input |= INPUT_A_DOWN;
    }

    // Don't update for these buttons if squished.
    if (m->squishTimer == 0) {
        if (m->controller->buttonPressed & B_BUTTON) {
            m->input |= INPUT_B_PRESSED;
        }

        if (m->controller->buttonDown & Z_TRIG) {
            m->input |= INPUT_Z_DOWN;
        }

        if (m->controller->buttonPressed & Z_TRIG) {
            m->input |= INPUT_Z_PRESSED;
        }
    }

    if (m->input & INPUT_A_PRESSED) {
        m->framesSinceA = 0;
    } else if (m->framesSinceA < 0xFF) {
        m->framesSinceA += 1;
    }

    if (m->input & INPUT_B_PRESSED) {
        m->framesSinceB = 0;
    } else if (m->framesSinceB < 0xff) {
        m->framesSinceB += 1;
    }
}

/**
 * Updates the joystick intended magnitude.
 */
void update_mario_joystick_inputs(struct MarioState *m) {
    struct Controller *controller = m->controller;
    f32 mag = ((controller->stickMag / 64.0f) * (controller->stickMag / 64.0f)) * 64.0f;

    if (m->squishTimer == 0) {
        m->intendedMag = mag / 2.0f;
    } else {
        m->intendedMag = mag / 8.0f;
    }

    if (m->intendedMag > 0.0f) {
#ifndef BETTERCAMERA
        m->intendedYaw = atan2s(-controller->stickY, controller->stickX) + m->area->camera->yaw;
#else
        if (gLakituState.mode != CAMERA_MODE_NEWCAM)
            m->intendedYaw = atan2s(-controller->stickY, controller->stickX) + m->area->camera->yaw;
        else
            m->intendedYaw = atan2s(-controller->stickY, controller->stickX)-newcam_yaw+0x4000;
#endif
        m->input |= INPUT_NONZERO_ANALOG;
    } else {
        m->intendedYaw = m->faceAngle[1];
    }
}

/**
 * Resolves wall collisions, and updates a variety of inputs.
 */
void update_mario_geometry_inputs(struct MarioState *m) {
    f32 gasLevel;
    f32 ceilToFloorDist;

    f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 60.0f, 50.0f);
    f32_find_wall_collision(&m->pos[0], &m->pos[1], &m->pos[2], 30.0f, 24.0f);

    m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);

    // If Mario is OOB, move his position to his graphical position (which was not updated)
    // and check for the floor there.
    // This can cause errant behavior when combined with astral projection,
    // since the graphical position was not Mario's previous location.
    if (m->floor == NULL) {
        vec3f_copy(m->pos, m->marioObj->header.gfx.pos);
        m->floorHeight = find_floor(m->pos[0], m->pos[1], m->pos[2], &m->floor);
    }

    m->ceilHeight = vec3f_find_ceil(&m->pos[0], m->floorHeight, &m->ceil);
    gasLevel = find_poison_gas_level(m->pos[0], m->pos[2]);
    m->waterLevel = find_water_level(m->pos[0], m->pos[2]);

    if (m->floor) {
        m->floorAngle = atan2s(m->floor->normal.z, m->floor->normal.x);
        m->terrainSoundAddend = mario_get_terrain_sound_addend(m);

        if ((m->pos[1] > m->waterLevel - 40) && mario_floor_is_slippery(m)) {
            m->input |= INPUT_ABOVE_SLIDE;
        }

        if ((m->floor->flags & SURFACE_FLAG_DYNAMIC)
            || (m->ceil && m->ceil->flags & SURFACE_FLAG_DYNAMIC)) {
            ceilToFloorDist = m->ceilHeight - m->floorHeight;

            if ((0.0f <= ceilToFloorDist) && (ceilToFloorDist <= 150.0f)) {
                m->input |= INPUT_SQUISHED;
            }
        }

        if (m->pos[1] > m->floorHeight + 100.0f) {
            m->input |= INPUT_OFF_FLOOR;
        }

        if (m->pos[1] < (m->waterLevel - 10)) {
            m->input |= INPUT_IN_WATER;
        }

        if (m->pos[1] < (gasLevel - 100.0f)) {
            m->input |= INPUT_IN_POISON_GAS;
        }

    } else {
        level_trigger_warp(m, WARP_OP_DEATH);
    }
}

/**
 * Handles Mario's input flags as well as a couple timers.
 */
void update_mario_inputs(struct MarioState *m) {
    m->particleFlags = 0;
    m->input = 0;
    m->collidedObjInteractTypes = m->marioObj->collidedObjInteractTypes;
    m->flags &= 0xFFFFFF;

    update_mario_button_inputs(m);
    update_mario_joystick_inputs(m);
    update_mario_geometry_inputs(m);

    debug_print_speed_action_normal(m);
    
    /* Moonjump cheat */
    while (Cheats.MoonJump == true && Cheats.EnableCheats == true && m->controller->buttonDown & L_TRIG ){
        m->vel[1] = 25;
        break;   // TODO: Unneeded break?
    }
    /*End of moonjump cheat */

    if (gCameraMovementFlags & CAM_MOVE_C_UP_MODE) {
        if (m->action & ACT_FLAG_ALLOW_FIRST_PERSON) {
            m->input |= INPUT_FIRST_PERSON;
        } else {
            gCameraMovementFlags &= ~CAM_MOVE_C_UP_MODE;
        }
    }

    if (!(m->input & (INPUT_NONZERO_ANALOG | INPUT_A_PRESSED))) {
        m->input |= INPUT_UNKNOWN_5;
    }

    if (m->marioObj->oInteractStatus
        & (INT_STATUS_HOOT_GRABBED_BY_MARIO | INT_STATUS_MARIO_UNK1 | INT_STATUS_MARIO_UNK4)) {
        m->input |= INPUT_UNKNOWN_10;
    }

    // This function is located near other unused trampoline functions,
    // perhaps logically grouped here with the timers.
    stub_mario_step_1(m);

    if (m->wallKickTimer > 0) {
        m->wallKickTimer--;
    }

    if (m->doubleJumpTimer > 0) {
        m->doubleJumpTimer--;
    }
}

/**
 * Set's the camera preset for submerged action behaviors.
 */
void set_submerged_cam_preset_and_spawn_bubbles(struct MarioState *m) {
    f32 heightBelowWater;
    s16 camPreset;

    if ((m->action & ACT_GROUP_MASK) == ACT_GROUP_SUBMERGED) {
        heightBelowWater = (f32)(m->waterLevel - 80) - m->pos[1];
        camPreset = m->area->camera->mode;

        if ((m->action & ACT_FLAG_METAL_WATER)) {
            if (camPreset != CAMERA_MODE_CLOSE) {
                set_camera_mode(m->area->camera, CAMERA_MODE_CLOSE, 1);
            }
        } else {
            if ((heightBelowWater > 800.0f) && (camPreset != CAMERA_MODE_BEHIND_MARIO)) {
                set_camera_mode(m->area->camera, CAMERA_MODE_BEHIND_MARIO, 1);
            }

            if ((heightBelowWater < 400.0f) && (camPreset != CAMERA_MODE_WATER_SURFACE)) {
                set_camera_mode(m->area->camera, CAMERA_MODE_WATER_SURFACE, 1);
            }

            // As long as Mario isn't drowning or at the top
            // of the water with his head out, spawn bubbles.
            if ((m->action & ACT_FLAG_INTANGIBLE) == 0) {
                if ((m->pos[1] < (f32)(m->waterLevel - 160)) || (m->faceAngle[0] < -0x800)) {
                    m->particleFlags |= PARTICLE_BUBBLE;
                }
            }
        }
    }
}

/**
 * Both increments and decrements Mario's HP.
 */
void update_mario_health(struct MarioState *m) {
    s32 terrainIsSnow;

    if (m->health >= 0x100) {
        // When already healing or hurting Mario, Mario's HP is not changed any more here.
        if (((u32) m->healCounter | (u32) m->hurtCounter) == 0) {
            if ((m->input & INPUT_IN_POISON_GAS) && ((m->action & ACT_FLAG_INTANGIBLE) == 0)) {
                if (((m->flags & MARIO_METAL_CAP) == 0) && (gDebugLevelSelect == 0)) {
                    m->health -= 4;
                }
            } else {
                if ((m->action & ACT_FLAG_SWIMMING) && ((m->action & ACT_FLAG_INTANGIBLE) == 0)) {
                    terrainIsSnow = (m->area->terrainType & TERRAIN_MASK) == TERRAIN_SNOW;

                    // When Mario is near the water surface, recover health (unless in snow),
                    // when in snow terrains lose 3 health.
                    // If using the debug level select, do not lose any HP to water.
                    if ((m->pos[1] >= (m->waterLevel - 140)) && !terrainIsSnow) {
                        m->health += 0x1A;
                    } else if (gDebugLevelSelect == 0) {
                        m->health -= (terrainIsSnow ? 3 : 1);
                    }
                }
            }
        }

        if (m->healCounter > 0) {
            m->health += 0x40;
            m->healCounter--;
        }
        if (m->hurtCounter > 0) {
            m->health -= 0x40;
            m->hurtCounter--;
        }

        if (m->health >= 0x881) {
            m->health = 0x880;
        }
        if (m->health < 0x100) {
            m->health = 0xFF;
        }

        // Play a noise to alert the player when Mario is close to drowning.
        if (((m->action & ACT_GROUP_MASK) == ACT_GROUP_SUBMERGED) && (m->health < 0x300)) {
            play_sound(SOUND_MOVING_ALMOST_DROWNING, gDefaultSoundArgs);
            if (!gRumblePakTimer) {
                gRumblePakTimer = 36;
                if (is_rumble_finished_and_queue_empty()) {
                    queue_rumble_data(3, 30);
                }
            }
        } else {
            gRumblePakTimer = 0;
        }
    }
}

/**
 * Updates some basic info for camera usage.
 */
void update_mario_info_for_cam(struct MarioState *m) {
    m->marioBodyState->action = m->action;
    m->statusForCamera->action = m->action;

    vec3s_copy(m->statusForCamera->faceAngle, m->faceAngle);

    if ((m->flags & MARIO_UNKNOWN_25) == 0) {
        vec3f_copy(m->statusForCamera->pos, m->pos);
    }
}

/**
 * Resets Mario's model, done every time an action is executed.
 */
void mario_reset_bodystate(struct MarioState *m) {
    struct MarioBodyState *bodyState = m->marioBodyState;

    bodyState->capState = MARIO_HAS_DEFAULT_CAP_OFF;
    bodyState->eyeState = MARIO_EYES_BLINK;
    bodyState->handState = MARIO_HAND_FISTS;
    bodyState->modelState = 0;
    bodyState->wingFlutter = FALSE;

    m->flags &= ~MARIO_METAL_SHOCK;
}

/**
 * Adjusts Mario's graphical height for quicksand.
 */
void sink_mario_in_quicksand(struct MarioState *m) {
    struct Object *o = m->marioObj;

    if (o->header.gfx.throwMatrix) {
        (*o->header.gfx.throwMatrix)[3][1] -= m->quicksandDepth;
    }

    o->header.gfx.pos[1] -= m->quicksandDepth;
}

/**
 * Is a binary representation of the frames to flicker Mario's cap when the timer
 * is running out.
 *
 * Equals [1000]^5 . [100]^8 . [10]^9 . [1] in binary, which is
 * 100010001000100010001001001001001001001001001010101010101010101.
 */
u64 sCapFlickerFrames = 0x4444449249255555;

/**
 * Updates the cap flags mainly based on the cap timer.
 */
u32 update_and_return_cap_flags(struct MarioState *m) {
    u32 flags = m->flags;
    u32 action;

    if (m->capTimer > 0) {
        action = m->action;

        if ((m->capTimer <= 60)
            || ((action != ACT_READING_AUTOMATIC_DIALOG) && (action != ACT_READING_NPC_DIALOG)
                && (action != ACT_READING_SIGN) && (action != ACT_IN_CANNON))) {
            m->capTimer -= 1;
        }

        if (m->capTimer == 0) {
            stop_cap_music();

            m->flags &= ~(MARIO_VANISH_CAP | MARIO_METAL_CAP | MARIO_WING_CAP);
            if ((m->flags & (MARIO_NORMAL_CAP | MARIO_VANISH_CAP | MARIO_METAL_CAP | MARIO_WING_CAP))
                == 0) {
                m->flags &= ~MARIO_CAP_ON_HEAD;
            }
        }

        if (m->capTimer == 0x3C) {
            fadeout_cap_music();
        }

        // This code flickers the cap through a long binary string, increasing in how
        // common it flickers near the end.
        if ((m->capTimer < 0x40) && ((1ULL << m->capTimer) & sCapFlickerFrames)) {
            flags &= ~(MARIO_VANISH_CAP | MARIO_METAL_CAP | MARIO_WING_CAP);
            if ((flags & (MARIO_NORMAL_CAP | MARIO_VANISH_CAP | MARIO_METAL_CAP | MARIO_WING_CAP))
                == 0) {
                flags &= ~MARIO_CAP_ON_HEAD;
            }
        }
    }

    return flags;
}

/**
 * Updates the Mario's cap, rendering, and hitbox.
 */
void mario_update_hitbox_and_cap_model(struct MarioState *m) {
    struct MarioBodyState *bodyState = m->marioBodyState;
    s32 flags = update_and_return_cap_flags(m);

    if (flags & MARIO_VANISH_CAP) {
        bodyState->modelState = MODEL_STATE_NOISE_ALPHA;
    }

    if (flags & MARIO_METAL_CAP) {
        bodyState->modelState |= MODEL_STATE_METAL;
    }

    if (flags & MARIO_METAL_SHOCK) {
        bodyState->modelState |= MODEL_STATE_METAL;
    }

    if (m->invincTimer >= 3) {
        //! (Pause buffered hitstun) Since the global timer increments while paused,
        //  this can be paused through to give continual invisibility. This leads to
        //  no interaction with objects.
        if (gGlobalTimer & 1) {
            gMarioState->marioObj->header.gfx.node.flags |= GRAPH_RENDER_INVISIBLE;
        }
    }

    if (flags & MARIO_CAP_IN_HAND) {
        if (flags & MARIO_WING_CAP) {
            bodyState->handState = MARIO_HAND_HOLDING_WING_CAP;
        } else {
            bodyState->handState = MARIO_HAND_HOLDING_CAP;
        }
    }

    if (flags & MARIO_CAP_ON_HEAD) {
        if (flags & MARIO_WING_CAP) {
            bodyState->capState = MARIO_HAS_WING_CAP_ON;
        } else {
            bodyState->capState = MARIO_HAS_DEFAULT_CAP_ON;
        }
    }

    // Short hitbox for crouching/crawling/etc.
    if (m->action & ACT_FLAG_SHORT_HITBOX) {
        m->marioObj->hitboxHeight = 100.0f;
    } else {
        m->marioObj->hitboxHeight = 160.0f;
    }

    if ((m->flags & MARIO_TELEPORTING) && (m->fadeWarpOpacity != 0xFF)) {
        bodyState->modelState &= ~0xFF;
        bodyState->modelState |= (0x100 | m->fadeWarpOpacity);
    }
}

/**
 * An unused and possibly a debug function. Z + another button input
 * sets Mario with a different cap.
 */
static void debug_update_mario_cap(u16 button, s32 flags, u16 capTimer, u16 capMusic) {
    // This checks for Z_TRIG instead of Z_DOWN flag
    // (which is also what other debug functions do),
    // so likely debug behavior rather than unused behavior.
    if ((gPlayer1Controller->buttonDown & Z_TRIG) && (gPlayer1Controller->buttonPressed & button)
        && ((gMarioState->flags & flags) == 0)) {
        gMarioState->flags |= (flags + MARIO_CAP_ON_HEAD);

        if (capTimer > gMarioState->capTimer) {
            gMarioState->capTimer = capTimer;
        }

        play_cap_music(capMusic);
    }
}

void func_sh_8025574C(void) {
    if (gMarioState->particleFlags & PARTICLE_HORIZONTAL_STAR) {
        queue_rumble_data(5, 80);
    } else if (gMarioState->particleFlags & PARTICLE_VERTICAL_STAR) {
        queue_rumble_data(5, 80);
    } else if (gMarioState->particleFlags & PARTICLE_TRIANGLE) {
        queue_rumble_data(5, 80);
    }
    if(gMarioState->heldObj && gMarioState->heldObj->behavior == segmented_to_virtual(bhvBobomb)) {
        reset_rumble_timers();
    }
}

/**
 * Main function for executing Mario's behavior.
 */
/**
 * Main function for executing Mario's behavior.
 */
s32 execute_mario_action(UNUSED struct Object *o) {
    s32 inLoop = TRUE;
    s32 mx = (s32) gMarioState->pos[0];
    s32 my = (s32) gMarioState->pos[1];
    s32 mz = (s32) gMarioState->pos[2];
   
    // --- CASTLE GROUNDS TREES ---
    if (gCurrLevelNum == LEVEL_CASTLE_GROUNDS) {

        // --- Tree 5800 ---
        {
            s32 treeX = -1900;
            s32 treeY = 518;
            s32 treeZ = 2868;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5800)) {
                    SM64AP_SendItem(5800);
                }
            }
        }

        // --- Tree 5801 ---
        {
            s32 treeX = -2566;
            s32 treeY = 469;
            s32 treeZ = 2626;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5801)) {
                    SM64AP_SendItem(5801);
                }
            }
        }

        // --- Tree 5802 ---
        {
            s32 treeX = -2021;
            s32 treeY = 726;
            s32 treeZ = 1468;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5802)) {
                    SM64AP_SendItem(5802);
                }
            }
        }

        // --- Tree 5803 ---
        {
            s32 treeX = -1333;
            s32 treeY = 841;
            s32 treeZ = 1881;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5803)) {
                    SM64AP_SendItem(5803);
                }
            }
        }

        // --- Tree 5804 ---
        {
            s32 treeX = -109;
            s32 treeY = 807;
            s32 treeZ = 3008;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5804)) {
                    SM64AP_SendItem(5804);
                }
            }
        }

        // --- Tree 5805 ---
        {
            s32 treeX = -5069;
            s32 treeY = 434;
            s32 treeZ = 3221;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5805)) {
                    SM64AP_SendItem(5805);
                }
            }
        }

        // --- Tree 5806 ---
        {
            s32 treeX = -6220;
            s32 treeY = 502;
            s32 treeZ = 3458;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5806)) {
                    SM64AP_SendItem(5806);
                }
            }
        }

        // --- Tree 5807 ---
        {
            s32 treeX = -6510;
            s32 treeY = 393;
            s32 treeZ = 1411;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5807)) {
                    SM64AP_SendItem(5807);
                }
            }
        }

        // --- Tree 5808 ---
        {
            s32 treeX = -5204;
            s32 treeY = 338;
            s32 treeZ = 811;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5808)) {
                    SM64AP_SendItem(5808);
                }
            }
        }

        // --- Tree 5809 ---
        {
            s32 treeX = -4711;
            s32 treeY = 497;
            s32 treeZ = 433;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5809)) {
                    SM64AP_SendItem(5809);
                }
            }
        }

        // --- Tree 5810 ---
        {
            s32 treeX = -5506;
            s32 treeY = 540;
            s32 treeZ = -661;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5810)) {
                    SM64AP_SendItem(5810);
                }
            }
        }

        // --- Tree 5811 ---
        {
            s32 treeX = -6269;
            s32 treeY = 502;
            s32 treeZ = -2145;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5811)) {
                    SM64AP_SendItem(5811);
                }
            }
        }

        // --- Tree 5812 ---
        {
            s32 treeX = -5600;
            s32 treeY = 491;
            s32 treeZ = -2627;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5812)) {
                    SM64AP_SendItem(5812);
                }
            }
        }

        // --- Tree 5813 ---
        {
            s32 treeX = -5957;
            s32 treeY = 659;
            s32 treeZ = -3447;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5813)) {
                    SM64AP_SendItem(5813);
                }
            }
        }

        // --- Tree 5814 ---
        {
            s32 treeX = 767;
            s32 treeY = 630;
            s32 treeZ = 2598;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5814)) {
                    SM64AP_SendItem(5814);
                }
            }
        }

        // --- Tree 5815 ---
        {
            s32 treeX = 767;
            s32 treeY = 630;
            s32 treeZ = 2598;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5815)) {
                    SM64AP_SendItem(5815);
                }
            }
        }

        // --- Tree 5816 ---
        {
            s32 treeX = 1132;
            s32 treeY = 502;
            s32 treeZ = 1132;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5816)) {
                    SM64AP_SendItem(5816);
                }
            }
        }

        // --- Tree 5817 ---
        {
            s32 treeX = 1919;
            s32 treeY = 330;
            s32 treeZ = 1157;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5817)) {
                    SM64AP_SendItem(5817);
                }
            }
        }

        // --- Tree 5818 ---
        {
            s32 treeX = 3153;
            s32 treeY = 247;
            s32 treeZ = 469;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5818)) {
                    SM64AP_SendItem(5818);
                }
            }
        }

        // --- Tree 5819 ---
        {
            s32 treeX = 6178;
            s32 treeY = 293;
            s32 treeZ = 167;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5819)) {
                    SM64AP_SendItem(5819);
                }
            }
        }

        // --- Tree 5820 ---
        {
            s32 treeX = 5774;
            s32 treeY = 473;
            s32 treeZ = -1114;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5820)) {
                    SM64AP_SendItem(5820);
                }
            }
        }

        // --- Tree 5821 ---
        {
            s32 treeX = 6399;
            s32 treeY = 547;
            s32 treeZ = -1680;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5821)) {
                    SM64AP_SendItem(5821);
                }
            }
        }

        // --- Tree 5822 ---
        {
            s32 treeX = 5954;
            s32 treeY = 566;
            s32 treeZ = -2846;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5822)) {
                    SM64AP_SendItem(5822);
                }
            }
        }

        // --- Tree 5823 ---
        {
            s32 treeX = 5457;
            s32 treeY = 584;
            s32 treeZ = -3259;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5823)) {
                    SM64AP_SendItem(5823);
                }
            }
        }

        // --- Tree 5824 ---
        {
            s32 treeX = 5868;
            s32 treeY = 725;
            s32 treeZ = -4453;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5824)) {
                    SM64AP_SendItem(5824);
                }
            }
        }

        // --- Tree 5825 ---
        {
            s32 treeX = 6408;
            s32 treeY = 937;
            s32 treeZ = -5314;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5825)) {
                    SM64AP_SendItem(5825);
                }
            }
        }
    }

    // --- CASTLE COURTYARD TREES ---
    if (gCurrLevelNum == LEVEL_CASTLE_COURTYARD) {

        // --- Tree 5826 ---
        {
            s32 treeX = -1868;
            s32 treeY = -120;
            s32 treeZ = -45;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5826)) {
                    SM64AP_SendItem(5826);
                }
            }
        }
        // --- Tree 5827 ---
        {
            s32 treeX = -2537;
            s32 treeY = -162;
            s32 treeZ = -759;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5827)) {
                    SM64AP_SendItem(5827);
                }
            }
        }
        // --- Tree 5828 ---
        {
            s32 treeX = -2820;
            s32 treeY = -156;
            s32 treeZ = -1317;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5828)) {
                    SM64AP_SendItem(5828);
                }
            }
        }
        // --- Tree 5829 ---
        {
            s32 treeX = -2446;
            s32 treeY = -161;
            s32 treeZ = -1786;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5829)) {
                    SM64AP_SendItem(5829);
                }
            }
        }
        // --- Tree 5830 ---
        {
            s32 treeX = -2732;
            s32 treeY = -161;
            s32 treeZ = -2166;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5830)) {
                    SM64AP_SendItem(5830);
                }
            }
        }
        // --- Tree 5831 ---
        {
            s32 treeX = -1640;
            s32 treeY = -119;
            s32 treeZ = -3228;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5831)) {
                    SM64AP_SendItem(5831);
                }
            }
        }
        // --- Tree 5832 ---
        {
            s32 treeX = -820;
            s32 treeY = 154;
            s32 treeZ = 201;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5832)) {
                    SM64AP_SendItem(5832);
                }
            }
        }
        // --- Tree 5833 ---
        {
            s32 treeX = 818;
            s32 treeY = 150;
            s32 treeZ = 203;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5833)) {
                    SM64AP_SendItem(5833);
                }
            }
        }
        // --- Tree 5834 ---
        {
            s32 treeX = -817;
            s32 treeY = 81;
            s32 treeZ = -3630;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5834)) {
                    SM64AP_SendItem(5834);
                }
            }
        }
        // --- Tree 5835 ---
        {
            s32 treeX = 824;
            s32 treeY = 144;
            s32 treeZ = -3633;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5835)) {
                    SM64AP_SendItem(5835);
                }
            }
        }
        // --- Tree 5836 ---
        {
            s32 treeX = 2042;
            s32 treeY = -78;
            s32 treeZ = -3032;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5836)) {
                    SM64AP_SendItem(5836);
                }
            }
        }
        // --- Tree 5837 ---
        {
            s32 treeX = 2444;
            s32 treeY = -70;
            s32 treeZ = -2330;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5837)) {
                    SM64AP_SendItem(5837);
                }
            }
        }
        // --- Tree 5838 ---
        {
            s32 treeX = 2272;
            s32 treeY = -77;
            s32 treeZ = -1432;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5838)) {
                    SM64AP_SendItem(5838);
                }
            }
        }
        // --- Tree 5839 ---
        {
            s32 treeX = 2769;
            s32 treeY = -89;
            s32 treeZ = -1523;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5839)) {
                    SM64AP_SendItem(5839);
                }
            }
        }
        // --- Tree 5840 ---
        {
            s32 treeX = 2382;
            s32 treeY = -36;
            s32 treeZ = -843;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5840)) {
                    SM64AP_SendItem(5840);
                }
            }
        }
        // --- Tree 5841 ---
        {
            s32 treeX = 1681;
            s32 treeY = -69;
            s32 treeZ = -132;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5841)) {
                    SM64AP_SendItem(5841);
                }
            }
        }
    }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_BOB) {
        
        // --- Tree 5842 ---
        {
            s32 treeX = -4268;
            s32 treeY = 195;
            s32 treeZ = 4768;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5842)) {
                    SM64AP_SendItem(5842);
                }
            }
        }
        // --- Tree 5843 ---
        {
            s32 treeX = -1509;
            s32 treeY = 272;
            s32 treeZ = 5094;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5843)) {
                    SM64AP_SendItem(5843);
                }
            }
        }
        // --- Tree 5844 ---
        {
            s32 treeX = 2911;
            s32 treeY = 925;
            s32 treeZ = 5917;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5844)) {
                    SM64AP_SendItem(5844);
                }
            }
        }
        // --- Tree 5845 ---
        {
            s32 treeX = 4208;
            s32 treeY = 1055;
            s32 treeZ = 3772;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5845)) {
                    SM64AP_SendItem(5845);
                }
            }
        }
        // --- Tree 5846 ---
        {
            s32 treeX = 5444;
            s32 treeY = 976;
            s32 treeZ = 6016;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5846)) {
                    SM64AP_SendItem(5846);
                }
            }
        }
        // --- Tree 5847 ---
        {
            s32 treeX = -3583;
            s32 treeY = 898;
            s32 treeZ = 2560;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5847)) {
                    SM64AP_SendItem(5847);
                }
            }
        }
        // --- Tree 5848 ---
        {
            s32 treeX = -4095;
            s32 treeY = 892;
            s32 treeZ = 3072;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5848)) {
                    SM64AP_SendItem(5848);
                }
            }
        }
        // --- Tree 5849 ---
        {
            s32 treeX = -5119;
            s32 treeY = 886;
            s32 treeZ = 2048;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5849)) {
                    SM64AP_SendItem(5849);
                }
            }
        }
        // --- Tree 5850 ---
        {
            s32 treeX = -6655;
            s32 treeY = 913;
            s32 treeZ = 3584;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5850)) {
                    SM64AP_SendItem(5850);
                }
            }
        }
        // --- Tree 5851 ---
        {
            s32 treeX = -4095;
            s32 treeY = 947;
            s32 treeZ = 1536;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5851)) {
                    SM64AP_SendItem(5851);
                }
            }
        }
        // --- Tree 5852 ---
        {
            s32 treeX = -6172;
            s32 treeY = 1168;
            s32 treeZ = -430;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5852)) {
                    SM64AP_SendItem(5852);
                }
            }
        }
        // --- Tree 5853 ---
        {
            s32 treeX = -5792;
            s32 treeY = 1181;
            s32 treeZ = -4654;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5853)) {
                    SM64AP_SendItem(5853);
                }
            }
        }
        // --- Tree 5854 ---
        {
            s32 treeX = -6804;
            s32 treeY = 1171;
            s32 treeZ = -4866;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5854)) {
                    SM64AP_SendItem(5854);
                }
            }
        }
        // --- Tree 5855 ---
        {
            s32 treeX = -6130;
            s32 treeY = 955;
            s32 treeZ = -6507;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5855)) {
                    SM64AP_SendItem(5855);
                }
            }
        }
        // --- Tree 5856 ---
        {
            s32 treeX = 6799;
            s32 treeY = 2154;
            s32 treeZ = -5587;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5856)) {
                    SM64AP_SendItem(5856);
                }
            }
        }
        // --- Tree 5857 ---
        {
            s32 treeX = 6033;
            s32 treeY = 2285;
            s32 treeZ = -7660;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5857)) {
                    SM64AP_SendItem(5857);
                }
            }
        }
        // --- Tree 5858 ---
        {
            s32 treeX = 4096;
            s32 treeY = 3253;
            s32 treeZ = 1638;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5858)) {
                    SM64AP_SendItem(5858);
                }
            }
        }
        // --- Pole 5859 ---
        {
            s32 treeX = 3304;
            s32 treeYMin = 4283;
            s32 treeYMax = 4965;
            s32 treeZ = -4603;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5859)) {
                    SM64AP_SendItem(5859);
                }
            }
        }
    }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_WF) {
        
        // --- Tree 5860 ---
        {
            s32 treeX = 2560;
            s32 treeY = 383;
            s32 treeZ = 4608;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5860)) {
                    SM64AP_SendItem(5860);
                }
            }
        }
        // --- Pole 5861 ---
        {
            s32 treeX = -2495;
            s32 treeYMin = 1321;
            s32 treeYMax = 1913;
            s32 treeZ = -256;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5861)) {
                    SM64AP_SendItem(5861);
                }
            }
        }
        // --- Pole 5862 ---
        {
            s32 treeX = -2560;
            s32 treeYMin = 2550;
            s32 treeYMax = 4632;
            s32 treeZ = -256;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5862)) {
                    SM64AP_SendItem(5862);
                }
            }
        }
    }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_CCM) {
        
        // --- Tree 5863 ---
        {
            s32 treeX = -1768;
            s32 treeY = 2709;
            s32 treeZ = -1793;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5863)) {
                    SM64AP_SendItem(5863);
                }
            }
        }
        // --- Tree 5864 ---
        {
            s32 treeX = -488;
            s32 treeY = 2647;
            s32 treeZ = -2305;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5864)) {
                    SM64AP_SendItem(5864);
                }
            }
        }
        // --- Tree 5865 ---
        {
            s32 treeX = 2237;
            s32 treeY = 2702;
            s32 treeZ = -1630;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5865)) {
                    SM64AP_SendItem(5865);
                }
            }
        }
        // --- Tree 5866 ---
        {
            s32 treeX = 2885;
            s32 treeY = 2645;
            s32 treeZ = -1638;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5866)) {
                    SM64AP_SendItem(5866);
                }
            }
        }
        // --- Tree 5867 ---
        {
            s32 treeX = -3443;
            s32 treeY = 850;
            s32 treeZ = -2713;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5867)) {
                    SM64AP_SendItem(5867);
                }
            }
        }
        // --- Tree 5868 ---
        {
            s32 treeX = -5892;
            s32 treeY = -1591;
            s32 treeZ = 811;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5868)) {
                    SM64AP_SendItem(5868);
                }
            }
        }
        // --- Tree 5869 ---
        {
            s32 treeX = -5201;
            s32 treeY = -1585;
            s32 treeZ = 2994;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5869)) {
                    SM64AP_SendItem(5869);
                }
            }
        }
        // --- Tree 5870 ---
        {
            s32 treeX = -5508;
            s32 treeY = -1619;
            s32 treeZ = 4148;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5870)) {
                    SM64AP_SendItem(5870);
                }
            }
        }
        // --- Tree 5871 ---
        {
            s32 treeX = -4576;
            s32 treeY = -1501;
            s32 treeZ = 4814;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5871)) {
                    SM64AP_SendItem(5871);
                }
            }
        }
        // --- Tree 5872 ---
        {
            s32 treeX = 1248;
            s32 treeY = -4417;
            s32 treeZ = 5474;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5872)) {
                    SM64AP_SendItem(5872);
                }
            }
        }
        // --- Tree 5873 ---
        {
            s32 treeX = 1989;
            s32 treeY = -4445;
            s32 treeZ = 4949;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5873)) {
                    SM64AP_SendItem(5873);
                }
            }
        }
        // --- Tree 5874 ---
        {
            s32 treeX = -1146;
            s32 treeY = -3439;
            s32 treeZ = 5919;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5874)) {
                    SM64AP_SendItem(5874);
                }
            }
        }
        // --- Tree 5875 ---
        {
            s32 treeX = -3748;
            s32 treeY = -4510;
            s32 treeZ = 4464;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5875)) {
                    SM64AP_SendItem(5875);
                }
            }
        }
     }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_JRB) {

       // --- Pillar 5876 ---
        {
            s32 treeX = 53;
            s32 treeYMin = 2340;
            s32 treeYMax = 2700;
            s32 treeZ = 2724;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5876)) {
                    SM64AP_SendItem(5876);
                }
            }
        }
         // --- Pillar 5877 ---
        {
            s32 treeX = 659;
            s32 treeYMin = 2540;
            s32 treeYMax = 2870;
            s32 treeZ = 3314;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5877)) {
                    SM64AP_SendItem(5877);
                }
            }
        }
         // --- Pillar 5878 ---
        {
            s32 treeX = 1087;
            s32 treeYMin = 2130;
            s32 treeYMax = 2540;
            s32 treeZ = 3798;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5878)) {
                    SM64AP_SendItem(5878);
                }
            }
        }
         // --- Pillar 5879 ---
        {
            s32 treeX = -2535;
            s32 treeYMin = 1060;
            s32 treeYMax = 1995;
            s32 treeZ = 6113;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5879)) {
                    SM64AP_SendItem(5879);
                }
            }
        }
    }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_LLL) {

       // --- Pole 5880 ---
        {
            s32 treeX = 728;
            s32 treeYMin = 2590;
            s32 treeYMax = 3140;
            s32 treeZ = -2754;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5880)) {
                    SM64AP_SendItem(5880);
                }
            }
        }
        // --- Pole 5881 ---
        {
            s32 treeX = 1043;
            s32 treeYMin = 2960;
            s32 treeYMax = 3700;
            s32 treeZ = -2679;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5881)) {
                    SM64AP_SendItem(5881);
                }
            }
        }
        // --- Pole 5882 ---
        {
            s32 treeX = 1078;
            s32 treeYMin = 2060;
            s32 treeYMax = 4040;
            s32 treeZ = -2269;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5882)) {
                    SM64AP_SendItem(5882);
                }
            }
        }
        // --- Pole 5883 ---
        {
            s32 treeX = 1413;
            s32 treeYMin = 3210;
            s32 treeYMax = 4000;
            s32 treeZ = -2190;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5883)) {
                    SM64AP_SendItem(5883);
                }
            }
        }
        // --- Pole 5884 ---
        {
            s32 treeX = 783;
            s32 treeYMin = 1110;
            s32 treeYMax = 2060;
            s32 treeZ = -47;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5884)) {
                    SM64AP_SendItem(5884);
                }
            }
        }
        // --- Pole 5885 ---
        {
            s32 treeX = 662;
            s32 treeYMin = 2140;
            s32 treeYMax = 3130;
            s32 treeZ = -47;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5885)) {
                    SM64AP_SendItem(5885);
                }
            }
        }
    }

    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_HMC) {

       // --- Pole 5886 ---
        {
            s32 treeX = 889;
            s32 treeYMin = 1015;
            s32 treeYMax = 2820;
            s32 treeZ = 3277;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5886)) {
                    SM64AP_SendItem(5886);
                }
            }
        }
        // --- Pole 5886 ---
        {
            s32 treeX = 799;
            s32 treeYMin = 1015;
            s32 treeYMax = 2775;
            s32 treeZ = 4434;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5887)) {
                    SM64AP_SendItem(5887);
                }
            }
        }
    }
    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_SSL) {

       // --- Tree 5888 ---
        {
            s32 treeX = -5989;
            s32 treeY = 46;
            s32 treeZ = -4850;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5888)) {
                    SM64AP_SendItem(5888);
                }
            }
        }
        // --- Pole 5889 ---
        {
            s32 treeX = 2867;
            s32 treeYMin = 630;
            s32 treeYMax = 1370;
            s32 treeZ = 2867;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5889)) {
                    SM64AP_SendItem(5889);
                }
            }
        }
        // --- Pole 5890 ---
        {
            s32 treeX = 0;
            s32 treeYMin = 3190;
            s32 treeYMax = 4080;
            s32 treeZ = 1331;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5890)) {
                    SM64AP_SendItem(5890);
                }
            }
        }
    }
    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_BITFS) {

       // --- Pole 5891 ---
        {
            s32 treeX = 6605;
            s32 treeYMin = -3071;
            s32 treeYMax = -2390;
            s32 treeZ = 266;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5891)) {
                    SM64AP_SendItem(5891);
                }
            }
        }
        // --- Pole 5892 ---
        {
            s32 treeX = 3890;
            s32 treeYMin = -2050;
            s32 treeYMax = -1260;
            s32 treeZ = 266;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5892)) {
                    SM64AP_SendItem(5892);
                }
            }
        }
        // --- Pole 5893 ---
        {
            s32 treeX = 613;
            s32 treeYMin = 3570;
            s32 treeYMax = 4520;
            s32 treeZ = 95;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5893)) {
                    SM64AP_SendItem(5893);
                }
            }
        }
        // --- Pole 5894 ---
        {
            s32 treeX = 1843;
            s32 treeYMin = 3565;
            s32 treeYMax = 4500;
            s32 treeZ = 95;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5894)) {
                    SM64AP_SendItem(5894);
                }
            }
        }
        // --- Pole 5895 ---
        {
            s32 treeX = 3072;
            s32 treeYMin = 3565;
            s32 treeYMax = 4520;
            s32 treeZ = 96;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5895)) {
                    SM64AP_SendItem(5895);
                }
            }
        }
    }
    // --- Bob-omb Battlefield TREES ---
    if (gCurrLevelNum == LEVEL_DDD) {

       // --- Moving Pole 5896 ---
        {
            s32 treeXMin = 5595;
            s32 treeXMax = 5615;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = 1250;
            s32 treeZMax = 3400;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5896)) {
                    SM64AP_SendItem(5896);
                }
            }
        }
        // --- Moving Pole 5897 ---
        {
            s32 treeXMin = 2120;
            s32 treeXMax = 5120;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = 3570;
            s32 treeZMax = 3590;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5897)) {
                    SM64AP_SendItem(5897);
                }
            }
        }
        // --- Moving Pole 5898 ---
        {
            s32 treeXMin = 3990;
            s32 treeXMax = 5110;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = 1265;
            s32 treeZMax = 1285;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5898)) {
                    SM64AP_SendItem(5898);
                }
            }
        }
        // --- Moving Pole 5899 ---
        {
            s32 treeXMin = 3000;
            s32 treeXMax = 4000;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = 1075;
            s32 treeZMax = 1075;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5899)) {
                    SM64AP_SendItem(5899);
                }
            }
        }
        // --- Moving Pole 5900 ---
        {
            s32 treeXMin = 1800;
            s32 treeXMax = 2900;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = 1275;
            s32 treeZMax = 1275;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5900)) {
                    SM64AP_SendItem(5900);
                }
            }
        }
        // --- Moving Pole 5901 ---
        {
            s32 treeXMin = 1830;
            s32 treeXMax = 1830;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = -1550;
            s32 treeZMax = 520;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5901)) {
                    SM64AP_SendItem(5901);
                }
            }
        }
        // --- Moving Pole 5902 ---
        {
            s32 treeXMin = 5760;
            s32 treeXMax = 5760;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = -2000;
            s32 treeZMax = 260;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5902)) {
                    SM64AP_SendItem(5902);
                }
            }
        }
        // --- Moving Pole 5902 ---
        {
            s32 treeXMin = 3300;
            s32 treeXMax = 5610;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = -1945;
            s32 treeZMax = -1945;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5902)) {
                    SM64AP_SendItem(5902);
                }
            }
        }
        // --- Moving Pole 5903 ---
        {
            s32 treeXMin = 3500;
            s32 treeXMax = 4850;

            s32 treeYMin = 890;
            s32 treeYMax = 1400;

            s32 treeZMin = -2250;
            s32 treeZMax = -2250;

            if (mx >= treeXMin && mx <= treeXMax &&
                my >= treeYMin && my <= treeYMax &&
                mz >= treeZMin && mz <= treeZMax) {

                if (!SM64AP_CheckedLoc(5903)) {
                    SM64AP_SendItem(5903);
                }
            }
        }
    }

    // --- CASTLE COURTYARD TREES ---
    if (gCurrLevelNum == LEVEL_WDW) {

        // --- Tree 5904 ---
        {
            s32 treeX = 1664;
            s32 treeY = -2311;
            s32 treeZ = -946;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5904)) {
                    SM64AP_SendItem(5904);
                }
            }
        }
        // --- Tree 5905 ---
        {
            s32 treeX = 1664;
            s32 treeY = -2311;
            s32 treeZ = -1637;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5905)) {
                    SM64AP_SendItem(5905);
                }
            }
         }
        // --- Pole 5906 ---
        {
            s32 treeX = -268;
            s32 treeYMin = -665;
            s32 treeYMax = 100;
            s32 treeZ = 3584;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5906)) {
                    SM64AP_SendItem(5906);
                }
            }
        }
    }
    // --- CASTLE COURTYARD TREES ---
    if (gCurrLevelNum == LEVEL_SL) {

        // --- Tree 5907 ---
        {
            s32 treeX = 5666;
            s32 treeY = 1195;
            s32 treeZ = -3341;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5907)) {
                    SM64AP_SendItem(5907);
                }
            }
        }
        // --- Tree 5908 ---
        {
            s32 treeX = 5395;
            s32 treeY = 1132;
            s32 treeZ = -5443;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5908)) {
                    SM64AP_SendItem(5908);
                }
            }
        }
        // --- Tree 5909 ---
        {
            s32 treeX = 3645;
            s32 treeY = 1207;
            s32 treeZ = -5889;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5909)) {
                    SM64AP_SendItem(5909);
                }
            }
        }
        // --- Tree 5910 ---
        {
            s32 treeX = 1919;
            s32 treeY = 1200;
            s32 treeZ = -4759;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5910)) {
                    SM64AP_SendItem(5910);
                }
            }
        }
        // --- Tree 5911 ---
        {
            s32 treeX = 1658;
            s32 treeY = 1676;
            s32 treeZ = -3605;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5911)) {
                    SM64AP_SendItem(5911);
                }
            }
        }
        // --- Tree 5912 ---
        {
            s32 treeX = -3769;
            s32 treeY = 1144;
            s32 treeZ = -1197;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5912)) {
                    SM64AP_SendItem(5912);
                }
            }
        }
        // --- Tree 5913 ---
        {
            s32 treeX = -2745;
            s32 treeY = 1226;
            s32 treeZ = -582;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5913)) {
                    SM64AP_SendItem(5913);
                }
            }
        }
        // --- Tree 5914 ---
        {
            s32 treeX = 1766;
            s32 treeY = 3009;
            s32 treeZ = -942;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5914)) {
                    SM64AP_SendItem(5914);
                }
            }
        }
        // --- Tree 5915 ---
        {
            s32 treeX = 1919;
            s32 treeY = 1200;
            s32 treeZ = -4759;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5915)) {
                    SM64AP_SendItem(5915);
                }
            }
        }
    }
    if (gCurrLevelNum == LEVEL_THI) {

        // --- Tree 5916 ---
        {
            s32 treeX = 4813;
            s32 treeY = -372;
            s32 treeZ = 2254;

            s32 dx = mx - treeX;
            s32 dy = my - treeY;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                dy > -300 && dy < 300 &&
                dz > -250 && dz < 250) {
                if (!SM64AP_CheckedLoc(5916)) {
                    SM64AP_SendItem(5916);
                }
            }
        }
        // --- Pole 5917 ---
        {
            s32 treeX = 7400;
            s32 treeYMin = -1530;
            s32 treeYMax = -880;
            s32 treeZ = -6300;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5917)) {
                    SM64AP_SendItem(5917);
                }
            }
        }
    }
    if (gCurrLevelNum == LEVEL_TTC) {
        
        // --- Pole 5918 ---
        {
            s32 treeX = -1080;
            s32 treeYMin = -835;
            s32 treeYMax = -20;
            s32 treeZ = 1573;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5918)) {
                    SM64AP_SendItem(5918);
                }
            }
        }
    }
    if (gCurrLevelNum == LEVEL_WMOTR) {
        
        // --- Pole 5919 ---
        {
            s32 treeX = 3996;
            s32 treeYMin = -2738;
            s32 treeYMax = -1969;
            s32 treeZ = 5477;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5919)) {
                    SM64AP_SendItem(5919);
                }
            }
        }
        // --- Pole 5920 ---
        {
            s32 treeX = -2669;
            s32 treeYMin = 3150;
            s32 treeYMax = 4250;
            s32 treeZ = -4369;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5920)) {
                    SM64AP_SendItem(5920);
                }
            }
        }
        // --- Pole 5921 ---
        {
            s32 treeX = -2980;
            s32 treeYMin = 4040;
            s32 treeYMax = 4250;
            s32 treeZ = -4248;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5921)) {
                    SM64AP_SendItem(5921);
                }
            }
        }
        // --- Pole 5922 ---
        {
            s32 treeX = -3290;
            s32 treeYMin = 3630;
            s32 treeYMax = 4250;
            s32 treeZ = -4477;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5922)) {
                    SM64AP_SendItem(5922);
                }
            }
        }
        // --- Pole 5923 ---
        {
            s32 treeX = -2911;
            s32 treeYMin = 3560;
            s32 treeYMax = 4250;
            s32 treeZ = -3967;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5923)) {
                    SM64AP_SendItem(5923);
                }
            }
        }
        // --- Pole 5924 ---
        {
            s32 treeX = -3258;
            s32 treeYMin = 3350;
            s32 treeYMax = 4250;
            s32 treeZ = -3946;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5924)) {
                    SM64AP_SendItem(5924);
                }
            }
        }
    }
    if (gCurrLevelNum == LEVEL_RR) {

        // --- Pole 5925 ---
        {
            s32 treeX = 614;
            s32 treeYMin = -2860;
            s32 treeYMax = -860;
            s32 treeZ = 3671;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5925)) {
                    SM64AP_SendItem(5925);
                }
            }
        }
        // --- Pole 5926 ---
        {
            s32 treeX = 2680;
            s32 treeYMin = 210;
            s32 treeYMax = 1150;
            s32 treeZ = 295;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5926)) {
                    SM64AP_SendItem(5926);
                }
            }
        }
        // --- Pole 5927 ---
        {
            s32 treeX = 3811;
            s32 treeYMin = 1030;
            s32 treeYMax = 1970;
            s32 treeZ = 295;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5927)) {
                    SM64AP_SendItem(5927);
                }
            }
        }
        // --- Pole 5928 ---
        {
            s32 treeX = 3554;
            s32 treeYMin = 2885;
            s32 treeYMax = 4771;
            s32 treeZ = -2327;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5928)) {
                    SM64AP_SendItem(5928);
                }
            }
        }
        // --- Pole 5929 ---
        {
            s32 treeX = 5119;
            s32 treeYMin = 3810;
            s32 treeYMax = 4745;
            s32 treeZ = 3325;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5929)) {
                    SM64AP_SendItem(5929);
                }
            }
        }
        // --- Pole 5930 ---
        {
            s32 treeX = 621;
            s32 treeYMin = -4600;
            s32 treeYMax = -3470;
            s32 treeZ = 7362;

            s32 dx = mx - treeX;
            s32 dz = mz - treeZ;

            if (dx > -250 && dx < 250 &&
                my >= treeYMin && my <= treeYMax &&
                dz > -250 && dz < 250) {

                if (!SM64AP_CheckedLoc(5930)) {
                    SM64AP_SendItem(5930);
                }
            }
        }
    }
    if (gCurrLevelNum == LEVEL_BITS) {
        
    // --- Pole 5931 ---
    {
        s32 treeX = -6460;
        s32 treeYMin = 2035;
        s32 treeYMax = 4065;
        s32 treeZ = -904;

        s32 dx = mx - treeX;
        s32 dz = mz - treeZ;

        if (dx > -250 && dx < 250 &&
            my >= treeYMin && my <= treeYMax &&
            dz > -250 && dz < 250) {

            if (!SM64AP_CheckedLoc(5931)) {
                SM64AP_SendItem(5931);
            }
        }
    }

    // --- Pole 5932 ---
    {
        s32 treeX = -3326;
        s32 treeYMin = 3220;
        s32 treeYMax = 3950;
        s32 treeZ = -905;

        s32 dx = mx - treeX;
        s32 dz = mz - treeZ;

        if (dx > -250 && dx < 250 &&
            my >= treeYMin && my <= treeYMax &&
            dz > -250 && dz < 250) {

            if (!SM64AP_CheckedLoc(5932)) {
                SM64AP_SendItem(5932);
            }
        }
    }

    /**
    * Cheat stuff
    */
    if (Cheats.EnableCheats) {
        if (Cheats.GodMode)
            gMarioState->health = 0x880;

        if (Cheats.InfiniteLives && gMarioState->numLives < 99)
            gMarioState->numLives += 1;

        if (Cheats.SuperSpeed && gMarioState->forwardVel > 0)
            gMarioState->forwardVel += 100;
    }

    SM64AP_ApplyProgressiveHealth();

    if (gMarioState->action) {
        gMarioState->marioObj->header.gfx.node.flags &= ~GRAPH_RENDER_INVISIBLE;
        mario_reset_bodystate(gMarioState);
        update_mario_inputs(gMarioState);
        mario_handle_special_floors(gMarioState);
        mario_process_interactions(gMarioState);

        while (inLoop) {
            switch (gMarioState->action & ACT_GROUP_MASK) {
                case ACT_GROUP_STATIONARY:
                    inLoop = mario_execute_stationary_action(gMarioState);
                    break;

                case ACT_GROUP_MOVING:
                    inLoop = mario_execute_moving_action(gMarioState);
                    break;

                case ACT_GROUP_AIRBORNE:
                    inLoop = mario_execute_airborne_action(gMarioState);
                    break;

                case ACT_GROUP_SUBMERGED:
                    inLoop = mario_execute_submerged_action(gMarioState);
                    break;

                case ACT_GROUP_CUTSCENE:
                    inLoop = mario_execute_cutscene_action(gMarioState);
                    break;

                case ACT_GROUP_AUTOMATIC:
                    inLoop = mario_execute_automatic_action(gMarioState);
                    break;

                case ACT_GROUP_OBJECT:
                    inLoop = mario_execute_object_action(gMarioState);
                    break;
            }
        }

        sink_mario_in_quicksand(gMarioState);
        squish_mario_model(gMarioState);
        set_submerged_cam_preset_and_spawn_bubbles(gMarioState);
        update_mario_health(gMarioState);
        SM64AP_ApplyProgressiveHealth();
        update_mario_info_for_cam(gMarioState);
        mario_update_hitbox_and_cap_model(gMarioState);

        if (gMarioState->floor->type == SURFACE_HORIZONTAL_WIND) {
            spawn_wind_particles(0, (gMarioState->floor->force << 8));
#ifndef VERSION_JP
            play_sound(SOUND_ENV_WIND2, gMarioState->marioObj->header.gfx.cameraToObject);
#endif
        }

        if (gMarioState->floor->type == SURFACE_VERTICAL_WIND) {
            spawn_wind_particles(1, 0);
#ifndef VERSION_JP
            play_sound(SOUND_ENV_WIND2, gMarioState->marioObj->header.gfx.cameraToObject);
#endif
        }

        play_infinite_stairs_music();
        gMarioState->marioObj->oInteractStatus = 0;
        func_sh_8025574C();

        return gMarioState->particleFlags;
    }

    return 0;
}

}

/**************************************************
 *                  INITIALIZATION                *
 **************************************************/

void init_mario(void) {
    Vec3s capPos;
    struct Object *capObject;

    unused80339F10 = 0;

    gMarioState->actionTimer = 0;
    gMarioState->framesSinceA = 0xFF;
    gMarioState->framesSinceB = 0xFF;

    gMarioState->invincTimer = 0;

    if (save_file_get_flags()
        & (SAVE_FLAG_CAP_ON_GROUND | SAVE_FLAG_CAP_ON_KLEPTO | SAVE_FLAG_CAP_ON_UKIKI
           | SAVE_FLAG_CAP_ON_MR_BLIZZARD)) {
        switch(save_file_get_cap_level()) {
            case LEVEL_SSL:
            case LEVEL_SL:
            case LEVEL_TTM:
                gMarioState->flags = 0;
                break;
            default:
                gMarioState->flags = (MARIO_CAP_ON_HEAD | MARIO_NORMAL_CAP);
                save_file_clear_flags(SAVE_FLAG_CAP_ON_GROUND | SAVE_FLAG_CAP_ON_KLEPTO | SAVE_FLAG_CAP_ON_MR_BLIZZARD | SAVE_FLAG_CAP_ON_UKIKI);
                break;
        }
    } else {
        gMarioState->flags = (MARIO_CAP_ON_HEAD | MARIO_NORMAL_CAP);
    }

    gMarioState->forwardVel = 0.0f;
    gMarioState->squishTimer = 0;

    gMarioState->hurtCounter = 0;
    gMarioState->healCounter = 0;

    gMarioState->capTimer = 0;
    gMarioState->quicksandDepth = 0.0f;

    gMarioState->heldObj = NULL;
    gMarioState->riddenObj = NULL;
    gMarioState->usedObj = NULL;

    gMarioState->waterLevel =
        find_water_level(gMarioSpawnInfo->startPos[0], gMarioSpawnInfo->startPos[2]);

    gMarioState->area = gCurrentArea;
    gMarioState->marioObj = gMarioObject;
    gMarioState->marioObj->header.gfx.unk38.animID = -1;
    vec3s_copy(gMarioState->faceAngle, gMarioSpawnInfo->startAngle);
    vec3s_set(gMarioState->angleVel, 0, 0, 0);
    vec3s_to_vec3f(gMarioState->pos, gMarioSpawnInfo->startPos);
    vec3f_set(gMarioState->vel, 0, 0, 0);
    gMarioState->floorHeight =
        find_floor(gMarioState->pos[0], gMarioState->pos[1], gMarioState->pos[2], &gMarioState->floor);

    if (gMarioState->pos[1] < gMarioState->floorHeight) {
        gMarioState->pos[1] = gMarioState->floorHeight;
    }

    gMarioState->marioObj->header.gfx.pos[1] = gMarioState->pos[1];

    gMarioState->action =
        (gMarioState->pos[1] <= (gMarioState->waterLevel - 100)) ? ACT_WATER_IDLE : ACT_IDLE;

    mario_reset_bodystate(gMarioState);
    update_mario_info_for_cam(gMarioState);
    gMarioState->marioBodyState->punchState = 0;

    gMarioState->marioObj->oPosX = gMarioState->pos[0];
    gMarioState->marioObj->oPosY = gMarioState->pos[1];
    gMarioState->marioObj->oPosZ = gMarioState->pos[2];

    gMarioState->marioObj->oMoveAnglePitch = gMarioState->faceAngle[0];
    gMarioState->marioObj->oMoveAngleYaw = gMarioState->faceAngle[1];
    gMarioState->marioObj->oMoveAngleRoll = gMarioState->faceAngle[2];

    vec3f_copy(gMarioState->marioObj->header.gfx.pos, gMarioState->pos);
    vec3s_set(gMarioState->marioObj->header.gfx.angle, 0, gMarioState->faceAngle[1], 0);

    if (save_file_get_cap_pos(capPos)) {
        capObject = spawn_object(gMarioState->marioObj, MODEL_MARIOS_CAP, bhvNormalCap);

        capObject->oPosX = capPos[0];
        capObject->oPosY = capPos[1];
        capObject->oPosZ = capPos[2];

        capObject->oForwardVelS32 = 0;

        capObject->oMoveAngleYaw = 0;
    }
}

void init_mario_from_save_file(void) {
    gMarioState->unk00 = 0;
    gMarioState->flags = 0;
    gMarioState->action = 0;
    gMarioState->spawnInfo = &gPlayerSpawnInfos[0];
    gMarioState->statusForCamera = &gPlayerCameraState[0];
    gMarioState->marioBodyState = &gBodyStates[0];
    gMarioState->controller = &gControllers[0];
    gMarioState->animation = &D_80339D10;

    gMarioState->numCoins = 0;
    gMarioState->numStars =
        save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_MIN - 1, COURSE_MAX - 1);
    gMarioState->numKeys = 0;

    gMarioState->numLives = 4;
    gMarioState->health = 0x880;

    gMarioState->prevNumStarsForDialog = gMarioState->numStars;
    gMarioState->unkB0 = 0xBD;

    gHudDisplay.coins = 0;
    gHudDisplay.wedges = 8;
}
