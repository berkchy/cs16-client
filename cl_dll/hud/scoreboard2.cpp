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
	int rankX;
	int nameX;
	int nameMax;
	int pingRight;
	int killRight;
	int deathRight;
	int hpRight;
	int moneyRight;
	int attrRight;
	bool showPing;
	bool showKills;
	bool showDeaths;
	bool showHp;
	bool showMoney;
	bool showFlags;
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

void SbBuildColumns( SbColumns &columns, int x, int width, const SbConfig &config, bool teamPanel )
{
	columns.rankX = x + 8;
	columns.nameX = x + 30;

	int right = x + width - 10;
	columns.showPing = config.showPing && width >= ( teamPanel ? 230 : 280 );
	columns.showKills = true;
	columns.showDeaths = width >= ( teamPanel ? 220 : 240 );
	columns.showHp = config.showHp && width >= ( teamPanel ? 330 : 430 );
	columns.showMoney = config.showMoney && width >= ( teamPanel ? 390 : 520 );
	columns.showFlags = config.showFlags && width >= ( teamPanel ? 280 : 360 );

	int numberWidth = SbTextWidth( "999" );
	int hpWidth = SbTextWidth( "100" );
	int moneyWidth = SbTextWidth( "$16000" );
	int flagWidth = SbTextWidth( "DEAD" );

	columns.pingRight = right;
	if( columns.showPing )
		right -= numberWidth + 10;

	columns.deathRight = right;
	if( columns.showDeaths )
		right -= numberWidth + 10;

	columns.killRight = right;
	if( columns.showKills )
		right -= numberWidth + 10;

	columns.hpRight = right;
	if( columns.showHp )
		right -= hpWidth + 10;

	columns.moneyRight = right;
	if( columns.showMoney )
		right -= moneyWidth + 10;

	columns.attrRight = right;
	if( columns.showFlags )
		right -= flagWidth + 8;

	columns.nameMax = right - columns.nameX - 4;
	if( columns.nameMax < 36 )
		columns.nameMax = 36;
}

void SbDrawColumnHeaders( const SbColumns &columns, int y, int width, int r, int g, int b )
{
	FillRGBABlend( columns.rankX - 6, y - 3, width, 14, 0, 0, 0, 96 );
	FillRGBA( columns.rankX - 6, y - 4, width, 1, 255, 255, 255, 16 );
	SbColor( r, g, b );
	SbText( columns.rankX, y, "#" );
	SbText( columns.nameX, y, "PLAYER" );

	if( columns.showKills )
		SbTextRight( columns.killRight, y, "K" );
	if( columns.showDeaths )
		SbTextRight( columns.deathRight, y, "D" );
	if( columns.showHp )
		SbTextRight( columns.hpRight, y, "HP" );
	if( columns.showMoney )
		SbTextRight( columns.moneyRight, y, "MONEY" );
	if( columns.showPing )
		SbTextRight( columns.pingRight, y, "PING" );
	if( columns.showFlags )
		SbTextRight( columns.attrRight, y, "FLAG" );
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
	int rank = 0;
	int hidden = 0;
	SbColumns columns;

	SbBuildColumns( columns, x, width, config, teamFilter == TEAM_CT || teamFilter == TEAM_TERRORIST );

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

		++rank;

		if( drawn >= maxRows )
		{
			++hidden;
			continue;
		}

		int rowY = y + drawn * rowHeight;

		if( player.self )
		{
			FillRGBABlend( x, rowY - 1, width, rowHeight, 255, 255, 255, 18 );
			FillRGBA( x, rowY - 1, 2, rowHeight, 235, 235, 232, 210 );
		}
		else if( drawn % 2 == 1 )
		{
			FillRGBABlend( x, rowY - 1, width, rowHeight, 255, 255, 255, 6 );
		}

		int r = 220, g = 224, b = 228;
		SbRankColor( rank, showTopRanks, r, g, b );
		SbColor( r, g, b );
		char rankBuffer[8];
		snprintf( rankBuffer, sizeof( rankBuffer ), "%d", rank );
		SbText( columns.rankX, rowY, rankBuffer );

		SbPlayerColor( player, r, g, b );
		if( config.highContrast )
		{
			r = player.dead ? 170 : 250;
			g = player.dead ? 174 : 252;
			b = player.dead ? 180 : 255;
		}
		SbColor( r, g, b );
		SbTextClipped( columns.nameX, rowY, columns.nameMax, player.name );

		if( columns.showFlags )
		{
			const char *flag = NULL;
			int fr = 170, fg = 176, fb = 184;

			if( config.showVersion )
			{
				const char *version = gEngfuncs.PlayerInfo_ValueForKey( player.index, "cscl_ver" );
				if( version && version[0] )
				{
					flag = version;
					fr = 142; fg = 146; fb = 152;
				}
			}
			else if( player.dead )
			{
				flag = "DEAD"; fr = 198; fg = 130; fb = 130;
			}
			else if( player.hasC4 )
			{
				flag = "BOMB"; fr = 214; fg = 180; fb = 108;
			}
			else if( player.vip )
			{
				flag = "VIP"; fr = 140; fg = 178; fb = 216;
			}
			else if( player.defuse )
			{
				flag = "KIT"; fr = 144; fg = 198; fb = 162;
			}

			if( flag )
			{
				SbColor( fr, fg, fb );
				SbTextRight( columns.attrRight, rowY, flag );
			}
		}

		SbColor( r, g, b );

		if( columns.showKills )
		{
			char buffer[16];
			snprintf( buffer, sizeof( buffer ), "%d", player.frags );
			SbColor( player.dead ? 150 : 238, player.dead ? 154 : 242, player.dead ? 160 : 248 );
			SbTextRight( columns.killRight, rowY, buffer );
		}

		if( columns.showDeaths )
		{
			char buffer[16];
			snprintf( buffer, sizeof( buffer ), "%d", player.deaths );
			SbColor( 170, 176, 184 );
			SbTextRight( columns.deathRight, rowY, buffer );
		}

		if( columns.showHp && !player.dead && player.health >= 0 )
		{
			char buffer[16];
			snprintf( buffer, sizeof( buffer ), "%d", player.health );
			SbHealthColor( player.health, r, g, b );
			SbColor( r, g, b );
			SbTextRight( columns.hpRight, rowY, buffer );
		}

		if( columns.showMoney && player.money >= 0 )
		{
			char buffer[24];
			snprintf( buffer, sizeof( buffer ), "$%d", player.money );
			SbColor( 210, 210, 206 );
			SbTextRight( columns.moneyRight, rowY, buffer );
		}

		if( columns.showPing )
		{
			char buffer[16];
			if( player.bot )
			{
				snprintf( buffer, sizeof( buffer ), "BOT" );
				SbColor( 150, 156, 164 );
			}
			else
			{
				snprintf( buffer, sizeof( buffer ), "%d", player.ping );
				SbPingColor( player.ping, false, r, g, b );
				SbColor( r, g, b );
			}
			SbTextRight( columns.pingRight, rowY, buffer );
		}

		++drawn;
	}

	if( hidden > 0 )
	{
		char buffer[48];
		snprintf( buffer, sizeof( buffer ), "+%d more", hidden );
		SbColor( 145, 150, 158 );
		SbText( x + 30, y + drawn * rowHeight, buffer );
	}

	return drawn;
}

void SbDrawTeam(
	const SbTeam &team,
	int x,
	int y,
	int width,
	int height,
	const SbPlayer *players,
	int count,
	const SbConfig &config )
{
	if( width <= 0 || height <= 0 )
		return;

	int r, g, b;
	SbTeamColor( team.number, r, g, b );

	SbPanel( x, y, width, height, 0, 0, 0, config.alpha );
	FillRGBABlend( x, y, width, 29, r, g, b, 46 );
	FillRGBA( x, y, width, 2, r, g, b, 224 );

	const char *name = team.number == TEAM_CT
		? Localize( "#Cstrike_ScoreBoard_CT" )
		: Localize( "#Cstrike_ScoreBoard_Ter" );

	SbColor( r, g, b );
	SbTextClipped( x + 12, y + 7, width - 90, name );

	char scoreBuffer[24];
	snprintf( scoreBuffer, sizeof( scoreBuffer ), "%d", team.frags );
	SbColor( 240, 241, 242 );
	SbTextRight( x + width - 12, y + 7, scoreBuffer );

	char subBuffer[80];
	int averagePing = team.players > 0 ? team.pingSum / team.players : 0;
	snprintf( subBuffer, sizeof( subBuffer ), "%d PLAYERS  %d ALIVE  %d MS", team.players, team.alive, averagePing );
	SbColor( 150, 153, 158 );
	SbTextClipped( x + 12, y + 20, width - 24, subBuffer );

	SbColumns columns;
	SbBuildColumns( columns, x, width, config, true );
	SbDrawColumnHeaders( columns, y + 33, width - 8, 148, 150, 154 );

	int headerHeight = 46;
	int maxRows = ( height - headerHeight - 4 ) / 16;
	SbDrawPlayerRows(
		x + 2,
		y + headerHeight,
		width - 4,
		maxRows,
		16,
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
	config.alpha = cl_scoreboard_alpha ? SbClamp( (int)cl_scoreboard_alpha->value, 120, 220 ) : 185;

	int screenWidth = ScreenWidth;
	int screenHeight = ScreenHeight;

	int x = (int)( screenWidth * 0.035f );
	int y = (int)( screenHeight * 0.09f );
	int width = screenWidth - x * 2;
	int height = screenHeight - y * 2;

	if( m_bForceDraw && xend > xstart && yend > ystart )
	{
		x = xstart;
		y = ystart;
		width = xend - xstart;
		height = yend - ystart;
	}

	if( width < 240 || height < 150 )
	{
		x = 8;
		y = 8;
		width = screenWidth - 16;
		height = screenHeight - 16;
	}

	SbFill( 0, 0, screenWidth, screenHeight, 0, 0, 0, config.highContrast ? 150 : 85 );
	SbPanel( x, y, width, height, 0, 0, 0, config.alpha );
	FillRGBA( x, y, width, 1, 255, 255, 255, 42 );

	int textR = 238;
	int textG = 239;
	int textB = 240;
	int mutedR = config.highContrast ? 210 : 154;
	int mutedG = config.highContrast ? 212 : 157;
	int mutedB = config.highContrast ? 214 : 162;

	const char *serverName = gHUD.m_szServerName[0] ? gHUD.m_szServerName : "NEXORA";
	SbColor( textR, textG, textB );
	SbTextClipped( x + 14, y + 9, width - 130, serverName );

	char countBuffer[48];
	snprintf( countBuffer, sizeof( countBuffer ), "%d PLAYERS", playerCount );
	SbColor( mutedR, mutedG, mutedB );
	SbTextRight( x + width - 14, y + 9, countBuffer );

	SbColor( mutedR, mutedG, mutedB );
	SbText( x + 14, y + 22, "NEXORA // SCOREBOARD" );

	FillRGBA( x + 14, y + 36, width - 28, 1, 255, 255, 255, 28 );

	int contentTop = y + 46;
	int footerY = y + height - 18;

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

	if( teamMode )
	{
		int reserve = otherPlayers > 0 ? 24 : 0;
		int areaBottom = footerY - 4 - reserve;
		int gap = 10;
		bool stacked = width < 760;

		if( stacked )
		{
			int panelHeight = ( areaBottom - contentTop - gap ) / 2;
			SbDrawTeam( ctTeam, x + 10, contentTop, width - 20, panelHeight, players, playerCount, config );
			SbDrawTeam( tTeam, x + 10, contentTop + panelHeight + gap, width - 20, panelHeight, players, playerCount, config );
		}
		else
		{
			int panelWidth = ( width - 20 - gap ) / 2;
			SbDrawTeam( ctTeam, x + 10, contentTop, panelWidth, areaBottom - contentTop, players, playerCount, config );
			SbDrawTeam( tTeam, x + 10 + panelWidth + gap, contentTop, panelWidth, areaBottom - contentTop, players, playerCount, config );
		}

		if( otherPlayers > 0 )
			SbDrawSpectators( players, playerCount, x + 14, areaBottom + 6, width - 28, config );
	}
	else
	{
		SbColumns columns;
		SbBuildColumns( columns, x + 10, width - 20, config, false );
		SbDrawColumnHeaders( columns, contentTop, width - 32, mutedR, mutedG, mutedB );

		int headerHeight = 22;
		int maxRows = ( footerY - contentTop - headerHeight - 4 ) / 17;
		SbDrawPlayerRows(
			x + 10,
			contentTop + headerHeight,
			width - 20,
			maxRows,
			17,
			players,
			playerCount,
			0,
			true,
			config );
	}

	SbColor( mutedR, mutedG, mutedB );
	SbText( x + 14, footerY, "HOLD TAB / TAP SCOREBOARD TO CLOSE" );

	return 1;
}
