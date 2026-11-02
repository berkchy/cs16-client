//
// scoreboard2.cpp
//
// Nexora scoreboard renderer built on the engine console font.
//

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include "draw_util.h"
#include "vgui_parser.h"
#include "cdll_dll.h"

extern int xstart, xend, ystart, yend;

namespace
{

const int SB_OTHER_TEAM = -1;

struct SbConfig
{
	bool showMoney;
	bool showHp;
	bool showPing;
	bool showFlags;
	bool showVersion;
	bool highContrast;
	int alpha;
};

struct SbPlayer
{
	int index;
	const char *name;
	short frags;
	short deaths;
	short ping;
	short health;
	short money;
	int team;
	bool dead;
	bool bot;
	bool hasC4;
	bool vip;
	bool defuse;
	bool self;
};

struct SbTeam
{
	int number;
	int players;
	int alive;
	int frags;
	int pingSum;
};

struct SbColumns
{
	int nameX;
	int nameMax;
	int pingLeft;
	int pingRight;
	int killLeft;
	int killRight;
	int deathLeft;
	int deathRight;
	int moneyLeft;
	int moneyRight;
	bool showPing;
	bool showKills;
	bool showDeaths;
	bool showMoney;
};

int SbClamp( int value, int low, int high )
{
	if( value < low )
		return low;
	if( value > high )
		return high;
	return value;
}

int SbTextWidth( const char *text )
{
	if( !text || !text[0] )
		return 0;

	int width = 0;
	int height = 0;
	gEngfuncs.pfnDrawConsoleStringLen( text, &width, &height );
	return width;
}

int SbTextHeight()
{
	int width = 0;
	int height = 0;
	gEngfuncs.pfnDrawConsoleStringLen( "Ag", &width, &height );
	return height > 0 ? height : 12;
}

void SbColor( int r, int g, int b )
{
	gEngfuncs.pfnDrawSetTextColor( r / 255.0f, g / 255.0f, b / 255.0f );
}

void SbText( int x, int y, const char *text )
{
	if( text && text[0] )
		gEngfuncs.pfnDrawConsoleString( x, y, (char *)text );
}

void SbTextRight( int right, int y, const char *text )
{
	SbText( right - SbTextWidth( text ), y, text );
}

void SbTextCenter( int center, int y, const char *text )
{
	SbText( center - SbTextWidth( text ) / 2, y, text );
}

void SbTextClipped( int x, int y, int maxWidth, const char *text )
{
	if( !text || !text[0] || maxWidth <= 0 )
		return;

	char buffer[256];
	int length = 0;

	for( const char *p = text; *p && length < (int)sizeof( buffer ) - 1; ++p )
	{
		buffer[length++] = *p;
		buffer[length] = 0;

		if( SbTextWidth( buffer ) > maxWidth )
		{
			buffer[length - 1] = 0;
			break;
		}
	}

	SbText( x, y, buffer );
}

void SbFill( int x, int y, int width, int height, int r, int g, int b, int a )
{
	if( width > 0 && height > 0 )
		FillRGBABlend( x, y, width, height, r, g, b, a );
}

void SbPanel( int x, int y, int width, int height, int r, int g, int b, int a )
{
	if( width <= 0 || height <= 0 )
		return;

	FillRGBABlend( x, y, width, height, r, g, b, a );
	FillRGBABlend( x, y, width, height / 3, 255, 255, 255, 3 );
	FillRGBA( x, y, width, 1, 255, 255, 255, 26 );
	FillRGBA( x, y + height - 1, width, 1, 0, 0, 0, 90 );
	FillRGBA( x, y, 1, height, 255, 255, 255, 14 );
	FillRGBA( x + width - 1, y, 1, height, 255, 255, 255, 14 );
}

void SbTeamColor( int team, int &r, int &g, int &b )
{
	switch( team )
	{
	case TEAM_CT:
		r = 92; g = 148; b = 212;
		break;
	case TEAM_TERRORIST:
		r = 212; g = 150; b = 84;
		break;
	default:
		r = 150; g = 156; b = 164;
		break;
	}
}

void SbPlayerColor( const SbPlayer &player, int &r, int &g, int &b )
{
	if( player.dead )
	{
		r = 132; g = 134; b = 138;
		return;
	}

	r = 232; g = 234; b = 236;
}

void SbPingColor( int ping, bool bot, int &r, int &g, int &b )
{
	if( bot )
	{
		r = 142; g = 146; b = 152;
	}
	else if( ping >= 150 )
	{
		r = 208; g = 132; b = 132;
	}
	else
	{
		r = 180; g = 184; b = 190;
	}
}

void SbHealthColor( int health, int &r, int &g, int &b )
{
	if( health <= 25 )
	{
		r = 216; g = 124; b = 124;
	}
	else
	{
		r = 226; g = 228; b = 230;
	}
}

void SbRankColor( int rank, bool topThree, int &r, int &g, int &b )
{
	if( topThree && rank == 1 )
	{
		r = 236; g = 236; b = 232;
	}
	else if( topThree && rank == 2 )
	{
		r = 198; g = 202; b = 208;
	}
	else if( topThree && rank == 3 )
	{
		r = 174; g = 168; b = 158;
	}
	else
	{
		r = 132; g = 136; b = 142;
	}
}

bool SbPlayerBefore( const SbPlayer &a, const SbPlayer &b )
{
	if( a.frags != b.frags )
		return a.frags > b.frags;
	if( a.deaths != b.deaths )
		return a.deaths < b.deaths;
	return stricmp( a.name, b.name ) < 0;
}

void SbSortPlayers( SbPlayer *players, int count )
{
	for( int i = 1; i < count; ++i )
	{
		SbPlayer key = players[i];
		int j = i - 1;

		while( j >= 0 && SbPlayerBefore( key, players[j] ) )
		{
			players[j + 1] = players[j];
			--j;
		}

		players[j + 1] = key;
	}
}

int SbCollectPlayers( SbPlayer *players, int maxPlayers )
{
	int count = 0;

	for( int i = 1; i <= MAX_PLAYERS && count < maxPlayers; ++i )
	{
		hud_player_info_t *info = &g_PlayerInfoList[i];
		if( !info->name || !info->name[0] )
			continue;

		extra_player_info_t *extra = &g_PlayerExtraInfo[i];
		SbPlayer &player = players[count++];

		player.index = i;
		player.name = info->name;
		player.frags = extra->frags;
		player.deaths = extra->deaths;
		player.ping = info->ping;
		player.health = extra->sb_health;
		player.money = extra->sb_account;
		player.dead = extra->dead != 0;
		player.hasC4 = extra->has_c4 != 0;
		player.vip = extra->vip != 0;
		player.defuse = extra->has_defuse_kit != 0;
		player.self = info->thisplayer != 0;
		player.team = extra->teamnumber;

		if( extra->teamname[0] )
		{
			if( !stricmp( extra->teamname, "CT" ) )
				player.team = TEAM_CT;
			else if( !stricmp( extra->teamname, "TERRORIST" ) )
				player.team = TEAM_TERRORIST;
			else if( !stricmp( extra->teamname, "SPECTATOR" ) )
				player.team = TEAM_SPECTATOR;
			else if( !stricmp( extra->teamname, "UNASSIGNED" ) )
				player.team = TEAM_UNASSIGNED;
		}

		const char *bot = gEngfuncs.PlayerInfo_ValueForKey( i, "*bot" );
		player.bot = player.ping <= 5 && bot && atoi( bot ) > 0;
	}

	SbSortPlayers( players, count );
	return count;
}

void SbBuildColumns( SbColumns &columns, int x, int width, const SbConfig &config )
{
	columns.showPing = config.showPing && width >= 170;
	columns.showKills = width >= 110;
	columns.showDeaths = width >= 150;
	columns.showMoney = config.showMoney && width >= 260;

	const int gap = 2;
	const int pingWidth = 38;
	const int deathWidth = 34;
	const int killWidth = 34;
	const int moneyWidth = 66;

	int right = x + width - 2;

	columns.pingRight = right;
	columns.pingLeft = right - pingWidth;
	if( columns.showPing )
		right -= pingWidth + gap;

	columns.deathRight = right;
	columns.deathLeft = right - deathWidth;
	if( columns.showDeaths )
		right -= deathWidth + gap;

	columns.killRight = right;
	columns.killLeft = right - killWidth;
	if( columns.showKills )
		right -= killWidth + gap;

	columns.moneyRight = right;
	columns.moneyLeft = right - moneyWidth;
	if( columns.showMoney )
		right -= moneyWidth + gap;

	columns.nameX = x + 6;
	columns.nameMax = right - columns.nameX - 4;
	if( columns.nameMax < 40 )
		columns.nameMax = 40;
}

void SbDrawColumnHeaders( const SbColumns &columns, int y, int width, int r, int g, int b )
{
	SbColor( r, g, b );
	SbText( columns.nameX, y, "PLAYER" );

	if( columns.showMoney )
		SbTextRight( columns.moneyRight - 4, y, "MONEY" );
	if( columns.showKills )
		SbTextRight( columns.killRight - 4, y, "K" );
	if( columns.showDeaths )
		SbTextRight( columns.deathRight - 4, y, "D" );
	if( columns.showPing )
		SbTextRight( columns.pingRight - 4, y, "PING" );

	FillRGBA( columns.nameX - 2, y + SbTextHeight() + 2, width, 1, 255, 255, 255, 18 );
}

int SbDrawPlayerRows(
	int x,
	int y,
	int width,
	int maxRows,
	int rowHeight,
	const SbPlayer *players,
	int count,
	int teamFilter,
	bool showTopRanks,
	const SbConfig &config )
{
	int drawn = 0;
	int hidden = 0;
	int textHeight = SbTextHeight();
	SbColumns columns;

	SbBuildColumns( columns, x, width, config );

	int nameCellRight = x + width;
	if( columns.showMoney )
		nameCellRight = columns.moneyLeft - 2;
	else if( columns.showKills )
		nameCellRight = columns.killLeft - 2;
	else if( columns.showDeaths )
		nameCellRight = columns.deathLeft - 2;
	else if( columns.showPing )
		nameCellRight = columns.pingLeft - 2;

	for( int i = 0; i < count; ++i )
	{
		const SbPlayer &player = players[i];

		if( teamFilter == SB_OTHER_TEAM )
		{
			if( player.team == TEAM_CT || player.team == TEAM_TERRORIST )
				continue;
		}
		else if( teamFilter != 0 && player.team != teamFilter )
		{
			continue;
		}

		if( drawn >= maxRows )
		{
			++hidden;
			continue;
		}

		int rowY = y + drawn * rowHeight;
		int cellHeight = rowHeight - 1;
		int textY = rowY + ( cellHeight - textHeight ) / 2;

		SbFill( x, rowY, nameCellRight - x, cellHeight, 255, 255, 255, 6 );
		if( columns.showMoney )
			SbFill( columns.moneyLeft, rowY, columns.moneyRight - columns.moneyLeft, cellHeight, 255, 255, 255, 5 );
		if( columns.showKills )
			SbFill( columns.killLeft, rowY, columns.killRight - columns.killLeft, cellHeight, 255, 255, 255, 5 );
		if( columns.showDeaths )
			SbFill( columns.deathLeft, rowY, columns.deathRight - columns.deathLeft, cellHeight, 255, 255, 255, 5 );
		if( columns.showPing )
			SbFill( columns.pingLeft, rowY, columns.pingRight - columns.pingLeft, cellHeight, 255, 255, 255, 5 );

		if( player.self )
		{
			int tr, tg, tb;
			SbTeamColor( player.team, tr, tg, tb );
			FillRGBABlend( x, rowY, width, cellHeight, tr, tg, tb, 18 );
		}

		int r = 232, g = 234, b = 236;
		SbPlayerColor( player, r, g, b );
		if( config.highContrast )
		{
			r = player.dead ? 150 : 250;
			g = player.dead ? 152 : 252;
			b = player.dead ? 156 : 255;
		}
		SbColor( r, g, b );
		SbTextClipped( columns.nameX, textY, columns.nameMax, player.name );

		if( columns.showKills )
		{
			char buffer[16];
			snprintf( buffer, sizeof( buffer ), "%d", player.frags );
			SbColor( player.dead ? 140 : 238, player.dead ? 142 : 242, player.dead ? 146 : 248 );
			SbTextRight( columns.killRight - 4, textY, buffer );
		}

		if( columns.showDeaths )
		{
			char buffer[16];
			snprintf( buffer, sizeof( buffer ), "%d", player.deaths );
			SbColor( 174, 178, 184 );
			SbTextRight( columns.deathRight - 4, textY, buffer );
		}

		if( columns.showMoney && player.money >= 0 )
		{
			char buffer[24];
			snprintf( buffer, sizeof( buffer ), "$%d", player.money );
			SbColor( 206, 208, 204 );
			SbTextRight( columns.moneyRight - 4, textY, buffer );
		}

		if( columns.showPing )
		{
			char buffer[16];
			if( player.bot )
			{
				snprintf( buffer, sizeof( buffer ), "BOT" );
				SbColor( 146, 150, 156 );
			}
			else
			{
				snprintf( buffer, sizeof( buffer ), "%d", player.ping );
				SbPingColor( player.ping, false, r, g, b );
				SbColor( r, g, b );
			}
			SbTextRight( columns.pingRight - 4, textY, buffer );
		}

		++drawn;
	}

	if( hidden > 0 )
	{
		char buffer[48];
		snprintf( buffer, sizeof( buffer ), "+%d more", hidden );
		SbColor( 145, 150, 158 );
		SbText( x + 6, y + drawn * rowHeight + 4, buffer );
	}

	return drawn;
}

void SbDrawTeam(
	const SbTeam &team,
	int x,
	int y,
	int width,
	int height,
	int rowHeight,
	const SbPlayer *players,
	int count,
	const SbConfig &config )
{
	if( width <= 0 || height <= 0 )
		return;

	int r, g, b;
	SbTeamColor( team.number, r, g, b );

	int infoWidth = width < 460 ? 62 : 88;
	const char *name = team.number == TEAM_CT
		? Localize( "#Cstrike_ScoreBoard_CT" )
		: Localize( "#Cstrike_ScoreBoard_Ter" );

	SbColor( r, g, b );
	SbTextClipped( x + 4, y + 2, infoWidth - 8, name );

	char scoreBuffer[24];
	snprintf( scoreBuffer, sizeof( scoreBuffer ), "%d", team.frags );
	float scoreScale = width < 600 ? 1.8f : 2.4f;
	int scoreWidth = DrawUtils::HudStringLen( scoreBuffer, scoreScale );
	int scoreY = y + SbTextHeight() + 6;
	DrawUtils::DrawHudString(
		x + ( infoWidth - scoreWidth ) / 2,
		scoreY,
		x + infoWidth - 2,
		scoreBuffer,
		238, 239, 240,
		scoreScale );

	SbDrawPlayerRows(
		x + infoWidth,
		y + 4,
		width - infoWidth,
		team.players > 0 ? team.players : 0,
		rowHeight,
		players,
		count,
		team.number,
		false,
		config );
}

void SbDrawSpectators(
	const SbPlayer *players,
	int count,
	int x,
	int y,
	int width,
	const SbConfig &config )
{
	char buffer[512];
	buffer[0] = 0;
	int length = 0;

	for( int i = 0; i < count; ++i )
	{
		const SbPlayer &player = players[i];
		if( player.team == TEAM_CT || player.team == TEAM_TERRORIST )
			continue;

		if( length > 0 && length < (int)sizeof( buffer ) - 3 )
		{
			buffer[length++] = ',';
			buffer[length++] = ' ';
			buffer[length] = 0;
		}

		int nameLength = (int)strlen( player.name );
		if( length + nameLength >= (int)sizeof( buffer ) - 1 )
			break;

		strncpy( buffer + length, player.name, sizeof( buffer ) - length - 1 );
		length += nameLength;
		buffer[length] = 0;
	}

	if( length == 0 )
		return;

	SbColor( 145, 150, 158 );
	SbText( x, y, "SPECTATORS:" );
	SbColor( 205, 210, 216 );
	SbTextClipped( x + SbTextWidth( "SPECTATORS:" ) + 6, y, width - SbTextWidth( "SPECTATORS:" ) - 6, buffer );
}

}

int CHudScoreboard :: DrawScoreboard2( float flTime )
{
	GetAllPlayersInfo();

	SbPlayer players[MAX_PLAYERS + 1];
	int playerCount = SbCollectPlayers( players, MAX_PLAYERS + 1 );

	SbConfig config;
	config.showMoney = cl_scoreboard_show_money && cl_scoreboard_show_money->value != 0.0f;
	config.showHp = cl_scoreboard_show_hp && cl_scoreboard_show_hp->value != 0.0f;
	config.showPing = cl_scoreboard_show_ping && cl_scoreboard_show_ping->value != 0.0f;
	config.showFlags = cl_scoreboard_show_flags && cl_scoreboard_show_flags->value != 0.0f;
	config.showVersion = cl_showplayerversion && cl_showplayerversion->value != 0.0f;
	config.highContrast = cl_scoreboard_high_contrast && cl_scoreboard_high_contrast->value != 0.0f;
	config.alpha = cl_scoreboard_alpha ? SbClamp( (int)cl_scoreboard_alpha->value, 170, 235 ) : 220;

	int screenWidth = ScreenWidth;
	int screenHeight = ScreenHeight;

	SbTeam ctTeam;
	SbTeam tTeam;
	memset( &ctTeam, 0, sizeof( ctTeam ) );
	memset( &tTeam, 0, sizeof( tTeam ) );
	ctTeam.number = TEAM_CT;
	tTeam.number = TEAM_TERRORIST;

	int otherPlayers = 0;
	for( int i = 0; i < playerCount; ++i )
	{
		const SbPlayer &player = players[i];

		if( player.team == TEAM_CT )
		{
			++ctTeam.players;
			if( !player.dead ) ++ctTeam.alive;
			ctTeam.frags += player.frags;
			ctTeam.pingSum += player.ping;
		}
		else if( player.team == TEAM_TERRORIST )
		{
			++tTeam.players;
			if( !player.dead ) ++tTeam.alive;
			tTeam.frags += player.frags;
			tTeam.pingSum += player.ping;
		}
		else
		{
			++otherPlayers;
		}
	}

	bool teamMode = gHUD.m_Teamplay != 0 && ( ctTeam.players > 0 || tTeam.players > 0 );

	const int headerHeight = 36;
	const int labelHeight = SbTextHeight() + 6;
	const int footerHeight = 20;
	const int padding = 14;
	const int teamGap = 8;
	const int spectatorHeight = otherPlayers > 0 ? SbTextHeight() + 4 : 0;

	int maxPanelHeight = screenHeight - 32;
	if( maxPanelHeight < 180 )
		maxPanelHeight = screenHeight - 8;

	int rowHeight;
	int ctHeight;
	int tHeight;

	if( teamMode )
	{
		int rowCount = ( ctTeam.players > 0 ? ctTeam.players : 1 ) + ( tTeam.players > 0 ? tTeam.players : 1 );
		int fixedHeight = headerHeight + labelHeight + footerHeight + padding * 2 + teamGap + spectatorHeight + 16;
		rowHeight = SbClamp( ( maxPanelHeight - fixedHeight ) / ( rowCount > 0 ? rowCount : 1 ), 13, 20 );
		ctHeight = ( ctTeam.players > 0 ? ctTeam.players : 1 ) * rowHeight + 10;
		tHeight = ( tTeam.players > 0 ? tTeam.players : 1 ) * rowHeight + 10;
	}
	else
	{
		int fixedHeight = headerHeight + labelHeight + footerHeight + padding * 2 + spectatorHeight + 8;
		rowHeight = SbClamp( ( maxPanelHeight - fixedHeight ) / ( playerCount > 0 ? playerCount : 1 ), 13, 20 );
		ctHeight = 0;
		tHeight = 0;
	}

	int contentHeight = headerHeight + labelHeight + footerHeight + padding * 2 + spectatorHeight + 8;
	if( teamMode )
		contentHeight += ctHeight + tHeight + teamGap;
	else
		contentHeight += ( playerCount > 0 ? playerCount : 1 ) * rowHeight;

	int panelWidth = (int)( screenWidth * 0.94f );
	if( panelWidth > 1000 )
		panelWidth = 1000;
	if( panelWidth < 280 )
		panelWidth = screenWidth - 16;

	int panelHeight = contentHeight;
	if( panelHeight > maxPanelHeight )
		panelHeight = maxPanelHeight;

	int panelX = ( screenWidth - panelWidth ) / 2;
	int panelY = ( screenHeight - panelHeight ) / 2;

	if( m_bForceDraw && xend > xstart && yend > ystart )
	{
		panelX = xstart;
		panelY = ystart;
		panelWidth = xend - xstart;
		panelHeight = yend - ystart;
	}

	if( panelWidth < 240 || panelHeight < 150 )
	{
		panelX = 8;
		panelY = 8;
		panelWidth = screenWidth - 16;
		panelHeight = screenHeight - 16;
	}

	int panelRight = panelX + panelWidth;
	int panelBottom = panelY + panelHeight;

	SbFill( 0, 0, screenWidth, screenHeight, 0, 0, 0, config.highContrast ? 170 : 110 );
	FillRGBABlend( panelX, panelY, panelWidth, panelHeight, 0, 0, 0, config.alpha );
	FillRGBA( panelX, panelY, panelWidth, 1, 255, 255, 255, 16 );
	FillRGBA( panelX, panelBottom - 1, panelWidth, 1, 0, 0, 0, 140 );

	int mutedR = config.highContrast ? 210 : 150;
	int mutedG = config.highContrast ? 212 : 154;
	int mutedB = config.highContrast ? 214 : 160;

	const char *serverName = gHUD.m_szServerName[0] ? gHUD.m_szServerName : "NEXORA";
	SbColor( 238, 239, 240 );
	SbTextClipped( panelX + padding, panelY + 9, panelWidth - 130, serverName );

	char countBuffer[48];
	snprintf( countBuffer, sizeof( countBuffer ), "%d PLAYERS", playerCount );
	SbColor( mutedR, mutedG, mutedB );
	SbTextRight( panelRight - padding, panelY + 9, countBuffer );

	FillRGBA( panelX + padding, panelY + 27, panelWidth - padding * 2, 1, 255, 255, 255, 26 );

	int contentTop = panelY + headerHeight;
	int footerY = panelBottom - footerHeight + 2;

	int infoWidth = ( panelWidth - padding * 2 ) < 460 ? 62 : 88;
	int tableX = panelX + padding + infoWidth;
	int tableWidth = panelWidth - padding * 2 - infoWidth;

	SbColumns columns;
	SbBuildColumns( columns, tableX, tableWidth, config );

	if( teamMode )
	{
		SbDrawColumnHeaders( columns, contentTop, tableWidth, mutedR, mutedG, mutedB );

		int ctY = contentTop + labelHeight;
		SbDrawTeam(
			ctTeam,
			panelX + padding,
			ctY,
			panelWidth - padding * 2,
			ctHeight,
			rowHeight,
			players,
			playerCount,
			config );

		int tY = ctY + ctHeight + teamGap;
		SbDrawTeam(
			tTeam,
			panelX + padding,
			tY,
			panelWidth - padding * 2,
			tHeight,
			rowHeight,
			players,
			playerCount,
			config );

		if( otherPlayers > 0 )
			SbDrawSpectators( players, playerCount, panelX + padding, tY + tHeight + 4, panelWidth - padding * 2, config );
	}
	else
	{
		SbDrawColumnHeaders( columns, contentTop, tableWidth, mutedR, mutedG, mutedB );

		int maxRows = ( footerY - contentTop - labelHeight - 8 ) / rowHeight;
		SbDrawPlayerRows(
			tableX,
			contentTop + labelHeight,
			tableWidth,
			maxRows,
			rowHeight,
			players,
			playerCount,
			0,
			false,
			config );
	}

	SbColor( mutedR, mutedG, mutedB );
	SbText( panelX + padding, footerY, "TAB / TAP SCOREBOARD TO CLOSE" );

	return 1;
}
