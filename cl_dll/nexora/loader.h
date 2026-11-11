#ifndef NEXORA_LOADER_H
#define NEXORA_LOADER_H

#include <nexora/nexora_plugin.h>

#ifdef __cplusplus
extern "C" {
#endif

void NXPluginLoader_Init( void );
void NXPluginLoader_Dispatch( int hook_id, void *args );
void NXPluginLoader_Command( void );
int NXPluginLoader_IsPluginPaused( int id );
int NX_CountHooksForOwner( int owner );

void NX_RegisterHook( int hook_id, nexora_hook_fn fn, void *user_data, int owner );
void NX_UnregisterHook( int hook_id, nexora_hook_fn fn, void *user_data, int owner );

#ifdef __cplusplus
}
#endif

#endif
