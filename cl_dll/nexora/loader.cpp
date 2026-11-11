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

extern cl_enginefunc_t gEngfuncs;

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
	NX_RegisterHook( hook_id, fn, user_data );
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
	NX_UnregisterHook( hook_id, fn, user_data );
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

	for ( const auto &plugin : loaded )
	{
		int (*init)( const nexora_plugin_api_t * ) =
			(int (*)( const nexora_plugin_api_t * ))dlsym( plugin.handle, "nexora_plugin_init" );

		if ( !init )
		{
			NX_ConPrintf( "Nexora plugin loader: %s missing nexora_plugin_init\n", plugin.info->name );
			continue;
		}

		if ( !init( &s_plugin_api ) )
		{
			NX_ConPrintf( "Nexora plugin loader: %s failed to initialize\n", plugin.info->name );
			continue;
		}

		NX_ConPrintf( "Nexora plugin loader: loaded %s %s\n", plugin.info->name, plugin.info->version );
	}
}

#else

void NXPluginLoader_Init( void )
{
	gEngfuncs.Con_Printf( "Nexora plugin loader: not implemented on this platform\n" );
}

#endif
