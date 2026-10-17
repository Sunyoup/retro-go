/*
 * heretic-go: Heretic (Raven Software, GPLv2 source release) for retro-go
 *
 * This file is the platform layer that replaces the DOS I_IBM.C / I_SOUND.C:
 * video, input, timer, zone memory, sound effects and music (mono).
 */

#include <stdlib.h>
#include <stdarg.h>
#include <doomdef.h>
#include <sounds.h>
#include <soundst.h>
#include <rg_system.h>
#include <oplplayer.h>
#include <mus2mid.h>
#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

#define AUDIO_SAMPLE_RATE 22050
#define AUDIO_BUFFER_LENGTH (AUDIO_SAMPLE_RATE / TICRATE + 1)
#define NUM_MIX_CHANNELS 8

static rg_surface_t *update;
static rg_app_t *app;

extern char *wadfiles[];
extern char *basedefault;
extern byte gammatable[5][256];
extern int usegamma;
extern int key_right, key_left, key_up, key_down;
extern int key_fire, key_use, key_strafe, key_speed;
extern int key_flyup, key_flydown, key_invleft, key_invright, key_useartifact;
extern weaponinfo_t wpnlev1info[NUMWEAPONS];
extern boolean WeaponInShareware[];
extern int snd_Channels;
extern int mus_lumpnum; // lump of the song being registered, see S_StartSong()

static byte *current_palette;

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
    static int64_t last_time = 0;

    if (UpdateState == I_NOUPDATE)
        return;
    rg_display_submit(update, 0);
    rg_display_sync(true); // Wait for update->data to be released
    UpdateState = I_NOUPDATE;

    // Count drawn frames (not game tics) for retro-go's FPS display
    int64_t now = rg_system_timer();
    rg_system_tick(now - last_time);
    last_time = now;
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
// Buttons (see Sunyoup/Note_Heretic.txt):
//   stick   move/turn, menus
//   A       fire                      menus: select, "yes"
//   B       hold: strafe + run        menus: back
//   START   open/use                  menus: select
//   SELECT  tap: next weapon, hold + stick left/right: inventory, hold + stick up: use artifact
//   OPTION  tap: automap, hold + stick up/down: fly up/down
//   MENU    tap: Heretic menu, hold: retro-go menu
//
// Every tic we compute the set of Heretic keys that should be down and post
// key events for the differences with the previous tic.
//

extern boolean MenuActive, askforquit;

#define KEY_TAB 9 // automap (am_map.h)

static byte keys_down[256];
static int pulse_key = -1, pulse_tics = 0; // a key held down for a couple of tics (tap actions)

static void set_key(byte *keys, int key)
{
    if (key >= 0 && key < 256)
        keys[key] = 1;
}

// Key ('1'..'7') of the next weapon the player owns and has ammo for, -1 if none
static int next_weapon_key(void)
{
    player_t *player = &players[consoleplayer];
    if (gamestate != GS_LEVEL || !player->mo || player->chickenTics)
        return -1;
    int current = player->pendingweapon != wp_nochange ? player->pendingweapon : player->readyweapon;
    if (current == wp_gauntlets)
        current = wp_staff; // they share the '1' key
    for (int i = 1; i <= wp_mace; i++)
    {
        int weapon = (current + i) % (wp_mace + 1);
        if (!player->weaponowned[weapon] && weapon != wp_staff)
            continue;
        if (shareware && !WeaponInShareware[weapon])
            continue;
        ammotype_t ammo = wpnlev1info[weapon].ammo;
        if (ammo != am_noammo && player->ammo[ammo] == 0)
            continue;
        return '1' + weapon;
    }
    return -1;
}

void I_StartTic(void)
{
    static int menu_delay = 0;
    static bool select_used = false, option_used = false;
    static uint32_t prev_joystick = 0;
    uint32_t joystick = rg_input_read_gamepad();
    bool in_menu = MenuActive || askforquit;
    byte keys[256] = {0};
    event_t event = {0};

    // MENU: short press opens Heretic's menu, long press opens retro-go's menu
    if (joystick & RG_KEY_MENU)
    {
        if (menu_delay++ == TICRATE / 2)
        {
            rg_gui_game_menu();
            joystick = prev_joystick = rg_input_read_gamepad();
        }
    }
    else
    {
        if (menu_delay > 0 && menu_delay < TICRATE / 2)
        {
            pulse_key = KEY_ESCAPE;
            pulse_tics = 2;
        }
        menu_delay = 0;
    }

    bool select = (joystick & RG_KEY_SELECT) && !in_menu;
    bool option = (joystick & RG_KEY_OPTION) && !in_menu;
    uint32_t stick = joystick & (RG_KEY_UP | RG_KEY_DOWN | RG_KEY_LEFT | RG_KEY_RIGHT);

    if (in_menu)
    {
        if (joystick & RG_KEY_UP)    set_key(keys, KEY_UPARROW);
        if (joystick & RG_KEY_DOWN)  set_key(keys, KEY_DOWNARROW);
        if (joystick & RG_KEY_LEFT)  set_key(keys, KEY_LEFTARROW);
        if (joystick & RG_KEY_RIGHT) set_key(keys, KEY_RIGHTARROW);
        if (joystick & RG_KEY_A)     { set_key(keys, KEY_ENTER); set_key(keys, 'y'); }
        if (joystick & RG_KEY_START) set_key(keys, KEY_ENTER);
        if (joystick & RG_KEY_B)     set_key(keys, KEY_BACKSPACE);
    }
    else if (select)
    {
        if (stick)
            select_used = true;
        if (joystick & RG_KEY_LEFT)  set_key(keys, key_invleft);
        if (joystick & RG_KEY_RIGHT) set_key(keys, key_invright);
        if (joystick & RG_KEY_UP)    set_key(keys, key_useartifact);
    }
    else if (option)
    {
        if (stick & (RG_KEY_UP | RG_KEY_DOWN))
            option_used = true;
        if (joystick & RG_KEY_UP)    set_key(keys, key_flyup);
        if (joystick & RG_KEY_DOWN)  set_key(keys, key_flydown);
        if (joystick & RG_KEY_LEFT)  set_key(keys, key_left);
        if (joystick & RG_KEY_RIGHT) set_key(keys, key_right);
    }
    else
    {
        if (joystick & RG_KEY_UP)    set_key(keys, key_up);
        if (joystick & RG_KEY_DOWN)  set_key(keys, key_down);
        if (joystick & RG_KEY_LEFT)  set_key(keys, key_left);
        if (joystick & RG_KEY_RIGHT) set_key(keys, key_right);
    }

    if (!in_menu)
    {
        if (joystick & RG_KEY_A)     set_key(keys, key_fire);
        if (joystick & RG_KEY_B)     { set_key(keys, key_strafe); set_key(keys, key_speed); }
        if (joystick & RG_KEY_START) set_key(keys, key_use);
    }

    // Taps of SELECT and OPTION (released without using the stick)
    if ((prev_joystick & RG_KEY_SELECT) && !(joystick & RG_KEY_SELECT))
    {
        if (!select_used && !in_menu && (pulse_key = next_weapon_key()) >= 0)
            pulse_tics = 2;
        select_used = false;
    }
    if ((prev_joystick & RG_KEY_OPTION) && !(joystick & RG_KEY_OPTION))
    {
        if (!option_used && !in_menu)
        {
            pulse_key = KEY_TAB;
            pulse_tics = 2;
        }
        option_used = false;
    }
    if (pulse_tics > 0)
    {
        set_key(keys, pulse_key);
        pulse_tics--;
    }

    for (int i = 0; i < 256; i++)
    {
        if (keys[i] != keys_down[i])
        {
            event.type = keys[i] ? ev_keydown : ev_keyup;
            event.data1 = i;
            D_PostEvent(&event);
            keys_down[i] = keys[i];
        }
    }

    prev_joystick = joystick;
}

//
// System
//

void I_Init(void)
{
    snd_Channels = NUM_MIX_CHANNELS; // the DOS default was 3
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
// Sound effects: software mixer, mono (the board has a single speaker)
//

// DMX sound lump: format (3), sample rate, number of samples, 8 bit unsigned samples
typedef struct {
    uint16_t format;
    uint16_t samplerate;
    uint32_t length;
    uint8_t samples[];
} dmx_sfx_t;

typedef struct {
    const dmx_sfx_t *sfx; // NULL if the channel is free
    uint32_t pos;         // 16.16 fixed point
    uint32_t step;        // 16.16 fixed point
    uint32_t end;         // 16.16 fixed point
    int volume;           // 0-127
    int handle;
} mix_channel_t;

static mix_channel_t mix_channels[NUM_MIX_CHANNELS];
static rg_audio_sample_t mixbuffer[AUDIO_BUFFER_LENGTH];
static int next_handle = 0;

static const music_player_t *music_player = &opl_synth_player;
static volatile bool music_playing = false;
const void *opl_genmidi_lump; // used by oplplayer.c

static void sound_task(void *arg)
{
    while (1)
    {
        bool have_music = music_playing;
        if (have_music)
            music_player->render(mixbuffer, AUDIO_BUFFER_LENGTH);

        for (int n = 0; n < AUDIO_BUFFER_LENGTH; n++)
        {
            // The music is rendered in stereo, mix it down to mono
            int sample = have_music ? (mixbuffer[n].left + mixbuffer[n].right) / 2 : 0;
            for (int i = 0; i < NUM_MIX_CHANNELS; i++)
            {
                mix_channel_t *chan = &mix_channels[i];
                const dmx_sfx_t *sfx = chan->sfx;
                if (!sfx)
                    continue;
                if (chan->pos >= chan->end)
                {
                    chan->sfx = NULL;
                    continue;
                }
                sample += (sfx->samples[chan->pos >> 16] - 128) * chan->volume;
                chan->pos += chan->step;
            }
            // 8 bit sample * 7 bit volume = 15 bit per sound, clip the sum
            if (sample > 32767)
                sample = 32767;
            else if (sample < -32768)
                sample = -32768;
            mixbuffer[n].left = mixbuffer[n].right = sample;
        }
        rg_audio_submit(mixbuffer, AUDIO_BUFFER_LENGTH);
    }
}

static mix_channel_t *find_channel(int handle)
{
    mix_channel_t *chan = &mix_channels[(handle & 0xFF) % NUM_MIX_CHANNELS];
    return (handle > 0 && chan->handle == handle && chan->sfx) ? chan : NULL;
}

void I_StartupSound(void)
{
    opl_genmidi_lump = W_CacheLumpName("GENMIDI", PU_STATIC);
    music_player->init(AUDIO_SAMPLE_RATE);
    rg_task_create("heretic_snd", &sound_task, NULL, 4 * 1024, RG_TASK_PRIORITY_2, 1);
}

void I_ShutdownSound(void) {}
void I_SetChannels(int channels) {}

int I_StartSound(int id, void *data, int vol, int sep, int pitch, int priority)
{
    const dmx_sfx_t *sfx = data;
    if (!sfx || sfx->format != 3 || sfx->length == 0)
        return 0;

    // Use a free channel, or steal the one closest to its end
    int slot = 0;
    uint32_t least_left = UINT32_MAX;
    for (int i = 0; i < NUM_MIX_CHANNELS; i++)
    {
        mix_channel_t *chan = &mix_channels[i];
        if (!chan->sfx)
        {
            slot = i;
            break;
        }
        if (chan->end - chan->pos < least_left)
        {
            least_left = chan->end - chan->pos;
            slot = i;
        }
    }

    // pitch: 128 is normal, Heretic varies it a little around 127
    mix_channel_t *chan = &mix_channels[slot];
    chan->sfx = NULL; // Stop the mixer from using the channel while we change it
    chan->pos = 0;
    chan->step = (uint32_t)(((uint64_t)sfx->samplerate << 16) * pitch / 128 / AUDIO_SAMPLE_RATE);
    chan->end = sfx->length << 16;
    chan->volume = RG_MIN(RG_MAX(vol, 0), 127);
    chan->handle = ((next_handle++ % 0x7FFFFF) + 1) << 8 | slot;
    chan->sfx = sfx;
    return chan->handle;
}

void I_StopSound(int handle)
{
    mix_channel_t *chan = find_channel(handle);
    if (chan)
        chan->sfx = NULL;
}

int I_SoundIsPlaying(int handle)
{
    return find_channel(handle) != NULL;
}

void I_UpdateSoundParams(int handle, int vol, int sep, int pitch)
{
    mix_channel_t *chan = find_channel(handle);
    if (chan)
        chan->volume = RG_MIN(RG_MAX(vol, 0), 127);
}

//
// Music: MUS -> MIDI -> OPL2 emulator (from prboom-go), uses the GENMIDI lump of the WAD
//

void I_SetMusicVolume(int volume)
{
    music_player->setvolume(volume); // 0-15
}

int I_RegisterSong(void *data)
{
    size_t len = W_LumpLength(mus_lumpnum);
    uint8_t *mid = NULL;
    size_t midlen;
    int handle = 0;

    if (mus2mid(data, len, &mid, &midlen, 64) == MUS_OK)
        handle = (int)music_player->registersong(mid, midlen);
    else
        RG_LOGE("mus2mid failed for lump %d\n", mus_lumpnum);
    free(mid);
    return handle;
}

void I_UnRegisterSong(int handle)
{
    if (handle)
        music_player->unregistersong((void *)handle);
}

void I_PlaySong(int handle, boolean looping)
{
    if (!handle)
        return;
    music_player->play((void *)handle, looping);
    music_playing = true;
}

void I_PauseSong(int handle)
{
    music_player->pause();
    music_playing = false;
}

void I_ResumeSong(int handle)
{
    music_player->resume();
    music_playing = true;
}

void I_StopSong(int handle)
{
    music_player->stop();
    music_playing = false;
}

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

    // Keep app_main alive: retro-go's system monitor keeps reading the main task's stack
    // statistics, returning would delete the task and it would read freed memory.
    while (1)
        rg_task_delay(1000);
}
