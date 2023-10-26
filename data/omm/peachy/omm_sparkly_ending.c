#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Ending cutscene
//

#if OMM_GAME_IS_SM64
static BAD_RETURN(s32) omm_sparkly_ending_cutscene_mario_and_toads(struct Camera *c) {
    cutscene_event(cutscene_ending_reset_spline, c, 0, 0);
    cutscene_event(cutscene_ending_look_at_sky, c, 0, 0);
    cutscene_event(cutscene_ending_zoom_fov, c, 0, 1049);
    cutscene_event(cutscene_ending_look_at_sky, c, 1050, -1);
    cutscene_spawn_obj(8, 1150);
    cutscene_spawn_obj(8, 1158);
    cutscene_spawn_obj(8, 1174);
    cutscene_spawn_obj(8, 1260);
}

struct Cutscene sOmmSparklyEndingCutscene[] = {
    { cutscene_ending_mario_fall, 170 },
    { cutscene_ending_mario_land, 70 },
    { cutscene_ending_mario_land_closeup, 75 },
    { cutscene_ending_stars_free_peach, 386 },
    { omm_sparkly_ending_cutscene_mario_and_toads, CUTSCENE_LOOP },
    { cutscene_ending_stop, 0 }
};
#endif

void omm_sparkly_ending_cutscene() {
#if OMM_GAME_IS_SM64
    static struct Cutscene *sCutsceneGoodEnding = NULL;
    extern struct Cutscene sCutsceneEnding[];
    switch (gOmmSparklyEnding) {
        case OMM_SPARKLY_ENDING_REGULAR:
        case OMM_SPARKLY_ENDING_PEACH: {
            if (sCutsceneGoodEnding) {
                mem_cpy(sCutsceneEnding, sCutsceneGoodEnding, sizeof(sOmmSparklyEndingCutscene));
                mem_del(sCutsceneGoodEnding);
                sCutsceneGoodEnding = NULL;
            }
        } break;

        case OMM_SPARKLY_ENDING_BAD: {
            if (!sCutsceneGoodEnding) {
                sCutsceneGoodEnding = mem_dup(sCutsceneEnding, sizeof(sOmmSparklyEndingCutscene));
                mem_cpy(sCutsceneEnding, sOmmSparklyEndingCutscene, sizeof(sOmmSparklyEndingCutscene));
            }
        } break;
    }
#endif
}

//
// Ending screen
//

#if OMM_GAME_IS_SM64
static const Gfx sOmmSparklyEndingScreenMario[] = {
    gsSPDisplayList(dl_proj_mtx_fullscreen),
    gsSPDisplayList(dl_hud_img_begin),
    gsDPLoadTextureBlock(OMM_TEXTURE_ENDING_0_MARIO, G_IM_FMT_RGBA, G_IM_SIZ_32b, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 0, 0, 0, 0),
    gsSPTextureRectangle(0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10),
    gsSPDisplayList(dl_hud_img_end),
    gsSPEndDisplayList(),
};

static const Gfx sOmmSparklyEndingScreenPeach[] = {
    gsSPDisplayList(dl_proj_mtx_fullscreen),
    gsSPDisplayList(dl_hud_img_begin),
    gsDPLoadTextureBlock(OMM_TEXTURE_ENDING_1, G_IM_FMT_RGBA, G_IM_SIZ_32b, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 0, 0, 0, 0),
    gsSPTextureRectangle(0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10),
    gsSPDisplayList(dl_hud_img_end),
    gsSPEndDisplayList(),
};

static const Gfx sOmmSparklyEndingScreenLuigi[] = {
    gsSPDisplayList(dl_proj_mtx_fullscreen),
    gsSPDisplayList(dl_hud_img_begin),
    gsDPLoadTextureBlock(OMM_TEXTURE_ENDING_0_LUIGI, G_IM_FMT_RGBA, G_IM_SIZ_32b, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 0, 0, 0, 0),
    gsSPTextureRectangle(0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10),
    gsSPDisplayList(dl_hud_img_end),
    gsSPEndDisplayList(),
};

static const Gfx sOmmSparklyEndingScreenWario[] = {
    gsSPDisplayList(dl_proj_mtx_fullscreen),
    gsSPDisplayList(dl_hud_img_begin),
    gsDPLoadTextureBlock(OMM_TEXTURE_ENDING_0_WARIO, G_IM_FMT_RGBA, G_IM_SIZ_32b, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 0, 0, 0, 0),
    gsSPTextureRectangle(0, 0, SCREEN_WIDTH << 2, SCREEN_HEIGHT << 2, G_TX_RENDERTILE, 0, 0, 4 << 10, 1 << 10),
    gsSPDisplayList(dl_hud_img_end),
    gsSPEndDisplayList(),
};

static const Gfx *sOmmSparklyEndingScreens[3][4] = {
    {                         NULL,                         NULL,                         NULL,                         NULL }, // Regular ending
    { sOmmSparklyEndingScreenMario,                         NULL, sOmmSparklyEndingScreenLuigi, sOmmSparklyEndingScreenWario }, // Bad ending
    { sOmmSparklyEndingScreenPeach, sOmmSparklyEndingScreenPeach, sOmmSparklyEndingScreenPeach, sOmmSparklyEndingScreenPeach }, // Peach ending
};

static const s32 sOmmSparklyEndingSounds[3] = {
    SOUND_MENU_THANK_YOU_PLAYING_MY_GAME, // Regular ending
    SOUND_OBJ_BOWSER_LAUGH,               // Bad ending
    SOUND_PEACH_THANK_YOU_MARIO,          // Peach ending
};
#endif

void omm_sparkly_ending_screen() {
#if OMM_GAME_IS_SM64
    const Gfx *gfx = sOmmSparklyEndingScreens[gOmmSparklyEnding][omm_player_get_selected_index()];
    if (gfx) {
        gSPDisplayList(gDisplayListHead++, gfx);
        static u32 sLastTick = 0;
        static s32 sSoundTimer = 0;
        sSoundTimer = (sSoundTimer + 1) * (gGlobalTimer == (sLastTick + 1));
        sLastTick = gGlobalTimer;
        switch (sSoundTimer) {
            case 001: sound_banks_disable(SEQ_PLAYER_SFX, 0xFFFF); break;
#if OMM_GAME_IS_R96X
            case 120: dynos_sound_stop(1); break;
#endif
            case 121: sound_banks_enable(SEQ_PLAYER_SFX, 0xFFFF); break;
            case 122: play_sound(sOmmSparklyEndingSounds[gOmmSparklyEnding], gGlobalSoundArgs); break;
        }
    }
#endif
}
