#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Geo layout
//

const GeoLayout omm_geo_snowball[] = {
    OMM_GEO_BILLBOARD_AND_BRANCH(white_particle_geo),
};

//
// Behavior
//

static void bhv_omm_snowball_delete(struct Object *o) {
    f32 scale = o->oScaleX;
    obj_play_sound(o, SOUND_OBJ_SNOW_SAND1);
    obj_spawn_particles(o, 8, MODEL_WHITE_PARTICLE, -o->hitboxDownOffset, 4 * scale, 2 * scale, 8 * scale, 4 * scale, -2 * sqrtf(scale), 0.3f * scale, 0.2f * scale);
    obj_mark_for_deletion(o);
}

static void bhv_omm_snowball_update() {
    struct Object *o = gCurrentObject;

    // In Mr. Blizzard's hand
    if (o->parentObj) {

        // Release if no longer in Mr. Blizzard's hand
        if (!o->parentObj->oMrBlizzardHeldObj) {
            o->parentObj = NULL;
            o->oVelX = 40.f * sins(o->oFaceAngleYaw);
            o->oVelY = 20.f;
            o->oVelZ = 40.f * coss(o->oFaceAngleYaw);
        }

        // Follow parent's hand
        else {
            s16 da = o->parentObj->oFaceAngleYaw + 0x4000;
            f32 dx = -88.f * o->parentObj->oScaleX * sins(da);
            f32 dy = 140.f * o->parentObj->oScaleY;
            f32 dz = -88.f * o->parentObj->oScaleX * coss(da);
            o->oPosX = o->parentObj->oPosX + dx;
            o->oPosY = o->parentObj->oPosY + dy;
            o->oPosZ = o->parentObj->oPosZ + dz;
            o->oFaceAngleYaw = o->parentObj->oFaceAngleYaw;
        }
    }

    // Released
    else {
        perform_object_step(o, OBJ_STEP_UPDATE_HOME);
        o->oVelY -= 2.f;

        // Out of bounds, or collided with a wall/ceiling/floor
        if (!o->oFloor || o->oWall || (o->oCeil && o->oCeil->normal.y > -0.9f) || o->oDistToFloor <= 5) {
            bhv_omm_snowball_delete(o);
            return;
        }
    }

    // Update
    vec3f_copy(o->oGfxPos, &o->oPosX);
    obj_set_params(o, 0, 0, 0, 0, !o->parentObj);
    obj_reset_hitbox(o, 30, 50, 0, 0, 15, 25);
    if (!o->parentObj) {
        struct Object *interacted = omm_obj_process_interactions(o, (o->oScaleX >= 2.f ? OBJ_INT_PRESET_SNOWBALL_LARGE : OBJ_INT_PRESET_SNOWBALL_SMALL));
        if (interacted && !omm_obj_is_collectible(interacted)) {
            bhv_omm_snowball_delete(o);
        }
    }
}

const BehaviorScript bhvOmmSnowball[] = {
    OBJ_TYPE_SPECIAL,
    0x08000000,
    0x0C000000, (uintptr_t) bhv_omm_snowball_update,
    0x09000000,
};

//
// Spawner
//

struct Object *omm_spawn_snowball(struct Object *o) {
    struct Object *snowball = obj_spawn_from_geo(o, omm_geo_snowball, bhvOmmSnowball);
    snowball->parentObj = o;
    return snowball;
}
