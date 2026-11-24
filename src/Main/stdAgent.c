#define _GNU_SOURCE
#include "Main/stdAgent.h"
#include "Main/stdAgentSession.h"
#include "SDL2_helper.h"
#include "globals.h"
#include "Main/Main.h"
#include "Gui/jkGUIRend.h"
#include "Platform/std3D.h"
#include "Win95/Window.h"
#include "World/sithWorld.h"
#include "Primitives/rdMatrix.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

extern SDL_Window* displayWindow;
static stdAgentSession session = {-1, 0, -1};
static stdAgentInput input;
static int active, quitRequested, pointerReady;
static float pointerX, pointerY;
static stdAgentCommand pendingCapture;
static uint64_t captureDeadline, presentedFrames;

/* Retail JK 1.01 draws at most 128 sectors and 32 translucent portal faces a
   frame and leaves the rest of the frame black; this engine's limits can be
   higher, so a level that is fine here can fail in retail. Each presented world
   frame's counts are sampled here, read-only, with peaks and the number of
   frames over the retail limits since the level last loaded. */
#define STDAGENT_RETAIL_VISIBLE_SECTORS 128
#define STDAGENT_RETAIL_ALPHA_ADJOINS 32
static struct {
    const void* world;
    uint32_t lastGameTimeMs;
    int unloaded;
    uint64_t frames, framesOverRetailSectors, framesOverRetailAlpha, framesAtEngineLimit;
    int visibleSectors, alphaAdjoins, peakVisibleSectors, peakAlphaAdjoins;
} renderStats;

static void sample_render_stats(void)
{
    SithWorld* world = sithWorld_g_pCurrentWorld;
    if (!(sith_bOpen && sithWorld_bLoaded && world)) { renderStats.unloaded = 1; return; }
    if (!jkMain_bLevelActive || jkGuiRend_IsMenuActive()) return;
    /* Start over for another world, after an unload, or when game time runs
       backwards: a reload or a restored save can reuse the world's address. */
    const uint32_t gameTimeMs = sithTime_g_msecGameTime;
    if (renderStats.world != (const void*)world || renderStats.unloaded || gameTimeMs < renderStats.lastGameTimeMs) {
        memset(&renderStats, 0, sizeof(renderStats));
        renderStats.world = world;
    }
    renderStats.lastGameTimeMs = gameTimeMs;
    const int sectors = sithRender_g_numVisibleSectors, alpha = sithRender_numAlphaAdjoins;
    ++renderStats.frames;
    renderStats.visibleSectors = sectors;
    renderStats.alphaAdjoins = alpha;
    if (sectors > renderStats.peakVisibleSectors) renderStats.peakVisibleSectors = sectors;
    if (alpha > renderStats.peakAlphaAdjoins) renderStats.peakAlphaAdjoins = alpha;
    if (sectors > STDAGENT_RETAIL_VISIBLE_SECTORS) ++renderStats.framesOverRetailSectors;
    if (alpha > STDAGENT_RETAIL_ALPHA_ADJOINS) ++renderStats.framesOverRetailAlpha;
    if (sectors >= SITH_MAX_VISIBLE_SECTORS || alpha >= SITH_MAX_VISIBLE_ALPHA_SURFACES) ++renderStats.framesAtEngineLimit;
}

int stdAgent_IsActive(void) { return active; }
int stdAgent_IsQuitRequested(void) { return quitRequested; }

static int option(const char* arg, const char* name)
{ return (arg[0] == '-' || arg[0] == '/') && !strcasecmp(arg+1, name); }

int stdAgent_Init(int argc, char** argv)
{
    int requested = 0, autostart = 0, singleplayer = 0, startupPath = 0;
    for (int i = 1; i < argc; ++i) if (option(argv[i], "agent")) ++requested;
    if (!requested) return 1;
    if (requested != 1) return 0;
#if defined(RENDER_GL11)
    return 0;
#endif
    const char* incompatible[] = {"capture", "renderprobe", "headless", "mp", "multiplayer", "dedicatedServer", "droidworks", "droidworksCompat", "dwCompat", "character"};
    size_t argumentBytes = 0;
    for (int i = 1; i < argc; ++i) {
        argumentBytes += strlen(argv[i]) + 1;
        if (argumentBytes >= 1024) return 0;
        for (size_t j = 0; j < sizeof(incompatible)/sizeof(incompatible[0]); ++j)
            if (option(argv[i], incompatible[j])) return 0;
        if (option(argv[i], "path")) {
            if (++startupPath != 1 || i+1 >= argc || !stdAgent_StartupPathValid(argv[i+1])) return 0;
        }
        if (option(argv[i], "autostart")) autostart = 1;
        if (option(argv[i], "sp") || option(argv[i], "singleplayer")) singleplayer = 1;
    }
    const char* capture = getenv("OPENJKDF2_CAPTURE_REQUEST");
    if ((capture && *capture) || (autostart && !singleplayer)) return 0;
    int previousId = session.lastId;
    if (!stdAgent_OpenSession(&session, getenv("OPENJKDF2_AGENT_DIR"))) return 0;
    session.lastId = previousId; // Keep the command sequence across ordinary in-process mod restarts.
    active = 1;
    fprintf(stderr, "agent: private local command channel enabled; normal real-time gameplay\n");
    return 1;
}

int stdAgent_KeyDown(int scancode, int physicalDown)
{
    return physicalDown || (active && scancode > 0 && scancode < 256 && input.keys[scancode]);
}

uint32_t stdAgent_MouseButtons(uint32_t physicalButtons)
{
    if (active) for (int button = 1; button <= 5; ++button)
        if (input.buttons[button]) physicalButtons |= SDL_BUTTON_MASK(button);
    return physicalButtons;
}

static int key_event(int scancode, int down)
{
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.timestamp = SDL_GetTicksNS();
    event.key.windowID = displayWindow ? SDL_GetWindowID(displayWindow) : 0;
    event.key.which = STDAGENT_KEYBOARD_ID;
    event.key.scancode = (SDL_Scancode)scancode;
    event.key.key = SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, false);
    event.key.down = down != 0;
    return SDL_PushEvent(&event);
}

static int button_event(int button, int down)
{
    SDL_Event event = {0};
    event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.timestamp = SDL_GetTicksNS();
    event.button.windowID = displayWindow ? SDL_GetWindowID(displayWindow) : 0;
    event.button.button = (unsigned char)button;
    event.button.down = down != 0;
    event.button.clicks = 1;
    event.button.x = pointerX; event.button.y = pointerY;
    return SDL_PushEvent(&event);
}

static void release_input(void)
{
    for (int key = 1; key < 256; ++key) if (input.keys[key]) key_event(key, 0);
    for (int button = 1; button <= 5; ++button) if (input.buttons[button]) button_event(button, 0);
    stdAgent_ClearInput(&input);
    /* Preserve any real held buttons if a queue is full during release. */
    float x, y;
    uint32_t buttons = SDL_GetMouseState(&x, &y);
    Window_bMouseLeft = !!(buttons & SDL_BUTTON_LMASK);
    Window_bMouseRight = !!(buttons & SDL_BUTTON_RMASK);
}

void stdAgent_Shutdown(void)
{
    active = 0;
    stdAgent_ClearInput(&input);
    stdAgent_CloseSession(&session);
}

static void respond(const stdAgentCommand* command, const char* result, const char* error)
{
    if (!stdAgent_Respond(&session, command, result, error)) {
        fprintf(stderr, "agent: response could not be published; releasing local input and disabling channel\n");
        release_input(); stdAgent_Shutdown();
    }
}

static void json_number(FILE* file, double value)
{ if (isfinite(value)) fprintf(file, "%.9g", value); else fputs("null", file); }

static void json_vector(FILE* file, const rdVector3* vector)
{
    fputc('[', file); json_number(file, vector->x); fputc(',', file);
    json_number(file, vector->y); fputc(',', file); json_number(file, vector->z); fputc(']', file);
}

static void json_name(FILE* file, const char* name, size_t capacity)
{
    fputc('"', file);
    for (size_t i = 0; i < capacity && name[i]; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c < 32 || c >= 127 || c == '"' || c == '\\') fprintf(file, "\\u%04x", c);
        else fputc(c, file);
    }
    fputc('"', file);
}

static int array_member(const void* entry, const void* array, size_t count, size_t stride)
{
    if (!entry || !array || !count || count > SIZE_MAX/stride) return 0;
    uintptr_t value = (uintptr_t)entry, begin = (uintptr_t)array;
    return value >= begin && value-begin < count*stride && (value-begin)%stride == 0;
}

static void state_response(const stdAgentCommand* command, uint64_t now)
{
    char* text = NULL; size_t size = 0;
    FILE* file = open_memstream(&text, &size);
    if (!file) { respond(command, NULL, "cannot allocate state response"); return; }
    int windowW = 0, windowH = 0, drawableW = 0, drawableH = 0;
    if (displayWindow) {
        SDL_GetWindowSize(displayWindow, &windowW, &windowH);
        SDL_GetWindowSizeInPixels(displayWindow, &drawableW, &drawableH);
    }
    fprintf(file, "{\"monotonicMs\":%llu,\"renderedFrame\":%llu,\"guiState\":%d,\"menuActive\":%s,\"levelActive\":%s,\"worldLoaded\":%s,\"multiplayer\":%s,\"gameTimeMs\":%u,\"paused\":%s,\"windowSize\":[%d,%d],\"drawableSize\":[%d,%d],\"pointer\":[%.0f,%.0f],\"heldKeys\":[",
        (unsigned long long)now, (unsigned long long)presentedFrames, jkSmack_currentGuiState,
        jkGuiRend_IsMenuActive() ? "true" : "false", jkMain_bLevelActive ? "true" : "false",
        sithWorld_bLoaded ? "true" : "false", sithNet_isMulti ? "true" : "false",
        sithTime_g_msecGameTime, sithTime_g_bPaused ? "true" : "false",
        windowW, windowH, drawableW, drawableH, (double)pointerX, (double)pointerY);
    int first = 1;
    for (int key = 1; key < 256; ++key) if (input.keys[key]) { fprintf(file, "%s%d", first ? "" : ",", key); first = 0; }
    fputs("],\"heldButtons\":[", file); first = 1;
    for (int button = 1; button <= 5; ++button) if (input.buttons[button]) { fprintf(file, "%s%d", first ? "" : ",", button); first = 0; }
    fputs("],\"world\":", file);
    SithWorld* world = sithWorld_g_pCurrentWorld;
    SithThing* player = sithPlayer_g_pLocalPlayerThing;
    int worldReady = sith_bOpen && sithWorld_bLoaded && jkMain_bLevelActive && world;
    if (worldReady) {
        json_name(file, world->map_jkl_fname, sizeof(world->map_jkl_fname));
        fprintf(file, ",\"thingCapacity\":%d,\"sectorCount\":%d", world->numThingsLoaded, world->numSectors);
    } else fputs("null,\"thingCapacity\":null,\"sectorCount\":null", file);
    /* The last presented world frame's draw counts and their peaks since this
       world loaded, against retail JK 1.01's limits and this build's own. */
    if (worldReady && renderStats.world == (const void*)world && renderStats.frames) {
        fprintf(file, ",\"render\":{\"frames\":%llu,\"visibleSectors\":%d,\"alphaAdjoins\":%d,\"peakVisibleSectors\":%d,"
            "\"peakAlphaAdjoins\":%d,\"framesOverRetailSectors\":%llu,\"framesOverRetailAlpha\":%llu,\"framesAtEngineLimit\":%llu,"
            "\"retailLimits\":{\"visibleSectors\":%d,\"alphaAdjoins\":%d},\"engineLimits\":{\"visibleSectors\":%d,\"alphaAdjoins\":%d}}",
            (unsigned long long)renderStats.frames, renderStats.visibleSectors, renderStats.alphaAdjoins,
            renderStats.peakVisibleSectors, renderStats.peakAlphaAdjoins,
            (unsigned long long)renderStats.framesOverRetailSectors, (unsigned long long)renderStats.framesOverRetailAlpha,
            (unsigned long long)renderStats.framesAtEngineLimit,
            STDAGENT_RETAIL_VISIBLE_SECTORS, STDAGENT_RETAIL_ALPHA_ADJOINS,
            (int)SITH_MAX_VISIBLE_SECTORS, (int)SITH_MAX_VISIBLE_ALPHA_SURFACES);
    } else fputs(",\"render\":null", file);
    fputs(",\"player\":", file);
    /* Loads and modal GUI draws can reenter the event pump. Never follow a
       previous world's player pointer, even if it has not yet been cleared. */
    if (worldReady && world->numThingsLoaded > 0
        && array_member(player, world->aThings, (size_t)world->numThingsLoaded, sizeof(SithThing))
        && player->type == SITH_THING_PLAYER) {
        int sector = -1;
        if (world->numSectors > 0 && array_member(player->sector, world->aSectors, (size_t)world->numSectors, sizeof(SithSector)))
            sector = (int)(((uintptr_t)player->sector - (uintptr_t)world->aSectors)/sizeof(SithSector));
        fprintf(file, "{\"thingId\":%u,\"signature\":%u,\"sector\":%d,\"position\":", player->idx, player->signature, sector);
        json_vector(file, &player->position);
        /* The player's sector light: a model is lit by its sector's ambient, so a
           level whose rooms bake to different ambients shows it here. */
        if (sector >= 0) {
            const SithSector* room = &world->aSectors[sector];
            fputs(",\"sectorLight\":{\"ambient\":", file); json_number(file, room->ambientLight);
            fputs(",\"extraLight\":", file); json_number(file, room->extraLight);
            fputs(",\"tint\":", file); json_vector(file, &room->tint);
            fputc('}', file);
        } else fputs(",\"sectorLight\":null", file);
        fputs(",\"velocity\":", file);
        if (player->moveType == SITH_MT_PHYSICS) json_vector(file, &player->physicsParams.vel); else fputs("null", file);
        fputs(",\"health\":", file); json_number(file, player->actorParams.health);
        fputs(",\"headPYR\":", file); json_vector(file, &player->actorParams.headPYR);
        /* The body's own pitch, yaw and roll in degrees, as JKL thing orientations
           write them: yaw 0 looks along +y and a positive yaw turns left. */
        rdVector3 bodyPYR;
        rdMatrix_ExtractAngles34(&player->orient, &bodyPYR);
        fputs(",\"orientationPYR\":", file); json_vector(file, &bodyPYR);
        /* Read-only inventory: the selected weapon bin and the amount in every
           registered bin, keyed by bin number (items.dat). The same rule as the
           player thing applies: only follow a player record inside the table. */
        SithPlayer* info = player->actorParams.pPlayer;
        if (array_member(info, jkPlayer_playerInfos, JKPLAYER_NUM_INFOS, sizeof(SithPlayer))) {
            fprintf(file, ",\"weapon\":%d,\"inventory\":{", info->curWeaponID);
            int firstBin = 1;
            const int bins = (int)(sizeof(info->aItems)/sizeof(info->aItems[0])) < SITHBIN_NUMBINS
                ? (int)(sizeof(info->aItems)/sizeof(info->aItems[0])) : SITHBIN_NUMBINS;
            for (int bin = 0; bin < bins; ++bin) {
                if (!(sithInventory_g_aTypes[bin].flags & SITHINVENTORY_TYPE_REGISTERED)) continue;
                fprintf(file, "%s\"%d\":", firstBin ? "" : ",", bin);
                json_number(file, info->aItems[bin].amount);
                firstBin = 0;
            }
            fputc('}', file);
        } else fputs(",\"weapon\":null,\"inventory\":null", file);
        fputc('}', file);
    } else fputs("null", file);
    fputc('}', file);
    int failed = ferror(file);
    if (fclose(file)) failed = 1;
    respond(command, failed ? NULL : text, failed ? "cannot serialize state" : NULL);
    free(text);
}

void stdAgent_Poll(void)
{
    if (!active) return;
    uint64_t now = SDL_GetTicks();
    if (!pointerReady) { SDL_GetMouseState(&pointerX, &pointerY); pointerReady = 1; }
    if (stdAgent_InputExpired(&input, now) || (sithNet_isMulti && input.leaseUntilMs)) release_input();
    if (pendingCapture.id) {
        if (now >= captureDeadline) {
            respond(&pendingCapture, NULL, "capture timed out waiting for a completed frame");
            memset(&pendingCapture, 0, sizeof(pendingCapture));
        }
        return;
    }
    stdAgentCommand command; const char* error;
    int received = stdAgent_Receive(&session, &command, &error);
    if (!received) return;
    if (received < 0) {
        respond(&command, NULL, error);
        if (received == -2 && active) { release_input(); stdAgent_Shutdown(); }
        return;
    }
    if (command.verb == STDAGENT_PING) {
        respond(&command, "{\"protocol\":\"openjkdf2-agent-v1\",\"leaseMs\":2000,\"maxCaptureWaitMs\":10000}", NULL); return;
    }
    if (command.verb == STDAGENT_STATE) { state_response(&command, now); return; }
    if (command.verb == STDAGENT_RELEASE || command.verb == STDAGENT_QUIT) {
        release_input();
        respond(&command, "{\"released\":true}", NULL);
        if (command.verb == STDAGENT_QUIT) { quitRequested = 1; g_should_exit = 1; }
        return;
    }
    if (command.verb == STDAGENT_CAPTURE) {
        pendingCapture = command; captureDeadline = now + 10000; return;
    }
    if (sithNet_isMulti) { respond(&command, NULL, "agent input is limited to local single-player sessions"); return; }
    if (command.verb == STDAGENT_TEXT) {
        stdAgent_ApplyInput(&input, &command, now);
        /* A character shortcut may open a modal menu. Publish acceptance before
           entering its message loop so the client can send the next command. */
        respond(&command, "{\"accepted\":true}", NULL);
        if (!active) return;
        for (size_t i = 0; command.text[i] && !quitRequested; ++i)
            Window_msg_main_handler(g_hWnd, WM_CHAR, (unsigned char)command.text[i], 0);
        return;
    }
    int accepted = 1;
    if (command.verb == STDAGENT_KEY && input.keys[command.a] != command.b)
        accepted = key_event(command.a, command.b);
    else if (command.verb == STDAGENT_BUTTON && input.buttons[command.a] != command.b)
        accepted = button_event(command.a, command.b);
    else if (command.verb == STDAGENT_MOUSE) {
        int width = 1, height = 1;
        if (displayWindow) SDL_GetWindowSize(displayWindow, &width, &height);
        pointerX = fmaxf(0, fminf((float)(width-1), pointerX + command.a));
        pointerY = fmaxf(0, fminf((float)(height-1), pointerY + command.b));
        SDL_Event event = {0}; event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.timestamp = SDL_GetTicksNS();
        event.motion.windowID = displayWindow ? SDL_GetWindowID(displayWindow) : 0;
        event.motion.state = stdAgent_MouseButtons(0);
        event.motion.x = pointerX; event.motion.y = pointerY;
        event.motion.xrel = (float)command.a; event.motion.yrel = (float)command.b;
        accepted = SDL_PushEvent(&event);
    }
    if (accepted) stdAgent_ApplyInput(&input, &command, now);
    else release_input();
    respond(&command, "{\"accepted\":true}", accepted ? NULL : "SDL input queue rejected the event");
}

void stdAgent_BeforeSwap(void)
{
    if (!active) return;
    ++presentedFrames;
    sample_render_stats();
    if (!pendingCapture.id) return;
    unsigned char* pixels = NULL; int width = 0, height = 0;
    int ok = 0;
#if !defined(RENDER_GL11)
    ok = std3D_ReadWindowRgb(&pixels, &width, &height);
#endif
    if (ok) ok = stdAgent_WriteFrame(&session, pendingCapture.id, pixels, width, height);
    free(pixels);
    char result[256];
    snprintf(result, sizeof(result), "{\"file\":\"frame-%d.ppm\",\"width\":%d,\"height\":%d,\"renderedFrame\":%llu,\"monotonicMs\":%llu}",
        pendingCapture.id, width, height, (unsigned long long)presentedFrames, (unsigned long long)SDL_GetTicks());
    respond(&pendingCapture, result, ok ? NULL : "cannot capture composed frame or exclusively create image");
    memset(&pendingCapture, 0, sizeof(pendingCapture));
}
