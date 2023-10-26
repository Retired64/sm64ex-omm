#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

#define OMM_CAPPY_MOTOS_HAND_POS_1  40.f, 90.f, 30.f
#define OMM_CAPPY_MOTOS_HAND_POS_2  40.f, 60.f, -50.f

enum sMotosActions {
    MOTOS_ACT_WAIT,
    MOTOS_ACT_PLAYER_SEARCH,
    MOTOS_ACT_PLAYER_CARRY,
    MOTOS_ACT_PLAYER_PITCH,
    MOTOS_ACT_CARRY_RUN,
    MOTOS_ACT_THROWN,
    MOTOS_ACT_RECOVER,
    MOTOS_ACT_DEATH
};

enum sMotosAnimations {
    MOTOS_ANIM_BASE,
    MOTOS_ANIM_CARRY,
    MOTOS_ANIM_CARRY_RUN,
    MOTOS_ANIM_CARRY_START,
    MOTOS_ANIM_DOWN_RECOVER,
    MOTOS_ANIM_DOWN_STOP,
    MOTOS_ANIM_PITCH,
    MOTOS_ANIM_SAFE_DOWN,
    MOTOS_ANIM_WAIT,
    MOTOS_ANIM_WALK,
};

//
// Init
//

bool omm_cappy_motos_init(struct Object *o) {
    if (o->oAction == MOTOS_ACT_PLAYER_CARRY ||
        o->oAction == MOTOS_ACT_PLAYER_PITCH ||
        o->oAction == MOTOS_ACT_CARRY_RUN ||
        o->oAction == MOTOS_ACT_DEATH) {
        return false;
    }
    gOmmObject->state.actionState = 0;
    gOmmObject->state.actionTimer = 0;
    gOmmObject->motos.heldObj = NULL;
    return true;
}

void omm_cappy_motos_end(struct Object *o) {
    omm_obj_throw(gOmmObject->motos.heldObj, 40.f, 20.f);
    obj_anim_play(o, MOTOS_ANIM_WAIT, 1.f);
    obj_set_vel(o, 0.f, 0.f, 0.f);
    o->oAction = MOTOS_ACT_WAIT;
}

f32 omm_cappy_motos_get_top(struct Object *o) {
    return 115.f * o->oScaleY;
}

//
// Update
//

static void omm_cappy_motos_try_to_hold_object(struct Object *o) {
    f32 dMax = 200.f + o->hitboxRadius;
    f32 yMin = o->oPosY - o->hitboxDownOffset - 50.f;
    f32 yMax = o->oPosY - o->hitboxDownOffset + 50.f + o->hitboxHeight;
    omm_array_for_each(omm_obj_get_holdable_behaviors(), p) {
        const BehaviorScript *bhv = (const BehaviorScript *) p->as_ptr;
        for_each_object_with_behavior(obj, bhv) {
            if (obj != o && !obj_is_dormant(obj)) {
                u16 angleDiff = (u16) abs_s((s16) (obj_get_object1_angle_yaw_to_object2(o, obj) - o->oFaceAngleYaw));
                if (angleDiff < 0x3000 && obj->oPosY >= yMin && obj->oPosY <= yMax && obj_get_horizontal_distance(o, obj) <= dMax) {
                    if (omm_obj_hold(obj)) {
                        obj_set_home(obj, obj->oPosX, obj->oPosY, obj->oPosZ);
                        gOmmObject->motos.heldObj = obj;
                        gOmmObject->state.actionState = 1;
                        omm_mario_lock(gMarioState, -1);
                        obj_play_sound(o, SOUND_GENERAL_CANNON_UP);
                        return;
                    }
                }
            }
        }
    }
}

s32 omm_cappy_motos_update(struct Object *o) {

    // Hitbox
    o->hitboxRadius = omm_capture_get_hitbox_radius(o);
    o->hitboxHeight = omm_capture_get_hitbox_height(o);
    o->hitboxDownOffset = omm_capture_get_hitbox_down_offset(o);
    o->oWallHitboxRadius = omm_capture_get_wall_hitbox_radius(o);

    // Properties
    POBJ_SET_ABOVE_WATER;
    POBJ_SET_UNDER_WATER;
    POBJ_SET_INVULNERABLE;
    POBJ_SET_IMMUNE_TO_FIRE;
    POBJ_SET_IMMUNE_TO_LAVA;
    POBJ_SET_IMMUNE_TO_SAND;
    POBJ_SET_IMMUNE_TO_WIND;

    // Inputs
    if (!obj_update_door(o) && !omm_mario_is_locked(gMarioState)) {
        obj_set_angle(o, 0, o->oFaceAngleYaw, 0);
        pobj_move(o, false, false, false);
        if (pobj_jump(o, 0, 1) == POBJ_RESULT_JUMP_START) {
            obj_play_sound(o, SOUND_OBJ_GOOMBA_ALERT);
        } else if (POBJ_B_BUTTON_PRESSED) {
            if (!gOmmObject->motos.heldObj) {
                omm_cappy_motos_try_to_hold_object(o);
            } else {
                gOmmObject->state.actionState = 2;
                omm_mario_lock(gMarioState, -1);
            }
        }
    }

    // Movement
    perform_object_step(o, POBJ_STEP_FLAGS);
    pobj_decelerate(o, 0.80f, 0.95f);
    pobj_apply_gravity(o, 1.f);
    pobj_handle_special_floors(o);
    pobj_stop_if_unpossessed();

    // Lava must be handled separately
    if (obj_is_on_ground(o) && o->oFloor->type == SURFACE_BURNING && !OMM_CHEAT_WALK_ON_LAVA) {
        omm_mario_unpossess_object(gMarioState, OMM_MARIO_UNPOSSESS_ACT_BURNT, false, 0);
        omm_mario_set_action(gMarioState, ACT_LAVA_BOOST, 0, 0);
        o->oAction = MOTOS_ACT_DEATH;
        pobj_stop_if_unpossessed();
    }

    // Interactions
    pobj_process_interactions(

    // Doors
    obj_open_door(o, obj);

    );
    pobj_stop_if_unpossessed();

    // Gfx
    obj_update_gfx(o);
    switch (gOmmObject->state.actionState) {

        // Idle, walk, jump
        case 0: {
            obj_anim_play(o, gOmmObject->motos.heldObj ? MOTOS_ANIM_CARRY_RUN : MOTOS_ANIM_WALK, 1.f);
            omm_obj_update_held_object(gOmmObject->motos.heldObj, o, OMM_CAPPY_MOTOS_HAND_POS_1);
            if (obj_is_on_ground(o)) {
                obj_make_step_sound_and_particle(
                    o, &gOmmObject->state.walkDistance,
                    omm_capture_get_walk_speed(o) * 16.f, o->oForwardVel,
                    SOUND_OBJ_BULLY_WALKING, OBJ_PARTICLE_NONE
                );
            }
        } break;

        // Hold
        case 1: {
            obj_anim_play(o, MOTOS_ANIM_CARRY_START, 1.5f);
            omm_obj_update_held_object(gOmmObject->motos.heldObj, o, OMM_CAPPY_MOTOS_HAND_POS_1);
            gOmmObject->motos.heldObj->oPosX = relerp_0_1_f(obj_anim_get_frame(o), 0, 30, gOmmObject->motos.heldObj->oHomeX, gOmmObject->motos.heldObj->oPosX);
            gOmmObject->motos.heldObj->oPosY = relerp_0_1_f(obj_anim_get_frame(o), 0, 30, gOmmObject->motos.heldObj->oHomeY, gOmmObject->motos.heldObj->oPosY);
            gOmmObject->motos.heldObj->oPosZ = relerp_0_1_f(obj_anim_get_frame(o), 0, 30, gOmmObject->motos.heldObj->oHomeZ, gOmmObject->motos.heldObj->oPosZ);
            gOmmObject->motos.heldObj->oPosY += 0.5f * o->hitboxHeight * sins(relerp_0_1_s(obj_anim_get_frame(o), 0, 30, 0, 0x8000));
            obj_update_gfx(gOmmObject->motos.heldObj);
            if (obj_anim_is_near_end(o)) {
                sound_stop(SOUND_GENERAL_CANNON_UP, o->oCameraToObject);
                omm_mario_unlock(gMarioState);
                gOmmObject->state.actionState = 0;
            }
        } break;

        // Throw
        case 2: {
            obj_anim_play(o, MOTOS_ANIM_PITCH, 1.5f);
            Vec3f pos1 = { OMM_CAPPY_MOTOS_HAND_POS_1 };
            Vec3f pos2 = { OMM_CAPPY_MOTOS_HAND_POS_2 };
            f32 t = sins(relerp_0_1_f(obj_anim_get_frame(o), 0, 14, 0, 0x8000));
            f32 x = lerp_f(t, pos1[0], pos2[0]);
            f32 y = lerp_f(t, pos1[1], pos2[1]);
            f32 z = lerp_f(t, pos1[2], pos2[2]);
            omm_obj_update_held_object(gOmmObject->motos.heldObj, o, x, y, z);
            if (gOmmObject->motos.heldObj && obj_anim_get_frame(o) >= 14) {
                omm_obj_throw(gOmmObject->motos.heldObj, 48.f, 24.f);
                obj_play_sound(o, SOUND_OBJ_UNKNOWN4);
                gOmmObject->motos.heldObj = NULL;
            } else if (obj_anim_is_near_end(o)) {
                omm_mario_unlock(gMarioState);
                gOmmObject->state.actionState = 0;
            }
        } break;
    }

    // Cappy values
    gOmmObject->cappy.offset[1] = 115.f;
    gOmmObject->cappy.scale     = 0.5f;

    // OK
    pobj_return_ok;
}
