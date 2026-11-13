#ifndef NEXORA_PLUGIN_H
#define NEXORA_PLUGIN_H

#include <stdint.h>

#define NEXORA_PLUGIN_API_VERSION 1

enum nexora_hook_e
{
	NEXORA_HOOK_CLIENT_INIT = 0,
	NEXORA_HOOK_CLIENT_SHUTDOWN,
	NEXORA_HOOK_CLIENT_CONNECT,
	NEXORA_HOOK_CLIENT_RESET,
	NEXORA_HOOK_PLAYER_MOVE_INIT,
	NEXORA_HOOK_PLAYER_MOVE,
	NEXORA_HOOK_CREATE_MOVE,
	NEXORA_HOOK_FRAME,
	NEXORA_HOOK_PAINT_HUD,
	NEXORA_HOOK_UPDATE_CLIENT_DATA,
	NEXORA_HOOK_PROCESS_PLAYER_STATE,
	NEXORA_HOOK_ADD_ENTITY,
	NEXORA_HOOK_CREATE_ENTITIES,
	NEXORA_HOOK_IN_COMMANDS,
	NEXORA_HOOK_DEATHMSG,
	NEXORA_HOOK_HUD_ELEMENT_DRAW,
	NEXORA_HOOK_NETMSG,
	NEXORA_HOOK_SCOREBOARD_TOGGLE,
	NEXORA_HOOK_AMMO_INFO,
	NEXORA_HOOK_CENTERPRINT,
	NEXORA_HOOK_HUD_ELEMENT_POS,
	NEXORA_HOOK_THIRDPERSON,
	NEXORA_HOOK_CHAT,
	NEXORA_HOOK_COUNT
};

typedef struct nexora_player_move_init_args_s
{
	void *pmove;
} nexora_player_move_init_args_t;

typedef struct nexora_player_move_args_s
{
	void *pmove;
	int server;
} nexora_player_move_args_t;

typedef struct nexora_create_move_args_s
{
	float frametime;
	void *cmd;
	int active;
} nexora_create_move_args_t;

typedef struct nexora_frame_args_s
{
	double time;
} nexora_frame_args_t;

typedef struct nexora_redraw_args_s
{
	float time;
	int intermission;
} nexora_redraw_args_t;

typedef struct nexora_update_client_data_args_s
{
	void *client_data;
	float time;
	int hide_hud;
} nexora_update_client_data_args_t;

typedef struct nexora_process_player_state_args_s
{
	void *dst;
	const void *src;
} nexora_process_player_state_args_t;

typedef struct nexora_add_entity_args_s
{
	int type;
	void *entity;
	const char *modelname;
	int allow;
} nexora_add_entity_args_t;

typedef struct nexora_deathmsg_args_s
{
	int killer;
	int victim;
	int headshot;
	const char *weapon;
	const char *killer_name;
	const char *victim_name;
	int killer_team;
	int victim_team;
	int killer_is_local;
	int victim_is_local;
	int is_suicide;
	int is_teamkill;
	int is_nonplayer_kill;
	float time;
} nexora_deathmsg_args_t;

typedef struct nexora_hud_element_args_s
{
	void *element;
	const char *name;
	int allow;
} nexora_hud_element_args_t;

typedef struct nexora_netmsg_args_s
{
	const char *name;
	int size;
	const void *buf;
} nexora_netmsg_args_t;

typedef struct nexora_scoreboard_args_s
{
	int visible;
} nexora_scoreboard_args_t;

typedef struct nexora_ammo_args_s
{
	int clip;
	int reserve;
	const char *weapon_name;
} nexora_ammo_args_t;

typedef struct nexora_centerprint_args_s
{
	const char *text;
	int allow;
} nexora_centerprint_args_t;

typedef struct nexora_hud_element_pos_args_s
{
	const char *name;
	int x;
	int y;
} nexora_hud_element_pos_args_t;

typedef struct nexora_thirdperson_args_s
{
	int allow;
} nexora_thirdperson_args_t;

typedef struct nexora_chat_args_s
{
	const char *text;
	int client_index;
	float time;
	int allow;
} nexora_chat_args_t;

typedef void (*nexora_hook_fn)(void *args, void *user_data);

typedef struct nexora_plugin_api_s
{
	uint32_t api_version;
	uint32_t flags;
	uint32_t reserved[2];
	void (*register_hook)(int hook_id, nexora_hook_fn fn, void *user_data);
	void (*unregister_hook)(int hook_id, nexora_hook_fn fn, void *user_data);
	void (*con_printf)(const char *fmt, ...);
	void *engine_funcs;
	void (*get_screen_size)(int *width, int *height);
	void *render_api;
} nexora_plugin_api_t;

typedef struct nexora_plugin_info_s
{
	const char *name;
	const char *version;
	uint32_t api_version;
	int priority;
} nexora_plugin_info_t;

#ifdef __cplusplus
extern "C" {
#endif

const nexora_plugin_info_t *nexora_plugin_query(void);
int nexora_plugin_init(const nexora_plugin_api_t *api);

#ifdef __cplusplus
}
#endif

#endif
