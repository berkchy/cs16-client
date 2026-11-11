#ifndef NEXORA_LOADER_H
#define NEXORA_LOADER_H

#include <nexora/nexora_plugin.h>

#ifdef __cplusplus
extern "C" {
#endif

void NXPluginLoader_Init( void );
void NXPluginLoader_Dispatch( int hook_id, void *args );

void NX_RegisterHook( int hook_id, nexora_hook_fn fn, void *user_data );
void NX_UnregisterHook( int hook_id, nexora_hook_fn fn, void *user_data );

#ifdef __cplusplus
}
#endif

#endif
