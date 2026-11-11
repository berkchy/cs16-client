#include "loader.h"
#include <vector>
#include <mutex>

struct nx_hook_entry_s
{
	nexora_hook_fn fn;
	void *user_data;
};

static std::vector<nx_hook_entry_s> s_hooks[NEXORA_HOOK_COUNT];
static std::mutex s_hook_mutex;

void NX_RegisterHook( int hook_id, nexora_hook_fn fn, void *user_data )
{
	if ( !fn || hook_id < 0 || hook_id >= NEXORA_HOOK_COUNT )
		return;

	std::lock_guard<std::mutex> lock( s_hook_mutex );
	s_hooks[hook_id].push_back( { fn, user_data } );
}

void NX_UnregisterHook( int hook_id, nexora_hook_fn fn, void *user_data )
{
	if ( !fn || hook_id < 0 || hook_id >= NEXORA_HOOK_COUNT )
		return;

	std::lock_guard<std::mutex> lock( s_hook_mutex );
	auto &hooks = s_hooks[hook_id];

	for ( auto it = hooks.begin(); it != hooks.end(); )
	{
		if ( it->fn == fn && it->user_data == user_data )
			it = hooks.erase( it );
		else
			++it;
	}
}

void NXPluginLoader_Dispatch( int hook_id, void *args )
{
	if ( hook_id < 0 || hook_id >= NEXORA_HOOK_COUNT )
		return;

	std::vector<nx_hook_entry_s> snapshot;

	{
		std::lock_guard<std::mutex> lock( s_hook_mutex );
		snapshot = s_hooks[hook_id];
	}

	for ( const auto &entry : snapshot )
		entry.fn( args, entry.user_data );
}
