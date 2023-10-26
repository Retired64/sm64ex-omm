#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Init
//

bool omm_cappy_hoot_init(struct Object *o) {
    if (o->oHootAvailability != HOOT_AVAIL_READY_TO_FLY) {
        return false;
    }

    gOmmObject->state.initialPos[0] = o->oHomeX;
    gOmmObject->state.initialPos[1] = o->oHomeY;
    gOmmObject->state.initialPos[2] = o->oHomeZ;
    return true;
}

void omm_cappy_hoot_end(struct Object *o) {
    o->oHomeX = gOmmObject->state.initialPos[0];
    o->oHomeY = gOmmObject->state.initialPos[1];
    o->oHomeZ = gOmmObject->state.initialPos[2];
    o->oCurrAnim = NULL;
    obj_anim_play(o, 0, 1.f);
}

f32 omm_cappy_hoot_get_top(struct Object *o) {
    return 100.f * o->oScaleY;
}

//
// Update
//

s32 omm_cappy_hoot_update(struct Object *o) {

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
    if (!omm_mario_is_locked(gMarioState)) {
        pobj_move(o, false, false, false);
        if (pobj_jump(o, 0, 6) == POBJ_RESULT_JUMP_START) {
            o->oCurrAnim = NULL;
            obj_play_sound(o, SOUND_GENERAL_SWISH_WATER);
        }
    }

    // Movement
    perform_object_step(o, POBJ_STEP_FLAGS);
    pobj_decelerate(o, 0.80f, 0.95f);
    pobj_apply_gravity(o, 1.f);
    pobj_handle_special_floors(o);
    pobj_stop_if_unpossessed();

    // Interactions
    pobj_process_interactions();
    pobj_stop_if_unpossessed();

    // Gfx
    obj_update_gfx(o);
    obj_anim_play(o, 0, (o->oVelY > 0.f) ? 2.f : 1.f);

    // Cappy values
    gOmmObject->cappy.offset[1] = 100.f;
    gOmmObject->cappy.offset[2] = 26.f;
    gOmmObject->cappy.scale     = 0.8f;

    // OK
    pobj_return_ok;
}
