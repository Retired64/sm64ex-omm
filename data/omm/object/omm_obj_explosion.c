#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Geo layout
//

const GeoLayout omm_geo_explosion[] = {
    OMM_GEO_BILLBOARD_AND_BRANCH(explosion_geo),
};

//
// Behavior
//

static void bhv_omm_explosion_update() {
    struct Object *o = gCurrentObject;
    if (o->oTimer >= 10) {
        spawn_object(o, MODEL_SMOKE, bhvBobombBullyDeathSmoke);
        obj_mark_for_deletion(o);
        return;
    }

    obj_scale(o, 1.f);
    obj_set_params(o, 0, 0, 0, 0, true);
    obj_reset_hitbox(o, 360, 480, 0, 0, 0, 120);
    obj_scale(o, 1.f + (o->oTimer / 3.f));
    o->oOpacity = max_s(0, 0xFF - (0x1C * o->oTimer));
    o->oAnimState++;

    // Damage King Bob-omb
    for_each_object_with_behavior(kbo, bhvKingBobomb) { if (kbo->oAction == 6) { kbo->oIntangibleTimer = 0; } }
    struct Object *interacted = omm_obj_process_interactions(o, OBJ_INT_PRESET_EXPLOSION);
    if (interacted &&
        interacted->behavior == bhvKingBobomb &&
        interacted->oHeldState == HELD_FREE && (
        interacted->oAction == 1 ||
        interacted->oAction == 2 ||
        interacted->oAction == 6)) {
        obj_set_angle(interacted, 0, obj_get_object1_angle_yaw_to_object2(o, interacted), 0);
        obj_set_forward_and_y_vel(interacted, 20, 50);
        obj_anim_play_with_sound(interacted, 6, 1, 0, true);
        interacted->oAction = 4;
    }
}

const BehaviorScript bhvOmmExplosion[] = {
    OBJ_TYPE_SPECIAL,
    0x110100C1,
    0x08000000,
    0x0C000000, (uintptr_t) bhv_omm_explosion_update,
    0x09000000,
};

//
// Spawner
//

struct Object *omm_spawn_explosion(struct Object *o) {
    struct Object *explosion  = obj_spawn_from_geo(o, omm_geo_explosion, bhvOmmExplosion);
    explosion->oAnimState     = -1;
    explosion->oGraphYOffset += 100;
    explosion->oOpacity       = 255;
    create_sound_spawner(SOUND_GENERAL2_BOBOMB_EXPLOSION);
    set_environmental_camera_shake(SHAKE_ENV_EXPLOSION);
    return explosion;
}
