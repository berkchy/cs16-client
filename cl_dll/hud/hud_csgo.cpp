//
// hud_csgo.cpp
//
// Optional CS:GO-style HUD selected with hud_style 1.
//

#include <stdio.h>
#include <string.h>

#include "hud.h"
#include "cl_util.h"
#include "draw_util.h"
#include "triangleapi.h"

extern float g_flScoreboardFade;

namespace
{

struct CsIcon
{
	const char *path;
	int texture;
	bool attempted;
};

CsIcon s_iconShield = { "gfx/scoreboard/shield.png", 0, false };
CsIcon s_iconCash = { "gfx/scoreboard/dollar.png", 0, false };
CsIcon s_iconAmmo = { "gfx/scoreboard/ammo.png", 0, false };
CsIcon s_iconTeamCT = { "gfx/scoreboard/ct_team.png", 0, false };
CsIcon s_iconTeamT = { "gfx/scoreboard/t_team.png", 0, false };

CsIcon s_iconDigits[10] =
{
	{ "gfx/scoreboard/timer_0.png", 0, false },
	{ "gfx/scoreboard/timer_1.png", 0, false },
	{ "gfx/scoreboard/timer_2.png", 0, false },
	{ "gfx/scoreboard/timer_3.png", 0, false },
	{ "gfx/scoreboard/timer_4.png", 0, false },
	{ "gfx/scoreboard/timer_5.png", 0, false },
	{ "gfx/scoreboard/timer_6.png", 0, false },
	{ "gfx/scoreboard/timer_7.png", 0, false },
	{ "gfx/scoreboard/timer_8.png", 0, false },
	{ "gfx/scoreboard/timer_9.png", 0, false },
};

CsIcon s_iconColon = { "gfx/scoreboard/timer_colon.png", 0, false };
CsIcon s_iconDash = { "gfx/scoreboard/timer_dash.png", 0, false };

cvar_t *hud_csgo_scale = NULL;

int CsLoad( CsIcon &icon )
{
	if( !icon.texture && !icon.attempted )
	{
		icon.attempted = true;
		icon.texture = gRenderAPI.GL_LoadTexture(
			icon.path, NULL, 0, TF_NEAREST | TF_NOMIPMAP | TF_CLAMP );
	}

	return icon.texture;
}

void CsDrawIconRect( CsIcon &icon, int x, int y, int width, int height, int r, int g, int b, int a )
{
	if( width <= 0 || height <= 0 )
		return;

	int texture = CsLoad( icon );
	if( !texture )
		return;

	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, texture );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );

	DrawUtils::Draw2DQuad( (float)x, (float)y, (float)( x + width ), (float)( y + height ) );
}

void CsDrawIconAdditive( CsIcon &icon, int x, int y, int size, int r, int g, int b, int a )
{
	if( size <= 0 )
		return;

	int texture = CsLoad( icon );
	if( !texture )
		return;

	gRenderAPI.GL_SelectTexture( 0 );
	gRenderAPI.GL_Bind( 0, texture );
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->Color4f( r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f );

	DrawUtils::Draw2DQuad( (float)x, (float)y, (float)( x + size ), (float)( y + size ) );
}

void CsDrawIcon( CsIcon &icon, int x, int y, int size, int r, int g, int b, int a )
{
	CsDrawIconRect( icon, x, y, size, size, r, g, b, a );
}

void CsDrawHealthCross( int x, int y, int size, int r, int g, int b, int a )
{
	int thickness = size / 3;
	if( thickness < 1 )
		thickness = 1;

	int offset = ( size - thickness ) / 2;
	FillRGBABlend( x + offset, y, thickness, size, r, g, b, a );
	FillRGBABlend( x, y + offset, size, thickness, r, g, b, a );
}

int CsGlyphBaseWidth( char c )
{
	if( c >= '0' && c <= '9' )
		return 18;
	if( c == ':' )
		return 8;
	if( c == '-' )
		return 12;
	return 0;
}

int CsGlyphWidth( char c, int height )
{
	int base = CsGlyphBaseWidth( c );
	if( base <= 0 )
		return 0;

	return ( base * height + 13 ) / 26;
}

int CsTextWidth( const char *text, int height )
{
	int width = 0;
	int gap = ( 2 * height + 13 ) / 26;

	for( const char *p = text; *p; ++p )
	{
		int glyph = CsGlyphWidth( *p, height );
		if( glyph <= 0 )
			continue;

		if( width > 0 )
			width += gap;

		width += glyph;
	}

	return width;
}

void CsDrawText( const char *text, int x, int y, int height, int r, int g, int b, int a )
{
	int gap = ( 2 * height + 13 ) / 26;

	for( const char *p = text; *p; ++p )
	{
		CsIcon *icon = NULL;
		if( *p >= '0' && *p <= '9' )
			icon = &s_iconDigits[*p - '0'];
		else if( *p == ':' )
			icon = &s_iconColon;
		else if( *p == '-' )
			icon = &s_iconDash;

		int width = CsGlyphWidth( *p, height );
		if( !icon || width <= 0 )
			continue;

		CsDrawIconRect( *icon, x, y, width, height, r, g, b, a );
		x += width + gap;
	}
}

void CsDrawMoney( int money, int left, int bottom, float scale )
{
	char buffer[16];
	sprintf( buffer, "%d", money );

	int numberHeight = (int)( 16 * scale );
	int iconSize = (int)( 13 * scale );
	int iconY = bottom - numberHeight + ( numberHeight - iconSize ) / 2;

	CsDrawIcon( s_iconCash, left, iconY, iconSize, 150, 205, 75, 235 );
	CsDrawText( buffer, left + iconSize + (int)( 4 * scale ), bottom - numberHeight,
		numberHeight, 150, 205, 75, 245 );
}

} // namespace

int CHudCsgo::Init( void )
{
	hud_csgo_scale = CVAR_CREATE( "hud_csgo_scale", "1", FCVAR_ARCHIVE );
	m_iFlags = HUD_DRAW;
	gHUD.AddHudElem( this );
	return 1;
}

int CHudCsgo::Draw( float flTime )
{
	if( !hud_style || hud_style->value < 0.5f )
		return 1;

	if( g_flScoreboardFade > 0.001f )
		return 1;

	if( gEngfuncs.IsSpectateOnly() )
		return 1;

	if( !( gHUD.m_iWeaponBits & ( 1 << ( WEAPON_SUIT ) ) ) )
		return 1;

	float scale = ScreenHeight / 480.0f;
	if( scale < 1.0f )
		scale = 1.0f;
	if( scale > 1.8f )
		scale = 1.8f;

	float userScale = hud_csgo_scale ? hud_csgo_scale->value : 1.0f;
	if( userScale < 0.5f )
		userScale = 0.5f;
	if( userScale > 2.0f )
		userScale = 2.0f;
	scale *= userScale;
	if( scale < 0.5f )
		scale = 0.5f;

	int pad = (int)( 20 * scale );
	int bottom = ScreenHeight - (int)( 18 * scale );

	if( !( gHUD.m_iHideHUDDisplay & HIDEHUD_HEALTH ) )
	{
		char buffer[16];
		int numberHeight = (int)( 23 * scale );
		int iconSize = (int)( 17 * scale );
		int textGap = (int)( 5 * scale );
		int iconY = bottom - numberHeight + ( numberHeight - iconSize ) / 2;

		sprintf( buffer, "%d", gHUD.m_Health.m_iHealth );

		int red = 238, green = 239, blue = 240;
		if( gHUD.m_Health.m_iHealth <= 25 )
		{
			red = 232;
			green = 104;
			blue = 104;
		}

		int healthX = pad;
		int healthTextX = healthX + iconSize + textGap;
		CsDrawHealthCross( healthX, iconY, iconSize, red, green, blue, 245 );
		CsDrawText( buffer, healthTextX,
			bottom - numberHeight, numberHeight, red, green, blue, 245 );

		int armorX = healthTextX + CsTextWidth( buffer, numberHeight ) + (int)( 18 * scale );
		sprintf( buffer, "%d", gHUD.m_Battery.GetArmor() );
		CsDrawIcon( s_iconShield, armorX, iconY, iconSize, 238, 239, 240, 240 );
		CsDrawText( buffer, armorX + iconSize + textGap,
			bottom - numberHeight, numberHeight, 238, 239, 240, 240 );

		if( !( gHUD.m_iHideHUDDisplay & HIDEHUD_MONEY ) )
			CsDrawMoney( gHUD.m_Money.GetMoney(), pad, bottom - (int)( 38 * scale ), scale );
	}

	if( !( gHUD.m_iHideHUDDisplay & HIDEHUD_WEAPONS ) )
	{
		int clip = -1;
		int reserve = -1;
		char weaponName[64] = { 0 };

		if( gHUD.m_Ammo.GetAmmoInfo( clip, reserve, weaponName, sizeof( weaponName ) ) )
		{
			int clipHeight = (int)( 23 * scale );
			int reserveHeight = (int)( 13 * scale );
			char clipBuffer[16] = { 0 };
			char reserveBuffer[16] = { 0 };

			if( clip >= 0 )
				sprintf( clipBuffer, "%d", clip );
			if( reserve >= 0 )
				sprintf( reserveBuffer, "%d", reserve );

			int gap = (int)( 6 * scale );
			int clipWidth = clipBuffer[0] ? CsTextWidth( clipBuffer, clipHeight ) : 0;
			int reserveWidth = reserveBuffer[0] ? CsTextWidth( reserveBuffer, reserveHeight ) : 0;
			int ammoIconSize = (int)( 17 * scale );
			int dividerWidth = (int)( 1 * scale );
			if( dividerWidth < 1 )
				dividerWidth = 1;
			int dividerGap = (int)( 7 * scale );

			int totalWidth = clipWidth + gap + dividerWidth + gap + reserveWidth + gap + ammoIconSize;
			if( clipWidth <= 0 )
				totalWidth -= gap;
			if( reserveWidth <= 0 )
				totalWidth -= gap;

			int x = ScreenWidth - pad - totalWidth;

			if( clipWidth > 0 )
			{
				CsDrawText( clipBuffer, x, bottom - clipHeight, clipHeight,
					238, 239, 240, 245 );
				x += clipWidth + dividerGap;
			}

			int dividerHeight = (int)( 16 * scale );
			int dividerY = bottom - reserveHeight - (int)( 3 * scale );
			FillRGBABlend( x, dividerY, dividerWidth, dividerHeight,
				180, 184, 190, 150 );
			x += dividerWidth + dividerGap;

			if( reserveWidth > 0 )
			{
				CsDrawText( reserveBuffer, x, bottom - reserveHeight, reserveHeight,
					198, 202, 208, 235 );
				x += reserveWidth + gap;
			}

			CsDrawIcon( s_iconAmmo, x, bottom - ammoIconSize, ammoIconSize,
				238, 239, 240, 245 );
		}
	}

	if( !( gHUD.m_iHideHUDDisplay & HIDEHUD_TIMER ) )
	{
		int timerHeight = (int)( 16 * scale );
		int timerWidth = 0;
		int timerX = 0;
		int remaining = gHUD.m_Timer.GetRemainingSeconds();

		if( remaining >= 0 )
		{
			char timerBuffer[16];
			sprintf( timerBuffer, "%02d:%02d", remaining / 60, remaining % 60 );

			timerWidth = CsTextWidth( timerBuffer, timerHeight );
			timerX = ( ScreenWidth - timerWidth ) / 2;
			CsDrawText( timerBuffer, timerX, (int)( 8 * scale ), timerHeight,
				238, 239, 240, 245 );
		}

		int ctPlayers = 0;
		int tPlayers = 0;
		int ctAlive = 0;
		int tAlive = 0;

		for( int i = 1; i <= MAX_PLAYERS; ++i )
		{
			if( g_PlayerExtraInfo[i].teamnumber == TEAM_CT )
			{
				++ctPlayers;
				if( !g_PlayerExtraInfo[i].dead )
					++ctAlive;
			}
			else if( g_PlayerExtraInfo[i].teamnumber == TEAM_TERRORIST )
			{
				++tPlayers;
				if( !g_PlayerExtraInfo[i].dead )
					++tAlive;
			}
		}

		if( remaining >= 0 && gHUD.m_Teamplay && ( ctPlayers > 0 || tPlayers > 0 ) )
		{
			int ctScore = 0;
			int tScore = 0;

			for( int i = 1; i <= gHUD.m_Scoreboard.m_iNumTeams; ++i )
			{
				if( g_TeamInfo[i].teamnumber == TEAM_CT )
					ctScore = g_TeamInfo[i].frags;
				else if( g_TeamInfo[i].teamnumber == TEAM_TERRORIST )
					tScore = g_TeamInfo[i].frags;
			}

			int iconSize = (int)( 20 * scale );
			int iconY = (int)( 6 * scale );
			int sideGap = (int)( 10 * scale );
			int teamGap = (int)( 4 * scale );
			int scoreHeight = (int)( 16 * scale );
			int aliveHeight = (int)( 7 * scale );
			int scoreY = iconY + ( iconSize - scoreHeight ) / 2;
			int aliveY = iconY + iconSize + (int)( 1 * scale );

			char ctScoreBuffer[16];
			char tScoreBuffer[16];
			char ctAliveBuffer[16];
			char tAliveBuffer[16];
			sprintf( ctScoreBuffer, "%d", ctScore );
			sprintf( tScoreBuffer, "%d", tScore );
			sprintf( ctAliveBuffer, "%d", ctAlive );
			sprintf( tAliveBuffer, "%d", tAlive );

			int ctScoreWidth = CsTextWidth( ctScoreBuffer, scoreHeight );
			int tScoreWidth = CsTextWidth( tScoreBuffer, scoreHeight );
			int ctAliveWidth = CsTextWidth( ctAliveBuffer, aliveHeight );
			int tAliveWidth = CsTextWidth( tAliveBuffer, aliveHeight );

			int ctScoreX = timerX - sideGap - ctScoreWidth;
			int ctIconX = ctScoreX - teamGap - iconSize;
			int tScoreX = timerX + timerWidth + sideGap;
			int tIconX = tScoreX + tScoreWidth + teamGap;

			CsDrawIconAdditive( s_iconTeamCT, ctIconX, iconY, iconSize, 255, 255, 255, 245 );
			CsDrawIconAdditive( s_iconTeamT, tIconX, iconY, iconSize, 255, 255, 255, 245 );

			CsDrawText( ctScoreBuffer, ctScoreX, scoreY,
				scoreHeight, 131, 165, 222, 240 );
			CsDrawText( tScoreBuffer, tScoreX, scoreY,
				scoreHeight, 232, 197, 111, 240 );

			CsDrawText( ctAliveBuffer, ctIconX + ( iconSize - ctAliveWidth ) / 2, aliveY,
				aliveHeight, 220, 220, 220, 200 );
			CsDrawText( tAliveBuffer, tIconX + ( iconSize - tAliveWidth ) / 2, aliveY,
				aliveHeight, 220, 220, 220, 200 );
		}
	}

	return 1;
}
