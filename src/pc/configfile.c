// configfile.c - handles loading and saving the configuration options
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>

#include "platform.h"
#include "configfile.h"
#include "cliopts.h"
#include "gfx/gfx_screen_config.h"
#include "gfx/gfx_window_manager_api.h"
#include "controller/controller_api.h"
#include "fs/fs.h"

#ifdef TOUCH_CONTROLS
#include "pc/controller/controller_touchscreen.h"
#endif

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

enum ConfigOptionType {
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_UINT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BIND,
};

struct ConfigOption {
    const char *name;
    enum ConfigOptionType type;
    union {
        bool *boolValue;
        unsigned int *uintValue;
        float *floatValue;
    };
};

/*
 *Config options and default values
 */

// Video/audio stuff
ConfigWindow configWindow       = {
    .x = WAPI_WIN_CENTERPOS,
    .y = WAPI_WIN_CENTERPOS,
    .w = DESIRED_SCREEN_WIDTH,
    .h = DESIRED_SCREEN_HEIGHT,
    .vsync = 1,
    .reset = false,
    .fullscreen = true,
    .exiting_fullscreen = false,
    .settings_changed = false,
};
unsigned int configFiltering    = 1;          // 0=force nearest, 1=linear, (TODO) 2=three-point
unsigned int configMasterVolume = MAX_VOLUME; // 0 - MAX_VOLUME
unsigned int configMusicVolume = MAX_VOLUME;
unsigned int configSfxVolume = MAX_VOLUME;
unsigned int configEnvVolume = MAX_VOLUME;

// Keyboard mappings (VK_ values, by default keyboard/gamepad/mouse)
unsigned int configKeyA[MAX_BINDS]          = { 0x0026,   0x1000,     VK_INVALID };
unsigned int configKeyB[MAX_BINDS]          = { 0x0033,   0x1002,     VK_INVALID };
unsigned int configKeyStart[MAX_BINDS]      = { 0x0039,   0x1006,     VK_INVALID };
unsigned int configKeyL[MAX_BINDS]          = { 0x002A,   0x1009,     VK_INVALID };
unsigned int configKeyR[MAX_BINDS]          = { 0x0036,   0x100A,     VK_INVALID };
unsigned int configKeyZ[MAX_BINDS]          = { 0x0025,   0x101A,     VK_INVALID };
unsigned int configKeyCUp[MAX_BINDS]        = { 0x0148,   VK_INVALID, VK_INVALID };
unsigned int configKeyCDown[MAX_BINDS]      = { 0x0150,   VK_INVALID, VK_INVALID };
unsigned int configKeyCLeft[MAX_BINDS]      = { 0x014B,   VK_INVALID, VK_INVALID };
unsigned int configKeyCRight[MAX_BINDS]     = { 0x014D,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickUp[MAX_BINDS]    = { 0x0011,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickDown[MAX_BINDS]  = { 0x001F,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickLeft[MAX_BINDS]  = { 0x001E,   VK_INVALID, VK_INVALID };
unsigned int configKeyStickRight[MAX_BINDS] = { 0x0020,   VK_INVALID, VK_INVALID };
unsigned int configStickDeadzone = 16; // 16*DEADZONE_STEP=4960 (the original default deadzone)
unsigned int configRumbleStrength = 50;
#ifdef EXTERNAL_DATA
bool configPrecacheRes = true;
#endif
#ifdef BETTERCAMERA
// BetterCamera settings
unsigned int configCameraXSens   = 50;
unsigned int configCameraYSens   = 50;
unsigned int configCameraAggr    = 0;
unsigned int configCameraPan     = 0;
unsigned int configCameraDegrade = 100; // 0 - 100%
bool         configCameraInvertX = true;
bool         configCameraInvertY = false;
bool         configEnableCamera  = true;
bool         configCameraAnalog  = true;
bool         configCameraMouse   = true;
#endif
bool         configSkipIntro     = 0;
bool         configHUD           = true;
#ifdef DISCORDRPC
bool         configDiscordRPC    = true;
#endif

static const struct ConfigOption options[] = {
    {.name = "fullscreen",           .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.fullscreen},
    {.name = "window_x",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.x},
    {.name = "window_y",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.y},
    {.name = "window_w",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.w},
    {.name = "window_h",             .type = CONFIG_TYPE_UINT, .uintValue = &configWindow.h},
    {.name = "vsync",                .type = CONFIG_TYPE_BOOL, .boolValue = &configWindow.vsync},
    {.name = "texture_filtering",    .type = CONFIG_TYPE_UINT, .uintValue = &configFiltering},
    {.name = "master_volume",        .type = CONFIG_TYPE_UINT, .uintValue = &configMasterVolume},
    {.name = "music_volume",         .type = CONFIG_TYPE_UINT, .uintValue = &configMusicVolume},
    {.name = "sfx_volume",           .type = CONFIG_TYPE_UINT, .uintValue = &configSfxVolume},
    {.name = "env_volume",           .type = CONFIG_TYPE_UINT, .uintValue = &configEnvVolume},
    {.name = "key_a",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyA},
    {.name = "key_b",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyB},
    {.name = "key_start",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyStart},
    {.name = "key_l",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyL},
    {.name = "key_r",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyR},
    {.name = "key_z",                .type = CONFIG_TYPE_BIND, .uintValue = configKeyZ},
    {.name = "key_cup",              .type = CONFIG_TYPE_BIND, .uintValue = configKeyCUp},
    {.name = "key_cdown",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyCDown},
    {.name = "key_cleft",            .type = CONFIG_TYPE_BIND, .uintValue = configKeyCLeft},
    {.name = "key_cright",           .type = CONFIG_TYPE_BIND, .uintValue = configKeyCRight},
    {.name = "key_stickup",          .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickUp},
    {.name = "key_stickdown",        .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickDown},
    {.name = "key_stickleft",        .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickLeft},
    {.name = "key_stickright",       .type = CONFIG_TYPE_BIND, .uintValue = configKeyStickRight},
    {.name = "stick_deadzone",       .type = CONFIG_TYPE_UINT, .uintValue = &configStickDeadzone},
    {.name = "rumble_strength",      .type = CONFIG_TYPE_UINT, .uintValue = &configRumbleStrength},
    #ifdef EXTERNAL_DATA
    {.name = "precache",             .type = CONFIG_TYPE_BOOL, .boolValue = &configPrecacheRes},
    #endif
    #ifdef BETTERCAMERA
    {.name = "bettercam_enable",     .type = CONFIG_TYPE_BOOL, .boolValue = &configEnableCamera},
    {.name = "bettercam_analog",     .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraAnalog},
    {.name = "bettercam_mouse_look", .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraMouse},
    {.name = "bettercam_invertx",    .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertX},
    {.name = "bettercam_inverty",    .type = CONFIG_TYPE_BOOL, .boolValue = &configCameraInvertY},
    {.name = "bettercam_xsens",      .type = CONFIG_TYPE_UINT, .uintValue = &configCameraXSens},
    {.name = "bettercam_ysens",      .type = CONFIG_TYPE_UINT, .uintValue = &configCameraYSens},
    {.name = "bettercam_aggression", .type = CONFIG_TYPE_UINT, .uintValue = &configCameraAggr},
    {.name = "bettercam_pan_level",  .type = CONFIG_TYPE_UINT, .uintValue = &configCameraPan},
    {.name = "bettercam_degrade",    .type = CONFIG_TYPE_UINT, .uintValue = &configCameraDegrade},
    #endif
    {.name = "skip_intro",           .type = CONFIG_TYPE_BOOL, .boolValue = &configSkipIntro},
    #ifdef DISCORDRPC
    {.name = "discordrpc_enable",    .type = CONFIG_TYPE_BOOL, .boolValue = &configDiscordRPC},
    #endif 
#ifdef TOUCH_CONTROLS
    {.name = "touch_stick_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_STICK].x},
    {.name = "touch_stick_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_STICK].y},
    {.name = "touch_stick_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_STICK].size},
    {.name = "touch_stick_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_STICK].anchor},
    {.name = "touch_mouse_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_MOUSE].x},
    {.name = "touch_mouse_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_MOUSE].y},
    {.name = "touch_mouse_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_MOUSE].size},
    {.name = "touch_mouse_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_MOUSE].anchor},
    {.name = "touch_a_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_A].x},
    {.name = "touch_a_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_A].y},
    {.name = "touch_a_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_A].size},
    {.name = "touch_a_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_A].anchor},
    {.name = "touch_b_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_B].x},
    {.name = "touch_b_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_B].y},
    {.name = "touch_b_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_B].size},
    {.name = "touch_b_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_B].anchor},
    {.name = "touch_x_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_X].x},
    {.name = "touch_x_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_X].y},
    {.name = "touch_x_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_X].size},
    {.name = "touch_x_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_X].anchor},
    {.name = "touch_y_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Y].x},
    {.name = "touch_y_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Y].y},
    {.name = "touch_y_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Y].size},
    {.name = "touch_y_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Y].anchor},
    {.name = "touch_start_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_START].x},
    {.name = "touch_start_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_START].y},
    {.name = "touch_start_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_START].size},
    {.name = "touch_start_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_START].anchor},
    {.name = "touch_l_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_L].x},
    {.name = "touch_l_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_L].y},
    {.name = "touch_l_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_L].size},
    {.name = "touch_l_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_L].anchor},
    {.name = "touch_r_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_R].x},
    {.name = "touch_r_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_R].y},
    {.name = "touch_r_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_R].size},
    {.name = "touch_r_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_R].anchor},
    {.name = "touch_z_x",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Z].x},
    {.name = "touch_z_y",                .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Z].y},
    {.name = "touch_z_size",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Z].size},
    {.name = "touch_z_anchor",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_Z].anchor},
    {.name = "touch_cup_x",              .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CUP].x},
    {.name = "touch_cup_y",              .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CUP].y},
    {.name = "touch_cup_size",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CUP].size},
    {.name = "touch_cup_anchor",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CUP].anchor},
    {.name = "touch_cdown_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CDOWN].x},
    {.name = "touch_cdown_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CDOWN].y},
    {.name = "touch_cdown_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CDOWN].size},
    {.name = "touch_cdown_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CDOWN].anchor},
    {.name = "touch_cleft_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CLEFT].x},
    {.name = "touch_cleft_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CLEFT].y},
    {.name = "touch_cleft_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CLEFT].size},
    {.name = "touch_cleft_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CLEFT].anchor},
    {.name = "touch_cright_x",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CRIGHT].x},
    {.name = "touch_cright_y",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CRIGHT].y},
    {.name = "touch_cright_size",        .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CRIGHT].size},
    {.name = "touch_cright_anchor",      .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_CRIGHT].anchor},
    {.name = "touch_dup_x",              .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DUP].x},
    {.name = "touch_dup_y",              .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DUP].y},
    {.name = "touch_dup_size",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DUP].size},
    {.name = "touch_dup_anchor",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DUP].anchor},
    {.name = "touch_ddown_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DDOWN].x},
    {.name = "touch_ddown_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DDOWN].y},
    {.name = "touch_ddown_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DDOWN].size},
    {.name = "touch_ddown_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DDOWN].anchor},
    {.name = "touch_dleft_x",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DLEFT].x},
    {.name = "touch_dleft_y",            .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DLEFT].y},
    {.name = "touch_dleft_size",         .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DLEFT].size},
    {.name = "touch_dleft_anchor",       .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DLEFT].anchor},
    {.name = "touch_dright_x",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DRIGHT].x},
    {.name = "touch_dright_y",           .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DRIGHT].y},
    {.name = "touch_dright_size",        .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DRIGHT].size},
    {.name = "touch_dright_anchor",      .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_DRIGHT].anchor},
    {.name = "touch_spin_x",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_SPIN].x},
    {.name = "touch_spin_y",             .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_SPIN].y},
    {.name = "touch_spin_size",          .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_SPIN].size},
    {.name = "touch_spin_anchor",        .type = CONFIG_TYPE_BIND, .uintValue = configControlElements[TOUCH_SPIN].anchor},
    {.name = "touch_autohide",           .type = CONFIG_TYPE_BOOL, .boolValue = &configAutohideTouch},
    {.name = "touch_slide",              .type = CONFIG_TYPE_BOOL, .boolValue = &configSlideTouch},
    {.name = "touch_snap",               .type = CONFIG_TYPE_BOOL, .boolValue = &configElementSnap},
    {.name = "render_cappy",             .type = CONFIG_TYPE_BOOL, .boolValue = &configRenderCappy},
#endif
};

// Reads an entire line from a file (excluding the newline character) and returns an allocated string
// Returns NULL if no lines could be read from the file
static char *read_file_line(fs_file_t *file) {
    char *buffer;
    size_t bufferSize = 8;
    size_t offset = 0; // offset in buffer to write

    buffer = malloc(bufferSize);
    while (1) {
        // Read a line from the file
        if (fs_readline(file, buffer + offset, bufferSize - offset) == NULL) {
            free(buffer);
            return NULL; // Nothing could be read.
        }
        offset = strlen(buffer);
        assert(offset > 0);

        // If a newline was found, remove the trailing newline and exit
        if (buffer[offset - 1] == '\n') {
            buffer[offset - 1] = '\0';
            break;
        }

        if (fs_eof(file)) // EOF was reached
            break;

        // If no newline or EOF was reached, then the whole line wasn't read.
        bufferSize *= 2; // Increase buffer size
        buffer = realloc(buffer, bufferSize);
        assert(buffer != NULL);
    }

    return buffer;
}

// Returns the position of the first non-whitespace character
static char *skip_whitespace(char *str) {
    while (isspace(*str))
        str++;
    return str;
}

// NULL-terminates the current whitespace-delimited word, and returns a pointer to the next word
static char *word_split(char *str) {
    // Precondition: str must not point to whitespace
    assert(!isspace(*str));

    // Find either the next whitespace char or end of string
    while (!isspace(*str) && *str != '\0')
        str++;
    if (*str == '\0') // End of string
        return str;

    // Terminate current word
    *(str++) = '\0';

    // Skip whitespace to next word
    return skip_whitespace(str);
}

// Splits a string into words, and stores the words into the 'tokens' array
// 'maxTokens' is the length of the 'tokens' array
// Returns the number of tokens parsed
static unsigned int tokenize_string(char *str, int maxTokens, char **tokens) {
    int count = 0;

    str = skip_whitespace(str);
    while (str[0] != '\0' && count < maxTokens) {
        tokens[count] = str;
        str = word_split(str);
        count++;
    }
    return count;
}

// Gets the config file path and caches it
const char *configfile_name(void) {
    return (gCLIOpts.ConfigFile[0]) ? gCLIOpts.ConfigFile : CONFIGFILE_DEFAULT;
}

// Loads the config file specified by 'filename'
void configfile_load(const char *filename) {
    fs_file_t *file;
    char *line;

    printf("Loading configuration from '%s'\n", filename);

    file = fs_open(filename);
    if (file == NULL) {
        // Create a new config file and save defaults
        printf("Config file '%s' not found. Creating it.\n", filename);
        configfile_save(filename);
        return;
    }

    // Go through each line in the file
    while ((line = read_file_line(file)) != NULL) {
        char *p = line;
        char *tokens[1 + MAX_BINDS];
        int numTokens;

        while (isspace(*p))
            p++;

        if (!*p || *p == '#') // comment or empty line
            continue;

        numTokens = tokenize_string(p, sizeof(tokens) / sizeof(tokens[0]), tokens);
        if (numTokens != 0) {
            if (numTokens >= 2) {
                const struct ConfigOption *option = NULL;

                for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
                    if (strcmp(tokens[0], options[i].name) == 0) {
                        option = &options[i];
                        break;
                    }
                }
                if (option == NULL)
                    printf("unknown option '%s'\n", tokens[0]);
                else {
                    switch (option->type) {
                        case CONFIG_TYPE_BOOL:
                            if (strcmp(tokens[1], "true") == 0)
                                *option->boolValue = true;
                            else
                                *option->boolValue = false;
                            break;
                        case CONFIG_TYPE_UINT:
                            sscanf(tokens[1], "%u", option->uintValue);
                            break;
                        case CONFIG_TYPE_BIND:
                            for (int i = 0; i < MAX_BINDS && i < numTokens - 1; ++i)
                                sscanf(tokens[i + 1], "%x", option->uintValue + i);
                            break;
                        case CONFIG_TYPE_FLOAT:
                            sscanf(tokens[1], "%f", option->floatValue);
                            break;
                        default:
                            assert(0); // bad type
                    }
                    printf("option: '%s', value:", tokens[0]);
                    for (int i = 1; i < numTokens; ++i) printf(" '%s'", tokens[i]);
                    printf("\n");
                }
            } else
                puts("error: expected value");
        }
        free(line);
    }

    fs_close(file);
}

// Writes the config file to 'filename'
void configfile_save(const char *filename) {
    FILE *file;

    printf("Saving configuration to '%s'\n", filename);

    file = fopen(fs_get_write_path(filename), "w");
    if (file == NULL) {
        // error
        return;
    }

    for (unsigned int i = 0; i < ARRAY_LEN(options); i++) {
        const struct ConfigOption *option = &options[i];

        switch (option->type) {
            case CONFIG_TYPE_BOOL:
                fprintf(file, "%s %s\n", option->name, *option->boolValue ? "true" : "false");
                break;
            case CONFIG_TYPE_UINT:
                fprintf(file, "%s %u\n", option->name, *option->uintValue);
                break;
            case CONFIG_TYPE_FLOAT:
                fprintf(file, "%s %f\n", option->name, *option->floatValue);
                break;
            case CONFIG_TYPE_BIND:
                fprintf(file, "%s ", option->name);
                for (int i = 0; i < MAX_BINDS; ++i)
                    fprintf(file, "%04x ", option->uintValue[i]);
                fprintf(file, "\n");
                break;
            default:
                assert(0); // unknown type
        }
    }

    fclose(file);
}
