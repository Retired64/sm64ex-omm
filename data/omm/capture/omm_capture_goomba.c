#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Init
//

bool omm_cappy_goomba_init(struct Object *o) {

    // Retrieve the nearest Goombas (up to OBJ_GOOMBA_STACK_MAX)
    // First Goomba is the one Mario's trying to capture
    struct Object *goombas[OBJ_GOOMBA_STACK_MAX] = { o };
    s32 goombaCount = 1;
    omm_array_for_each(omm_obj_get_goomba_behaviors(), p) {
        const BehaviorScript *bhv = (const BehaviorScript *) p->as_ptr;
        for_each_object_with_behavior(goomba, bhv) {
            if (goomba == o) continue;
            f32 dist = obj_get_horizontal_distance(o, goomba);
            f32 ydif = abs_f(goomba->oPosY - o->oPosY);
            if (dist < 20.f && ydif < (70.f * OBJ_GOOMBA_STACK_MAX * o->oScaleY)) {
                goombas[goombaCount++] = goomba;
                if (goombaCount >= OBJ_GOOMBA_STACK_MAX) {
                    break;
                }
            }
        }
    }

    // Store the lowest Y value, this will be the Y value of the base
    f32 lowestY = CELL_HEIGHT_LIMIT;
    for (s32 i = 0; i != goombaCount; ++i) {
        lowestY = min_f(lowestY, goombas[i]->oPosY);
    }

    // Setup the first Goomba
    o->oPosY = lowestY;
    o->oGoombaStackParent = o;
    obj_update_gfx(o);

    // Setup the Goomba stack
    for (s32 i = 1; i != goombaCount; ++i) {
        gOmmObject->goomba.stackObj[i - 1] = goombas[i];
        goombas[i]->oPosY = o->oPosY + i * o->oScaleY * 65.f;
        goombas[i]->curBhvCommand = bhvOmmGoombaStackCapture;
        goombas[i]->bhvStackIndex = 0;
        goombas[i]->oInteractStatus = 0;
        goombas[i]->oIntangibleTimer = -1;
        goombas[i]->oGoombaStackParent = o;
        obj_update_gfx(goombas[i]);
    }
    gOmmObject->goomba.stackCount = (goombaCount - 1);
    gOmmObject->state.actionTimer = 0;
    return true;
}

void omm_cappy_goomba_end(struct Object *o) {
    struct MarioState *m = gMarioState;

    // Reset Goomba's state
    o->curBhvCommand = o->behavior;
    o->bhvStackIndex = 0;
    o->oAction = 0;
    o->oInteractStatus = 0;
    o->oIntangibleTimer = 3;
    o->oGoombaStackParent = o;

    // Stay stacked when released, but break the stack if Mario gets damaged
    bool stack = (m->action == ACT_IDLE || m->action == ACT_FREEFALL || m->action == ACT_WATER_IDLE || m->action == ACT_OMM_LEAVE_OBJECT_JUMP || m->action == ACT_OMM_LEAVE_OBJECT_WATER);
    for (u8 i = 0; i != gOmmObject->goomba.stackCount; ++i) {
        struct Object *goomba = gOmmObject->goomba.stackObj[i];
        goomba->curBhvCommand = (stack ? bhvOmmGoombaStack : goomba->behavior);
        goomba->bhvStackIndex = 0;
        goomba->oAction = stack * (i + 1);
        goomba->oInteractStatus = 0;
        goomba->oIntangibleTimer = 3;
        goomba->oGoombaStackParent = o;
    }
}

f32 omm_cappy_goomba_get_top(UNUSED struct Object *o) {
    return omm_capture_get_hitbox_height(o) * (1 + gOmmObject->goomba.stackCount);
}

//
// Update
//

s32 omm_cappy_goomba_update(struct Object *o) {

    // Hitbox
    o->hitboxRadius = omm_capture_get_hitbox_radius(o);
    o->hitboxHeight = omm_capture_get_hitbox_height(o) * (gOmmObject->goomba.stackCount + 1);
    o->hitboxDownOffset = omm_capture_get_hitbox_down_offset(o);
    o->oWallHitboxRadius = omm_capture_get_wall_hitbox_radius(o);

    // Properties
    POBJ_SET_ABOVE_WATER;
    POBJ_SET_ABLE_TO_MOVE_ON_SLOPES;

    // Inputs
    if (!obj_update_door(o) && !omm_mario_is_locked(gMarioState)) {
        pobj_move(o, POBJ_B_BUTTON_DOWN, false, false);
        switch (pobj_jump(o, 0, 1)) {
            case POBJ_RESULT_JUMP_START: {
                obj_play_sound(o, SOUND_OBJ_GOOMBA_ALERT);
                if (gOmmMario->spin.timer) {
                    gOmmObject->state.actionTimer = 8;
                    o->oVelY = max_f(o->oVelY, 1.35f * omm_capture_get_jump_velocity(o) * POBJ_PHYSICS_JUMP);
                }
            } break;
            case POBJ_RESULT_JUMP_HOLD: {
                if (gOmmObject->state.actionTimer) {
                    o->oVelY = max_f(o->oVelY, 1.35f * omm_capture_get_jump_velocity(o) * POBJ_PHYSICS_JUMP);
                }
            } break;
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

    // Goomba stack
    if (gOmmObject->goomba.stackCount < (OBJ_GOOMBA_STACK_MAX - 1) && omm_obj_is_goomba(obj) && obj->oIntangibleTimer == 0) {

        // Fall check
        if ((!obj_is_on_ground(o) && o->oVelY <= 0.f) || (obj_is_on_ground(o) && o->oPosY > obj->oPosY + obj->hitboxHeight / 4.f)) {
            
            // Height check
            f32 y1 = obj->oPosY;
            f32 y2 = obj->oPosY + obj->hitboxHeight * 1.6f;
            if (y1 < o->oPosY && o->oPosY < y2) {

                // Radius check
                f32 r2 = sqr_f(o->hitboxRadius + obj->hitboxRadius);
                f32 d2 = sqr_f(o->oPosX - obj->oPosX) + sqr_f(o->oPosZ - obj->oPosZ);
                if (d2 < r2) {

                    // Add goomba to stack
                    gOmmObject->goomba.stackObj[gOmmObject->goomba.stackCount++] = obj;
                    obj->curBhvCommand = bhvOmmGoombaStackCapture;
                    obj->bhvStackIndex = 0;
                    obj->oInteractStatus = INT_STATUS_INTERACTED;
                    obj->oIntangibleTimer = -1;
                    obj->oGoombaStackParent = o;
                    obj_spawn_white_puff(obj, SOUND_OBJ_DEFAULT_DEATH);
                }
            }
        }
    }

    );
    pobj_stop_if_unpossessed();

    // Gfx
    obj_update_gfx(o);
    obj_anim_play(o, 0, (o->oVelY <= 0.f) * max_f(1.f, o->oForwardVel * 2.f / (omm_capture_get_walk_speed(o))));
    obj_update_blink_state(o, &o->oGoombaBlinkTimer, 30, 50, 5);
    if (obj_is_on_ground(o)) {
        obj_make_step_sound_and_particle(o, &gOmmObject->state.walkDistance, omm_capture_get_walk_speed(o) * 8.f, o->oForwardVel, SOUND_OBJ_GOOMBA_WALK, OBJ_PARTICLE_NONE);
        obj_spawn_particle_preset(o, PARTICLE_DUST * (gOmmMario->spin.timer != 0), false);
    } else {
        gOmmMario->spin.timer = 0;
    }
    if (gOmmObject->state.actionTimer) {
        o->oGfxAngle[1] += 0x2000 * gOmmObject->state.actionTimer;
        obj_spawn_particle_preset(o, PARTICLE_SPARKLES, false);
        gOmmObject->state.actionTimer--;
    }

    // Update Goomba stack
    for (u8 i = 0; i != gOmmObject->goomba.stackCount; ++i) {
        struct Object *obj = gOmmObject->goomba.stackObj[i];
        obj_set_pos(obj, o->oPosX, o->oPosY + omm_capture_get_hitbox_height(o) * (i + 1), o->oPosZ);
        obj_set_home(obj, o->oPosX, o->oPosY, o->oPosZ);
        obj_set_angle(obj, o->oFaceAnglePitch, o->oGfxAngle[1], o->oFaceAngleRoll);
        obj_set_scale(obj, o->oGfxScale[0], o->oGfxScale[1], o->oGfxScale[2]);
        obj_update_gfx(obj);
        obj_anim_play(obj, 0, 1.f);
        obj_update_blink_state(obj, &obj->oGoombaBlinkTimer, 30, 50, 5);
    }

    // Cappy values
    gOmmObject->cappy.offset[1] = (omm_capture_get_hitbox_height(o) / o->oScaleY) * (1 + gOmmObject->goomba.stackCount);
    gOmmObject->cappy.scale     = 0.8f;

    // OK
    pobj_return_ok;
}
