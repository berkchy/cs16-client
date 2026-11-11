/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
//
// hud_redraw.cpp
//
#include <math.h>
#include "hud.h"
#include "cl_util.h"
#include "triangleapi.h"

#include <string.h>

#include "draw_util.h"
#include "nexora/loader.h"

#define MAX_LOGO_FRAMES 56

int grgLogoFrame[MAX_LOGO_FRAMES] = 
{
	1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 13, 13, 13, 13, 13, 12, 11, 10, 9, 8, 14, 15,
	16, 17, 18, 19, 20, 20, 20, 20, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 
	29, 29, 29, 29, 29, 28, 27, 26, 25, 24, 30, 31 
};


extern int g_iVisibleMouse;

static const char *NXHudElementName( CHudBase *element )
{
	if ( !element )
		return "unknown";

	if ( element == static_cast<CHudBase *>( &gHUD.m_Ammo ) ) return "ammo";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Health ) ) return "health";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Spectator ) ) return "spectator";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Geiger ) ) return "geiger";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Battery ) ) return "battery";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Train ) ) return "train";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Flash ) ) return "flashlight";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Message ) ) return "message";
	if ( element == static_cast<CHudBase *>( &gHUD.m_StatusBar ) ) return "statusbar";
	if ( element == static_cast<CHudBase *>( &gHUD.m_DeathNotice ) ) return "deathnotice";
	if ( element == static_cast<CHudBase *>( &gHUD.m_SayText ) ) return "saytext";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Menu ) ) return "menu";
	if ( element == static_cast<CHudBase *>( &gHUD.m_AmmoSecondary ) ) return "ammosecondary";
	if ( element == static_cast<CHudBase *>( &gHUD.m_TextMessage ) ) return "textmessage";
	if ( element == static_cast<CHudBase *>( &gHUD.m_StatusIcons ) ) return "statusicons";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Scoreboard ) ) return "scoreboard";
	if ( element == static_cast<CHudBase *>( &gHUD.m_MOTD ) ) return "motd";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Money ) ) return "money";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Timer ) ) return "timer";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Radio ) ) return "radio";
	if ( element == static_cast<CHudBase *>( &gHUD.m_ProgressBar ) ) return "progressbar";
	if ( element == static_cast<CHudBase *>( &gHUD.m_SniperScope ) ) return "sniperscope";
	if ( element == static_cast<CHudBase *>( &gHUD.m_NVG ) ) return "nvg";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Radar ) ) return "radar";
	if ( element == static_cast<CHudBase *>( &gHUD.m_SpectatorGui ) ) return "spectator_gui";
	if ( element == static_cast<CHudBase *>( &gHUD.m_Scenario ) ) return "scenario";

	return "unknown";
}

float HUD_GetFOV( void );

// Think
void CHud::Think(void)
{
	int newfov;

	extern int g_weaponselect_frames;
	if( g_weaponselect_frames )
		g_weaponselect_frames--;

	for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
	{
		if( pList->p->m_iFlags & HUD_THINK )
			pList->p->Think();
	}

	newfov = HUD_GetFOV();
	m_iFOV = newfov ? newfov : default_fov->value;

	// the clients fov is actually set in the client data update section of the hud

	// Set a new sensitivity
	if ( m_iFOV == default_fov->value )
	{  
		// reset to saved sensitivity
		m_flMouseSensitivity = 0;
	}
	else
	{  
		// set a new sensitivity that is proportional to the change from the FOV default
		m_flMouseSensitivity = sensitivity->value * ((float)newfov / (float)default_fov->value) * zoom_sens_ratio->value;
	}

	// think about default fov
	if ( m_iFOV == 0 )
	{  // only let players adjust up in fov,  and only if they are not overriden by something else
		m_iFOV = max( default_fov->value, 90 );
	}

}

// Redraw
// step through the local data,  placing the appropriate graphics & text as appropriate
// returns 1 if they've changed, 0 otherwise
int CHud :: Redraw( float flTime, int intermission )
{
	m_fOldTime = m_flTime;	// save time of previous redraw
	m_flTime = flTime;
	m_flTimeDelta = (double)m_flTime - m_fOldTime;
	static int m_flShotTime = 0;

	// Clock was reset, reset delta
	if ( m_flTimeDelta < 0 )
		m_flTimeDelta = 0;

	if (m_flShotTime && m_flShotTime < flTime)
	{
		gEngfuncs.pfnClientCmd("snapshot\n");
		m_flShotTime = 0;
	}

	m_iIntermission = intermission;

	UpdateDefaultHUDColor();

	if ( m_pCvarDraw->value && (intermission || !(m_iHideHUDDisplay & HIDEHUD_ALL) ) )
	{
		for( HUDLIST *pList = m_pHudList; pList; pList = pList->pNext )
		{
			if( pList->p->m_iFlags & HUD_DRAW )
			{
					if( intermission && !(pList->p->m_iFlags & HUD_INTERMISSION) )
						continue; // skip no-intermission during intermission

					{
						nexora_hud_element_args_t nxArgs;

						nxArgs.element = pList->p;
						nxArgs.name = NXHudElementName( pList->p );
						nxArgs.allow = 1;

						NXPluginLoader_Dispatch( NEXORA_HOOK_HUD_ELEMENT_DRAW, &nxArgs );

						if ( !nxArgs.allow )
							continue;
					}

					pList->p->Draw( flTime );
			}
		}
	}

	// are we in demo mode? do we need to draw the logo in the top corner?
	if (m_iLogo)
	{
		int x, y, i;

		if (m_hsprLogo == 0)
			m_hsprLogo = LoadSprite("sprites/%d_logo.spr");

		SPR_Set(m_hsprLogo, 250, 250, 250 );
		
		x = SPR_Width(m_hsprLogo, 0);
		x = ScreenWidth - x;
		y = SPR_Height(m_hsprLogo, 0)/2;

		// Draw the logo at 20 fps
		int iFrame = (int)(flTime * 20) % MAX_LOGO_FRAMES;
		i = grgLogoFrame[iFrame] - 1;

		SPR_DrawAdditive(i, x, y, NULL);
	}

	// update codepage parameters
	if( !stricmp( con_charset->string, "cp1251" ))
	{
		g_codepage = 1251;
	}
	else if( !stricmp( con_charset->string, "cp1252" ))
	{
		g_codepage = 1252;
	}
	else
	{
		g_codepage = 0;
	}

	g_accept_utf8 = !stricmp( cl_charset->string, "utf-8" );

	return 1;
}

void CHud::UpdateDefaultHUDColor()
{
	int r, g, b;

	if (sscanf(m_pCvarColor->string, "%d %d %d", &r, &g, &b) == 3) {
		r = max(r, 0);
		g = max(g, 0);
		b = max(b, 0);

		r = min(r, 255);
		g = min(g, 255);
		b = min(b, 255);

		m_iDefaultHUDColor = (r << 16) | (g << 8) | b;
	} else {
		m_iDefaultHUDColor = RGB_YELLOWISH;
	}
}
