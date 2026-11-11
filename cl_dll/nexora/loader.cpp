#ifndef _WIN32
#define _GNU_SOURCE 1
#include <dlfcn.h>
#include <dirent.h>
#endif

#include "loader.h"
#include "hud.h"
#include "netadr.h"
#include "pmtrace.h"
#include "pm_shared.h"
#include "APIProxy.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <algorithm>

#ifndef _WIN32
#include <strings.h>
#else
#include <string.h>
#endif

extern cl_enginefunc_t gEngfuncs;

struct NXPluginRecord
{
	void *handle;
	const nexora_plugin_info_t *info;
	std::string name;
	std::string version;
	int priority;
	bool paused;
};

static std::vector<NXPluginRecord> s_plugins;
static int s_currentPlugin = -1;

#if defined(__ANDROID__)
	#if defined(__aarch64__)
		#define NEXORA_ABI_TAG "arm64"
	#elif defined(__arm__)
		#define NEXORA_ABI_TAG "arm"
	#elif defined(__x86_64__)
		#define NEXORA_ABI_TAG "x86_64"
	#elif defined(__i386__)
		#define NEXORA_ABI_TAG "x86"
	#else
		#define NEXORA_ABI_TAG ""
	#endif
#elif defined(__linux__) || defined(__APPLE__)
	#if defined(__x86_64__)
		#define NEXORA_ABI_TAG "amd64"
	#elif defined(__i386__)
		#define NEXORA_ABI_TAG "i386"
	#elif defined(__aarch64__)
		#define NEXORA_ABI_TAG "arm64"
	#else
		#define NEXORA_ABI_TAG ""
	#endif
#else
	#define NEXORA_ABI_TAG ""
#endif

#if defined(__APPLE__)
	#define NEXORA_PLUGIN_EXT ".dylib"
#else
	#define NEXORA_PLUGIN_EXT ".so"
#endif

static const char *nexora_abi_suffix( void )
{
	static std::string s;
	if ( s.empty() && NEXORA_ABI_TAG[0] != '\0' )
		s = std::string( "_nexora_" ) + NEXORA_ABI_TAG + NEXORA_PLUGIN_EXT;
	return s.c_str();
}

static void NX_ConPrintf( const char *fmt, ... )
{
	char buf[1024];
	va_list args;
	va_start( args, fmt );
	vsnprintf( buf, sizeof( buf ), fmt, args );
	va_end( args );
	buf[sizeof( buf ) - 1] = '\0';
	gEngfuncs.Con_Printf( "%s", buf );
}

static void NXPluginAPI_RegisterHook( int hook_id, nexora_hook_fn fn, void *user_data )
{
	NX_RegisterHook( hook_id, fn, user_data, s_currentPlugin );
}

static void NXPluginAPI_GetScreenSize( int *width, int *height )
{
	if ( width )
		*width = gHUD.m_scrinfo.iWidth;

	if ( height )
		*height = gHUD.m_scrinfo.iHeight;
}

static void NXPluginAPI_UnregisterHook( int hook_id, nexora_hook_fn fn, void *user_data )
{
	NX_UnregisterHook( hook_id, fn, user_data, s_currentPlugin );
}

static nexora_plugin_api_t s_plugin_api =
{
	NEXORA_PLUGIN_API_VERSION,
	0,
	{ 0, 0 },
	NXPluginAPI_RegisterHook,
	NXPluginAPI_UnregisterHook,
	NX_ConPrintf,
	&gEngfuncs,
	NXPluginAPI_GetScreenSize,
	&gRenderAPI,
};

extern "C" {

int NXPluginLoader_IsPluginPaused( int id )
{
	if ( id < 0 || id >= (int)s_plugins.size() )
		return 0;
	return s_plugins[id].paused ? 1 : 0;
}

static int NXNameCompare( const char *a, const char *b )
{
#if defined( _WIN32 )
	return _stricmp( a, b );
#else
	return strcasecmp( a, b );
#endif
}

static int NXFindPlugin( const char *token )
{
	if ( !token || !token[0] )
		return -1;

	char *end = nullptr;
	long idx = strtol( token, &end, 10 );

	if ( end && *end == '\0' )
		return ( idx >= 1 && idx <= (long)s_plugins.size() ) ? (int)idx - 1 : -1;

	for ( size_t i = 0; i < s_plugins.size(); i++ )
	{
		if ( NXNameCompare( s_plugins[i].name.c_str(), token ) == 0 )
			return (int)i;
	}

	return -1;
}

static void NXPrintCommandHelp( void )
{
	NX_ConPrintf( "nxmod commands:\n" );
	NX_ConPrintf( "  nxmod list                 - list loaded plugins\n" );
	NX_ConPrintf( "  nxmod info <name|index>    - show plugin details\n" );
	NX_ConPrintf( "  nxmod pause <name|index>   - disable a plugin's hooks\n" );
	NX_ConPrintf( "  nxmod unpause <name|index> - re-enable a paused plugin\n" );
	NX_ConPrintf( "  nxmod toggle <name|index>  - toggle plugin state\n" );
	NX_ConPrintf( "  nxmod reload [name|index]  - re-run plugin init (re-registers hooks)\n" );
	NX_ConPrintf( "  nxmod help                 - show this help\n" );
}

static void NXPrintPluginList( void )
{
	NX_ConPrintf( "nxmod: %d plugin(s) loaded\n", (int)s_plugins.size() );

	for ( size_t i = 0; i < s_plugins.size(); i++ )
	{
		const NXPluginRecord &record = s_plugins[i];
		NX_ConPrintf( "  [%d] %s %s pri=%d hooks=%d %s\n",
			(int)i + 1, record.name.c_str(), record.version.c_str(),
			record.priority, NX_CountHooksForOwner( (int)i ),
			record.paused ? "(paused)" : "(active)" );
	}
}

static void NXPrintPluginInfo( int index )
{
	const NXPluginRecord &record = s_plugins[index];
	NX_ConPrintf( "nxmod: plugin [%d]\n", index + 1 );
	NX_ConPrintf( "  name:     %s\n", record.name.c_str() );
	NX_ConPrintf( "  version:  %s\n", record.version.c_str() );
	NX_ConPrintf( "  priority: %d\n", record.priority );
	NX_ConPrintf( "  api:      v%u\n", (unsigned)record.info->api_version );
	NX_ConPrintf( "  hooks:    %d\n", NX_CountHooksForOwner( index ) );
	NX_ConPrintf( "  state:    %s\n", record.paused ? "paused" : "active" );
}

static void NXSetPluginPaused( int index, bool paused )
{
	s_plugins[index].paused = paused;
	NX_ConPrintf( "nxmod: %s %s\n", s_plugins[index].name.c_str(),
		paused ? "paused" : "unpaused" );
}

static void NXReloadPlugin( int index )
{
#if defined( _WIN32 )
	NX_ConPrintf( "nxmod: reload is not supported on this platform\n" );
	return;
#else
	NXPluginRecord &record = s_plugins[index];

	int (*init)( const nexora_plugin_api_t * ) =
		(int (*)( const nexora_plugin_api_t * ))dlsym( record.handle, "nexora_plugin_init" );

	if ( !init )
	{
		NX_ConPrintf( "nxmod: %s missing nexora_plugin_init\n", record.name.c_str() );
		return;
	}

	s_currentPlugin = index;
	int result = init( &s_plugin_api );
	s_currentPlugin = -1;

	if ( result )
		NX_ConPrintf( "nxmod: reloaded %s\n", record.name.c_str() );
	else
		NX_ConPrintf( "nxmod: %s reload failed\n", record.name.c_str() );
#endif
}

void NXPluginLoader_Command( void )
{
	if ( gEngfuncs.Cmd_Argc() < 2 )
	{
		NXPrintCommandHelp();
		return;
	}

	const char *sub = gEngfuncs.Cmd_Argv( 1 );

	if ( NXNameCompare( sub, "help" ) == 0 )
	{
		NXPrintCommandHelp();
		return;
	}

	if ( NXNameCompare( sub, "list" ) == 0 )
	{
		NXPrintPluginList();
		return;
	}

	const char *arg = gEngfuncs.Cmd_Argc() > 2 ? gEngfuncs.Cmd_Argv( 2 ) : nullptr;

	if ( NXNameCompare( sub, "reload" ) == 0 )
	{
		if ( !arg )
		{
			for ( size_t i = 0; i < s_plugins.size(); i++ )
				NXReloadPlugin( (int)i );
			return;
		}

		int index = NXFindPlugin( arg );
		if ( index < 0 )
		{
			NX_ConPrintf( "nxmod: unknown plugin '%s'\n", arg );
			return;
		}
		NXReloadPlugin( index );
		return;
	}

	int index = NXFindPlugin( arg );

	if ( index < 0 )
	{
		NX_ConPrintf( "nxmod: usage: nxmod %s <name|index>\n", sub );
		return;
	}

	if ( NXNameCompare( sub, "info" ) == 0 )
	{
		NXPrintPluginInfo( index );
		return;
	}

	if ( NXNameCompare( sub, "pause" ) == 0 )
	{
		NXSetPluginPaused( index, true );
		return;
	}

	if ( NXNameCompare( sub, "unpause" ) == 0 )
	{
		NXSetPluginPaused( index, false );
		return;
	}

	if ( NXNameCompare( sub, "toggle" ) == 0 )
	{
		NXSetPluginPaused( index, !s_plugins[index].paused );
		return;
	}

	NX_ConPrintf( "nxmod: unknown command '%s'\n", sub );
	NXPrintCommandHelp();
}

#ifndef _WIN32

static std::string NXModuleDir( void )
{
	Dl_info info = {};
	if ( !dladdr( (void *)&NXModuleDir, &info ) || !info.dli_fname )
		return std::string();

	std::string path( info.dli_fname );
	size_t slash = path.find_last_of( '/' );

	if ( slash == std::string::npos )
		return std::string( "." );

	return path.substr( 0, slash );
}

static bool NXMatchesPluginSuffix( const std::string &name, const std::string &suffix )
{
	if ( name.compare( 0, 3, "lib" ) != 0 )
		return false;

	if ( name.length() <= 3 + suffix.length() )
		return false;

	return name.compare( name.length() - suffix.length(), suffix.length(), suffix ) == 0;
}

void NXPluginLoader_Init( void )
{
	static bool s_initialized = false;

	if ( s_initialized )
		return;

	s_initialized = true;

	gEngfuncs.pfnAddCommand( "nxmod", NXPluginLoader_Command );

	const char *suffix = nexora_abi_suffix();

	if ( !suffix || suffix[0] == '\0' )
	{
		NX_ConPrintf( "Nexora plugin loader: unsupported platform, no plugins loaded\n" );
		return;
	}

	std::string dir = NXModuleDir();

	if ( dir.empty() )
	{
		NX_ConPrintf( "Nexora plugin loader: could not determine module directory\n" );
		return;
	}

	DIR *d = opendir( dir.c_str() );

	if ( !d )
	{
		NX_ConPrintf( "Nexora plugin loader: could not open %s\n", dir.c_str() );
		return;
	}

	std::vector<std::string> candidates;

	while ( const dirent *entry = readdir( d ) )
	{
		std::string name( entry->d_name );

		if ( NXMatchesPluginSuffix( name, suffix ) )
			candidates.push_back( name );
	}

	closedir( d );

	if ( candidates.empty() )
		return;

	std::sort( candidates.begin(), candidates.end() );

	struct nx_loaded_plugin_s
	{
		void *handle;
		const nexora_plugin_info_t *info;
		std::string path;
	};

	std::vector<nx_loaded_plugin_s> loaded;

	for ( const std::string &name : candidates )
	{
		std::string path = dir + "/" + name;
		void *handle = dlopen( path.c_str(), RTLD_NOW | RTLD_NODELETE );

		if ( !handle )
		{
			NX_ConPrintf( "Nexora plugin loader: %s failed to load: %s\n", name.c_str(), dlerror() );
			continue;
		}

		const nexora_plugin_info_t *(*query)( void ) =
			(const nexora_plugin_info_t *(*)( void ))dlsym( handle, "nexora_plugin_query" );

		if ( !query )
		{
			NX_ConPrintf( "Nexora plugin loader: %s missing nexora_plugin_query\n", name.c_str() );
			continue;
		}

		const nexora_plugin_info_t *info = query();

		if ( !info )
		{
			NX_ConPrintf( "Nexora plugin loader: %s returned null plugin info\n", name.c_str() );
			continue;
		}

		if ( info->api_version != NEXORA_PLUGIN_API_VERSION )
		{
			NX_ConPrintf( "Nexora plugin loader: %s requires API v%u, loader provides v%u\n",
				name.c_str(), info->api_version, ( uint32_t )NEXORA_PLUGIN_API_VERSION );
			continue;
		}

		loaded.push_back( { handle, info, path } );
	}

	std::sort( loaded.begin(), loaded.end(),
		[]( const nx_loaded_plugin_s &a, const nx_loaded_plugin_s &b ) {
			return a.info->priority < b.info->priority;
		} );

	s_plugins.clear();

	for ( const auto &plugin : loaded )
	{
		s_plugins.push_back( { plugin.handle, plugin.info, plugin.info->name,
			plugin.info->version, (int)plugin.info->priority, false } );
	}

	for ( size_t i = 0; i < s_plugins.size(); i++ )
	{
		NXPluginRecord &plugin = s_plugins[i];

		int (*init)( const nexora_plugin_api_t * ) =
			(int (*)( const nexora_plugin_api_t * ))dlsym( plugin.handle, "nexora_plugin_init" );

		if ( !init )
		{
			NX_ConPrintf( "Nexora plugin loader: %s missing nexora_plugin_init\n", plugin.name.c_str() );
			plugin.paused = true;
			continue;
		}

		s_currentPlugin = (int)i;

		if ( !init( &s_plugin_api ) )
		{
			NX_ConPrintf( "Nexora plugin loader: %s failed to initialize\n", plugin.name.c_str() );
			s_currentPlugin = -1;
			plugin.paused = true;
			continue;
		}

		s_currentPlugin = -1;

		NX_ConPrintf( "Nexora plugin loader: loaded %s %s\n", plugin.name.c_str(), plugin.version.c_str() );
	}
}

#else

void NXPluginLoader_Init( void )
{
	gEngfuncs.Con_Printf( "Nexora plugin loader: not implemented on this platform\n" );
	gEngfuncs.pfnAddCommand( "nxmod", NXPluginLoader_Command );
}

#endif

}
