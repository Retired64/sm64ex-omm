#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Init
//

bool omm_cappy_mr_i_init(struct Object *o) {
    if (o->oBehParams2ndByte != 0 || ( // Don't capture Big Mr. I
        o->oAction != 1 &&
        o->oAction != 2)) {
        return false;
    }

    struct Object *iris = obj_get_nearest_with_behavior(o, bhvMrIBody);
    if (iris) iris->oRoom = -1;
    gOmmObject->state.actionTimer = 0;
    return true;
}

void omm_cappy_mr_i_end(struct Object *o) {
    obj_drop_to_floor(o);
    o->oHomeX = o->oPosX;
    o->oHomeY = o->oPosY;
    o->oHomeZ = o->oPosZ;
    o->oMrIUnkFC = 0;
    o->oAction = 1;
}

f32 omm_cappy_mr_i_get_top(struct Object *o) {
    return 180.f * o->oScaleY;
}

//
// Update
//

s32 omm_cappy_mr_i_update(struct Object *o) {

    // Hitbox
    o->hitboxRadius = omm_capture_get_hitbox_radius(o);
    o->hitboxHeight = omm_capture_get_hitbox_height(o);
    o->hitboxDownOffset = omm_capture_get_hitbox_down_offset(o);
    o->oWallHitboxRadius = omm_capture_get_wall_hitbox_radius(o);

    // Properties
    POBJ_SET_ABOVE_WATER;
    POBJ_SET_UNDER_WATER;
    POBJ_SET_ABLE_TO_MOVE_ON_SLOPES;

    // Inputs
    if (!obj_update_door(o) && !omm_mario_is_locked(gMarioState)) {
        pobj_move(o, false, false, false);
        if (pobj_jump(o, 0, 1) == POBJ_RESULT_JUMP_START) {
            obj_play_sound(o, SOUND_OBJ_GOOMBA_ALERT);
        }

        // Shoot
        // Hold B to shoot faster, further and with bigger projectiles
        if (POBJ_B_BUTTON_DOWN) {
            gOmmObject->state.actionTimer = min_s(gOmmObject->state.actionTimer + 1, 20);
        } else if (gOmmObject->state.actionTimer > 0) {
            f32 power = 1.f + 2.f * (gOmmObject->state.actionTimer / 20.f);
            omm_spawn_mr_i_beam(o, power);
            gOmmObject->state.actionTimer = 0;
        }
    }

    // Movement
    perform_object_step(o, POBJ_STEP_FLAGS);
    pobj_decelerate(o, 0.80f, 0.95f);
    pobj_apply_gravity(o, 1.f);
    pobj_handle_special_floors(o);
    pobj_stop_if_unpossessed();

    // Interactions
    pobj_process_interactions(

    // Doors
    obj_open_door(o, obj);

    );
    pobj_stop_if_unpossessed();

    // Gfx
    obj_set_angle(o, 0, o->oFaceAngleYaw, 0);
    obj_update_gfx(o);

    // Cappy values
    gOmmObject->cappy.offset[1] = 180.f;
    gOmmObject->cappy.scale     = 1.5f;

    // OK
    pobj_return_ok;
}
