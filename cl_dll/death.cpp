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
// death notice
//
#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"

#include <string.h>
#include <stdio.h>
#include "draw_util.h"
#include "strl.h"
#include "triangleapi.h"

float color[3];

struct DeathNoticeItem {
	char szKiller[MAX_PLAYER_NAME_LENGTH*2];
	char szVictim[MAX_PLAYER_NAME_LENGTH*2];
	char szWeapon[32];
	int iId;	// the index number of the associated sprite
	bool bSuicide;
	bool bTeamKill;
	bool bNonPlayerKill;
	float flDisplayTime;
	float flSpawnTime;
	float *KillerColor;
	float *VictimColor;
	int iHeadShotId;
};

#define MAX_DEATHNOTICES	4
static int DEATHNOTICE_DISPLAY_TIME = 6;

#define DEATHNOTICE_TOP		32

DeathNoticeItem rgDeathNoticeList[ MAX_DEATHNOTICES + 1 ];

struct KillFeedIcon
{
	const char *path;
	int texture;
	bool attempted;
};

static KillFeedIcon kfRifle = { "gfx/scoreboard/rifle.png", 0, false };
static KillFeedIcon kfGun = { "gfx/scoreboard/kill_gun.png", 0, false };
static KillFeedIcon kfGrenade = { "gfx/scoreboard/kill_grenade.png", 0, false };
static KillFeedIcon kfExplosion = { "gfx/scoreboard/explosion.png", 0, false };
static KillFeedIcon kfSkull = { "gfx/scoreboard/kill_skull.png", 0, false };
static KillFeedIcon kfHeadshot = { "gfx/scoreboard/headshot.png", 0, false };

cvar_t *hud_killfeed_fade;
cvar_t *hud_killfeed_fadein;
cvar_t *hud_killfeed_fadeout;
cvar_t *hud_killfeed_fxtime;
cvar_t *hud_killfeed_holdtime;
cvar_t *hud_killfeed_color;
cvar_t *hud_killfeed_alpha;
cvar_t *hud_killfeed_bgcolor;
cvar_t *hud_killfeed_bgalpha;

static void KfDrawIcon( KillFeedIcon &icon, int x, int y, int size,
	int r, int g, int b, int a )
{
	if( !icon.attempted )
	{
		icon.attempted = true;
		icon.texture = gRenderAPI.GL_LoadTexture( icon.path, NULL, 0,
			TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	}

	if( !icon.texture )
		return;

	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, icon.texture );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );
	DrawUtils::Draw2DQuad( (float)x, (float)y, (float)( x + size ), (float)( y + size ) );
}

static KillFeedIcon *KfWeaponIcon( const DeathNoticeItem &item )
{
	if( item.bNonPlayerKill || item.bSuicide || !item.szKiller[0] )
		return &kfSkull;

	if( strstr( item.szWeapon, "hegrenade" ) )
		return &kfExplosion;
	if( strstr( item.szWeapon, "grenade" ) || strstr( item.szWeapon, "flashbang" ) )
		return &kfGrenade;
	if( strstr( item.szWeapon, "knife" ) )
		return &kfGun;

	return &kfRifle;
}

static void KfParseColor( const char *value, int &r, int &g, int &b )
{
	r = g = b = 255;
	if( value && value[0] )
		sscanf( value, "%d %d %d", &r, &g, &b );
	r = max( 0, min( 255, r ) );
	g = max( 0, min( 255, g ) );
	b = max( 0, min( 255, b ) );
}

static void KfDrawNotice( DeathNoticeItem &item, int index, float flTime )
{
	int scale = (int)( ScreenHeight / 480.0f );
	if( scale < 1 ) scale = 1;
	if( scale > 2 ) scale = 2;

	const int pad = 14 * scale;
	const int rowHeight = 28 * scale;
	const int iconSize = 22 * scale;
	const int hsSize = 15 * scale;
	const int gap = 6 * scale;
	const int top = 92 * scale;
	const int y = top + index * rowHeight;

	KillFeedIcon *weapon = KfWeaponIcon( item );
	int killerWidth = item.szKiller[0] ? DrawUtils::ConsoleStringLen( item.szKiller ) : 0;
	int victimWidth = item.szVictim[0] ? DrawUtils::ConsoleStringLen( item.szVictim ) : 0;
	bool showHeadshot = item.iHeadShotId != 0;

	float fade = 1.0f;
	if( hud_killfeed_fade && hud_killfeed_fade->value >= 0.5f )
	{
		float fxTime = hud_killfeed_fxtime ? hud_killfeed_fxtime->value : 0.25f;
		fxTime = max( 0.05f, min( 2.0f, fxTime ) );

		float elapsed = flTime - item.flSpawnTime;
		float remaining = item.flDisplayTime - flTime;

		if( hud_killfeed_fadein && hud_killfeed_fadein->value >= 0.5f && elapsed < fxTime )
			fade = min( fade, elapsed / fxTime );
		if( hud_killfeed_fadeout && hud_killfeed_fadeout->value >= 0.5f && remaining < fxTime )
			fade = min( fade, remaining / fxTime );
	}

	if( fade < 0.0f ) fade = 0.0f;
	if( fade > 1.0f ) fade = 1.0f;

	int textBaseAlpha = hud_killfeed_alpha ? (int)hud_killfeed_alpha->value : 235;
	textBaseAlpha = max( 0, min( 255, textBaseAlpha ) );
	int bgBaseAlpha = hud_killfeed_bgalpha ? (int)hud_killfeed_bgalpha->value : 110;
	bgBaseAlpha = max( 0, min( 255, bgBaseAlpha ) );

	float textAlpha = ( textBaseAlpha / 255.0f ) * fade;
	int bgAlpha = (int)( bgBaseAlpha * fade );

	int right = ScreenWidth - pad;
	int x = right;
	if( victimWidth > 0 )
		x -= victimWidth;
	if( showHeadshot )
		x -= gap + hsSize;
	x -= gap + iconSize;
	if( killerWidth > 0 )
		x -= gap + killerWidth;

	int left = x - pad;
	int width = right - left;

	int bgR, bgG, bgB;
	KfParseColor( hud_killfeed_bgcolor ? hud_killfeed_bgcolor->string : NULL, bgR, bgG, bgB );
	if( bgAlpha > 0 )
		FillRGBABlend( left, y - 5 * scale, width, rowHeight - 4 * scale,
			bgR, bgG, bgB, bgAlpha );

	int cfgR, cfgG, cfgB;
	bool useConfigColor = hud_killfeed_color && hud_killfeed_color->string[0];
	if( useConfigColor )
		KfParseColor( hud_killfeed_color->string, cfgR, cfgG, cfgB );

	x = right;
	if( victimWidth > 0 )
	{
		x -= victimWidth;
		if( useConfigColor )
			DrawUtils::SetConsoleTextColor( ( cfgR / 255.0f ) * textAlpha,
				( cfgG / 255.0f ) * textAlpha, ( cfgB / 255.0f ) * textAlpha );
		else if( item.VictimColor )
			DrawUtils::SetConsoleTextColor( item.VictimColor[0] * textAlpha,
				item.VictimColor[1] * textAlpha, item.VictimColor[2] * textAlpha );
		else
			DrawUtils::SetConsoleTextColor( 0.92f * textAlpha, 0.92f * textAlpha, 0.92f * textAlpha );
		DrawUtils::DrawConsoleString( x, y, item.szVictim );
	}

	if( showHeadshot )
	{
		x -= gap + hsSize;
		KfDrawIcon( kfHeadshot, x, y + 1 * scale, hsSize, 255, 206, 74, (int)( 235 * fade ) );
	}

	x -= gap + iconSize;
	KfDrawIcon( *weapon, x, y, iconSize, 238, 239, 240, (int)( 245 * fade ) );

	if( killerWidth > 0 )
	{
		x -= gap + killerWidth;
		if( useConfigColor )
			DrawUtils::SetConsoleTextColor( ( cfgR / 255.0f ) * textAlpha,
				( cfgG / 255.0f ) * textAlpha, ( cfgB / 255.0f ) * textAlpha );
		else if( item.KillerColor )
			DrawUtils::SetConsoleTextColor( item.KillerColor[0] * textAlpha,
				item.KillerColor[1] * textAlpha, item.KillerColor[2] * textAlpha );
		else
			DrawUtils::SetConsoleTextColor( 0.92f * textAlpha, 0.92f * textAlpha, 0.92f * textAlpha );
		DrawUtils::DrawConsoleString( x, y, item.szKiller );
	}
}

cvar_t *cl_killsound;
cvar_t *cl_killsound_path;

int CHudDeathNotice :: Init( void )
{
	gHUD.AddHudElem( this );

	HOOK_MESSAGE( gHUD.m_DeathNotice, DeathMsg );

	hud_deathnotice_time = CVAR_CREATE( "hud_deathnotice_time", "6", FCVAR_ARCHIVE );
	hud_killfeed_fade = CVAR_CREATE( "hud_killfeed_fade", "1", FCVAR_ARCHIVE );
	hud_killfeed_fadein = CVAR_CREATE( "hud_killfeed_fadein", "1", FCVAR_ARCHIVE );
	hud_killfeed_fadeout = CVAR_CREATE( "hud_killfeed_fadeout", "1", FCVAR_ARCHIVE );
	hud_killfeed_fxtime = CVAR_CREATE( "hud_killfeed_fxtime", "0.25", FCVAR_ARCHIVE );
	hud_killfeed_holdtime = CVAR_CREATE( "hud_killfeed_holdtime", "5", FCVAR_ARCHIVE );
	hud_killfeed_color = CVAR_CREATE( "hud_killfeed_color", "", FCVAR_ARCHIVE );
	hud_killfeed_alpha = CVAR_CREATE( "hud_killfeed_alpha", "235", FCVAR_ARCHIVE );
	hud_killfeed_bgcolor = CVAR_CREATE( "hud_killfeed_bgcolor", "0 0 0", FCVAR_ARCHIVE );
	hud_killfeed_bgalpha = CVAR_CREATE( "hud_killfeed_bgalpha", "110", FCVAR_ARCHIVE );
	cl_killsound = CVAR_CREATE( "cl_killsound", "0", FCVAR_ARCHIVE );
	cl_killsound_path = CVAR_CREATE( "cl_killsound_path", "buttons/bell1.wav", FCVAR_ARCHIVE );
	m_iFlags = 0;

	return 1;
}


void CHudDeathNotice :: InitHUDData( void )
{
	memset( rgDeathNoticeList, 0, sizeof(rgDeathNoticeList) );
}


int CHudDeathNotice :: VidInit( void )
{
	m_HUD_d_skull = gHUD.GetSpriteIndex( "d_skull" );
	m_HUD_d_headshot = gHUD.GetSpriteIndex("d_headshot");

	return 1;
}

int CHudDeathNotice :: Draw( float flTime )
{
	int x, y, r, g, b, i;
	const bool csgoKillfeed = hud_style && hud_style->value >= 0.5f;

	for( i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;  // we've gone through them all

		if ( rgDeathNoticeList[i].flDisplayTime < flTime )
		{ // display time has expired
			// remove the current item from the list
			memmove( &rgDeathNoticeList[i], &rgDeathNoticeList[i+1], sizeof(DeathNoticeItem) * (MAX_DEATHNOTICES - i) );
			i--;  // continue on the next item;  stop the counter getting incremented
			continue;
		}

		float maxDisplay = DEATHNOTICE_DISPLAY_TIME;
		if( csgoKillfeed && hud_killfeed_holdtime )
			maxDisplay = max( maxDisplay, hud_killfeed_holdtime->value );
		rgDeathNoticeList[i].flDisplayTime = min( rgDeathNoticeList[i].flDisplayTime, flTime + maxDisplay );

		if( csgoKillfeed )
		{
			KfDrawNotice( rgDeathNoticeList[i], i, flTime );
			continue;
		}

		// Hide when scoreboard drawing. It will break triapi
		//if ( gViewPort && gViewPort->AllowedToPrintText() )
		//if ( !gHUD.m_iNoConsolePrint )
		{
			// Draw the death notice
			if( !g_iUser1 )
			{
				y = YRES(DEATHNOTICE_TOP) + 2 + (20 * i);  //!!!
			}
			else
			{
				y = ScreenHeight / 5 + 2 + (20 * i);
			}

			int id = (rgDeathNoticeList[i].iId == -1) ? m_HUD_d_skull : rgDeathNoticeList[i].iId;
			x = ScreenWidth - DrawUtils::ConsoleStringLen(rgDeathNoticeList[i].szVictim) - (gHUD.GetSpriteRect(id).Width());
			if( rgDeathNoticeList[i].iHeadShotId )
				x -= gHUD.GetSpriteRect(m_HUD_d_headshot).Width();

			if ( !rgDeathNoticeList[i].bSuicide )
			{
				x -= (5 + DrawUtils::ConsoleStringLen( rgDeathNoticeList[i].szKiller ) );

				// Draw killers name
				if ( rgDeathNoticeList[i].KillerColor )
					DrawUtils::SetConsoleTextColor( rgDeathNoticeList[i].KillerColor[0], rgDeathNoticeList[i].KillerColor[1], rgDeathNoticeList[i].KillerColor[2] );
				x = 5 + DrawUtils::DrawConsoleString( x, y, rgDeathNoticeList[i].szKiller );
			}

			r = 255;  g = 80;	b = 0;
			if ( rgDeathNoticeList[i].bTeamKill )
			{
				r = 10;	g = 240; b = 10;  // display it in sickly green
			}

			// Draw death weapon
			SPR_Set( gHUD.GetSprite(id), r, g, b );
			SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(id) );

			x += (gHUD.GetSpriteRect(id).Width());

			if( rgDeathNoticeList[i].iHeadShotId)
			{
				SPR_Set( gHUD.GetSprite(m_HUD_d_headshot), r, g, b );
				SPR_DrawAdditive( 0, x, y, &gHUD.GetSpriteRect(m_HUD_d_headshot));
				x += (gHUD.GetSpriteRect(m_HUD_d_headshot).Width());
			}

			// Draw victims name (if it was a player that was killed)
			if (!rgDeathNoticeList[i].bNonPlayerKill)
			{
				if ( rgDeathNoticeList[i].VictimColor )
					DrawUtils::SetConsoleTextColor( rgDeathNoticeList[i].VictimColor[0], rgDeathNoticeList[i].VictimColor[1], rgDeathNoticeList[i].VictimColor[2] );
				x = DrawUtils::DrawConsoleString( x, y, rgDeathNoticeList[i].szVictim );
			}
		}
	}

	if( i == 0 )
		m_iFlags &= ~HUD_DRAW; // disable hud item

	return 1;
}

// This message handler may be better off elsewhere
int CHudDeathNotice :: MsgFunc_DeathMsg( const char *pszName, int iSize, void *pbuf )
{
	m_iFlags |= HUD_DRAW;

	BufferReader reader( pszName, pbuf, iSize );

	int killer = reader.ReadByte();
	int victim = reader.ReadByte();
	int headshot = reader.ReadByte();

	char killedwith[32];
	strlcpy( killedwith, "d_", sizeof( killedwith ) );
	strlcat( killedwith, reader.ReadString(), sizeof( killedwith ) );

	//if (gViewPort)
	//	gViewPort->DeathMsg( killer, victim );
	gHUD.m_Scoreboard.DeathMsg( killer, victim );

	gHUD.m_Spectator.DeathMessage(victim);
	int i;
	for ( i = 0; i < MAX_DEATHNOTICES; i++ )
	{
		if ( rgDeathNoticeList[i].iId == 0 )
			break;
	}
	if ( i == MAX_DEATHNOTICES )
	{ // move the rest of the list forward to make room for this item
		memmove( rgDeathNoticeList, rgDeathNoticeList+1, sizeof(DeathNoticeItem) * MAX_DEATHNOTICES );
		i = MAX_DEATHNOTICES - 1;
	}

	memset( &rgDeathNoticeList[i], 0, sizeof( DeathNoticeItem ) );
	strlcpy( rgDeathNoticeList[i].szWeapon, killedwith + 2, sizeof( rgDeathNoticeList[i].szWeapon ) );
	rgDeathNoticeList[i].flSpawnTime = gHUD.m_flTime;

	//if (gViewPort)
		//gViewPort->GetAllPlayersInfo();
	gHUD.m_Scoreboard.GetAllPlayersInfo();

	// Get the Killer's name
	const char *killer_name = NULL;
	bool killer_this_player = false;
	if ( killer >= 1 && killer <= MAX_PLAYERS )
	{
		killer_name = g_PlayerInfoList[killer].name;
		killer_this_player = g_PlayerInfoList[killer].thisplayer;
	}

	if ( !killer_name )
	{
		killer_name = "";
		rgDeathNoticeList[i].szKiller[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].KillerColor = GetClientColor( killer );
		strlcpy( rgDeathNoticeList[i].szKiller, killer_name, sizeof( rgDeathNoticeList[i].szKiller ) );
	}

	// Get the Victim's name
	const char *victim_name = NULL;

	if ( victim >= 1 && victim <= MAX_PLAYERS )
		victim_name = g_PlayerInfoList[ victim ].name;

	if ( !victim_name )
	{
		victim_name = "";
		rgDeathNoticeList[i].szVictim[0] = 0;
	}
	else
	{
		rgDeathNoticeList[i].VictimColor = GetClientColor( victim );
		strlcpy( rgDeathNoticeList[i].szVictim, victim_name, sizeof( rgDeathNoticeList[i].szVictim ) );
	}

	// Is it a non-player object kill?
	// If victim is 255, the killer killed a specific, non-player object (like a sentrygun)
	if( victim == 255 )
	{
		rgDeathNoticeList[i].bNonPlayerKill = true;

		// Store the object's name in the Victim slot (skip the d_ bit)
		strlcpy( rgDeathNoticeList[i].szVictim, killedwith+2, sizeof( rgDeathNoticeList[i].szVictim ) );
	}
	else
	{
		if ( killer == victim || killer == 0 )
			rgDeathNoticeList[i].bSuicide = true;

		if ( !strncmp( killedwith, "d_teammate", sizeof(killedwith)  ) )
			rgDeathNoticeList[i].bTeamKill = true;
	}

	rgDeathNoticeList[i].iHeadShotId = headshot;

	// Find the sprite in the list
	int spr = gHUD.GetSpriteIndex( killedwith );

	rgDeathNoticeList[i].iId = spr;

	float holdTime = hud_deathnotice_time->value;
	if( hud_style && hud_style->value >= 0.5f && hud_killfeed_holdtime )
		holdTime = hud_killfeed_holdtime->value;
	if( holdTime < 0.5f )
		holdTime = 0.5f;
	rgDeathNoticeList[i].flDisplayTime = gHUD.m_flTime + holdTime;

	// Play kill sound
	if ((killer_this_player || g_iUser2 == killer) &&
		!rgDeathNoticeList[i].bNonPlayerKill &&
		!rgDeathNoticeList[i].bSuicide &&
		cl_killsound->value > 0.0f)
	{
		PlaySound(cl_killsound_path->string, cl_killsound->value);
	}

	if (rgDeathNoticeList[i].bNonPlayerKill)
	{
		ConsolePrint( rgDeathNoticeList[i].szKiller );
		ConsolePrint( " killed a " );
		ConsolePrint( rgDeathNoticeList[i].szVictim );
		ConsolePrint( "\n" );
	}
	else
	{
		// record the death notice in the console
		if ( rgDeathNoticeList[i].bSuicide )
		{
			ConsolePrint( rgDeathNoticeList[i].szVictim );

			if ( !strncmp( killedwith, "d_world", sizeof(killedwith)  ) )
			{
				ConsolePrint( " died" );
			}
			else
			{
				ConsolePrint( " killed self" );
			}
		}
		else if ( rgDeathNoticeList[i].bTeamKill )
		{
			ConsolePrint( rgDeathNoticeList[i].szKiller );
			ConsolePrint( " killed his teammate " );
			ConsolePrint( rgDeathNoticeList[i].szVictim );
		}
		else
		{
			if( headshot )
				ConsolePrint( "*** ");
			ConsolePrint( rgDeathNoticeList[i].szKiller );
			ConsolePrint( " killed " );
			ConsolePrint( rgDeathNoticeList[i].szVictim );
		}

		if ( *killedwith && (*killedwith > 13 ) && strncmp( killedwith, "d_world", sizeof(killedwith) ) && !rgDeathNoticeList[i].bTeamKill )
		{
			if ( headshot )
				ConsolePrint(" with a headshot from ");
			else
				ConsolePrint(" with ");

			ConsolePrint( killedwith+2 ); // skip over the "d_" part
		}

		if( headshot ) ConsolePrint( " ***");
		ConsolePrint( "\n" );
	}

	return 1;
}

