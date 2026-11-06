#include "Main/jkEditor.h"
#include "jkGUIEsc.h"

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdString.h"
#include "General/stdFont.h"
#include "Engine/rdMaterial.h" // TODO move tVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUIObjectives.h"
#include "Gui/jkGUIMap.h"
#include "Gui/jkGUISaveLoad.h"
#include "Gui/jkGUISetup.h"
#include "Gui/jkGUIForce.h"
#include "World/jkPlayer.h"
#include "Main/jk.h"
#include "Main/jkStrings.h"
#include "Main/jkMain.h"
#include "Dss/sithMulti.h"
#include "Devices/sithControl.h"
#include "Platform/std3D.h"
#include "Win95/stdComm.h"
#include <stdlib.h>
#include <stdio.h>

enum jkGuiEscButton_t
{
    JKGUIESC_RETURNTOGAME = 1,
    JKGUIESC_OBJECTIVES   = 10,
    JKGUIESC_MAP          = 11,
    JKGUIESC_JEDIPOWERS   = 12,
    JKGUIESC_LOAD         = 13,
    JKGUIESC_RESTART      = 14,
    JKGUIESC_SAVE         = 15,
    JKGUIESC_SETUP        = 16,
    JKGUIESC_ABORT        = 17
};

enum jkGuiEscElement_t
{
    JKGUIESC_ELMT_OBJECTIVES   = 0,
    JKGUIESC_ELMT_MAP          = 1,
    JKGUIESC_ELMT_JEDIPOWERS   = 2,
    JKGUIESC_ELMT_RETURNTOGAME = 3,
    JKGUIESC_ELMT_LOAD         = 4,
    JKGUIESC_ELMT_SAVE         = 5,
    JKGUIESC_ELMT_RESTART      = 6,
    JKGUIESC_ELMT_SETUP        = 7,
    JKGUIESC_ELMT_ABORT        = 8,
};

static jkGuiElement jkGuiEsc_aElements[14] = {
    { ELEMENT_TEXTBUTTON, JKGUIESC_OBJECTIVES,   5, "GUI_OBJECTIVES",     3, {  0, 50,  400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_MAP,          5, "GUI_MAP",            3, {  0, 100, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_JEDIPOWERS,   5, "GUI_JEDIPOWERS",     3, {  0, 150, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RETURNTOGAME, 5, "GUI_RETURN_TO_GAME", 3, {  0, 240, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_LOAD,         5, "GUI_LOAD",           3, {400, 270, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SAVE,         5, "GUI_SAVE",           3, {400, 320, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RESTART,      5, "GUI_RESTART",        3, {400, 220, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SETUP,        5, "GUI_SETUP",          3, {400, 370, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_ABORT,        5, "GUI_ABORT",          3, {400, 420, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, 322, 5, NULL, 3, {0, 300, 390, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, 323, 5, NULL, 3, {0, 350, 390, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, 324, 5, NULL, 3, {0, 400, 390, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    { ELEMENT_TEXTBUTTON, 325, 5, NULL, 3, {400,170,240,40}, 1},
    { ELEMENT_END,        0,                     0,  NULL,                0, {0},                  0,  0,  0,  0,  0,  0, {0}, 0}
};

static jkGuiMenu jkGuiEsc_menu = { jkGuiEsc_aElements, -1, 0x0FFFF, 0x0FFFF, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };
static void jkGuiEsc_CaptureEpisode(jkGuiMenu *menu){
    static int frames,stage;if(++frames%3)return;
#ifdef SDL2_RENDER
    if(!stage){char path[512];snprintf(path,sizeof(path),"%s.pause.png",getenv("GEOSITH_EPISODE_CAPTURE"));std3D_ScreenshotWindow(path);}
#endif
    menu->lastClicked=stage++?JKGUIESC_RETURNTOGAME:getenv("GEOSITH_NETWORK_DETAILS_CAPTURE")?325:324;
}

void jkGuiEsc_Startup()
{
    jkGui_InitMenu(&jkGuiEsc_menu, jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    jkGuiEsc_aElements[9].wstr = u"Save Level";
    jkGuiEsc_aElements[10].wstr = u"Level Header";
    jkGuiEsc_aElements[11].wstr = u"Episode Editor";
    jkGuiEsc_aElements[12].wstr = u"Network Details";
    jkGuiEsc_bInitialized = 1;
}

void jkGuiEsc_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    
    jkGuiEsc_bInitialized = 0;
}

#ifdef PLATFORM_GNS
static char16_t networkDetailLines[3][128];
static void networkDetailsTick(jkGuiMenu *menu){char status[256];stdComm_GNS_GetStatus(status,sizeof(status));size_t at=0;
    for(int line=0;line<3;line++){char part[86]={0};size_t left=strlen(status+at),n=left>78?78:left;if(n&&left>78)while(n>40&&status[at+n]!=' ')n--;memcpy(part,status+at,n);at+=n;while(status[at]==' ')at++;stdString_CharToWchar(networkDetailLines[line],part,128);menu->paElements[line+1].wstr=networkDetailLines[line];jkGuiRend_UpdateAndDrawClickable(&menu->paElements[line+1],menu,1);}
#ifdef SDL2_RENDER
    if(getenv("GEOSITH_NETWORK_DETAILS_CAPTURE")&&getenv("GEOSITH_STRESS")){static int frames;if(++frames==3){std3D_ScreenshotWindow(getenv("GEOSITH_NETWORK_DETAILS_CAPTURE"));menu->lastClicked=-1;}}
#endif
}
static void networkDetails(void){int previousNetSafe=jkEditor_NetSafeDialog(1);jkGuiElement elements[10]={0};elements[0]=(jkGuiElement){ELEMENT_TEXT,0,2,u"Network Details",3,{20,20,600,30},1};
    for(int i=1;i<=3;i++)elements[i]=(jkGuiElement){ELEMENT_TEXT,0,0,NULL,3,{20,70+(i-1)*24,600,24},1};
    elements[4]=(jkGuiElement){ELEMENT_CHECKBOX,1,0,u"Allow connected players to edit and undo",0,{30,175,580,30},sithNet_isServer};elements[4].selectedTextEntry=stdComm_GNS_EditsAllowed();
    elements[5]=(jkGuiElement){ELEMENT_TEXT,0,0,u"Internet: share the host's public IP and hosting UDP port.",3,{20,230,600,24},1};
    elements[6]=(jkGuiElement){ELEMENT_TEXT,0,0,u"LAN: type LAN in Join Game. VPN: enter the host's VPN IP.",3,{20,260,600,24},1};
    elements[7]=(jkGuiElement){ELEMENT_TEXTBUTTON,2,2,u"Retry Router Mapping",3,{20,330,280,40},sithNet_isServer};
    if(stdComm_GNS_IsRelay()){elements[5].wstr=u"Internet: choose the same relay address as the host.";elements[6].wstr=u"Relay connections do not need player port forwarding.";elements[7].bIsVisible=0;}
    elements[8]=(jkGuiElement){ELEMENT_TEXTBUTTON,-1,2,u"Close",3,{340,400,280,40},1};elements[9].type=ELEMENT_END;
    jkGuiMenu menu={elements,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};menu.idkFunc=networkDetailsTick;jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&elements[8]);
    for(;;){int result=jkGuiRend_DisplayAndReturnClicked(&menu);if(sithNet_isServer)stdComm_GNS_AllowEdits(elements[4].selectedTextEntry);if(result==2){stdComm_GNS_RetryRouter();continue;}break;}jkEditor_NetSafeDialog(previousNetSafe);
}
#endif

void jkGuiEsc_Show()
{
    int32_t v3; // eax
    jkGuiEsc_aElements[9].bIsVisible = jkEditor_IsLevel();
    jkGuiEsc_aElements[10].bIsVisible = jkEditor_IsLevel();
    jkGuiEsc_aElements[11].bIsVisible = jkEditor_IsLevel();
    jkGuiEsc_aElements[12].bIsVisible = sithNet_isMulti;
    jkGuiEsc_menu.idkFunc=(getenv("GEOSITH_EPISODE_CAPTURE")||getenv("GEOSITH_NETWORK_DETAILS_CAPTURE"))&&getenv("GEOSITH_STRESS")?jkGuiEsc_CaptureEpisode:NULL;

    if ( sithNet_isMulti )
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = !!(sithMulti_multiModeFlags & MULTIMODEFLAG_COOP); // Added: co-op
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
    }
    else
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 1;

        // MOTS added
        if (Main_bMotsCompat) {
            if (sithPlayer_g_pLocalPlayerThing->flags & SITH_TF_DEAD || sithPlayer_g_pLocalPlayerThing->actorParams.flags & SITH_AF_CONTROLSDISABLED)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
            if (sithPlayer_g_pLocalPlayerThing->actorParams.flags & SITH_AF_CONTROLSDISABLED) {
                jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
            }
        }
        else {
            if (sithPlayer_g_pLocalPlayerThing->flags & SITH_TF_DEAD)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        }
    }

    while ( 1 )
    {
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiEsc_menu, &jkGuiEsc_aElements[JKGUIESC_ELMT_RETURNTOGAME]);
        switch (jkGuiRend_DisplayAndReturnClicked(&jkGuiEsc_menu))
        {
            case -1:
                return;

            case 322:
                jkEditor_SaveDialog();
                continue;
            case 323:
                jkEditor_HeaderDialog();
                continue;
            case 324:
                jkEditor_EpisodeDialog();
                continue;
            case 325:
#ifdef PLATFORM_GNS
                networkDetails();
#endif
                continue;

            case JKGUIESC_OBJECTIVES:
                jkGuiObjectives_Show();
                continue;

            case JKGUIESC_MAP:
                jkGuiMap_Show();
                continue;

            case JKGUIESC_JEDIPOWERS:
                jkGuiForce_Show(0, 0.0, 0.0, 0, 0, 0);
                continue;

            case JKGUIESC_LOAD:
                v3 = jkGuiSaveLoad_Show(0);
                if ( v3 == 1 )
                {
                    jkMain_MissionReload();
                    jkGuiRend_UpdateSurface();
                    return;
                }
                if ( v3 != 34 )
                    continue;
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_RESTART:
                if ( !jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_RESTART_MISSION"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_RESTART")) )
                    continue;
                jkPlayer_LoadAutosave();
                sithControl_ResetCrouchToggle(); // Added: Toggle crouch state
                jkMain_MissionReload();
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_SAVE:
                if ( jkGuiSaveLoad_Show(1) != 1 )
                    continue;

            case JKGUIESC_RETURNTOGAME:
                jkMain_MissionReload();
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_SETUP:
                jkGuiSetup_Show();
                continue;

            case JKGUIESC_ABORT:
                if ( !jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_ABORT_GAME"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_ABORT")) )
                    continue;
                jkMain_MenuReturn();
                jkGuiRend_UpdateSurface();
                return;

            default:
                continue;
        }
    }
}
