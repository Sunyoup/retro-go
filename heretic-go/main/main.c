/*
 * heretic-go: Heretic (Raven Software, GPLv2 source release) for retro-go
 *
 * This file is the platform layer that replaces the DOS I_IBM.C / I_SOUND.C:
 * video, input, timer, zone memory and (for now silent) sound.
 */

#include <stdlib.h>
#include <stdarg.h>
#include <doomdef.h>
#include <sounds.h>
#include <soundst.h>
#include <rg_system.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

#define AUDIO_SAMPLE_RATE 22050

static rg_surface_t *update;
static rg_app_t *app;

extern char *wadfiles[];
extern char *basedefault;
extern byte gammatable[5][256];
extern int usegamma;
extern int key_right, key_left, key_up, key_down;
extern int key_fire, key_use, key_invright;

static byte *current_palette;

// Heretic keys sent for each button. Several keys per button are fine.
static const struct {int mask; int *key; int code;} keymap[] = {
    {RG_KEY_UP,     &key_up},
    {RG_KEY_DOWN,   &key_down},
    {RG_KEY_LEFT,   &key_left},
    {RG_KEY_RIGHT,  &key_right},
    {RG_KEY_A,      &key_fire},
    {RG_KEY_A,      NULL, 'y'},           // answer yes in the menus
    {RG_KEY_B,      &key_use},
    {RG_KEY_B,      NULL, KEY_BACKSPACE}, // back in the menus
    {RG_KEY_START,  NULL, KEY_ENTER},     // menu select / use artifact
    {RG_KEY_SELECT, &key_invright},
    {RG_KEY_OPTION, NULL, 9},             // automap (TAB)
};

//
// Video
//

void I_SetPalette(byte *palette)
{
    current_palette = palette;
    for (int i = 0; i < 256; i++)
    {
        int r = gammatable[usegamma][palette[i * 3 + 0]];
        int g = gammatable[usegamma][palette[i * 3 + 1]];
        int b = gammatable[usegamma][palette[i * 3 + 2]];
        uint16_t color = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        update->palette[i] = color << 8 | color >> 8;
    }
}

void I_InitGraphics(void)
{
    // Draw directly into the surface instead of the buffer allocated by V_Init
    screen = update->data;
    I_SetPalette(W_CacheLumpName("PLAYPAL", PU_CACHE));
}

void I_Update(void)
{
    if (UpdateState == I_NOUPDATE)
        return;
    rg_display_submit(update, 0);
    rg_display_sync(true); // Wait for update->data to be released
    UpdateState = I_NOUPDATE;
}

void I_StartFrame(void)
{
}

void I_WaitVBL(int count)
{
    rg_usleep(count * 1000000 / 70);
}

void I_BeginRead(void)
{
}

void I_EndRead(void)
{
}

//
// Timer
//

int I_GetTime(void)
{
    return rg_system_timer() * TICRATE / 1000000;
}

//
// Input
//

void I_StartTic(void)
{
    static int64_t last_time = 0;
    static uint32_t prev_joystick = 0;
    static int menu_delay = 0;
    uint32_t joystick = rg_input_read_gamepad();
    uint32_t changed = prev_joystick ^ joystick;
    event_t event = {0};

    // Short press on MENU opens Heretic's menu, long press opens retro-go's menu
    if (joystick & RG_KEY_MENU)
    {
        if (menu_delay++ == TICRATE / 2)
        {
            rg_gui_game_menu();
            joystick = rg_input_read_gamepad();
            changed = 0;
        }
    }
    else
    {
        if (menu_delay > 0 && menu_delay < TICRATE / 2)
        {
            event.type = ev_keydown;
            event.data1 = KEY_ESCAPE;
            D_PostEvent(&event);
            event.type = ev_keyup;
            D_PostEvent(&event);
        }
        menu_delay = 0;
    }

    if (changed)
    {
        for (int i = 0; i < RG_COUNT(keymap); i++)
        {
            if (changed & keymap[i].mask)
            {
                event.type = (joystick & keymap[i].mask) ? ev_keydown : ev_keyup;
                event.data1 = keymap[i].key ? *keymap[i].key : keymap[i].code;
                D_PostEvent(&event);
            }
        }
    }

    rg_system_tick(rg_system_timer() - last_time);
    last_time = rg_system_timer();
    prev_joystick = joystick;
}

//
// System
//

void I_Init(void)
{
    S_Init();
    S_Start();
}

byte *I_ZoneBase(int *size)
{
    // Leave some memory for retro-go, the WAD directory and the lump cache table
    size_t heap = 6 * 1024 * 1024;
#ifdef ESP_PLATFORM
    size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    if (largest < heap + 512 * 1024)
        heap = largest - 512 * 1024;
#endif
    byte *ptr = NULL;
    while (!ptr && heap >= 0x180000)
    {
        ptr = malloc(heap);
        if (!ptr)
            heap -= 0x10000;
    }
    if (!ptr)
        I_Error("Insufficient memory for the zone!");
    RG_LOGI("Zone: %d bytes allocated\n", (int)heap);
    *size = heap;
    return ptr;
}

void I_Error(char *error, ...)
{
    char buffer[256];
    va_list argptr;
    va_start(argptr, error);
    vsnprintf(buffer, sizeof(buffer), error, argptr);
    va_end(argptr);
    RG_PANIC(buffer);
}

void I_Quit(void)
{
    M_SaveDefaults();
    rg_system_exit();
}

//
// Sound (silent for now)
//

void I_StartupSound(void) {}
void I_ShutdownSound(void) {}
void I_SetChannels(int channels) {}
void I_SetMusicVolume(int volume) {}
int I_RegisterSong(void *data) { return 1; }
void I_UnRegisterSong(int handle) {}
void I_PlaySong(int handle, boolean looping) {}
void I_PauseSong(int handle) {}
void I_ResumeSong(int handle) {}
void I_StopSong(int handle) {}
int I_StartSound(int id, void *data, int vol, int sep, int pitch, int priority) { return id + 1; }
void I_StopSound(int handle) {}
int I_SoundIsPlaying(int handle) { return 0; }
void I_UpdateSoundParams(int handle, int vol, int sep, int pitch) {}

//
// retro-go
//

static bool screenshot_handler(const char *filename, int width, int height)
{
    return rg_surface_save_image_file(update, filename, width, height);
}

static bool save_state_handler(const char *filename)
{
    rg_gui_alert("Not implemented", "Please use the in-game menu");
    return false;
}

static bool load_state_handler(const char *filename)
{
    rg_gui_alert("Not implemented", "Please use the in-game menu");
    return false;
}

static bool reset_handler(bool hard)
{
    return false;
}

static void event_handler(int event, void *arg)
{
    if (event == RG_EVENT_REDRAW)
        rg_display_submit(update, 0);
}

static bool is_iwad(const char *path)
{
    char header[16] = {0};
    void *data = &header;
    size_t data_len = 16;
    rg_storage_read_file(path, &data, &data_len, RG_FILE_USER_BUFFER);
    return header[0] == 'I' && header[1] == 'W';
}

static void heretic_task(void *arg)
{
    D_DoomMain(); // Never returns
}

void app_main(void)
{
    const rg_handlers_t handlers = {
        .loadState = &load_state_handler,
        .saveState = &save_state_handler,
        .reset = &reset_handler,
        .screenshot = &screenshot_handler,
        .event = &event_handler,
    };

    app = rg_system_init(AUDIO_SAMPLE_RATE, &handlers, NULL);
    rg_system_set_tick_rate(TICRATE);

    update = rg_surface_create(SCREENWIDTH, SCREENHEIGHT, RG_PIXEL_PAL565_BE, MEM_FAST);

    const char *iwad = app->romPath;
    if (!iwad || !is_iwad(iwad))
    {
        iwad = rg_gui_file_picker("Select Heretic WAD", RG_BASE_PATH_ROMS "/heretic", is_iwad, false, false);
        if (!iwad)
            rg_system_exit();
        rg_gui_draw_hourglass();
    }

    rg_storage_mkdir(RG_BASE_PATH_SAVES "/heretic");
    rg_storage_mkdir(RG_BASE_PATH_CONFIG);

    static char *heretic_argv[] = {"heretic", NULL};
    myargc = 1;
    myargv = heretic_argv;
    wadfiles[0] = (char *)iwad;
    wadfiles[1] = NULL; // the other entries are development lumps
    basedefault = RG_BASE_PATH_CONFIG "/heretic.cfg";

    // The original was a DOS program with a big stack: run it in a task with more stack than app_main's 8KB
    rg_task_create("heretic", &heretic_task, NULL, 16 * 1024, RG_TASK_PRIORITY_2, 0);
}
