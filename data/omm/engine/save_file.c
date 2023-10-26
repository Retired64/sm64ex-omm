#define OMM_ALL_HEADERS
#include "data/omm/omm_includes.h"
#undef OMM_ALL_HEADERS
#pragma GCC push_options
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define OMM_SETTINGS        "[settings]"
#define OMM_SAVEFILE_NAME   "omm_save_file.txt"
#define OMM_SAVEFILE_BACKUP "omm_save_file.backup"

u8 gLastCompletedCourseNum = LEVEL_CASTLE;
u8 gLastCompletedLevelNum = LEVEL_NONE;
u8 gLastCompletedStarNum = 0;
u8 gCurrCourseStarFlags = 0;
u8 gGotFileCoinHiScore = 0;
u8 gSpecialTripleJump = 0;

//
// Flags and courses
//

typedef struct {
    const char *name;
    u32 value;
} OmmNameValue;

static const OmmNameValue sOmmFlags[] = {
    { "wing_cap", SAVE_FLAG_HAVE_WING_CAP },
    { "metal_cap", SAVE_FLAG_HAVE_METAL_CAP },
    { "vanish_cap", SAVE_FLAG_HAVE_VANISH_CAP },
    { "bowser_key_1", SAVE_FLAG_HAVE_KEY_1 },
    { "bowser_key_2", SAVE_FLAG_HAVE_KEY_2 },
    { "moat_drained", SAVE_FLAG_MOAT_DRAINED },
    { "ddd_moved_back", SAVE_FLAG_DDD_MOVED_BACK },
    { "pps_door", SAVE_FLAG_UNLOCKED_PSS_DOOR },
    { "wf_door", SAVE_FLAG_UNLOCKED_WF_DOOR },
    { "jrb_door", SAVE_FLAG_UNLOCKED_JRB_DOOR },
    { "ccm_door", SAVE_FLAG_UNLOCKED_CCM_DOOR },
    { "8_star_door", SAVE_FLAG_UNLOCKED_BITDW_DOOR },
    { "30_star_door", SAVE_FLAG_UNLOCKED_BITFS_DOOR },
    { "50_star_door", SAVE_FLAG_UNLOCKED_50_STAR_DOOR },
    { "basement_door", SAVE_FLAG_UNLOCKED_BASEMENT_DOOR },
    { "upstairs_door", SAVE_FLAG_UNLOCKED_UPSTAIRS_DOOR },
};
#define OMM_FLAGS_COUNT array_length(sOmmFlags)

static const OmmNameValue sOmmCourses[] = {
    { "bob", COURSE_BOB },
    { "wf", COURSE_WF },
    { "jrb", COURSE_JRB },
    { "ccm", COURSE_CCM },
    { "bbh", COURSE_BBH },
    { "hmc", COURSE_HMC },
    { "lll", COURSE_LLL },
    { "ssl", COURSE_SSL },
    { "ddd", COURSE_DDD },
    { "sl", COURSE_SL },
    { "wdw", COURSE_WDW },
    { "ttm", COURSE_TTM },
    { "thi", COURSE_THI },
    { "ttc", COURSE_TTC },
    { "rr", COURSE_RR },
    { "bitdw", COURSE_BITDW },
    { "bitfs", COURSE_BITFS },
    { "bits", COURSE_BITS },
    { "pss", COURSE_PSS },
    { "cotmc", COURSE_COTMC },
    { "totwc", COURSE_TOTWC },
    { "vcutm", COURSE_VCUTM },
    { "wmotr", COURSE_WMOTR },
    { "sa", COURSE_SA },
    { "castle", COURSE_COUNT },
};
#define OMM_COURSES_COUNT array_length(sOmmCourses)

//
// Collectibles
//

#define OMM_COLLECTIBLE_GET_B(collectibles, start, index)           (((collectibles) >> (start + index)) & 1)
#define OMM_COLLECTIBLE_SET_B(collectibles, start, index, value)    collectibles &= ~(1llu << ((start) + (index))); collectibles |= (((u64) (value)) << ((start) + (index)))
#define OMM_COLLECTIBLE_GET_I(collectibles, start, length)          (((collectibles) >> (start)) & ((1llu << (length)) - 1llu))
#define OMM_COLLECTIBLE_SET_I(collectibles, start, length, value)   collectibles &= ~(((1llu << (length)) - 1llu) << (start)); collectibles |= ((((u64) (value)) & ((1llu << (length)) - 1llu)) << (start))

//
// Data
//

typedef struct {
    u8 stars;
    s16 score;
    bool cannon;
} OmmSaveCourse;

typedef struct {
    u32 flags;
    u64 collectibles;
    s32 lastCourse;
    OmmSaveCourse courses[COURSE_COUNT];
} OmmSaveFile;

typedef struct {
    const char *name;
    s32 modes;
    s32 starsPerCourse;
    const char *collectibles;
    OmmSaveFile files[NUM_SAVE_FILES + 1][2];
} OmmSaveBuffer;

static OmmSaveBuffer sOmmSaveBuffers[OMM_GAME_COUNT] = {
    [OMM_GAME_SMEX] = { "smex", 1, 7, NULL, { { { 0 } } } },
    [OMM_GAME_XALO] = { "xalo", 1, 7, NULL, { { { 0 } } } },
#if OMM_GAME_IS_R96X
    [OMM_GAME_R96X] = { "r96x", 1, 7, "b00" STRINGIFY(NUM_KEYS) "b" STRINGIFY(NUM_KEYS) STRINGIFY(NUM_WARIO_COINS), { { { 0 } } } },
#else
    [OMM_GAME_R96X] = { "r96x", 1, 7, "b0016b1616", { { { 0 } } } },
#endif
    [OMM_GAME_SMMS] = { "smms", 1, 7, NULL, { { { 0 } } } },
    [OMM_GAME_SM74] = { "sm74", 2, 7, NULL, { { { 0 } } } },
    [OMM_GAME_SMSR] = { "smsr", 1, 7, NULL, { { { 0 } } } },
    [OMM_GAME_SMGS] = { "smgs", 1, 7, NULL, { { { 0 } } } },
};

static bool sOmmSaveFileShouldWrite = false;

//
// Utils
//

#define sOmmSaveBuffer                      (&sOmmSaveBuffers[OMM_GAME_SAVE])
#define sOmmSaveFile                        (&sOmmSaveBuffer->files[fileIndex][modeIndex])

#define COURSE_CASTLE                       (COURSE_COUNT - 1)
#define COURSE_INDEX_INCLUDING_CASTLE       ((courseIndex == -1) ? COURSE_CASTLE : courseIndex)
#define CHECK_FILE_INDEX(i, fail)           if (i < 0 || i >= NUM_SAVE_FILES + 1) { fail; }
#define CHECK_MODE_INDEX(i, fail)           if (i < 0 || i >= sOmmSaveBuffer->modes) { fail; }
#define CHECK_COURSE_INDEX(i, fail)         if (i < -1 || i >= COURSE_COUNT) { fail; }

//
// Read
//

typedef struct { char args[4][128]; } Token;
static Token __tokenize(const char *buffer) {
    Token token; mem_clr(&token, sizeof(token));
    char temp[128];
    s32 index = 0;
    s32 length = 0;
    s32 maxlen = strlen(buffer);
    for (s32 i = 0; i <= maxlen; ++i) {
        char c = buffer[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '_')) {
            if (length < 127) {
                temp[length++] = c;
            }
        } else {
            if (length > 0) {
                mem_cpy(token.args[index++], temp, length);
                if (index >= 4) {
                    return token;
                }
            }
            length = 0;
        }
    }
    return token;
}

static void __trim_left(char *buffer) {
    s32 l = strlen(buffer);
    s32 i = 0;
    while (buffer[i] != 0 && buffer[i] <= ' ') ++i;
    mem_mov(buffer, buffer + i, l - i + 1);
}

void omm_save_file_load_all() {
    static bool sOmmSaveFileLoaded = false;
    static bool sOmmSaveFileBackup = false;
    if (!sOmmSaveFileLoaded) {
        str_cat_paths_sa(filename, 256, sys_user_path(), (sOmmSaveFileBackup ? OMM_SAVEFILE_BACKUP : OMM_SAVEFILE_NAME));
        FILE *f = fopen(filename, "r");
        bool loaded = false;
        if (f) {
            OmmSaveBuffer *saveBuffer = NULL;
            OmmSaveFile *saveFile = NULL;
            char buffer[1024];
            while (fgets(buffer, 1024, f)) {
                __trim_left(buffer);
                if (buffer[0] != '#') {
                    Token token = __tokenize(buffer);

                    // Settings flag, the savefile is successfully loaded
                    if (strstr(buffer, OMM_SETTINGS)) {
                        loaded = true;
                    }

                    // Version
                    else if (buffer[0] == '[') {

                        // Buffer
                        for (s32 i = 0; i != array_length(sOmmSaveBuffers); ++i) {
                            if (strcmp(sOmmSaveBuffers[i].name, token.args[0]) == 0) {

                                // File index
                                s32 fileIndex = token.args[1][0] - 'A';

                                // Mode index
                                s32 modeIndex = token.args[1][1] - '0';

                                // Save file
                                saveBuffer = &sOmmSaveBuffers[i];
                                saveFile = &saveBuffer->files[fileIndex][modeIndex];
                                saveFile->flags |= SAVE_FLAG_FILE_EXISTS;
                                break;
                            }
                        }
                    }

                    // Save data
                    else if (saveFile) {

                        // Settings data
                        READ_KBINDS(gOmmControlsButtonA);
                        READ_KBINDS(gOmmControlsButtonB);
                        READ_KBINDS(gOmmControlsButtonX);
                        READ_KBINDS(gOmmControlsButtonY);
                        READ_KBINDS(gOmmControlsButtonStart);
                        READ_KBINDS(gOmmControlsButtonSpin);
                        READ_KBINDS(gOmmControlsTriggerL);
                        READ_KBINDS(gOmmControlsTriggerR);
                        READ_KBINDS(gOmmControlsTriggerZ);
                        READ_KBINDS(gOmmControlsCUp);
                        READ_KBINDS(gOmmControlsCDown);
                        READ_KBINDS(gOmmControlsCLeft);
                        READ_KBINDS(gOmmControlsCRight);
                        READ_KBINDS(gOmmControlsDUp);
                        READ_KBINDS(gOmmControlsDDown);
                        READ_KBINDS(gOmmControlsDLeft);
                        READ_KBINDS(gOmmControlsDRight);
                        READ_KBINDS(gOmmControlsStickUp);
                        READ_KBINDS(gOmmControlsStickDown);
                        READ_KBINDS(gOmmControlsStickLeft);
                        READ_KBINDS(gOmmControlsStickRight);
                        READ_CHOICE(gOmmFrameRate);
                        READ_TOGGLE(gOmmShowFPS);
                        READ_CHOICE(gOmmTextureCaching);
                        READ_CHOICE_SC(gOmmCharacter);
                        READ_CHOICE_SC(gOmmMovesetType);
                        READ_CHOICE_SC(gOmmCapType);
                        READ_CHOICE_SC(gOmmStarsMode);
                        READ_CHOICE_SC(gOmmPowerUpsType);
                        READ_CHOICE_SC(gOmmCameraMode);
                        READ_CHOICE_SC(gOmmSparklyStarsMode);
                        READ_CHOICE_SC(gOmmSparklyStarsHintAtLevelEntry);
                        READ_CHOICE(gOmmExtrasMarioColors);
                        READ_CHOICE(gOmmExtrasPeachColors);
                        READ_TOGGLE_SC(gOmmExtrasSMOAnimations);
                        READ_TOGGLE_SC(gOmmExtrasCappyAndTiara);
                        READ_TOGGLE_SC(gOmmExtrasColoredStars);
                        READ_TOGGLE_SC(gOmmExtrasVanishingHUD);
                        READ_TOGGLE_SC(gOmmExtrasRevealSecrets);
                        READ_TOGGLE_SC(gOmmExtrasRedCoinsRadar);
                        READ_TOGGLE_SC(gOmmExtrasShowStarNumber);
                        READ_TOGGLE_SC(gOmmExtrasInvisibleMode);
                        READ_CHOICE_SC(gOmmExtrasSparklyStarsReward);
#if OMM_CODE_DEBUG
                        READ_TOGGLE_SC(gOmmDebugHitbox);
                        READ_TOGGLE_SC(gOmmDebugHurtbox);
                        READ_TOGGLE_SC(gOmmDebugWallbox);
                        READ_TOGGLE_SC(gOmmDebugSurface);
                        READ_TOGGLE_SC(gOmmDebugMario);
                        READ_TOGGLE_SC(gOmmDebugCappy);
#endif
#if OMM_CODE_DEV
#include "data/omm/dev/omm_dev_opt_read.inl"
#endif
                
                        // Sparkly Stars data
                        if (omm_sparkly_read(array_of(const char*) { token.args[0], token.args[1] })) {
                            continue;
                        }

                        // Mario colors
                        if (omm_mario_colors_read(array_of(const char*) { token.args[0], token.args[1] })) {
                            continue;
                        }

                        // Stats data
                        if (gOmmData->readStats(array_of(const char*) { token.args[0], token.args[1], token.args[2] })) {
                            continue;
                        }

                        // Collectibles
                        if (strcmp(token.args[0], "collectibles") == 0) {
                            if (saveBuffer->collectibles) {
                                saveFile->collectibles = 0;
                                s32 numFields = strlen(saveBuffer->collectibles) / 5;
                                for (s32 i = 0; i != numFields; ++i) {
                                    char fieldType = saveBuffer->collectibles[5 * i + 0];
                                    s32 fieldStart = (((saveBuffer->collectibles[5 * i + 1] - '0') * 10) + (saveBuffer->collectibles[5 * i + 2] - '0'));
                                    s32 fieldLength = (((saveBuffer->collectibles[5 * i + 3] - '0') * 10) + (saveBuffer->collectibles[5 * i + 4] - '0'));
                                    const char *field = token.args[1 + i];
                                    switch (fieldType) {
                                        case 'i': {
                                            u64 x = 0;
                                            sscanf(field, "%llu", &x);
                                            OMM_COLLECTIBLE_SET_I(saveFile->collectibles, fieldStart, fieldLength, x);
                                        } break;

                                        case 'b': {
                                            for (s32 j = 0; j != fieldLength; ++j) {
                                                OMM_COLLECTIBLE_SET_B(saveFile->collectibles, fieldStart, j, field[j] == '1');
                                            }
                                        } break;
                                    }
                                }
                            }
                        }

                        // Flag
                        for (s32 i = 0; i != OMM_FLAGS_COUNT; ++i) {
                            if (strcmp(sOmmFlags[i].name, token.args[0]) == 0) {
                                saveFile->flags |= ((token.args[1][0] == '1') ? sOmmFlags[i].value : 0);
                                break;
                            }
                        }

                        // Course
                        for (s32 i = 0; i != OMM_COURSES_COUNT; ++i) {
                            if (strcmp(sOmmCourses[i].name, token.args[0]) == 0) {
                                s32 courseIndex = sOmmCourses[i].value - 1;

                                // Stars
                                for (s32 b = 0; b != saveBuffer->starsPerCourse; ++b) {
                                    saveFile->courses[courseIndex].stars |= ((token.args[1][b] == '1') << b);
                                }

                                // Cannon
                                saveFile->courses[courseIndex].cannon = (token.args[2][0] == '1');

                                // Score
                                s32 coinScore = 0;
                                sscanf(token.args[3], "%d", &coinScore);
                                saveFile->courses[courseIndex].score = clamp_s(coinScore, 0, 999);
                                break;
                            }
                        }

                        // Last course visited
                        if (strcmp("last", token.args[0]) == 0) {
                            sscanf(token.args[1], "%d", &saveFile->lastCourse);
                            saveFile->lastCourse = clamp_s(saveFile->lastCourse, COURSE_NONE, COURSE_CAKE_END) % COURSE_CAKE_END;
                        }
                    }
                }
            }
            fclose(f);
            controller_reconfigure();
        }

        // Something went wrong... load the backup!
        if (!loaded && !sOmmSaveFileBackup) {
            sOmmSaveFileBackup = true;
            omm_save_file_load_all();
        }
        
        // Everything is ok
        else {

            // The backup was loaded, regenerate the save file
            if (loaded && sOmmSaveFileBackup) {
                str_cat_paths_sa(backupFilename, 256, sys_user_path(), OMM_SAVEFILE_BACKUP);
                FILE *backup = fopen(backupFilename, "rb");
                if (backup) {
                    str_cat_paths_sa(saveFilename, 256, sys_user_path(), OMM_SAVEFILE_NAME);
                    FILE *save = fopen(saveFilename, "wb");
                    if (save) {
                        char buffer[0x1000]; s32 length = 0;
                        while ((length = (s32) fread(buffer, sizeof(char), 0x1000, backup)) != 0) {
                            fwrite(buffer, sizeof(char), length, save);
                        }
                        fclose(save);
                    }
                    fclose(backup);
                }
            }

            // Load complete
            sOmmSaveFileLoaded = true;
            sOmmSaveFileShouldWrite = false;
        }
    }
}

//
// Write
//

static bool omm_save_file_is_empty(const OmmSaveFile *savefile) {
    static const OmmSaveFile sOmmEmptySaveFile = { 0 };
    return mem_eq(savefile, &sOmmEmptySaveFile, sizeof(sOmmEmptySaveFile));
}

#define write(...) { head += sprintf(head, __VA_ARGS__); }
static void omm_save_file_write() {
    char saveFileBuffer[0x10000];
    char *head = saveFileBuffer;

    // Set current save file as existing if not empty (i.e. at least 1 star, 1 collectible or 1 in-game flag)
    s32 fileIndex = (omm_is_main_menu() ? 0 : gCurrSaveFileNum) - 1;
    s32 modeIndex = OMM_GAME_MODE;
    if (fileIndex >= 0 && fileIndex < NUM_SAVE_FILES) {
        if (!omm_save_file_is_empty(sOmmSaveFile)) {
            sOmmSaveFile->flags |= SAVE_FLAG_FILE_EXISTS;
        }
    }

    // Header
    write("# --- Odyssey Mario's Moveset save file ---\n");
    write("# ------------- by PeachyPeach ------------\n");
    write("# All games saves are stored in this file.\n");
    write("# Lines starting with # are comments.\n");
    write("# Games sections:\n");
    write("# - Every game save starts with a pair [xxxx:yy]:\n");
    write("#   - xxxx is the game code name\n");
    write("#   - yy is the save file name\n");
    write("# - Flags:\n");
    write("#   - 0 = locked and 1 = unlocked\n");
    write("# - Courses:\n");
    write("#   - the first 7 digits are the stars (1 = collected, 0 = not)\n");
    write("#   - the next digit is the cannon (1 = open, 0 = close)\n");
    write("#   - the last number is the coin score\n");
    write("# - Collectibles:\n");
    write("#   - Render96-alpha:\n");
    write("#     - the first number is the selected character (0 = Mario, 1 = Luigi, 2 = Wario)\n");
    write("#     - the next 10 digits are the Luigi keys (1 = collected, 0 = not)\n");
    write("#     - the last 6 digits are the Wario coins (1 = collected, 0 = not)\n");
    write("# The [sparkly_stars] and [stats] sections should not be edited manually.\n");
    write("# The [mario_colors] and [peach_colors] sections can be edited in-game with the Palette Editor.\n");
    write("# The [settings] section can be edited in-game with the Options menu.\n");
    write("\n");

    // Versions
    for (s32 s = 0; s != array_length(sOmmSaveBuffers); ++s) {
        const OmmSaveBuffer *saveBuffer = &sOmmSaveBuffers[s];
        for (s32 i = 0; i != NUM_SAVE_FILES; ++i) {
            for (s32 j = 0; j != saveBuffer->modes; ++j) {
                const OmmSaveFile *saveFile = &saveBuffer->files[i][j];
                if (saveFile->flags & SAVE_FLAG_FILE_EXISTS) {

                    // Name
                    write("[%s:%c%d]\n", saveBuffer->name, i + 'A', j);

                    // Flags
                    for (s32 k = 0; k != OMM_FLAGS_COUNT; ++k) {
                        write("%s = %d\n", sOmmFlags[k].name, ((saveFile->flags & sOmmFlags[k].value) != 0));
                    }

                    // Courses
                    for (s32 k = 0; k != OMM_COURSES_COUNT; ++k) {
                        write("%s = ", sOmmCourses[k].name);
                        s32 courseIndex = sOmmCourses[k].value - 1;

                        // Stars
                        for (s32 b = 0; b != saveBuffer->starsPerCourse; ++b) {
                            write("%d", ((saveFile->courses[courseIndex].stars >> b) & 1));
                        }

                        // Cannon
                        write(" %d", (saveFile->courses[courseIndex].cannon != 0));

                        // Score
                        write(" %hd\n", saveFile->courses[courseIndex].score);
                    }

                    // Last course visited
                    write("last = %d\n", saveFile->lastCourse);

                    // Collectibles
                    if (saveBuffer->collectibles) {
                        write("collectibles =");
                        s32 numFields = strlen(saveBuffer->collectibles) / 5;
                        for (s32 i = 0; i != numFields; ++i) {
                            char fieldType = saveBuffer->collectibles[5 * i + 0];
                            s32 fieldStart = (((saveBuffer->collectibles[5 * i + 1] - '0') * 10) + (saveBuffer->collectibles[5 * i + 2] - '0'));
                            s32 fieldLength = (((saveBuffer->collectibles[5 * i + 3] - '0') * 10) + (saveBuffer->collectibles[5 * i + 4] - '0'));
                            switch (fieldType) {
                                case 'i': {
                                    u64 x = OMM_COLLECTIBLE_GET_I(saveFile->collectibles, fieldStart, fieldLength);
                                    write(" %llu", x);
                                } break;

                                case 'b': {
                                    write(" ");
                                    for (s32 j = 0; j != fieldLength; ++j) {
                                        s32 bit = (s32) OMM_COLLECTIBLE_GET_B(saveFile->collectibles, fieldStart, j);
                                        write("%d", bit);
                                    }
                                } break;
                            }
                        }
                        write("\n");
                    }
                    write("\n");
                }
            }
        }
    }

    // Sparkly Stars data
    omm_sparkly_write(&head);

    // Mario colors
    omm_mario_colors_write(&head);

    // Stats data
    gOmmData->writeStats(&head);
        
    // Settings data
    write(OMM_SETTINGS "\n");
    WRITE_KBINDS(gOmmControlsButtonA);
    WRITE_KBINDS(gOmmControlsButtonB);
    WRITE_KBINDS(gOmmControlsButtonX);
    WRITE_KBINDS(gOmmControlsButtonY);
    WRITE_KBINDS(gOmmControlsButtonStart);
    WRITE_KBINDS(gOmmControlsButtonSpin);
    WRITE_KBINDS(gOmmControlsTriggerL);
    WRITE_KBINDS(gOmmControlsTriggerR);
    WRITE_KBINDS(gOmmControlsTriggerZ);
    WRITE_KBINDS(gOmmControlsCUp);
    WRITE_KBINDS(gOmmControlsCDown);
    WRITE_KBINDS(gOmmControlsCLeft);
    WRITE_KBINDS(gOmmControlsCRight);
    WRITE_KBINDS(gOmmControlsDUp);
    WRITE_KBINDS(gOmmControlsDDown);
    WRITE_KBINDS(gOmmControlsDLeft);
    WRITE_KBINDS(gOmmControlsDRight);
    WRITE_KBINDS(gOmmControlsStickUp);
    WRITE_KBINDS(gOmmControlsStickDown);
    WRITE_KBINDS(gOmmControlsStickLeft);
    WRITE_KBINDS(gOmmControlsStickRight);
    WRITE_CHOICE(gOmmFrameRate);
    WRITE_TOGGLE(gOmmShowFPS);
    WRITE_CHOICE(gOmmTextureCaching);
    WRITE_CHOICE_SC(gOmmCharacter);
    WRITE_CHOICE_SC(gOmmMovesetType);
    WRITE_CHOICE_SC(gOmmCapType);
    WRITE_CHOICE_SC(gOmmStarsMode);
    WRITE_CHOICE_SC(gOmmPowerUpsType);
    WRITE_CHOICE_SC(gOmmCameraMode);
    WRITE_CHOICE_SC(gOmmSparklyStarsMode);
    WRITE_CHOICE_SC(gOmmSparklyStarsHintAtLevelEntry);
    WRITE_CHOICE(gOmmExtrasMarioColors);
    WRITE_CHOICE(gOmmExtrasPeachColors);
    WRITE_TOGGLE_SC(gOmmExtrasSMOAnimations);
    WRITE_TOGGLE_SC(gOmmExtrasCappyAndTiara);
    WRITE_TOGGLE_SC(gOmmExtrasColoredStars);
    WRITE_TOGGLE_SC(gOmmExtrasVanishingHUD);
    WRITE_TOGGLE_SC(gOmmExtrasRevealSecrets);
    WRITE_TOGGLE_SC(gOmmExtrasRedCoinsRadar);
    WRITE_TOGGLE_SC(gOmmExtrasShowStarNumber);
    WRITE_TOGGLE_SC(gOmmExtrasInvisibleMode);
    WRITE_CHOICE_SC(gOmmExtrasSparklyStarsReward);
#if OMM_CODE_DEBUG
    WRITE_TOGGLE_SC(gOmmDebugHitbox);
    WRITE_TOGGLE_SC(gOmmDebugHurtbox);
    WRITE_TOGGLE_SC(gOmmDebugWallbox);
    WRITE_TOGGLE_SC(gOmmDebugSurface);
    WRITE_TOGGLE_SC(gOmmDebugMario);
    WRITE_TOGGLE_SC(gOmmDebugCappy);
#endif
#if OMM_CODE_DEV
#include "data/omm/dev/omm_dev_opt_write.inl"
#endif
    write("\n");

    // Create a back-up
    str_cat_paths_sa(backupFilename, 256, sys_user_path(), OMM_SAVEFILE_BACKUP);
    FILE *backup = fopen(backupFilename, "wb");
    if (backup) {
        fwrite(saveFileBuffer, sizeof(char), head - saveFileBuffer, backup);
        fclose(backup);
    }

    // Write the save file
    str_cat_paths_sa(saveFilename, 256, sys_user_path(), OMM_SAVEFILE_NAME);
    FILE *save = fopen(saveFilename, "wb");
    if (save) {
        fwrite(saveFileBuffer, sizeof(char), head - saveFileBuffer, save);
        fclose(save);
    }

    // Done
    controller_reconfigure();
    sOmmSaveFileShouldWrite = false;
}
#undef write

void omm_save_file_do_save() {
    sOmmSaveFileShouldWrite = true;
}

//
// Copy
//

void omm_save_file_copy(s32 fileIndex, s32 modeIndex, s32 destIndex) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_FILE_INDEX(destIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    mem_cpy(&sOmmSaveBuffer->files[destIndex][modeIndex], &sOmmSaveBuffer->files[fileIndex][modeIndex], sizeof(sOmmSaveBuffer->files[fileIndex][modeIndex]));
    omm_save_file_do_save();
}

//
// Erase
//

void omm_save_file_erase(s32 fileIndex, s32 modeIndex) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    mem_clr(sOmmSaveFile, sizeof(*sOmmSaveFile));
    omm_save_file_do_save();
}

//
// Get
//

bool omm_save_file_exists(s32 fileIndex, s32 modeIndex) {
    CHECK_FILE_INDEX(fileIndex, return false);
    CHECK_MODE_INDEX(modeIndex, return false);
    return (sOmmSaveFile->flags & SAVE_FLAG_FILE_EXISTS) != 0;
}

u32 omm_save_file_get_flags(s32 fileIndex, s32 modeIndex) {
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    if (gCurrCreditsEntry || gCurrDemoInput) return 0;
    return sOmmSaveFile->flags;
}

u32 omm_save_file_get_star_flags(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    CHECK_COURSE_INDEX(courseIndex, return 0);
    return sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].stars;
}

u32 omm_save_file_get_cannon_unlocked(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    CHECK_COURSE_INDEX(courseIndex, return 0);
    return sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].cannon;
}

s32 omm_save_file_get_course_star_count(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    s32 count = 0;
    u32 starFlags = omm_save_file_get_star_flags(fileIndex, modeIndex, courseIndex);
    for (s32 i = 0; i != sOmmSaveBuffer->starsPerCourse; ++i) {
        count += ((starFlags >> i) & 1);
    }
    return count;
}

s32 omm_save_file_get_course_coin_score(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    CHECK_COURSE_INDEX(courseIndex, return 0);
    return sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].score;
}

s32 omm_save_file_get_total_star_count(s32 fileIndex, s32 modeIndex, s32 courseIndexMin, s32 courseIndexMax) {
    s32 count = 0;
    for (s32 courseIndex = courseIndexMin; courseIndex <= courseIndexMax; ++courseIndex) {
        count += omm_save_file_get_course_star_count(fileIndex, modeIndex, courseIndex);
    }
    if (courseIndexMax < COURSE_CASTLE) {
        count += omm_save_file_get_course_star_count(fileIndex, modeIndex, -1);
    }
    return count;
}

s32 omm_save_file_get_total_coin_score(s32 fileIndex, s32 modeIndex, s32 courseIndexMin, s32 courseIndexMax) {
    s32 count = 0;
    for (s32 courseIndex = courseIndexMin; courseIndex <= courseIndexMax; ++courseIndex) {
        count += omm_save_file_get_course_coin_score(fileIndex, modeIndex, courseIndex);
    }
    return count;
}

s32 omm_save_file_get_last_course(s32 fileIndex, s32 modeIndex) {
    CHECK_FILE_INDEX(fileIndex, return COURSE_NONE);
    CHECK_MODE_INDEX(modeIndex, return COURSE_NONE);
    return omm_save_file_exists(fileIndex, modeIndex) ? sOmmSaveFile->lastCourse : COURSE_NONE;
}

u32 omm_save_file_get_taken_luigi_key(s32 fileIndex, s32 modeIndex, s32 keyIndex) {
#if OMM_GAME_IS_R96X
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    return OMM_COLLECTIBLE_GET_B(sOmmSaveFile->collectibles, 0, keyIndex);
#else
    return 0;
#endif
}

u32 omm_save_file_get_taken_wario_coin(s32 fileIndex, s32 modeIndex, s32 coinIndex) {
#if OMM_GAME_IS_R96X
    CHECK_FILE_INDEX(fileIndex, return 0);
    CHECK_MODE_INDEX(modeIndex, return 0);
    return OMM_COLLECTIBLE_GET_B(sOmmSaveFile->collectibles, NUM_KEYS, coinIndex);
#else
    return 0;
#endif
}

s32 omm_save_file_get_luigi_keys_count(s32 fileIndex, s32 modeIndex) {
#if OMM_GAME_IS_R96X
    s32 count = 0;
    for (s32 keyIndex = 0; keyIndex != NUM_KEYS; ++keyIndex) {
        count += omm_save_file_get_taken_luigi_key(fileIndex, modeIndex, keyIndex);
    }
    return count;
#else
    return 0;
#endif
}

s32 omm_save_file_get_wario_coins_count(s32 fileIndex, s32 modeIndex) {
#if OMM_GAME_IS_R96X
    s32 count = 0;
    for (s32 coinIndex = 0; coinIndex != NUM_WARIO_COINS; ++coinIndex) {
        count += omm_save_file_get_taken_wario_coin(fileIndex, modeIndex, coinIndex);
    }
    return count;
#else
    return 0;
#endif
}

//
// Set
//

void omm_save_file_set_flags(s32 fileIndex, s32 modeIndex, u32 flags) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    sOmmSaveFile->flags |= flags;
    omm_save_file_set_last_course(fileIndex, modeIndex, gCurrCourseNum - 1);
    omm_save_file_do_save();
}

void omm_save_file_set_star_flags(s32 fileIndex, s32 modeIndex, s32 courseIndex, u32 stars) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    CHECK_COURSE_INDEX(courseIndex, return);
    sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].stars |= stars;
    omm_save_file_set_last_course(fileIndex, modeIndex, courseIndex);
    omm_save_file_do_save();
}

void omm_save_file_set_cannon_unlocked(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    CHECK_COURSE_INDEX(courseIndex, return);
    sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].cannon = true;
    omm_save_file_set_last_course(fileIndex, modeIndex, courseIndex);
    omm_save_file_do_save();
}

void omm_save_file_set_last_course(s32 fileIndex, s32 modeIndex, s32 courseIndex) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    CHECK_COURSE_INDEX(courseIndex, return);
    if (!omm_is_main_menu()) {
        sOmmSaveFile->lastCourse = courseIndex + 1;
    }
}

void omm_save_file_set_taken_luigi_key(s32 fileIndex, s32 modeIndex, s32 keyIndex) {
#if OMM_GAME_IS_R96X
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    OMM_COLLECTIBLE_SET_B(sOmmSaveFile->collectibles, 0, keyIndex, 1);
    omm_save_file_set_last_course(fileIndex, modeIndex, gCurrCourseNum - 1);
    omm_save_file_do_save();
#endif
}

void omm_save_file_set_taken_wario_coin(s32 fileIndex, s32 modeIndex, s32 coinIndex) {
#if OMM_GAME_IS_R96X
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    OMM_COLLECTIBLE_SET_B(sOmmSaveFile->collectibles, NUM_KEYS, coinIndex, 1);
    omm_save_file_set_last_course(fileIndex, modeIndex, gCurrCourseNum - 1);
    omm_save_file_do_save();
#endif
}

void omm_save_file_collect_star_or_key(s32 fileIndex, s32 modeIndex, s32 levelIndex, s32 starIndex, s32 coins) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    s32 courseIndex = omm_level_get_course(levelIndex + 1) - 1;
    CHECK_COURSE_INDEX(courseIndex, return);

    // Update globals
    gLastCompletedLevelNum = OMM_BOWSER_IN_THE_LEVEL(levelIndex + 1);
    gLastCompletedStarNum = starIndex + 1;
    gGotFileCoinHiScore = 0;

    // Star flag
    s32 starFlag = (1 << starIndex);
    omm_stars_set_bits(starFlag);
    switch (levelIndex + 1) {
        case LEVEL_BOWSER_1: {
            omm_save_file_set_flags(fileIndex, modeIndex, SAVE_FLAG_HAVE_KEY_1);
            omm_speedrun_split(OMM_SPLIT_BOWSER);
        } break;

        case LEVEL_BOWSER_2: {
            omm_save_file_set_flags(fileIndex, modeIndex, SAVE_FLAG_HAVE_KEY_2);
            omm_speedrun_split(OMM_SPLIT_BOWSER);
        } break;

        case LEVEL_BOWSER_3: {
            omm_save_file_set_flags(fileIndex, modeIndex, 0);
            omm_speedrun_split(OMM_SPLIT_BOWSER);
            gOmmStats->starsCollected++;
        } break;

        default: {
            omm_save_file_set_star_flags(fileIndex, modeIndex, courseIndex, starFlag);
            omm_speedrun_split(OMM_SPLIT_STAR);
            gOmmStats->starsCollected++;
        } break;
    }

    // New high score
    if (coins > omm_save_file_get_course_coin_score(fileIndex, modeIndex, courseIndex)) {
        sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].score = coins;
        gGotFileCoinHiScore = 1;
    }
}

void omm_save_file_clear_flags(s32 fileIndex, s32 modeIndex, u32 flags) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    sOmmSaveFile->flags &= ~flags;
    sOmmSaveFile->flags |= SAVE_FLAG_FILE_EXISTS;
    omm_save_file_set_last_course(fileIndex, modeIndex, gCurrCourseNum - 1);
    omm_save_file_do_save();
}

void omm_save_file_clear_star_flags(s32 fileIndex, s32 modeIndex, s32 courseIndex, u32 stars) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_MODE_INDEX(modeIndex, return);
    CHECK_COURSE_INDEX(courseIndex, return);
    sOmmSaveFile->courses[COURSE_INDEX_INCLUDING_CASTLE].stars &= ~stars;
    omm_save_file_set_last_course(fileIndex, modeIndex, courseIndex);
    omm_save_file_do_save();
}

//
// Checkpoint
//

static struct { s32 levelNum, courseNum, actNum, areaIndex, warpNode; } sWarpCheckpoint;

bool warp_checkpoint_check(struct WarpNode *warpNode, s32 actIndex) {
    if (warpNode) {
        s32 destCourseNum = omm_level_get_course(warpNode->destLevel & 0x7F);
        if (sWarpCheckpoint.courseNum != COURSE_NONE &&
            sWarpCheckpoint.actNum == actIndex + 1 &&
            gSavedCourseNum == destCourseNum) {
            warpNode->destLevel = sWarpCheckpoint.levelNum;
            warpNode->destArea = sWarpCheckpoint.areaIndex;
            warpNode->destNode = sWarpCheckpoint.warpNode;
            return true;
        }
    }
    sWarpCheckpoint.courseNum = COURSE_NONE;
    return false;
}

void warp_checkpoint_check_if_should_set(struct WarpNode *warpNode, s32 courseIndex, s32 actIndex) {
#if OMM_GAME_IS_SM64
    if (gOmmSparkly->cheatDetected && gOmmSparklyCheats->counter >= 3) {
        initiate_warp(LEVEL_CASTLE, 4, 0x0A + (random_u16() % 4), sDelayedWarpArg);
        return;
    }
#endif
    if (warpNode->destLevel & 0x80) {
        sWarpCheckpoint.levelNum = (warpNode->destLevel & 0x7F);
        sWarpCheckpoint.courseNum = courseIndex + 1;
        sWarpCheckpoint.actNum = actIndex + 1;
        sWarpCheckpoint.areaIndex = warpNode->destArea;
        sWarpCheckpoint.warpNode = warpNode->destNode;
    }
}

void warp_checkpoint_disable() {
    sWarpCheckpoint.courseNum = COURSE_NONE;
}

//
// Auto-save
//

OMM_ROUTINE_UPDATE(omm_save_file_auto_save) {
    if (sOmmSaveFileShouldWrite) {
        omm_save_file_write();
    }
}

//
// Complete save cheat
//

void omm_set_complete_save_file(s32 fileIndex, s32 modeIndex) {
    CHECK_FILE_INDEX(fileIndex, return);
    CHECK_FILE_INDEX(modeIndex, return);

    // Flags
    omm_save_file_set_flags(fileIndex, modeIndex, SAVE_FLAG_ALL_FLAGS);

    // Stars
    for (s32 levelNum = LEVEL_MIN; levelNum <= LEVEL_MAX; ++levelNum) {
        if (levelNum != LEVEL_ENDING) {
            omm_save_file_set_star_flags(fileIndex, modeIndex, omm_level_get_course(levelNum) - 1, omm_stars_get_bits_total(levelNum, modeIndex));
        }
    }

    // Cannons
    for (s32 courseNum = COURSE_MIN; courseNum != COURSE_MAX; courseNum++) {
        omm_save_file_set_cannon_unlocked(fileIndex, modeIndex, courseNum - 1);
    }

    // Collectibles
#if OMM_GAME_IS_R96X
    for (s32 keyIndex = 0; keyIndex != NUM_KEYS; ++keyIndex) {
        omm_save_file_set_taken_luigi_key(fileIndex, modeIndex, keyIndex);
    }
    for (s32 coinIndex = 0; coinIndex != NUM_WARIO_COINS; ++coinIndex) {
        omm_save_file_set_taken_wario_coin(fileIndex, modeIndex, coinIndex);
    }
#endif

    // Unlock Peach
    // That's the "unlock Peach" cheat code that only works with OMM save files :)
    if (!omm_player_is_unlocked(OMM_PLAYER_PEACH)) {
        omm_sparkly_read(array_of(const char *) { "sparkly_stars", "GfXjKVYgaaJtJcEoXos7yYX2qxnlWT7u6suhEpchlobJxzcvvTeTVoJVNz4" });
    }
}

#pragma GCC pop_options
