#ifndef __CORE_API_H
#define __CORE_API_H

#include <dirent.h>

#if !defined(TickType_t)
typedef uint32_t TickType_t;
#endif

#define MAKE_MAGIC(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
#define CORE_API_MAGIC  MAKE_MAGIC('D', 'A', 'R', 'T')
#define CORE_API_VERSION_MAJOR 1
#define CORE_API_VERSION_MINOR 0
#define CORE_API_VERSION       ((CORE_API_VERSION_MAJOR << 16) | CORE_API_VERSION_MINOR)

struct retro_core_t {
   void (*retro_init)(void);
   void (*retro_deinit)(void);
   unsigned (*retro_api_version)(void);
   void (*retro_get_system_info)(struct retro_system_info*);
   void (*retro_get_system_av_info)(struct retro_system_av_info*);
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_set_controller_port_device)(unsigned, unsigned);
   void (*retro_reset)(void);
   void (*retro_run)(void);
   size_t (*retro_serialize_size)(void);
   bool (*retro_serialize)(void*, size_t);
   bool (*retro_unserialize)(const void*, size_t);
   void (*retro_cheat_reset)(void);
   void (*retro_cheat_set)(unsigned, bool, const char*);
   bool (*retro_load_game)(const struct retro_game_info*);
   bool (*retro_load_game_special)(unsigned, const struct retro_game_info*, size_t);
   void (*retro_unload_game)(void);
   unsigned (*retro_get_region)(void);
   void *(*retro_get_memory_data)(unsigned);
   size_t (*retro_get_memory_size)(unsigned);
};

struct retro_header_t {
   uint32_t magic;
   uint32_t version;
   struct retro_core_t core_exports;
};

struct frontend_functions_t {
   int (*printf)(const char *__restrict, ...);
   void (*frontend_log_cb)(enum retro_log_level level, const char *tag, const char *fmt, ...);
   void (*_exit)(int status);
   void (*abort)(void);
   void *(*malloc)(size_t __size);
   void *(*memset)(void *ptr, int value, size_t num);
   void (*free)(void *ptr);
   void *(*calloc)(size_t nmemb, size_t size);
   void *(*realloc)(void *ptr, size_t size);
   int (*stat)(const char *path, struct stat *sbuf);
   int (*fstat)(int fd, struct stat *sbuf);
   int (*kill)(pid_t pid, int sig);
   pid_t (*getpid)(void);
   int (*gettimeofday)(struct timeval *tv, void *tz);
   TickType_t (*xTaskGetTickCount)();
   int (*open)(const char *pathname, int flags, ...);
   int (*close)(int fd);
   int (*write)(int fd, const void *buf, size_t count);
   int (*read)(int fd, void *buf, size_t count);
   int (*isatty)(int fd);
   off_t (*lseek)(int fd, off_t offset, int whence);
   int (*unlink)(const char *__path);
   DIR *(*opendir)(const char *path);
   int (*closedir)(DIR *dir);
   struct dirent *(*readdir)(DIR *dir);
};

extern struct frontend_functions_t frontend_functions;
typedef struct retro_header_t *(*core_entry_t)(struct frontend_functions_t *frontend_funcs);

#endif
