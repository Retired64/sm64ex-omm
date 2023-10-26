#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Init
//

bool omm_cappy_scuttlebug_init(UNUSED struct Object *o) {
    gOmmObject->state.actionState = 0;
    return true;
}

void omm_cappy_scuttlebug_end(struct Object *o) {
    o->oHomeY = FLOOR_LOWER_LIMIT;
}

f32 omm_cappy_scuttlebug_get_top(struct Object *o) {
    return 90.f * o->oScaleY;
}

//
// Update
//

s32 omm_cappy_scuttlebug_update(struct Object *o) {

    // Hitbox
    o->hitboxRadius = omm_capture_get_hitbox_radius(o);
    o->hitboxHeight = omm_capture_get_hitbox_height(o);
    o->hitboxDownOffset = omm_capture_get_hitbox_down_offset(o);
    o->oWallHitboxRadius = omm_capture_get_wall_hitbox_radius(o);

    // Properties
    POBJ_SET_ABOVE_WATER;
    POBJ_SET_ABLE_TO_MOVE_ON_SLOPES;

    // Inputs
    if (!obj_update_door(o) && !omm_mario_is_locked(gMarioState)) {
        if (o->oWall && gOmmObject->state.actionState) {
            s16 wallAngle = atan2s(o->oWall->normal.z, o->oWall->normal.x);
            s16 diffAngle = (s16) gOmmMario->capture.stickYaw - (s16) wallAngle;
            if (abs_s(diffAngle) < 0x4000) {
                gOmmMario->capture.stickYaw = wallAngle + 0x8000 - diffAngle;
            }
            o->oFloor = o->oWall;
            o->oFloorHeight = o->oPosY;
        }
        pobj_move(o, POBJ_B_BUTTON_DOWN, false, false);
        if (pobj_jump(o, 0, 1) == POBJ_RESULT_JUMP_START) {
            obj_play_sound(o, SOUND_OBJ2_SCUTTLEBUG_ALERT);
        }
    }

    // Movement
    static const s16 sStarParticlesAngles[] = {
        0xE000, 0x0000,
        0x0000, 0x0000,
        0x2000, 0x0000,
        0xE99A, 0x1666,
        0x1666, 0x1666,
        0xE99A, 0xE99A,
        0x1666, 0xE99A
    };
    perform_object_step(o, POBJ_STEP_FLAGS);
    pobj_decelerate(o, 0.85f, 0.95f);
    if (o->oWall && (o->oVelY <= 0.f) && (o->oDistToFloor > o->hitboxRadius) && POBJ_B_BUTTON_DOWN) {
        o->oVelY = 0;
        if (!gOmmObject->state.actionState) {
            s16 wallAngle = 0x8000 + atan2s(o->oWall->normal.z, o->oWall->normal.x);
            for (s32 i = 0; i < 7; ++i) {
                struct Object *obj = spawn_object(o, MODEL_CARTOON_STAR, bhvWallTinyStarParticle);
                obj->oMoveAngleYaw = wallAngle + sStarParticlesAngles[2 * i] + 0x8000;
                obj->oVelY = sins(sStarParticlesAngles[2 * i + 1]) * 25.f;
                obj->oForwardVel = coss(sStarParticlesAngles[2 * i + 1]) * 25.f;
            }
            obj_play_sound(o, SOUND_OBJ_DEFAULT_DEATH);
        }
        gOmmObject->state.actionState = 1;
    } else {
        pobj_apply_gravity(o, 1.f);
        gOmmObject->state.actionState = 0;
    }
    pobj_handle_special_floors(o);
    pobj_stop_if_unpossessed();

    // Interactions
    pobj_process_interactions(

    // Doors
    obj_open_door(o, obj);

    );
    pobj_stop_if_unpossessed();

    // Gfx
    obj_update_gfx(o);
    if (gOmmObject->state.actionState) {
        s16 wallAngle = 0x8000 + atan2s(o->oWall->normal.z, o->oWall->normal.x);
        o->oGfxPos[0] += o->oWallHitboxRadius * sins(wallAngle);
        o->oGfxPos[2] += o->oWallHitboxRadius * coss(wallAngle);
        o->oGfxAngle[0] = -0x4000;
        o->oGfxAngle[1] = wallAngle;
    }
    obj_anim_play(o, 0, (o->oVelY <= 0.f) * max_f(1.f, o->oForwardVel * 2.f / (omm_capture_get_walk_speed(o))));
    if (obj_is_on_ground(o)) {
        obj_make_step_sound_and_particle(o, &gOmmObject->state.walkDistance, omm_capture_get_walk_speed(o) * 12.f, o->oForwardVel, SOUND_OBJ2_SCUTTLEBUG_WALK, OBJ_PARTICLE_NONE);
    }

    // Cappy values
    gOmmObject->cappy.copyGfx   = true;
    gOmmObject->cappy.offset[1] = 90.f;
    gOmmObject->cappy.scale     = 0.8f;

    // OK
    pobj_return_ok;
}
