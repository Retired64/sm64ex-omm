#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS

//
// Buffers
//

static Vtx sOmmPowerMeterVtx[0x2000];
static Gfx sOmmPowerMeterGfx[0x4000];
static Vtx *sOmmPowerMeterVtxHead = NULL;
static Gfx *sOmmPowerMeterGfxHead = NULL;
static Vtx sOmmO2MeterVtx[OMM_RENDER_O2_NUM_QUADS * 4];
static Gfx sOmmO2MeterGfx[OMM_RENDER_O2_NUM_QUADS * 2 + 1];
static s32 sOmmCoinSparkles[OMM_RENDER_HUD_COIN_SPARKLE_COUNT] = { 0 };

//
// Frame interpolation
//

static InterpData sOmmPowerMeter[1];
static InterpData sOmmO2Meter[1];
static InterpData sOmmTimer[1];
static InterpData sOmmRedCoinsArrow[1];
extern InterpData sOmmStatsBoard[1];

static Gfx *omm_render_hud_power_meter_with_offset(Gfx *pos, f32 ticks, f32 dx, f32 dy, f32 luPos, f32 alpha);
static Gfx *omm_render_hud_o2_meter_gauge(Gfx *pos, f32 x, f32 y, f32 alpha);
static Gfx *omm_render_hud_timer_m_s_ms(Gfx *pos, f32 timer);
static Gfx *omm_render_hud_red_coins_radar_arrow(Gfx *pos, f32 angle);
extern Gfx *omm_render_stats_board(Gfx *pos, f32 t);

void gfx_interpolate_frame_hud(f32 t) {

    // Power meter (health)
    if (sOmmPowerMeter->pos) {
        interp_data_lerp(sOmmPowerMeter, t);
        omm_render_hud_power_meter_with_offset(sOmmPowerMeter->pos, sOmmPowerMeter->s, sOmmPowerMeter->x, sOmmPowerMeter->y, sOmmPowerMeter->z, sOmmPowerMeter->a);
    }

    // Air meter (O2)
    if (sOmmO2Meter->pos) {
        interp_data_lerp(sOmmO2Meter, t);
        omm_render_hud_o2_meter_gauge(sOmmO2Meter->pos, sOmmO2Meter->x, sOmmO2Meter->y, sOmmO2Meter->a);
    }

    // Timer
    if (sOmmTimer->pos) {
        interp_data_lerp(sOmmTimer, t);
        omm_render_hud_timer_m_s_ms(sOmmTimer->pos, sOmmTimer->t);
    }

    // Red coins radar arrow
    if (sOmmRedCoinsArrow->pos) {
        interp_data_lerp(sOmmRedCoinsArrow, t);
        omm_render_hud_red_coins_radar_arrow(sOmmRedCoinsArrow->pos, sOmmRedCoinsArrow->a);
    }

    // Stats board
    if (sOmmStatsBoard->pos) {
        interp_data_lerp(sOmmStatsBoard, t);
        omm_render_stats_board(sOmmStatsBoard->pos, sOmmStatsBoard->t);
    }
}

void gfx_clear_frame_hud() {
    sOmmPowerMeter->pos = NULL;
    sOmmO2Meter->pos = NULL;
    sOmmTimer->pos = NULL;
    sOmmRedCoinsArrow->pos = NULL;
    sOmmStatsBoard->pos = NULL;
}

//
// Vanishing HUD
//

typedef struct {
    s32 alwaysActive, displayOnPause;
    s32 fadeIn, fadeOut;
    s32 timerMin, timerMax;
    s32 timerAlphaMin, timerAlphaMax;
    s32 alphaMin, alphaMax;
    s32 timer, prev;
} OmmHudTimer;

static OmmHudTimer sOmmHudStarsTimer  = { 0, 1, +1, +2,   0, 30,  0, 20, 0x00, 0xFF, 0, 0 };
static OmmHudTimer sOmmHudCoinsTimer  = { 0, 1, +1, +2, -60, 30,  0, 20, 0x00, 0xFF, 0, 0 };
static OmmHudTimer sOmmHudVibeTimer   = { 0, 1, +1, +2, -45, 30,  0, 20, 0x00, 0xFF, 0, 0 };
static OmmHudTimer sOmmHudHealthTimer = { 0, 1, +1, +2, -60, 30,  0, 20, 0x40, 0xFF, 0, 0 };
static OmmHudTimer sOmmHudOxygenTimer = { 1, 0, -1, +1,   0, 30, 24, 30, 0x00, 0xFF, 0, 0 };
static OmmHudTimer sOmmHudCameraTimer = { 0, 0, +1, +2, -45, 30,  0, 20, 0x00, 0xFF, 0, 0 };

#if OMM_CODE_DEV
u8 *gOmmHudStarsTimerData  = (u8 *) &sOmmHudStarsTimer;
u8 *gOmmHudCoinsTimerData  = (u8 *) &sOmmHudCoinsTimer;
u8 *gOmmHudVibeTimerData   = (u8 *) &sOmmHudVibeTimer;
u8 *gOmmHudHealthTimerData = (u8 *) &sOmmHudHealthTimer;
u8 *gOmmHudOxygenTimerData = (u8 *) &sOmmHudOxygenTimer;
u8 *gOmmHudCameraTimerData = (u8 *) &sOmmHudCameraTimer;
const s32 gOmmHudTimerSize = sizeof(OmmHudTimer);
#endif

static bool should_instant_fade_in(struct MarioState *m, OmmHudTimer *timer) {
    bool res = false; 
    if (timer == &sOmmHudStarsTimer) {
        res = omm_mario_is_star_dancing(m);
    } else if (timer == &sOmmHudCoinsTimer) {
        res = (timer->prev < gHudDisplay.coins);
        timer->prev = gHudDisplay.coins;
    } else if (timer == &sOmmHudVibeTimer) {
        res = (gOmmPeach->vibeGauge != 0);
    } else if (timer == &sOmmHudHealthTimer) {
        s32 health = (OMM_MOVESET_ODYSSEY ? gOmmMario->state.ticks : m->health);
        res = (timer->prev != health);
        timer->prev = health;
    } else if (timer == &sOmmHudOxygenTimer) {
        res = (gOmmMario->state.o2 != 0);
    } else if (timer == &sOmmHudCameraTimer) {
        s32 status = (omm_camera_is_available(m) ? ((s32) (gOmmCameraMode << 4) + omm_camera_get_relative_dist_mode()) : update_camera_hud_status(gCamera));
        res = (timer->prev != status);
        timer->prev = status;
    }
    return res;
}

static u8 vanishing_hud_update_timer_and_get_alpha(struct MarioState *m, OmmHudTimer *timer) {
    if (OMM_EXTRAS_VANISHING_HUD || timer->alwaysActive) {
        
        // Pause
        if (gMenuMode == 0 || gMenuMode == 1) {
            if (timer->displayOnPause) {
                return 0xFF;
            }
            if (!timer->alwaysActive) {
                return 0x00;
            }
        }

        // Instant fade-in, fade cooldown, fade-in or fade-out
        if (should_instant_fade_in(m, timer)) {
            timer->timer = timer->timerMin;
        } else if (timer->timer < 0) {
            timer->timer = min_s(timer->timer + timer->fadeIn, 0);
        } else {
            struct Controller *cont = m->controller;
            bool contLeftStickIdle = !cont || ((cont->stickX == 0) && (cont->stickY == 0) && (cont->stickMag == 0));
            bool contButtonDownIdle = !cont || ((cont->buttonDown & ~(U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS | R_TRIG | L_TRIG)) == 0);
            bool contButtonPressedIdle = !cont || ((cont->buttonPressed & ~(U_CBUTTONS | D_CBUTTONS | L_CBUTTONS | R_CBUTTONS | R_TRIG | L_TRIG)) == 0);
            if (omm_mario_is_reading(m) || (omm_mario_is_idling(m) && contLeftStickIdle && contButtonDownIdle && contButtonPressedIdle)) {
                timer->timer = max_s(timer->timer - timer->fadeIn, 0);
            } else {
                timer->timer = min_s(timer->timer + timer->fadeOut, timer->timerMax);
            }
        }

        // Return alpha value
        return clamp_s(relerp_0_1_s(timer->timer, timer->timerAlphaMin, timer->timerAlphaMax, timer->alphaMax, timer->alphaMin), 0x00, 0xFF);
    }
    return 0xFF;
}

OMM_ROUTINE_LEVEL_ENTRY(omm_render_at_level_entry) {
    sOmmHudStarsTimer.timer = 0;
    sOmmHudCoinsTimer.timer = 0;
    sOmmHudHealthTimer.timer = 0;
    sOmmHudOxygenTimer.timer = sOmmHudOxygenTimer.timerMax;
    sOmmHudCameraTimer.timer = 0;
}

//
// Stars
//

bool omm_render_hud_stars(s16 x, s16 y, u8 alpha, s32 levelNum, bool cond, bool shadow) {
    static const u8 sStarShading[2][2] = { { 0xA0, 0xFF }, { 0x00, 0x00 } };
    static const u8 sStarOpacity[2][2] = { { 0xFF, 0xFF }, { 0x00, 0x80 } };

    // Star counter
    u8 t = omm_stars_get_bits_total(levelNum, OMM_GAME_MODE);
    if (t == 0 || !cond ||
        levelNum == LEVEL_BOWSER_1 ||
        levelNum == LEVEL_BOWSER_2 ||
        levelNum == LEVEL_BOWSER_3) {
        omm_render_glyph_hud(x, y, 0xFF, 0xFF, 0xFF, alpha, omm_render_get_star_glyph(0, OMM_EXTRAS_COLORED_STARS), shadow);
        return false;
    }

    // Course stars
    s32 courseNum = omm_level_get_course(levelNum);
    u8 f = omm_save_file_get_star_flags(gCurrSaveFileNum - 1, OMM_GAME_MODE, courseNum - 1);
    u8 s = omm_stars_get_bits();
    for (u8 i = 0; i != OMM_NUM_STARS_MAX_PER_COURSE; ++i) {
        u8 bit = (1 << i);
        if (bit & t) {
            u8 shading = sStarShading[!(bit & f)][!(bit & s)];
            u8 opacity = sStarOpacity[!(bit & f)][!(bit & s)];
            omm_render_glyph_hud(x, y, shading, shading, shading, (alpha * opacity) / 0xFF, omm_render_get_star_glyph(clamp_s(courseNum, 0, 16), OMM_EXTRAS_COLORED_STARS), shadow);
            x += OMM_RENDER_STAR_OFFSET_X;
        }
    }

    // Sparkly Star
    if (OMM_SPARKLY_MODE_IS_ENABLED) {
        s32 sparklyMode = gOmmSparklyMode;
        for (s32 i = -1; i != 8; ++i) {
            s32 starIndex = omm_sparkly_get_index(sparklyMode, levelNum, i == -1 ? gCurrAreaIndex : i);
            if (starIndex != -1) {
                bool collected = omm_sparkly_is_star_collected(sparklyMode, starIndex);
                bool state = OMM_SPARKLY_STATE_IS_OK;
                u8 shading = sStarShading[!collected][state];
                u8 opacity = sStarOpacity[!collected][state];
                omm_render_glyph_hud(x, y, shading, shading, shading, (alpha * opacity) / 0xFF, OMM_SPARKLY_HUD_GLYPH[sparklyMode], shadow);
                return true;
            }
        }
    }

    return true;
}

//
// Coins
//

void omm_render_hud_coins(s16 x, s16 y, u8 alpha, s32 coins, bool shadow) {
    omm_render_glyph_hud(x, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_COIN, shadow);
    omm_render_number_hud(x + (OMM_RENDER_VALUE_NUMBER_X - OMM_RENDER_VALUE_GLYPH_X), y, alpha, coins, 3, true, shadow);
}

//
// Camera
//

void omm_render_hud_camera(struct MarioState *m) {
    if (gHudDisplay.flags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) {
        u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudCameraTimer);
        if (alpha) {

            // OMM cam
            if (!OMM_CAMERA_CLASSIC) {
                omm_render_create_dl_ortho_matrix();
                omm_render_glyph_hud(OMM_RENDER_CAMERA_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_CAMERA, false);
                if (OMM_CAMERA_8_DIRECTIONS) {
                    omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_CAMERA_8, false);
                } else if (OMM_CAMERA_16_DIRECTIONS) {
                    omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_CAMERA_16, false);
                }
                s32 mode = omm_camera_get_relative_dist_mode();
                s32 zoom = abs_s(mode);
                s32 sign = (mode > 0 ? -1 : +1);
                for (s32 i = zoom; i > 0; --i) {
                    omm_render_glyph_hud(OMM_RENDER_CAMERA_X, OMM_RENDER_CAMERA_Y + sign * (OMM_RENDER_CAMERA_OFFSET_Y + OMM_RENDER_CAMERA_STEP_Y * i), 0xFF, 0xFF, 0xFF, alpha, (mode > 0 ? OMM_TEXTURE_HUD_CAMERA_FAR : OMM_TEXTURE_HUD_CAMERA_NEAR), false);
                }
            }
            
            // Puppy/better cam
            else if (BETTER_CAM_IS_ENABLED) {
                omm_render_create_dl_ortho_matrix();
                omm_render_glyph_hud(OMM_RENDER_CAMERA_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_CAMERA], false);
#if BETTER_CAM_IS_PUPPY_CAM
                omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, texture_hud_char_puppycam, false);
#else
                omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_LAKITU_HEAD], false);
#endif
            }

            // Lakitu cam
            else {
                s32 status = update_camera_hud_status(gCamera);
                if (status) {
                    omm_render_create_dl_ortho_matrix();
                    omm_render_glyph_hud(OMM_RENDER_CAMERA_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_CAMERA], false);
                    switch (status & CAM_STATUS_MODE_GROUP) {
                        case CAM_STATUS_MARIO:  omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_MARIO_HEAD], false); break;
                        case CAM_STATUS_LAKITU: omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_LAKITU_HEAD], false); break;
                        case CAM_STATUS_FIXED:  omm_render_glyph_hud(OMM_RENDER_CAMERA_X + OMM_RENDER_CAMERA_OFFSET_X, OMM_RENDER_CAMERA_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_FIXED], false); break;
                    }
                    switch (status & CAM_STATUS_C_MODE_GROUP) {
                        case CAM_STATUS_C_DOWN: omm_render_glyph_hud_small(OMM_RENDER_CAMERA_X + OMM_RENDER_GLYPH_SIZE / 4, OMM_RENDER_CAMERA_Y + OMM_RENDER_GLYPH_SIZE / 4 - OMM_RENDER_CAMERA_OFFSET_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_ARROW_DOWN], false); break;
                        case CAM_STATUS_C_UP:   omm_render_glyph_hud_small(OMM_RENDER_CAMERA_X + OMM_RENDER_GLYPH_SIZE / 4, OMM_RENDER_CAMERA_Y + OMM_RENDER_GLYPH_SIZE / 4 + OMM_RENDER_CAMERA_OFFSET_Y, 0xFF, 0xFF, 0xFF, alpha, ((const u8 **) main_hud_camera_lut)[GLYPH_CAM_ARROW_UP], false); break;
                    }
                }
            }
        }
    }
}

//
// Health meter
//

static void omm_render_hud_power_meter_background(f32 x, f32 y, f32 alpha) {
    for (s32 i = 0; i != OMM_RENDER_POWER_BACKGROUND_NUM_TRIS; ++i) {
        f32 a0 = ((i + 0) * 65536.f) / OMM_RENDER_POWER_BACKGROUND_NUM_TRIS;
        f32 a1 = ((i + 1) * 65536.f) / OMM_RENDER_POWER_BACKGROUND_NUM_TRIS;
        f32 d  = OMM_RENDER_POWER_SEGMENT_RADIUS_3 + 0.5f;
        sOmmPowerMeterVtxHead[0] = (Vtx) { { { x,                y,                0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_BACKGROUND_R, OMM_RENDER_POWER_BACKGROUND_G, OMM_RENDER_POWER_BACKGROUND_B, (u8) clamp_f(alpha, 0, 255) } } };
        sOmmPowerMeterVtxHead[1] = (Vtx) { { { x + d * sins(a0), y + d * coss(a0), 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_BACKGROUND_R, OMM_RENDER_POWER_BACKGROUND_G, OMM_RENDER_POWER_BACKGROUND_B, (u8) clamp_f(alpha, 0, 255) } } };
        sOmmPowerMeterVtxHead[2] = (Vtx) { { { x + d * sins(a1), y + d * coss(a1), 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_BACKGROUND_R, OMM_RENDER_POWER_BACKGROUND_G, OMM_RENDER_POWER_BACKGROUND_B, (u8) clamp_f(alpha, 0, 255) } } };
        gSPVertex(sOmmPowerMeterGfxHead++, sOmmPowerMeterVtxHead, 3, 0);
        gSP1Triangle(sOmmPowerMeterGfxHead++, 0, 2, 1, 0x0);
        sOmmPowerMeterVtxHead += 3;
    }
}

static void omm_render_hud_power_meter_segments(f32 x0, f32 y0, f32 alpha, f32 ticks, bool isLifeUp) {
    f32 ma = (65536.f * ticks) / (OMM_RENDER_POWER_FULL_SEGMENTS * OMM_RENDER_POWER_TICKS_PER_SEGMENT);
    f32 da = (65536.f)         / (OMM_RENDER_POWER_FULL_SEGMENTS * OMM_RENDER_POWER_SEGMENT_NUM_QUADS);
    for (s32 segment = 0; segment != OMM_RENDER_POWER_FULL_SEGMENTS; ++segment) {
        f32 sa = (segment * 65536 + 32768) / OMM_RENDER_POWER_FULL_SEGMENTS;
        f32 x = x0 + OMM_RENDER_POWER_SEGMENT_DELTA * sins(sa);
        f32 y = y0 + OMM_RENDER_POWER_SEGMENT_DELTA * coss(sa);
        for (s32 i = 0; i != OMM_RENDER_POWER_SEGMENT_NUM_QUADS; ++i) {
            f32 a0 = da * (segment * OMM_RENDER_POWER_SEGMENT_NUM_QUADS + i + 0);
            f32 a1 = da * (segment * OMM_RENDER_POWER_SEGMENT_NUM_QUADS + i + 1);
            s32 index = 0;
            if (((a0 + a1) / 2) <= ma) {
                if (isLifeUp) {
                    index = 4;
                } else if (ticks < OMM_RENDER_POWER_TICKS_CRITICAL) {
                    index = 1;
                } else if (ticks < OMM_RENDER_POWER_TICKS_LOW) {
                    index = 2;
                } else if (ticks < OMM_RENDER_POWER_TICKS_NORMAL) {
                    index = 3;
                } else {
                    index = 4;
                }
            }
            sOmmPowerMeterVtxHead[0] = (Vtx) { { { x + sins(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_0, y + coss(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_0, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_BORDER_R, OMM_RENDER_POWER_SEGMENT_BORDER_G, OMM_RENDER_POWER_SEGMENT_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[1] = (Vtx) { { { x + sins(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_1, y + coss(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_1, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_CENTER_R, OMM_RENDER_POWER_SEGMENT_CENTER_G, OMM_RENDER_POWER_SEGMENT_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[2] = (Vtx) { { { x + sins(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_2, y + coss(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_2, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_CENTER_R, OMM_RENDER_POWER_SEGMENT_CENTER_G, OMM_RENDER_POWER_SEGMENT_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[3] = (Vtx) { { { x + sins(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_3, y + coss(a0) * OMM_RENDER_POWER_SEGMENT_RADIUS_3, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_BORDER_R, OMM_RENDER_POWER_SEGMENT_BORDER_G, OMM_RENDER_POWER_SEGMENT_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[4] = (Vtx) { { { x + sins(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_0, y + coss(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_0, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_BORDER_R, OMM_RENDER_POWER_SEGMENT_BORDER_G, OMM_RENDER_POWER_SEGMENT_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[5] = (Vtx) { { { x + sins(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_1, y + coss(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_1, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_CENTER_R, OMM_RENDER_POWER_SEGMENT_CENTER_G, OMM_RENDER_POWER_SEGMENT_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[6] = (Vtx) { { { x + sins(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_2, y + coss(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_2, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_CENTER_R, OMM_RENDER_POWER_SEGMENT_CENTER_G, OMM_RENDER_POWER_SEGMENT_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
            sOmmPowerMeterVtxHead[7] = (Vtx) { { { x + sins(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_3, y + coss(a1) * OMM_RENDER_POWER_SEGMENT_RADIUS_3, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_SEGMENT_BORDER_R, OMM_RENDER_POWER_SEGMENT_BORDER_G, OMM_RENDER_POWER_SEGMENT_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
            gSPVertex(sOmmPowerMeterGfxHead++, sOmmPowerMeterVtxHead, 8, 0);
            gSP2Triangles(sOmmPowerMeterGfxHead++, 2, 6, 3, 0x0, 3, 6, 7, 0x0);
            gSP2Triangles(sOmmPowerMeterGfxHead++, 1, 5, 2, 0x0, 2, 5, 6, 0x0);
            gSP2Triangles(sOmmPowerMeterGfxHead++, 0, 4, 1, 0x0, 1, 4, 5, 0x0);
            sOmmPowerMeterVtxHead += 8;
        }
    }
}

static void omm_render_hud_power_meter_heart(f32 x, f32 y, f32 alpha, f32 ticks) {
   for (s32 i = 0; i != OMM_RENDER_POWER_HEART_NUM_PIECES; ++i) {
       f32 t0 = (i + 0 - 32) / (OMM_RENDER_POWER_HEART_NUM_PIECES / 2.f);
       f32 t1 = (i + 1 - 32) / (OMM_RENDER_POWER_HEART_NUM_PIECES / 2.f);
       s32 index = OMM_RENDER_POWER_HEART_PULSE;

       // Heart shape <3
       f32 a0 = t0 * 65536.f / (2.f * M_PI);
       f32 a1 = t1 * 65536.f / (2.f * M_PI);
       f32 r0 = (1.f - abs_f(t0)) * (1.f + 3.f * abs_f(t0));
       f32 r1 = (1.f - abs_f(t1)) * (1.f + 3.f * abs_f(t1));
       f32 dx0 = r0 * sins(a0);
       f32 dy0 = r0 * coss(a0) - 0.65f;
       f32 dx1 = r1 * sins(a1);
       f32 dy1 = r1 * coss(a1) - 0.65f;
       sOmmPowerMeterVtxHead[0] = (Vtx) { { { x,                                           y,                                           0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_HEART_CENTER_R, OMM_RENDER_POWER_HEART_CENTER_G, OMM_RENDER_POWER_HEART_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
       sOmmPowerMeterVtxHead[1] = (Vtx) { { { x + dx0 * OMM_RENDER_POWER_HEART_RADIUS_1_X, y + dy0 * OMM_RENDER_POWER_HEART_RADIUS_1_Y, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_HEART_CENTER_R, OMM_RENDER_POWER_HEART_CENTER_G, OMM_RENDER_POWER_HEART_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
       sOmmPowerMeterVtxHead[2] = (Vtx) { { { x + dx0 * OMM_RENDER_POWER_HEART_RADIUS_2_X, y + dy0 * OMM_RENDER_POWER_HEART_RADIUS_2_Y, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_HEART_BORDER_R, OMM_RENDER_POWER_HEART_BORDER_G, OMM_RENDER_POWER_HEART_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
       sOmmPowerMeterVtxHead[3] = (Vtx) { { { x + dx1 * OMM_RENDER_POWER_HEART_RADIUS_1_X, y + dy1 * OMM_RENDER_POWER_HEART_RADIUS_1_Y, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_HEART_CENTER_R, OMM_RENDER_POWER_HEART_CENTER_G, OMM_RENDER_POWER_HEART_CENTER_B, (u8) clamp_f(alpha, 0, 255) } } };
       sOmmPowerMeterVtxHead[4] = (Vtx) { { { x + dx1 * OMM_RENDER_POWER_HEART_RADIUS_2_X, y + dy1 * OMM_RENDER_POWER_HEART_RADIUS_2_Y, 0 }, 0, { 0, 0 }, { OMM_RENDER_POWER_HEART_BORDER_R, OMM_RENDER_POWER_HEART_BORDER_G, OMM_RENDER_POWER_HEART_BORDER_B, (u8) clamp_f(alpha, 0, 255) } } };
       gSPVertex(sOmmPowerMeterGfxHead++, sOmmPowerMeterVtxHead, 5, 0);
       gSP2Triangles(sOmmPowerMeterGfxHead++, 2, 1, 4, 0x0, 1, 3, 4, 0x0);
       gSP1Triangle(sOmmPowerMeterGfxHead++, 1, 0, 3, 0x0);
       sOmmPowerMeterVtxHead += 5;
   }
}

static void omm_render_hud_power_meter_number(f32 x, f32 y, f32 alpha, f32 segments) {
    static const char *sOmmNumberGlyphs[] = {
        OMM_TEXTURE_HUD_0, OMM_TEXTURE_HUD_1,
        OMM_TEXTURE_HUD_2, OMM_TEXTURE_HUD_3,
        OMM_TEXTURE_HUD_4, OMM_TEXTURE_HUD_5,
        OMM_TEXTURE_HUD_6, OMM_TEXTURE_HUD_7,
        OMM_TEXTURE_HUD_8, OMM_TEXTURE_HUD_9,
    };
    f32 w = OMM_RENDER_GLYPH_SIZE;
    f32 h = OMM_RENDER_GLYPH_SIZE;
    s16 x0 = (s16) (x * 4.f);
    s16 y0 = (s16) ((SCREEN_HEIGHT - h - y) * 4.f);
    s16 x1 = (s16) ((x + w) * 4.f);
    s16 y1 = (s16) ((SCREEN_HEIGHT - y) * 4.f);
    gDPSetTexturePersp(sOmmPowerMeterGfxHead++, G_TP_NONE);
    gDPSetRenderMode(sOmmPowerMeterGfxHead++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineLERP(sOmmPowerMeterGfxHead++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetEnvColor(sOmmPowerMeterGfxHead++, 0xFF, 0xFF, 0xFF, (u8) clamp_f(alpha, 0, 255));
    gDPSetTextureFilter(sOmmPowerMeterGfxHead++, G_TF_POINT);
    gSPTexture(sOmmPowerMeterGfxHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(sOmmPowerMeterGfxHead++, sOmmNumberGlyphs[(s32) segments], G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    gSPTextureRectangle(sOmmPowerMeterGfxHead++, x0, y0, x1, y1, G_TX_RENDERTILE, 0, 0, (s16) (0x4000 / w), (s16) (0x4000 / h));
    gDPSetTexturePersp(sOmmPowerMeterGfxHead++, G_TP_PERSP);
    gDPSetRenderMode(sOmmPowerMeterGfxHead++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineLERP(sOmmPowerMeterGfxHead++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    gDPSetEnvColor(sOmmPowerMeterGfxHead++, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetTextureFilter(sOmmPowerMeterGfxHead++, G_TF_BILERP);
    gSPTexture(sOmmPowerMeterGfxHead++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
}

static void omm_render_hud_power_meter_health_gauge(f32 x, f32 y, f32 alpha, f32 ticks, f32 luPos, bool isLifeUp) {
    if (!isLifeUp) {
        ticks = min_s(ticks, omm_health_get_max_ticks_hud(0));
        omm_render_hud_power_meter_segments(x, y, alpha, ticks, false);
        if (luPos > 0.1f) {
            omm_render_hud_power_meter_heart(x, y - OMM_RENDER_POWER_HEART_OFFSET_Y, alpha, ticks);
            omm_render_hud_power_meter_number(x - OMM_RENDER_POWER_NUMBER_OFFSET_X, y - OMM_RENDER_POWER_NUMBER_OFFSET_Y, alpha, OMM_RENDER_POWER_TICKS_TO_SEGMENTS);
        }
    } else if (OMM_MOVESET_ODYSSEY_3H && ticks > omm_health_to_ticks(OMM_HEALTH_ODYSSEY_3_SEGMENTS)) {
        x = x + luPos * ((SCREEN_WIDTH * 0.5f) - x);
        y = y + luPos * ((SCREEN_HEIGHT * 0.65f) - y);
        omm_render_hud_power_meter_background(x, y, alpha);
        omm_render_hud_power_meter_segments(x, y, alpha, ticks - omm_health_get_max_ticks_hud(0), true);
        omm_render_hud_power_meter_heart(x, y - OMM_RENDER_POWER_HEART_OFFSET_Y, alpha, ticks);
        omm_render_hud_power_meter_number(x - OMM_RENDER_POWER_NUMBER_OFFSET_X, y - OMM_RENDER_POWER_NUMBER_OFFSET_Y, alpha, OMM_RENDER_POWER_TICKS_TO_SEGMENTS);
    }
}

static Gfx *omm_render_hud_power_meter_with_offset(Gfx *pos, f32 ticks, f32 dx, f32 dy, f32 luPos, f32 alpha) {
    sOmmPowerMeterGfxHead = sOmmPowerMeterGfx;
    sOmmPowerMeterVtxHead = sOmmPowerMeterVtx;
    OMM_RENDER_ENABLE_ALPHA(pos++);
    gSPDisplayList(pos++, sOmmPowerMeterGfx);
    gSPClearGeometryMode(sOmmPowerMeterGfxHead++, G_LIGHTING);
    gDPSetCombineLERP(sOmmPowerMeterGfxHead++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    if (OMM_MOVESET_ODYSSEY) {
        omm_render_hud_power_meter_health_gauge(OMM_RENDER_POWER_X + dx, OMM_RENDER_POWER_Y + dy, alpha, ticks, luPos, false);
        omm_render_hud_power_meter_health_gauge(OMM_RENDER_POWER_X + dx - OMM_RENDER_POWER_LIFE_UP_OFFSET_X, OMM_RENDER_POWER_Y + dy, alpha, ticks, luPos, true);
    } else {
        omm_render_hud_power_meter_health_gauge(OMM_RENDER_POWER_X, OMM_RENDER_POWER_Y, alpha, ticks, 1.f, false);
    }
    gSPSetGeometryMode(sOmmPowerMeterGfxHead++, G_LIGHTING);
    gSPEndDisplayList(sOmmPowerMeterGfxHead);
    return pos;
}

void omm_render_hud_power_meter(struct MarioState *m) {
    if ((gHudDisplay.flags & HUD_DISPLAY_FLAG_CAMERA_AND_POWER) && !OMM_MOVESET_ODYSSEY_1H) {
        u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudHealthTimer);
        if (alpha) {
            omm_render_create_dl_ortho_matrix();
            f32 ticks, dx, dy, dz;
            if (OMM_MOVESET_ODYSSEY) {
                f32 rumble = (omm_is_game_paused() ? 0.f : omm_health_get_life_gauge_rumble_strength(m));
                ticks = gOmmMario->state.ticks;
                dx = (random_float() * rumble) - (rumble / 2.f);
                dy = (random_float() * rumble) - (rumble / 2.f);
                dz = omm_health_get_life_up_gauge_position(ticks);
            } else {
                ticks = (s32) (m->health >> 8);
                dx = 0.f;
                dy = 0.f;
                dz = 0.f;
            }
            interp_data_update(sOmmPowerMeter, !(sOmmPowerMeter->z1 == 0.f && dz == 1.f), gDisplayListHead, dx, dy, dz, alpha, ticks, 0);
            gDisplayListHead = omm_render_hud_power_meter_with_offset(gDisplayListHead, sOmmPowerMeter->s0, sOmmPowerMeter->x0, sOmmPowerMeter->y0, sOmmPowerMeter->z0, sOmmPowerMeter->a0);
        }
    }
}

//
// Oxygen meter
//

static Gfx *omm_render_hud_o2_meter_gauge(Gfx *pos, f32 x, f32 y, f32 alpha) {
    f32 t = clamp_0_1_f((f32) (gOmmMario->state.o2 - (OMM_O2_MAX_DURATION / 100.f)) / (f32) (OMM_O2_MAX_DURATION * 0.99f));
    f32 j = t * OMM_RENDER_O2_NUM_QUADS;
    bool blink = (t == 1.f) && (gGlobalTimer & 1);

    // Vertices
    Vtx *vtx = sOmmO2MeterVtx;
    for (s32 i = 0; i != OMM_RENDER_O2_NUM_QUADS; ++i) {
        s32 a0 = ((i + 0) * -65536) / OMM_RENDER_O2_NUM_QUADS;
        s32 a1 = ((i + 1) * -65536) / OMM_RENDER_O2_NUM_QUADS;
        vtx[0] = (Vtx) { { { x + OMM_RENDER_O2_RADIUS_IN  * sins(a0), y + OMM_RENDER_O2_RADIUS_IN  * coss(a0), 0 }, 0, { 0, 0 }, { OMM_RENDER_O2_COLOR_R, OMM_RENDER_O2_COLOR_G, OMM_RENDER_O2_COLOR_B, (u8) clamp_f(alpha, 0, 255) } } };
        vtx[1] = (Vtx) { { { x + OMM_RENDER_O2_RADIUS_OUT * sins(a0), y + OMM_RENDER_O2_RADIUS_OUT * coss(a0), 0 }, 0, { 0, 0 }, { OMM_RENDER_O2_COLOR_R, OMM_RENDER_O2_COLOR_G, OMM_RENDER_O2_COLOR_B, (u8) clamp_f(alpha, 0, 255) } } };
        vtx[2] = (Vtx) { { { x + OMM_RENDER_O2_RADIUS_IN  * sins(a1), y + OMM_RENDER_O2_RADIUS_IN  * coss(a1), 0 }, 0, { 0, 0 }, { OMM_RENDER_O2_COLOR_R, OMM_RENDER_O2_COLOR_G, OMM_RENDER_O2_COLOR_B, (u8) clamp_f(alpha, 0, 255) } } };
        vtx[3] = (Vtx) { { { x + OMM_RENDER_O2_RADIUS_OUT * sins(a1), y + OMM_RENDER_O2_RADIUS_OUT * coss(a1), 0 }, 0, { 0, 0 }, { OMM_RENDER_O2_COLOR_R, OMM_RENDER_O2_COLOR_G, OMM_RENDER_O2_COLOR_B, (u8) clamp_f(alpha, 0, 255) } } };
        vtx += 4;
    }

    // Triangles
    Gfx *gfx = sOmmO2MeterGfx;
    for (s32 i = 0; i != OMM_RENDER_O2_NUM_QUADS; ++i) {
        gSPVertex(gfx++, sOmmO2MeterVtx + 4 * i, 4, 0);
        gSP2Triangles(gfx++, 0, 1, 2, 0, 3, 2, 1, 0);
    }
    gSPEndDisplayList(gfx);

    // Display list
    OMM_RENDER_ENABLE_ALPHA(pos++);
    gSPClearGeometryMode(pos++, G_LIGHTING);
    gDPSetCombineLERP(pos++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    gSPDisplayList(pos++, sOmmO2MeterGfx);
    gSPSetGeometryMode(pos++, G_LIGHTING);
    return pos;
}

void omm_render_hud_o2_meter(struct MarioState *m) {
    if (OMM_MOVESET_ODYSSEY && (gHudDisplay.flags & OMM_HUD_DISPLAY_FLAG_O2_METER)) {
        u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudOxygenTimer);
        if (alpha) {

            // Camera normal
            Vec3f camN;
            vec3f_dif(camN, gLakituState.focus, gLakituState.pos);
            vec3f_normalize(camN);

            // Camera horizontal axis
            s16 camYaw = atan2s(camN[2], camN[0]) - 0x4000;
            Vec3f camH = { sins(camYaw), 0.f, coss(camYaw) };

            // Camera vertical axis
            Vec3f camV;
            vec3f_cross(camV, camN, camH);
            vec3f_normalize(camV);

            // Mario's position to camera plane
            Vec2f pos2d;
            f32 dist;
            vec3f_to_2d_plane(pos2d, &dist, m->pos, gLakituState.pos, camN, camH, camV);
            if (dist > 0.f) {
                f32 x = (SCREEN_WIDTH  / 2) * (1.f + (pos2d[0] / dist)) + ((SCREEN_WIDTH  * 1.6f) / sqrtf(dist));
                f32 y = (SCREEN_HEIGHT / 2) * (1.f - (pos2d[1] / dist)) + ((SCREEN_HEIGHT * 3.2f) / sqrtf(dist));
                interp_data_update(sOmmO2Meter, sOmmO2Meter->t1 >= (f32) gGlobalTimer - 1, gDisplayListHead, x, y, 0, alpha, 0, gGlobalTimer);
                gDisplayListHead = omm_render_hud_o2_meter_gauge(gDisplayListHead, sOmmO2Meter->x0, sOmmO2Meter->y0, sOmmO2Meter->a0);
            }
        }
    }
}

//
// Timer
//

static Gfx *omm_render_hud_timer_m_s_ms(Gfx *pos, f32 timer) {
    static const char *sOmmTimerGlyphs[] = {
        OMM_TEXTURE_HUD_0, OMM_TEXTURE_HUD_1,
        OMM_TEXTURE_HUD_2, OMM_TEXTURE_HUD_3,
        OMM_TEXTURE_HUD_4, OMM_TEXTURE_HUD_5,
        OMM_TEXTURE_HUD_6, OMM_TEXTURE_HUD_7,
        OMM_TEXTURE_HUD_8, OMM_TEXTURE_HUD_9,
        OMM_TEXTURE_HUD_M, OMM_TEXTURE_HUD_S,
    };

    // Clock
    s16 clockW = (OMM_RENDER_GLYPH_SIZE * 4) / 3;
    s16 clockX = OMM_RENDER_TIMER_X - (clockW * 3) / 2;
    s16 clockY = OMM_RENDER_TIMER_Y - (clockW - OMM_RENDER_GLYPH_SIZE) / 2;

    // Rotating clock hand
    static Vtx sClockHandVtx[4] = {
        {{{ 0, 0, 0 }, 0, { 0x0000, 0x2000 }, { 0xFF, 0xFF, 0xFF, 0xFF }}},
        {{{ 0, 0, 0 }, 0, { 0x2000, 0x2000 }, { 0xFF, 0xFF, 0xFF, 0xFF }}},
        {{{ 0, 0, 0 }, 0, { 0x2000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF }}},
        {{{ 0, 0, 0 }, 0, { 0x0000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF }}},
    };
    s16 handW = (clockW * 3) / 4;
    s16 handX = (clockW / 2) + clockX;
    s16 handY = (clockW / 2) + clockY;
    s16 handR = ((timer * 0x10000) / 30.f);
    vec3f_set(sClockHandVtx[0].v.ob, handX + handW * coss(0xA000 - handR), handY + handW * sins(0xA000 - handR), 0);
    vec3f_set(sClockHandVtx[1].v.ob, handX + handW * coss(0xE000 - handR), handY + handW * sins(0xE000 - handR), 0);
    vec3f_set(sClockHandVtx[2].v.ob, handX + handW * coss(0x2000 - handR), handY + handW * sins(0x2000 - handR), 0);
    vec3f_set(sClockHandVtx[3].v.ob, handX + handW * coss(0x6000 - handR), handY + handW * sins(0x6000 - handR), 0);
    gDPSetTexturePersp(pos++, G_TP_NONE);
    gDPSetRenderMode(pos++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineLERP(pos++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetTextureFilter(pos++, G_TF_POINT);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(pos++, OMM_TEXTURE_HUD_CLOCK_HAND, G_IM_FMT_RGBA, G_IM_SIZ_32b, 256, 256, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    gSPVertex(pos++, sClockHandVtx, 4, 0);
    gSP2Triangles(pos++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPSetTexturePersp(pos++, G_TP_PERSP);
    gDPSetRenderMode(pos++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineLERP(pos++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gDPSetTextureFilter(pos++, G_TF_BILERP);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);

    // Timer digits
    s32 ms = 1000.f * (timer / 30.f);
    s32 glyphs[] = {
        ((ms / 600000) % 10),
        ((ms /  60000) % 10),
        10,
        ((ms /  10000) %  6),
        ((ms /   1000) % 10),
        11,
        ((ms /    100) % 10),
        ((ms /     10) % 10),
    };
    for (s32 i = 0, j = 0; i != 8; ++i) {
        j = max_s(j, glyphs[i] * (glyphs[i] < 10));
        s16 x = OMM_RENDER_TIMER_X + i * OMM_RENDER_TIMER_OFFSET_X;
        s16 y = OMM_RENDER_TIMER_Y;
        s16 w = OMM_RENDER_GLYPH_SIZE;
        s16 h = OMM_RENDER_GLYPH_SIZE;
        gDPSetTexturePersp(pos++, G_TP_NONE);
        gDPSetRenderMode(pos++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
        gDPSetCombineLERP(pos++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0);
        gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF - (0xAA * (j == 0)));
        gDPSetTextureFilter(pos++, G_TF_POINT);
        gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
        gDPLoadTextureBlock(pos++, sOmmTimerGlyphs[glyphs[i]], G_IM_FMT_RGBA, G_IM_SIZ_32b, 16, 16, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
        gSPTextureRectangle(pos++, (x) << 2, (SCREEN_HEIGHT - h - y) << 2, (x + w) << 2, (SCREEN_HEIGHT - y) << 2, G_TX_RENDERTILE, 0, 0, (0x4000 / w), (0x4000 / h));
        gDPSetTexturePersp(pos++, G_TP_PERSP);
        gDPSetRenderMode(pos++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
        gDPSetCombineLERP(pos++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
        gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
        gDPSetTextureFilter(pos++, G_TF_BILERP);
        gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    }
    return pos;
}

void omm_render_hud_timer() {
    if (gHudDisplay.flags & HUD_DISPLAY_FLAG_TIMER) {

        // Clock
        s16 clockW = (OMM_RENDER_GLYPH_SIZE * 4) / 3;
        s16 clockX = OMM_RENDER_TIMER_X - (clockW * 3) / 2;
        s16 clockY = OMM_RENDER_TIMER_Y - (clockW - OMM_RENDER_GLYPH_SIZE) / 2;
        omm_render_texrect(clockX, clockY, clockW, clockW, 16, 16, 0xFF, 0xFF, 0xFF, 0xFF, OMM_TEXTURE_HUD_CLOCK, false);

        // Clock hand and timer
        omm_render_create_dl_ortho_matrix();
        interp_data_update(sOmmTimer, sOmmTimer->s1 >= (f32) gGlobalTimer - 1, gDisplayListHead, 0, 0, 0, 0, gGlobalTimer, gHudDisplay.timer);
        gDisplayListHead = omm_render_hud_timer_m_s_ms(gDisplayListHead, sOmmTimer->t0);
    }
}

//
// Cannon
//

void omm_render_hud_cannon_reticle() {
    if (gCurrentArea && gCurrentArea->camera && gCurrentArea->camera->mode == CAMERA_MODE_INSIDE_CANNON) {
        render_hud_cannon_reticle();
    }
}

//
// Values
//

static s16 omm_render_hud_star_count(struct MarioState *m, s16 y) {
    if (gHudDisplay.flags & HUD_DISPLAY_FLAG_STAR_COUNT) {
        u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudStarsTimer);
        if (alpha) {
            s32 levelNum = gCurrLevelNum;
            if (omm_render_hud_stars(OMM_RENDER_VALUE_GLYPH_X, y, alpha, levelNum, !omm_is_game_paused() && levelNum != OMM_LEVEL_END, false)) {
                y -= OMM_RENDER_OFFSET_Y;
            } else {

                // Power star counter
                omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, alpha, gHudDisplay.stars, 3, true, false);
                y -= OMM_RENDER_OFFSET_Y;

                // Sparkly Star counter
                if (OMM_SPARKLY_MODE_IS_ENABLED) {
                    s32 sparklyMode = gOmmSparklyMode;
                    omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_SPARKLY_HUD_GLYPH[sparklyMode], false);
                    omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, alpha, omm_sparkly_get_collected_count(sparklyMode), 3, true, false);
                    y -= OMM_RENDER_OFFSET_Y;
                }
            }
        }
    }
    return y;
}

static s16 omm_render_hud_coin_count(struct MarioState *m, s16 y) {
    if (gHudDisplay.flags & HUD_DISPLAY_FLAG_COIN_COUNT) {
        u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudCoinsTimer);
        if (alpha) {

            // Coins
            omm_render_hud_coins(OMM_RENDER_VALUE_GLYPH_X, y, alpha, gHudDisplay.coins, false);

            // Sparkles
            if (OMM_EXTRAS_VANISHING_HUD || sOmmHudCoinsTimer.alwaysActive) {

                // New sparkle
                if (sOmmHudCoinsTimer.timer == sOmmHudCoinsTimer.timerMin) {
                    for (s32 i = 0; i != OMM_RENDER_HUD_COIN_SPARKLE_COUNT; ++i) {
                        if (sOmmCoinSparkles[i] <= 0) {
                            sOmmCoinSparkles[i] = OMM_RENDER_HUD_COIN_SPARKLE_FRAMES * OMM_RENDER_HUD_COIN_SPARKLE_DELAY;
                            break;
                        }
                    }
                }

                // Update
                for (s32 i = 0; i != OMM_RENDER_HUD_COIN_SPARKLE_COUNT; ++i) {
                    if (sOmmCoinSparkles[i]-- > 0) {
                        omm_render_texrect(
                            OMM_RENDER_HUD_COIN_SPARKLE_X, OMM_RENDER_HUD_COIN_SPARKLE_Y,
                            OMM_RENDER_HUD_COIN_SPARKLE_SIZE, OMM_RENDER_HUD_COIN_SPARKLE_SIZE,
                            32, 32, 0xFF, 0xFF, 0xFF, 0xFF,
                            (const void *) (array_of(const char *) {
                                OMM_ASSET_SPARKLE_5,
                                OMM_ASSET_SPARKLE_4,
                                OMM_ASSET_SPARKLE_3,
                                OMM_ASSET_SPARKLE_2,
                                OMM_ASSET_SPARKLE_1,
                                OMM_ASSET_SPARKLE_0,
                            })[sOmmCoinSparkles[i] / OMM_RENDER_HUD_COIN_SPARKLE_DELAY],
                            false
                        );
                    }
                }
            }
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_vibe_gauge(struct MarioState *m, s16 y) {
    if (gHudDisplay.flags & OMM_HUD_DISPLAY_FLAG_VIBE_GAUGE) {
        if (OMM_PLAYER_IS_PEACH) {
            u8 alpha = vanishing_hud_update_timer_and_get_alpha(m, &sOmmHudVibeTimer);
            if (alpha) {
                switch (gOmmPeach->vibeType) {
                    case OMM_PEACH_VIBE_TYPE_NONE:  omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_VIBE_NORMAL, false); break;
                    case OMM_PEACH_VIBE_TYPE_JOY:   omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_VIBE_JOY, false); break;
                    case OMM_PEACH_VIBE_TYPE_RAGE:  omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_VIBE_RAGE, false); break;
                    case OMM_PEACH_VIBE_TYPE_GLOOM: omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_VIBE_GLOOM, false); break;
                    case OMM_PEACH_VIBE_TYPE_CALM:  omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_VIBE_CALM, false); break;
                }
                s32 value = relerp_0_1_s(gOmmPeach->vibeGauge, OMM_PEACH_VIBE_GAUGE_MAX, OMM_PEACH_VIBE_GAUGE_HEART_INC, 0, 100);
                omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, alpha, value, 3, true, false);
                y -= OMM_RENDER_OFFSET_Y;
            }
        }
    }
    return y;
}

static s16 omm_render_hud_power_up(struct MarioState *m, s16 y) {
    static const char *sPowerUpGlyphs[4][2][2] = {
        { { OMM_TEXTURE_HUD_CAP_MARIO,   OMM_TEXTURE_HUD_CAP_MARIO_METAL   },
          { OMM_TEXTURE_HUD_CAPPY_MARIO, OMM_TEXTURE_HUD_CAPPY_MARIO_METAL }, },
        { { OMM_TEXTURE_HUD_CAP_PEACH,   OMM_TEXTURE_HUD_CAP_PEACH_METAL   },
          { OMM_TEXTURE_HUD_CAPPY_PEACH, OMM_TEXTURE_HUD_CAPPY_PEACH_METAL }, },
        { { OMM_TEXTURE_HUD_CAP_LUIGI,   OMM_TEXTURE_HUD_CAP_LUIGI_METAL   },
          { OMM_TEXTURE_HUD_CAPPY_LUIGI, OMM_TEXTURE_HUD_CAPPY_LUIGI_METAL }, },
        { { OMM_TEXTURE_HUD_CAP_WARIO,   OMM_TEXTURE_HUD_CAP_WARIO_METAL   },
          { OMM_TEXTURE_HUD_CAPPY_WARIO, OMM_TEXTURE_HUD_CAPPY_WARIO_METAL }, },
    };

    s32 idx = omm_player_get_selected_index();
    bool cc = !OMM_CAP_CLASSIC;
    bool wc = (m->flags & MARIO_WING_CAP) != 0;
    bool mc = (m->flags & MARIO_METAL_CAP) != 0;
    bool vc = (m->flags & MARIO_VANISH_CAP) != 0;

    // Render the cap power-up remaining time
    if (m->capTimer > 0 && (wc || mc || vc)) {
        u8 alpha = (vc ? 0x80 : 0xFF);
        if (wc) omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, OMM_TEXTURE_HUD_CAP_WINGS, false);
        omm_render_glyph_hud(OMM_RENDER_VALUE_GLYPH_X, y, 0xFF, 0xFF, 0xFF, alpha, sPowerUpGlyphs[idx][cc][mc], false);
        omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, (m->capTimer + 29) / 30, 3, true, false);
        y -= OMM_RENDER_OFFSET_Y;
    }
    return y;
}

static s16 omm_render_hud_sparkly_coins(s16 y) {
    s32 numCoins = omm_sparkly_context_get_data(OMM_SPARKLY_DATA_COINS);
    if (numCoins > 0 && OMM_SPARKLY_STATE_IS_OK) {
        s32 collectedCoins = (
            gOmmSparklyContext->coinsYellow * 1 +
            gOmmSparklyContext->coinsRed * 2 +
            gOmmSparklyContext->coinsBlue * 5
        );
        s32 coins = numCoins - collectedCoins;
        if (coins > 0) {
            s32 coinW = OMM_RENDER_GLYPH_SIZE;
            s32 coinX = OMM_RENDER_VALUE_GLYPH_X - (coinW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 coinY = y - (coinW - OMM_RENDER_GLYPH_SIZE) / 2;

            // Coin type
            static struct { u8 r, g, b; s32 value; } sCoins[] = {
                { 0xFF, 0xFF, 0x00, 1 }, // Yellow
                { 0xFF, 0x00, 0x00, 2 }, // Red
                { 0x78, 0x78, 0xFF, 5 }, // Blue
                { 0xFF, 0xFF, 0x00, 1 },
            };
            s32 coinType = min_s(3,
                0 * omm_sparkly_context_get_data(OMM_SPARKLY_DATA_ONLY_COIN_Y) +
                1 * omm_sparkly_context_get_data(OMM_SPARKLY_DATA_ONLY_COIN_R) +
                2 * omm_sparkly_context_get_data(OMM_SPARKLY_DATA_ONLY_COIN_B)
            );
            
            // Render the coin icon
            omm_render_texrect(
                coinX, coinY, coinW, coinW, 32, 32, 
                sCoins[coinType].r, sCoins[coinType].g, sCoins[coinType].b, 0xFF,
                (const void *) (array_of(const char *) {
                OMM_ASSET_COIN_0,
                OMM_ASSET_COIN_1,
                OMM_ASSET_COIN_2,
                OMM_ASSET_COIN_3,
                })[(gGlobalTimer >> 1) & 3],
                false
            );
            
            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, coins / sCoins[coinType].value, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_red_coins(s16 y) {
    s32 numRedCoins = omm_sparkly_context_get_data(OMM_SPARKLY_DATA_RED_COINS);
    if (numRedCoins > 0 && OMM_SPARKLY_STATE_IS_OK) {
        s32 redCoins = numRedCoins - gOmmSparklyContext->coinsRed;
        if (redCoins > 0) {
            s32 redCoinW = OMM_RENDER_GLYPH_SIZE;
            s32 redCoinX = OMM_RENDER_VALUE_GLYPH_X - (redCoinW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 redCoinY = y - (redCoinW - OMM_RENDER_GLYPH_SIZE) / 2;
            
            // Render the red coin or star shard icon
            if (omm_sparkly_context_get_data(OMM_SPARKLY_DATA_SHARDS)) {
                omm_render_texrect(
                    redCoinX, redCoinY, redCoinW, redCoinW,
                    32, 32, 0xFF, 0xFF, 0xFF, 0xFF,
                    (const void *) (array_of(const char *) {
                    OMM_ASSET_SPARKLE_5,
                    OMM_ASSET_SPARKLE_4,
                    OMM_ASSET_SPARKLE_3,
                    OMM_ASSET_SPARKLE_2,
                    OMM_ASSET_SPARKLE_1,
                    OMM_ASSET_SPARKLE_0,
                    })[(gGlobalTimer >> 1) % 6],
                    false
                );
            } else {
                omm_render_texrect(
                    redCoinX, redCoinY, redCoinW, redCoinW,
                    32, 32, 0xFF, 0x00, 0x00, 0xFF,
                    (const void *) (array_of(const char *) {
                    OMM_ASSET_COIN_0,
                    OMM_ASSET_COIN_1,
                    OMM_ASSET_COIN_2,
                    OMM_ASSET_COIN_3,
                    })[(gGlobalTimer >> 1) & 3],
                    false
                );
            }
            
            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, redCoins, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_flames(s16 y) {
    if (omm_sparkly_context_get_data(OMM_SPARKLY_DATA_FLAMES) && OMM_SPARKLY_STATE_IS_OK) {
        s32 flames = omm_sparkly_context_get_remaining_flames();
        if (flames > 0) {
            s32 flameW = 20;
            s32 flameX = OMM_RENDER_VALUE_GLYPH_X - (flameW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 flameY = y - (flameW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 flameT = (gGlobalTimer / 2) % 8;

            // Render the animated flame
            omm_render_texrect(
                flameX, flameY, flameW, flameW,
                32, 32, 0xFF, 0x00, 0x00, 0xFF,
                (const void *) (array_of(const char *) {
                    OMM_ASSET_FLAME_0,
                    OMM_ASSET_FLAME_1,
                    OMM_ASSET_FLAME_2,
                    OMM_ASSET_FLAME_3,
                    OMM_ASSET_FLAME_4,
                    OMM_ASSET_FLAME_5,
                    OMM_ASSET_FLAME_6,
                    OMM_ASSET_FLAME_7,
                })[flameT],
                false
            );

            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, flames, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_boxes(s16 y) {
    if (omm_sparkly_context_get_data(OMM_SPARKLY_DATA_BOXES) && OMM_SPARKLY_STATE_IS_OK) {
        s32 boxes = omm_sparkly_context_get_remaining_boxes();
        if (boxes > 0) {
            s32 boxW = OMM_RENDER_GLYPH_SIZE;
            s32 boxX = OMM_RENDER_VALUE_GLYPH_X - (boxW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 boxY = y - (boxW - OMM_RENDER_GLYPH_SIZE) / 2;

            // Render the box icon
            omm_render_texrect(
                boxX, boxY, boxW, boxW,
                32, 32, 0xFF, 0xFF, 0xFF, 0xFF,
                (const void *) OMM_ASSET_EXCLAMATION_BOX,
                false
            );

            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, boxes, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_mushrooms(s16 y) {
    s32 numMushrooms = omm_sparkly_context_get_data(OMM_SPARKLY_DATA_MUSHROOMS);
    if (numMushrooms > 0 && OMM_SPARKLY_STATE_IS_OK) {
        s32 mushrooms = numMushrooms - gOmmSparklyContext->mushrooms;
        if (mushrooms > 0) {
            s32 mushroomW = OMM_RENDER_GLYPH_SIZE;
            s32 mushroomX = OMM_RENDER_VALUE_GLYPH_X - (mushroomW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 mushroomY = y - (mushroomW - OMM_RENDER_GLYPH_SIZE) / 2;

            // Render the mushroom icon
            omm_render_texrect(
                mushroomX, mushroomY, mushroomW, mushroomW,
                32, 64, 0xFF, 0xFF, 0xFF, 0xFF,
                (const void *) OMM_ASSET_MUSHROOM_1UP,
                false
            );

            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, mushrooms, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_star_rings(s16 y) {
    if (omm_sparkly_context_get_data(OMM_SPARKLY_DATA_RINGS) && OMM_SPARKLY_STATE_IS_OK) {
        s32 rings = omm_sparkly_context_get_remaining_star_rings();
        if (rings > 0) {
            s32 ringW = OMM_RENDER_GLYPH_SIZE;
            s32 ringX = OMM_RENDER_VALUE_GLYPH_X + ringW / 2;
            s32 ringY = y + ringW / 2;
            f32 ringS = ringW / 512.f;

            // Render the star ring
            create_dl_translation_matrix(G_MTX_PUSH, ringX, ringY, 0);
            create_dl_scale_matrix(G_MTX_PUSH, ringS, ringS, ringS);
            gDPSetEnvColor(gDisplayListHead++, 0xFF, 0xFF, 0xFF, 0xFF);
            gSPDisplayList(gDisplayListHead++, omm_geo_star_ring[11]);
            gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
            gSPPopMatrix(gDisplayListHead++, G_MTX_MODELVIEW);
            
            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, rings, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

static s16 omm_render_hud_sparkly_enemies(s16 y) {
    if (omm_sparkly_context_get_data(OMM_SPARKLY_DATA_ENEMIES) && OMM_SPARKLY_STATE_IS_OK) {
        s32 enemies = omm_sparkly_context_get_remaining_enemies();
        if (enemies > 0) {
            s32 enemyW = 14;
            s32 enemyX = OMM_RENDER_VALUE_GLYPH_X - (enemyW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 enemyY = y - (enemyW - OMM_RENDER_GLYPH_SIZE) / 2;
            s32 enemyT = gGlobalTimer % 30;

            // Render the animated flame
            omm_render_texrect(
                enemyX, enemyY, enemyW, enemyW,
                512, 512, 0xFF, 0xFF, 0xFF, 0xFF,
                (const void *) (array_of(const char *) {
                    OMM_TEXTURE_HUD_ENEMY_0,
                    OMM_TEXTURE_HUD_ENEMY_1,
                    OMM_TEXTURE_HUD_ENEMY_2,
                    OMM_TEXTURE_HUD_ENEMY_3,
                    OMM_TEXTURE_HUD_ENEMY_4,
                    OMM_TEXTURE_HUD_ENEMY_5,
                    OMM_TEXTURE_HUD_ENEMY_6,
                    OMM_TEXTURE_HUD_ENEMY_7,
                    OMM_TEXTURE_HUD_ENEMY_8,
                    OMM_TEXTURE_HUD_ENEMY_9,
                    OMM_TEXTURE_HUD_ENEMY_10,
                    OMM_TEXTURE_HUD_ENEMY_11,
                    OMM_TEXTURE_HUD_ENEMY_12,
                    OMM_TEXTURE_HUD_ENEMY_13,
                    OMM_TEXTURE_HUD_ENEMY_14,
                    OMM_TEXTURE_HUD_ENEMY_15,
                    OMM_TEXTURE_HUD_ENEMY_16,
                    OMM_TEXTURE_HUD_ENEMY_17,
                    OMM_TEXTURE_HUD_ENEMY_18,
                    OMM_TEXTURE_HUD_ENEMY_19,
                    OMM_TEXTURE_HUD_ENEMY_20,
                    OMM_TEXTURE_HUD_ENEMY_21,
                    OMM_TEXTURE_HUD_ENEMY_22,
                    OMM_TEXTURE_HUD_ENEMY_23,
                    OMM_TEXTURE_HUD_ENEMY_24,
                    OMM_TEXTURE_HUD_ENEMY_25,
                    OMM_TEXTURE_HUD_ENEMY_26,
                    OMM_TEXTURE_HUD_ENEMY_27,
                    OMM_TEXTURE_HUD_ENEMY_28,
                    OMM_TEXTURE_HUD_ENEMY_29,
                })[enemyT],
                false
            );
            
            // Render the number
            omm_render_number_hud(OMM_RENDER_VALUE_NUMBER_X, y, 0xFF, enemies, 3, true, false);
            y -= OMM_RENDER_OFFSET_Y;
        }
    }
    return y;
}

void omm_render_hud_values(struct MarioState *m) {
    s16 y = OMM_RENDER_VALUE_Y;
    y = omm_render_hud_star_count(m, y);
    y = omm_render_hud_coin_count(m, y);
    y = omm_render_hud_vibe_gauge(m, y);
    y = omm_render_hud_power_up(m, y);
    if (gHudDisplay.flags & OMM_HUD_DISPLAY_FLAG_SPARKLY_STARS_CONDITIONS) {
        y = omm_render_hud_sparkly_coins(y);
        y = omm_render_hud_sparkly_red_coins(y);
        y = omm_render_hud_sparkly_flames(y);
        y = omm_render_hud_sparkly_boxes(y);
        y = omm_render_hud_sparkly_mushrooms(y);
        y = omm_render_hud_sparkly_star_rings(y);
        y = omm_render_hud_sparkly_enemies(y);
    }
}

//
// Red coins radar
//

static Gfx *omm_render_hud_red_coins_radar_arrow(Gfx *pos, f32 angle) {
    static Vtx sRadarArrowVtx[4] = {
        { { { 0, 0, 0 }, 0, { 0x0000, 0x1000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x1000, 0x1000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x1000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x0000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } }
    };

    // Vertices
    for (s32 i = 0; i != 4; ++i) {
        s16 a = angle + ((i * 0x4000) - 0x6000);
        sRadarArrowVtx[i].v.ob[0] = OMM_RENDER_RADAR_X + OMM_RENDER_RADAR_RADIUS * coss(angle + 0x4000) + OMM_RENDER_RADAR_ARROW_SIZE * OMM_INVSQRT2 * coss(a);
        sRadarArrowVtx[i].v.ob[1] = OMM_RENDER_RADAR_Y + OMM_RENDER_RADAR_RADIUS * sins(angle + 0x4000) + OMM_RENDER_RADAR_ARROW_SIZE * OMM_INVSQRT2 * sins(a);
    }

    // Display list
    gDPSetRenderMode(pos++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineLERP(pos++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(pos++, OMM_TEXTURE_HUD_ARROW, G_IM_FMT_RGBA, G_IM_SIZ_32b, 128, 128, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    gSPVertex(pos++, sRadarArrowVtx, 4, 0);
    gSP2Triangles(pos++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPSetRenderMode(pos++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineLERP(pos++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    return pos;
}

void omm_render_hud_red_coins_radar(struct MarioState *m) {
    if (OMM_EXTRAS_RED_COINS_RADAR && (gHudDisplay.flags & OMM_HUD_DISPLAY_FLAG_RED_COINS_RADAR)) {
        struct Object *radarTarget = obj_get_red_coin_star();
        if (radarTarget) {

            // Target and display the nearest red coin
            struct Object *redCoin = obj_get_nearest_with_behavior(m->marioObj, bhvRedCoin);
            if (redCoin) {
                radarTarget = redCoin;
                omm_render_create_dl_ortho_matrix();
                omm_render_texrect(
                    OMM_RENDER_RADAR_X - OMM_RENDER_RADAR_COIN_SIZE / 2,
                    OMM_RENDER_RADAR_Y - OMM_RENDER_RADAR_COIN_SIZE / 2,
                    OMM_RENDER_RADAR_COIN_SIZE,
                    OMM_RENDER_RADAR_COIN_SIZE,
                    32, 32, 0xFF, 0x00, 0x00, 0xFF,
                    (const void *) (array_of(const char *) {
                    OMM_ASSET_COIN_0,
                    OMM_ASSET_COIN_1,
                    OMM_ASSET_COIN_2,
                    OMM_ASSET_COIN_3,
                    })[(gGlobalTimer >> 1) & 3],
                    false
                );
            }
            
            // Target and display the red coin star
            else {
                omm_render_create_dl_ortho_matrix();
                omm_render_texrect(
                    OMM_RENDER_RADAR_X - OMM_RENDER_RADAR_STAR_SIZE / 2,
                    OMM_RENDER_RADAR_Y - OMM_RENDER_RADAR_STAR_SIZE / 2,
                    OMM_RENDER_RADAR_STAR_SIZE,
                    OMM_RENDER_RADAR_STAR_SIZE,
                    32, 32, 0xFF, 0x80, 0x80, 0xFF,
                    (const void *) (array_of(const char *) {
                    OMM_ASSET_RED_STAR_0,
                    OMM_ASSET_RED_STAR_1,
                    OMM_ASSET_RED_STAR_2,
                    OMM_ASSET_RED_STAR_3,
                    OMM_ASSET_RED_STAR_4,
                    OMM_ASSET_RED_STAR_5,
                    OMM_ASSET_RED_STAR_6,
                    OMM_ASSET_RED_STAR_7,
                    })[(gGlobalTimer >> 1) & 7],
                    false
                );
            }
            
            // Display radar arrow
            s16 angle = atan2s(
                radarTarget->oPosZ - m->pos[2],
                radarTarget->oPosX - m->pos[0]
            ) - atan2s(
                m->pos[2] - gCamera->pos[2],
                m->pos[0] - gCamera->pos[0]
            );
            omm_render_create_dl_ortho_matrix();
            interp_data_update(sOmmRedCoinsArrow, true, gDisplayListHead, 0, 0, 0, angle, 0, 0);
            if (sOmmRedCoinsArrow->a0 * sOmmRedCoinsArrow->a1 < 0) { // Check the -32768/+32767 over/underflow
                f32 dl = 0x8000 - abs_f(sOmmRedCoinsArrow->a0);
                f32 dr = 0x8000 - abs_f(sOmmRedCoinsArrow->a1);
                sOmmRedCoinsArrow->a0 -= (dl + dr < 0x8000) * sign_f(sOmmRedCoinsArrow->a0) * 0x10000;
            }
            gDisplayListHead = omm_render_hud_red_coins_radar_arrow(gDisplayListHead, sOmmRedCoinsArrow->a0);
        }
    }
}

//
// Monty mole hole indicator
//

static Gfx *omm_render_hud_monty_mole_hole_indicator_arrow(Gfx *pos, f32 x, f32 y, f32 radius, f32 angle, f32 scale) {
    static Vtx sMontyMoleHoleIndicatorArrow[4] = {
        { { { 0, 0, 0 }, 0, { 0x0000, 0x1000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x1000, 0x1000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x1000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } },
        { { { 0, 0, 0 }, 0, { 0x0000, 0x0000 }, { 0xFF, 0xFF, 0xFF, 0xFF } } }
    };

    // Vertices
    for (s32 i = 0; i != 4; ++i) {
        s16 a = angle + ((i * 0x4000) - 0x6000);
        sMontyMoleHoleIndicatorArrow[i].v.ob[0] = x +         (radius * coss(angle + 0x4000) + OMM_RENDER_RADAR_ARROW_SIZE * OMM_INVSQRT2 * coss(a));
        sMontyMoleHoleIndicatorArrow[i].v.ob[1] = y + scale * (radius * sins(angle + 0x4000) + OMM_RENDER_RADAR_ARROW_SIZE * OMM_INVSQRT2 * sins(a));
    }
    
    // Display list
    gDPSetRenderMode(pos++, G_RM_AA_XLU_SURF, G_RM_AA_XLU_SURF2);
    gDPSetCombineLERP(pos++, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0, TEXEL0, 0, ENVIRONMENT, 0);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPLoadTextureBlock(pos++, OMM_TEXTURE_HUD_ARROW_2, G_IM_FMT_RGBA, G_IM_SIZ_32b, 128, 128, 0, G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
    gSPVertex(pos++, sMontyMoleHoleIndicatorArrow, 4, 0);
    gSP2Triangles(pos++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPSetRenderMode(pos++, G_RM_AA_ZB_OPA_SURF, G_RM_AA_ZB_OPA_SURF2);
    gDPSetCombineLERP(pos++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, SHADE);
    gDPSetEnvColor(pos++, 0xFF, 0xFF, 0xFF, 0xFF);
    gSPTexture(pos++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_OFF);
    return pos;
}

void omm_render_hud_monty_mole_hole_indicator() {
    struct Object *currHole = gOmmMario->capture.obj->oMontyMoleCurrentHole;
    struct Object *nextHole = gOmmMario->capture.obj->oMontyMoleTargetHole;
    Vec3f src3d = { currHole->oPosX, currHole->oPosY, currHole->oPosZ };

    // Camera normal
    Vec3f camN;
    vec3f_dif(camN, gLakituState.focus, gLakituState.pos);
    vec3f_normalize(camN);

    // Camera horizontal axis
    s16 camYaw = atan2s(camN[2], camN[0]) - 0x4000;
    Vec3f camH = { sins(camYaw), 0.f, coss(camYaw) };

    // Camera vertical axis
    Vec3f camV;
    vec3f_cross(camV, camN, camH);
    vec3f_normalize(camV);

    // Hole position to camera plane
    Vec2f pos2d;
    f32 dist;
    vec3f_to_2d_plane(pos2d, &dist, src3d, gLakituState.pos, camN, camH, camV);
    if (dist > 0.f) {
        f32 x = (SCREEN_WIDTH  / 2) * (1.f + (1.75f * (pos2d[0] / dist)));
        f32 y = (SCREEN_HEIGHT / 2) * (1.f - (2.25f * (pos2d[1] / dist)));

        // Angle, radius and scale
        s16 angle = atan2s(
            nextHole->oPosZ - currHole->oPosZ,
            nextHole->oPosX - currHole->oPosX
        ) - atan2s(
            currHole->oPosZ - gCamera->pos[2],
            currHole->oPosX - gCamera->pos[0]
        );
        f32 radius = (120 * SCREEN_HEIGHT) / dist;
        f32 scale = relerp_0_1_f(abs_f(coss(angle)), 0, 1, 1, (1 + abs_f(camN[1])) / 2);

        // Render
        omm_render_create_dl_ortho_matrix();
        gDisplayListHead = omm_render_hud_monty_mole_hole_indicator_arrow(gDisplayListHead, x, y, radius, angle, scale);  
    }
}

//
// HUD
//

void omm_render_hud() {
    struct MarioState *m = gMarioState;

    // Cake end screen
    // Render the custom cake end screen on top of
    // the regular cake end screen, if necessary
    if (omm_is_ending_cake_screen()) {
        omm_sparkly_ending_screen();
        return;
    }

    // Version number
    // Only on title screen/main menu
    if (omm_is_main_menu()) {
        const u8 *textVersion = omm_text_convert("OMM V" STRINGIFY(OMM_VERSION), false);
        s32 textVersionWidth = omm_render_get_string_width(textVersion);
        omm_render_string_left_align(GFX_DIMENSIONS_FROM_RIGHT_EDGE(textVersionWidth + 2), 3, 0xFF, 0xFF, 0xFF, 0xFF, textVersion, true);
        return;
    }

    // Monty mole capture hole indicator
    if (omm_mario_is_capture(m) &&
        gOmmMario->capture.bhv == bhvMontyMole &&
        gOmmMario->capture.obj != NULL &&
        gOmmMario->capture.obj->oMontyMoleCurrentHole &&
        gOmmMario->capture.obj->oMontyMoleTargetHole) {
        omm_render_hud_monty_mole_hole_indicator();
    }

    // HUD
    if (gHudDisplay.flags && configHUD) {
        omm_render_create_dl_ortho_matrix();
        omm_render_hud_cannon_reticle();
        omm_render_effect_dark_mode();
        omm_render_effect_freeze();
        omm_render_effect_vibe();
        omm_render_effect_you_got_a_star();
        omm_render_hud_o2_meter(m);
        omm_render_hud_power_meter(m);
        omm_render_hud_red_coins_radar(m);
        omm_render_hud_values(m);
        omm_render_hud_camera(m);
        omm_render_hud_timer();
    }
}
