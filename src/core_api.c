#include <stdarg.h>
#include <reent.h>
#include <string.h>
#include <sys/types.h>

#include <libretro.h>
#include <core_api.h>

extern int (*xlog)(const char *, ...);
extern void (*frontend_log_cb)(enum retro_log_level level, const char *tag, const char *fmt, ...);
extern void full_cache_flush();
void __libc_init_array(void){}

struct retro_header_t header = {
	.magic = CORE_API_MAGIC,
	.version = CORE_API_VERSION,
	.core_exports = {
			.retro_init = retro_init,
			.retro_deinit = retro_deinit,
			.retro_api_version = retro_api_version,
			.retro_get_system_info = retro_get_system_info,
			.retro_get_system_av_info = retro_get_system_av_info,
			.retro_set_environment = retro_set_environment,
			.retro_set_video_refresh = retro_set_video_refresh,
			.retro_set_audio_sample = retro_set_audio_sample,
			.retro_set_audio_sample_batch = retro_set_audio_sample_batch,
			.retro_set_input_poll = retro_set_input_poll,
			.retro_set_input_state = retro_set_input_state,
			.retro_set_controller_port_device = retro_set_controller_port_device,
			.retro_reset = retro_reset,
			.retro_run = retro_run,
			.retro_serialize_size = retro_serialize_size,
			.retro_serialize = retro_serialize,
			.retro_unserialize = retro_unserialize,
			.retro_cheat_reset = retro_cheat_reset,
			.retro_cheat_set = retro_cheat_set,
			.retro_load_game = retro_load_game,
			.retro_load_game_special = retro_load_game_special,
			.retro_unload_game = retro_unload_game,
			.retro_get_region = retro_get_region,
			.retro_get_memory_data = retro_get_memory_data,
			.retro_get_memory_size = retro_get_memory_size,
	}
};

static void clear_bss(struct frontend_functions_t *frontend_funcs) {
	extern void *__bss_start;
	extern void *_end;

    void *start = &__bss_start;
    void *end = &_end;

	frontend_funcs->memset(start, 0, end - start);

	//frontend_funcs->frontend_log_cb(RETRO_LOG_DEBUG, "CORE_API" ,"clear_bss: start=%p end=%p\n", &__bss_start, &_end);
}

// call_ctors currently is not being used since __libc_init_array will handle that instead.
// but leave it here for now if there would be a need to debug a crash during the static init phase.
static void call_ctors() {
	typedef void (*func_ptr) (void);
	extern func_ptr __init_array_start;
	extern func_ptr __init_array_end;

	frontend_log_cb(RETRO_LOG_DEBUG, "CORE_API" ,"call_ctors: start=%p end=%p\n", &__init_array_start, &__init_array_end);

	// call ctors from last to first
	for (func_ptr *pfunc = &__init_array_end - 1; pfunc >= &__init_array_start; --pfunc) {
		frontend_log_cb(RETRO_LOG_DEBUG, "CORE_API" ,"pfunc=%p func=%p\n", pfunc, *pfunc);
		(*pfunc)();
	}
}

// TODO: need a place to call dtors as well. maybe when retro_deinit is called?
static void call_dtors() {
	typedef void (*func_ptr) (void);
	extern func_ptr __fini_array_start;
	extern func_ptr __fini_array_end;

	// dtors are called in reverse order of ctors
	for (func_ptr *pfunc = &__fini_array_start; pfunc < &__fini_array_end; ++pfunc) {
		(*pfunc)();
	}
}

// __core_entry__ must be placed at a known location in the binary (at the beginning)
// so that when the loader actually loads the binary into mem address 0x87000000,
// then __core_entry__ will be the first function there for the loader to call.
struct retro_header_t *__core_entry__(struct frontend_functions_t *frontend_funcs) __attribute__((section(".init.core_entry"), used));

struct retro_header_t *__core_entry__(struct frontend_functions_t *frontend_funcs) {
	clear_bss(frontend_funcs);
	full_cache_flush();
	frontend_functions = *frontend_funcs;
	xlog = frontend_functions.printf;
	frontend_log_cb = frontend_functions.frontend_log_cb;
	frontend_log_cb(RETRO_LOG_INFO, "CORE_API" ,"Functions mapped\n");

	extern void __libc_init_array (void);
	__libc_init_array();

	frontend_log_cb(RETRO_LOG_INFO, "CORE_API" ,"libc initialized\n");

	return &header;
}
