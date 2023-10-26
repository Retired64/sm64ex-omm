#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

static bool is_object_destructible(struct Object *o) {
    return
        (o->behavior == bhvBowlingBall) ||
        (o->behavior == bhvPitBowlingBall) ||
        (o->behavior == bhvFreeBowlingBall) ||
        (o->behavior == bhvMrBlizzard) ||
        (o->behavior == bhvSnowmansBottom) ||
        (o->behavior == bhvBulletBill) ||
        (o->behavior == bhvHauntedChair) ||
        (o->behavior == bhvJrbSlidingBox) ||
        (o->behavior == bhvMrI) ||
        (o->behavior == bhvSpiny) ||
        (o->behavior == bhvBigBoulder) ||
        (o->behavior == bhvMrIParticle) ||
        (o->behavior == bhvMadPiano) ||
        (o->behavior == bhvMontyMoleRock) ||
        (o->behavior == bhvMrBlizzardSnowball) ||
        (o->behavior == bhvWaterBomb);
}

static void omm_mario_destroy_or_push_away_object(struct MarioState *m, struct Object *o) {
    if (is_object_destructible(o)) {
        obj_destroy(o);
    } else {
        f32 x = m->pos[0];
        f32 z = m->pos[2];
        push_mario_out_of_object(m, o, 0);
        o->oPosX -= (m->pos[0] - x);
        o->oPosZ -= (m->pos[2] - z);
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->pos[0] = x;
        m->pos[2] = z;
    }
}

void push_mario_out_of_object(struct MarioState *m, struct Object *o, f32 padding) {
    if (m->action != ACT_GETTING_BLOWN &&
        m->action != ACT_THROWN_FORWARD &&
        m->action != ACT_THROWN_BACKWARD) {
        f32 dx = m->pos[0] - o->oPosX;
        f32 dz = m->pos[2] - o->oPosZ;
        f32 dv = sqrtf(dx * dx + dz * dz);
        f32 dm = o->hitboxRadius + m->marioObj->hitboxRadius + padding;
        if (dv < dm) {
            s16 pushAngle = (dv ? atan2s(dz, dx) : m->faceAngle[1]);
            f32 mx = o->oPosX + dm * sins(pushAngle);
            f32 my = m->pos[1];
            f32 mz = o->oPosZ + dm * coss(pushAngle);
            f32_find_wall_collision(&mx, &my, &mz, 60.f, 50.f);
            struct Surface *floor = NULL;
            f32 fy = find_floor(mx, my, mz, &floor);
            if (floor) {
                vec3f_set(m->pos, mx, max_f(my, fy), mz);
                m->floor = floor;
                m->floorHeight = fy;
                if (my > fy + 100.f) {
                    m->input |= INPUT_OFF_FLOOR;
                }
            }
        }
    }
}

//
// Preprocess interactions
// Interactions types that are preprocessed here are not processed in mario_process_interactions
//

static bool sMarioIsPunching = false;
static bool sMarioIsKicking  = false;
static bool sMarioIsTripping = false;
u32 determine_interaction(struct MarioState *m, struct Object *o) {
    u32 interaction = 0;

    // Bounces
    if (m->action & ACT_FLAG_AIR) {
        if (m->vel[1] < 0.f && m->pos[1] > o->oPosY) {
            interaction = INT_HIT_FROM_ABOVE;
        } else if (m->vel[1] > 0.f && m->pos[1] < o->oPosY) {
            interaction = INT_HIT_FROM_BELOW;
        }
    }

    // Attacks
    if (m->action & ACT_FLAG_ATTACKING) {

        // Punching / Kicking
        if (omm_mario_is_punching(m) || omm_mario_is_kicking(m)) {
            s16 dYawToObject = mario_obj_angle_to_object(m, o) - m->faceAngle[1];
            if (sMarioIsPunching && abs_s(dYawToObject) <= 0x2AAA) {
                interaction = INT_PUNCH;
            }
            if (sMarioIsKicking && abs_s(dYawToObject) <= 0x2AAA) {
                interaction = INT_KICK;
            }
            if (sMarioIsTripping && abs_s(dYawToObject) <= 0x4000) {
                interaction = INT_TRIP;
            }
        }
        
        // Ground pounding
        else if (omm_mario_is_ground_pounding(m) || m->action == ACT_TWIRLING) {
            if (m->vel[1] < 0.f) {
                interaction = INT_GROUND_POUND_OR_TWIRL;
            }
        }
        
        // Landing
        else if (omm_mario_is_ground_pound_landing(m) || m->action == ACT_TWIRL_LAND) {
            if (m->vel[1] < 0.f && m->actionState == 0) {
                interaction = INT_GROUND_POUND_OR_TWIRL;
            }
        }
        
        // Slide kicking
        else if (m->action == ACT_SLIDE_KICK || m->action == ACT_SLIDE_KICK_SLIDE) {
            interaction = INT_SLIDE_KICK;
        }
        
        // Riding shell
        else if (m->action & ACT_FLAG_RIDING_SHELL) {
            interaction = INT_FAST_ATTACK_OR_SHELL;
        }
        
        // Going fast?
        else if (abs_f(m->forwardVel) >= 26.f) {
            interaction = INT_FAST_ATTACK_OR_SHELL;
        }
    }

    return interaction;
}

bool omm_mario_interact_coin(struct MarioState *m, struct Object *o) {
    if (o->respawnInfoType == OMM_OBJECT_LOST_COIN) gOmmSparklyContext->coinsLost++;
    else if (o->oDamageOrCoinValue == 1) gOmmSparklyContext->coinsYellow++;
    else if (o->oDamageOrCoinValue == 2) gOmmSparklyContext->coinsRed++;
    else if (o->oDamageOrCoinValue == 5) gOmmSparklyContext->coinsBlue++;
    gOmmStats->coinsCollected += o->oDamageOrCoinValue;
    interact_coin(m, INTERACT_COIN, o);
    return true;
}

bool omm_mario_interact_star_or_key(struct MarioState *m, struct Object *o) {
    if (!omm_mario_is_dead(m) && o) {

        // Sparkly star
        if (omm_sparkly_interact_star(m, o)) {
            return true;
        }
        
        // Collect star or key
        omm_sparkly_interact_grand_star(m, o);
        omm_save_file_collect_star_or_key(gCurrSaveFileNum - 1, OMM_GAME_MODE, gCurrLevelNum - 1, (o->oBehParams >> 24) & 0x1F, m->numCoins);
        m->numStars = omm_save_file_get_total_star_count(gCurrSaveFileNum - 1, OMM_GAME_MODE, COURSE_MIN - 1, COURSE_MAX - 1);

        // Update Mario
        mario_stop_riding_and_holding(m);
        update_mario_sound_and_camera(m);
        obj_play_sound(m->marioObj, SOUND_MENU_STAR_SOUND);
        spawn_object(o, MODEL_NONE, bhvStarKeyCollectionPuffSpawner);
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->usedObj = o;

        // Grand star?
        // Some rom-hacks replace the jumbo star cutscene by a warp to a 'we saved the world' cutscene level
        if (o->behavior == bhvGrandStar || (o->oInteractionSubtype & INT_SUBTYPE_GRAND_STAR)) {
            if (o->oInteractType & INTERACT_WARP) {
                interact_warp(m, INTERACT_WARP, o);
            } else {
                omm_mario_set_action(m, ACT_JUMBO_STAR_CUTSCENE, 0, 0xFFFF); 
            }
            return true;
        }

        // Mario exits the course if:
        // - All stars have been collected, or
        // - It's not a noExit star and Non-Stop mode is disabled
        bool noExit = !OMM_ALL_STARS && (OMM_STARS_NON_STOP || (o->oInteractionSubtype & INT_SUBTYPE_NO_EXIT));
        if (!noExit) {
            m->hurtCounter = 0;
            m->healCounter = 0;
            m->capTimer = min_s(m->capTimer, 1);
            drop_queued_background_music();
            fadeout_level_music(126);
        } else if (omm_mario_is_capture(m) && (!OMM_GAME_IS_SM64 || !(gCurrCourseNum == COURSE_WF && gLastCompletedStarNum == 1))) {
            omm_mario_lock_star_grab(m);
            return true;
        }

        // Action
        u32 starGrabAction = (noExit ? ACT_STAR_DANCE_NO_EXIT : ACT_STAR_DANCE_EXIT);
        if (m->action & ACT_FLAG_SWIMMING) starGrabAction = ACT_STAR_DANCE_WATER;
        if (m->action & ACT_FLAG_METAL_WATER) starGrabAction = ACT_STAR_DANCE_WATER;
        if (m->action & ACT_FLAG_AIR) starGrabAction = ACT_FALL_AFTER_STAR_GRAB;
        omm_mario_unpossess_object(m, OMM_MARIO_UNPOSSESS_ACT_JUMP_OUT, false, 0);
        omm_mario_set_action(m, starGrabAction, noExit, 0);
        vec3f_copy(&m->marioObj->oPosX, m->pos);
        return true;
    }
    return false;
}

bool omm_mario_interact_warp(struct MarioState *m, struct Object *o) {
#if OMM_GAME_IS_R96X
    if (!(o->oInteractionSubtype & INT_SUBTYPE_FADING_WARP) && (m->action != ACT_EMERGE_FROM_PIPE) && (o->oObjectID == 1)) {
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->usedObj = o;
        mario_stop_riding_and_holding(m);
        obj_play_sound(m->marioObj, SOUND_MENU_ENTER_PIPE);
        play_transition(WARP_TRANSITION_FADE_INTO_MARIO, 0x15, 0x00, 0x00, 0x00);
        
        // Left to right: Luigi, Mario, Wario
        s32 target = OMM_PLAYER_MARIO;
        switch (o->oBehParams) {
            case 0: target = OMM_PLAYER_LUIGI; break;
            case 1: target = OMM_PLAYER_MARIO; break;
            case 2: target = OMM_PLAYER_WARIO; break;
        }
        
        // If already selected, select Peach if unlocked
        // Otherwise, switch character if unlocked
        if (omm_player_is_selected(target) && omm_player_is_unlocked(OMM_PLAYER_PEACH)) {
            omm_player_select(OMM_PLAYER_PEACH);
            omm_mario_set_action(m, ACT_CHARACTER_SWITCH, TRUE, 0);
        } else if (omm_player_is_unlocked(target)) {
            omm_player_select(target);
            omm_mario_set_action(m, ACT_CHARACTER_SWITCH, TRUE, 0);
        } else {
            omm_mario_set_action(m, ACT_CHARACTER_SWITCH, FALSE, 0);
        }
        return true;
    }
#endif

    // Grand star?
    // Some rom-hacks replace the jumbo star cutscene by a warp to a 'we saved the world' cutscene level
    if (o->behavior == bhvGrandStar) {
        omm_speedrun_split(OMM_SPLIT_BOWSER);
        mario_stop_riding_and_holding(m);
        obj_play_sound(m->marioObj, SOUND_MENU_STAR_SOUND);
        spawn_object(o, MODEL_NONE, bhvStarKeyCollectionPuffSpawner);
        interact_warp(m, INTERACT_WARP, o);
        return true;
    }
    return false;
}

static bool omm_mario_interact_whirlpool(struct MarioState *m, UNUSED struct Object *o) {
    return omm_mario_has_metal_cap(m);
}

static bool omm_mario_interact_strong_wind(struct MarioState *m, UNUSED struct Object *o) {
    return omm_mario_has_metal_cap(m);
}

static bool omm_mario_interact_flame(struct MarioState *m, struct Object *o) {
    if (omm_peach_vibe_is_gloom() && omm_obj_is_flame(o)) {
        obj_spawn_white_puff(o, SOUND_GENERAL_FLAME_OUT);
        obj_mark_for_deletion(o);
        return true;
    }
#if OMM_GAME_IS_R96X
    if (OMM_SPARKLY_MODE_IS_ENABLED && omm_sparkly_is_bowser_4_battle() && m->action == ACT_WARIO_CHARGE) {
        o->oInteractStatus = INT_STATUS_INTERACTED;
        m->interactObj = o;
        m->marioObj->oMarioBurnTimer = 0;
        update_mario_sound_and_camera(m);
        drop_and_set_mario_action(m, ACT_BURNING_JUMP, 1);
        obj_play_sound(m->marioObj, SOUND_MARIO_ON_FIRE);
        return true;
    }
#else
    OMM_UNUSED(m);
#endif
    return false;
}

static bool omm_mario_interact_clam_or_bubba(struct MarioState *m, UNUSED struct Object *o) {
    return omm_mario_has_metal_cap(m);
}

static bool omm_mario_interact_shock(struct MarioState *m, UNUSED struct Object *o) {
    return omm_mario_has_metal_cap(m);
}

static bool omm_mario_interact_mr_blizzard(struct MarioState *m, struct Object *o) {
    if (omm_mario_has_metal_cap(m)) {
        omm_mario_destroy_or_push_away_object(m, o);
        return true;
    }
    return false;
}

static bool omm_mario_interact_unknown_08(struct MarioState *m, struct Object *o) {
    if (omm_mario_has_metal_cap(m)) {
        omm_mario_destroy_or_push_away_object(m, o);
        return true;
    }
    return false;
}

static bool omm_mario_interact_damage(struct MarioState *m, struct Object *o) {
    if (omm_mario_has_metal_cap(m)) {
        omm_mario_destroy_or_push_away_object(m, o);
        return true;
    }
    return false;
}

static bool omm_mario_interact_koopa_shell(struct MarioState *m, struct Object *o) {
    if (!(m->action & ACT_FLAG_RIDING_SHELL) && !(o->oInteractStatus & INT_STATUS_STOP_RIDING) && !o->oSubAction) {
        u32 interaction = determine_interaction(m, o);
        if ((interaction & (INT_GROUND_POUND_OR_TWIRL | INT_KICK | INT_HIT_FROM_ABOVE)) || m->action == ACT_WALKING || m->action == ACT_HOLD_WALKING) {
            m->interactObj = o;
            m->usedObj = o;
            m->riddenObj = o;
            o->oInteractStatus = INT_STATUS_INTERACTED;
            mario_drop_held_object(m);
            update_mario_sound_and_camera(m);
            audio_play_shell_music();
            if (m->action & ACT_FLAG_AIR) {
                omm_mario_set_action(m, ACT_RIDING_SHELL_FALL, 0, Z_TRIG);
            } else {
                omm_mario_set_action(m, ACT_RIDING_SHELL_GROUND, 0, Z_TRIG);
            }
            return true;
        }
        push_mario_out_of_object(m, o, 2);
    }
    return true; // don't execute the vanilla interaction
}

static s32 get_cap(struct Object *o) {
    if (o->behavior == bhvNormalCap) return MARIO_NORMAL_CAP;
    if (o->behavior == bhvWingCap)   return MARIO_WING_CAP;
    if (o->behavior == bhvMetalCap)  return MARIO_METAL_CAP;
    if (o->behavior == bhvVanishCap) return MARIO_VANISH_CAP;
    return 0;
}

bool omm_mario_interact_cap(struct MarioState *m, struct Object *o) {
    if (omm_peach_vibe_is_active()) {
        return true;
    }
    s32 cap = get_cap(o);
    if (cap && !omm_mario_is_capture(m) && m->action != ACT_GETTING_BLOWN) {
        gOmmStats->capsCollected++;
        if (OMM_POWER_UPS_IMPROVED) {
            m->interactObj = o;
            m->flags &= ~MARIO_CAP_ON_HEAD & ~MARIO_CAP_IN_HAND;
            m->flags |= cap;
            switch (cap) {
                case MARIO_NORMAL_CAP: audio_stop_cap_music();        m->capTimer = 1; break;
                case MARIO_WING_CAP:   audio_play_wing_cap_music();   m->capTimer = max_s(m->capTimer, OMM_IMPROVED_WING_CAP_DURATION); break;
                case MARIO_METAL_CAP:  audio_play_metal_cap_music();  m->capTimer = max_s(m->capTimer, OMM_IMPROVED_METAL_CAP_DURATION); break;
                case MARIO_VANISH_CAP: audio_play_vanish_cap_music(); m->capTimer = max_s(m->capTimer, OMM_IMPROVED_VANISH_CAP_DURATION); break;
            }
            
            obj_play_sound(m->marioObj, SOUND_MENU_STAR_SOUND);
            obj_play_sound(m->marioObj, SOUND_MARIO_HERE_WE_GO);
            o->oInteractStatus = INT_STATUS_INTERACTED;

            if ((m->action & ACT_FLAG_IDLE) || (m->action == ACT_WALKING)) {
                m->flags |= MARIO_CAP_IN_HAND;
                omm_mario_set_action(m, ACT_PUTTING_ON_CAP, 0, 0);
            } else {
                m->flags |= MARIO_CAP_ON_HEAD;
            }
            return true;
        }
    }
    return false;
}

static bool omm_mario_interact_pole(struct MarioState *m, struct Object *o) {
    return (m->action & ACT_FLAG_RIDING_SHELL) || o == gOmmMario->state.poleObject;
}

static bool omm_mario_interact_grabbable(struct MarioState *m, struct Object *o) {
    if ((o->oInteractionSubtype & INT_SUBTYPE_GRABS_MARIO) && obj_is_object1_facing_object2(o, m->marioObj, 0x3000)) {
        return omm_mario_has_metal_cap(m);
    }
    return false;
}

struct OmmInteractionHandler { u32 interactType; bool (*handler)(struct MarioState *, struct Object *); };
static const struct OmmInteractionHandler sOmmInteractionHandlers[] = {
    { INTERACT_COIN,            omm_mario_interact_coin },
    { INTERACT_STAR_OR_KEY,     omm_mario_interact_star_or_key },
    { INTERACT_WARP,            omm_mario_interact_warp },
    { INTERACT_WHIRLPOOL,       omm_mario_interact_whirlpool },
    { INTERACT_STRONG_WIND,     omm_mario_interact_strong_wind },
    { INTERACT_FLAME,           omm_mario_interact_flame },
    { INTERACT_CLAM_OR_BUBBA,   omm_mario_interact_clam_or_bubba },
    { INTERACT_SHOCK,           omm_mario_interact_shock },
    { INTERACT_MR_BLIZZARD,     omm_mario_interact_mr_blizzard },
    { INTERACT_UNKNOWN_08,      omm_mario_interact_unknown_08 },
    { INTERACT_DAMAGE,          omm_mario_interact_damage },
    { INTERACT_KOOPA_SHELL,     omm_mario_interact_koopa_shell },
    { INTERACT_CAP,             omm_mario_interact_cap },
    { INTERACT_POLE,            omm_mario_interact_pole },
    { INTERACT_GRABBABLE,       omm_mario_interact_grabbable },
};
static const s32 sOmmInteractionHandlerCount = sizeof(sOmmInteractionHandlers) / sizeof(sOmmInteractionHandlers[0]);

void omm_mario_preprocess_interactions(struct MarioState *m) {

    // Unset pole object if no pole interaction
    if (!(m->collidedObjInteractTypes & INTERACT_POLE)) {
        gOmmData->mario->state.poleObject = NULL;
    }

    // Preprocess interactions
    if (!(m->action & ACT_FLAG_INTANGIBLE)) {
        for (s32 i = 0; i < sOmmInteractionHandlerCount; i++) {
            u32 interactType = sOmmInteractionHandlers[i].interactType;
            if (m->collidedObjInteractTypes & interactType) {
                struct Object *o = mario_get_collided_object(m, interactType);
                if (!(o->oInteractStatus & INT_STATUS_INTERACTED) && sOmmInteractionHandlers[i].handler(m, o)) {
                    m->collidedObjInteractTypes &= ~interactType;
                }
            }
        }
    }

    // Save the attack flags, and remove them from Mario when jump-kicking if Odyssey Moveset is enabled
    sMarioIsPunching = (m->flags & MARIO_PUNCHING);
    sMarioIsKicking  = (m->flags & MARIO_KICKING);
    sMarioIsTripping = (m->flags & MARIO_TRIPPING);
    if (OMM_MOVESET_ODYSSEY && omm_mario_is_kicking(m)) {
        m->flags &= ~(MARIO_PUNCHING | MARIO_KICKING | MARIO_TRIPPING);
    }
}
