#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Init
//

bool omm_cappy_snufit_init(struct Object *o) {
    obj_scale(o, 1.f);
    obj_set_angle(o, 0, o->oFaceAngleYaw, 0);
    o->oPosY -= 60.f * o->oScaleY;
    o->oSnufitScale = 1.f;
    o->oSnufitBodyScale = 1000.f;
    gOmmObject->state.actionState = 0;
    gOmmObject->state.actionTimer = 0;
    return true;
}

void omm_cappy_snufit_end(struct Object *o) {
    obj_scale(o, 1.f);
    obj_set_angle(o, 0, o->oFaceAngleYaw, 0);
    o->oPosY += 60.f * o->oScaleY;
    o->oAction = SNUFIT_ACT_IDLE;
    o->oSnufitRecoil = 0;
    o->oSnufitBullets = 0;
    o->oSnufitScale = 1.f;
    o->oSnufitBodyScale = 1000.f;
    o->oSnufitBodyScalePeriod = 0x8000;
    o->oSnufitBodyBaseScale = 167;
    
    // Compute new home pos, so the snufit pos matches its orbit
    o->oHomeX = o->oPosX - 100.f * coss(o->oSnufitCircularPeriod);
    o->oHomeY = o->oPosY -   8.f * coss(4000 * gGlobalTimer);
    o->oHomeZ = o->oPosZ - 100.f * sins(o->oSnufitCircularPeriod);
}

f32 omm_cappy_snufit_get_top(struct Object *o) {
    return 90.f * o->oScaleY;
}

//
// Update
//

s32 omm_cappy_snufit_update(struct Object *o) {

    // Hitbox
    o->hitboxRadius = omm_capture_get_hitbox_radius(o);
    o->hitboxHeight = omm_capture_get_hitbox_height(o);
    o->hitboxDownOffset = omm_capture_get_hitbox_down_offset(o);
    o->oWallHitboxRadius = omm_capture_get_wall_hitbox_radius(o);

    // Properties
    POBJ_SET_ABOVE_WATER;
    POBJ_SET_ABLE_TO_MOVE_ON_SLOPES;

    // Inputs
    if (!omm_mario_is_locked(gMarioState)) {
        pobj_move(o, false, false, false);
        if (pobj_jump(o, 0, 1) == POBJ_RESULT_JUMP_START) {
            obj_play_sound(o, SOUND_OBJ_GOOMBA_ALERT);
        }

        // Shoot 3 balls
        if (POBJ_B_BUTTON_PRESSED) {
            omm_spawn_snufit_ball(o, 0, (random_u16() & 15) == 0);
            omm_spawn_snufit_ball(o, 3, (random_u16() & 15) == 0);
            omm_spawn_snufit_ball(o, 6, (random_u16() & 15) == 0);
        }

        // Hold B for at least 1 second to start to rapid-fire
        if (POBJ_B_BUTTON_DOWN) {
            if (gOmmObject->state.actionTimer++ >= 15) {
                gOmmObject->state.actionState = 1;
                if (gOmmObject->state.actionTimer & 1) {
                    omm_spawn_snufit_ball(o, 0, (random_u16() & 15) == 0);
                }
            }
        } else {
            gOmmObject->state.actionState = 0;
            gOmmObject->state.actionTimer = 0;
        }
    }

    // Movement
    perform_object_step(o, POBJ_STEP_FLAGS);
    pobj_decelerate(o, 0.80f, 0.80f);
    pobj_apply_gravity(o, 1.f);
    pobj_handle_special_floors(o);
    pobj_stop_if_unpossessed();

    // Interactions
    pobj_process_interactions();
    pobj_stop_if_unpossessed();

    // Gfx
    obj_update_gfx(o);
    f32 yOff = 8.f * coss(4000 * gGlobalTimer);
    o->oGfxPos[1] += 60.f * o->oScaleY + yOff;

    // Cappy values
    gOmmObject->cappy.offset[1] = 90.f + yOff;
    gOmmObject->cappy.scale     = 0.8f;

    // OK
    pobj_return_ok;
}
