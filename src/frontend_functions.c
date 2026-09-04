/*
    The purpose of this file is to wrap functions passed via frontend_functions as regular functions needed by the cores.
    Wrapping seems preferable here compared to pointers to cause the least amount of conflicts.
    More or less a replacement for lib.c from Multicore.
*/
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libretro.h>
#include <dirent.h>
#include <frontend_functions.h>
#include <core_api.h>

#undef _malloc_r
#undef _free_r
#undef _calloc_r
#undef _realloc_r

char* ram_buffer = NULL;
size_t ram_buffer_size = 64 * 1024 * 1024;  // 64 MB

struct frontend_functions_t frontend_functions;

int (*xlog)(const char *format, ...) = printf;
void (*frontend_log_cb)(enum retro_log_level level, const char *tag, const char *fmt, ...) = NULL;

//      System Calls       //
__attribute__((noreturn))
void _exit(int status) {
    frontend_functions._exit(status);
    while (1) {}  // Should never return
}

__attribute__((noreturn))
void abort(void) {
    frontend_functions.abort();
    while (1) {}  // Should never return
}

void full_cache_flush() {
	unsigned idx;

	// Index_Writeback_Inv_D
	for (idx = 0x80000000; idx <= 0x80004000; idx += 16) // all of D-cache
		asm volatile("cache 1, 0(%0); cache 1, 0(%0)" : : "r"(idx));

	asm volatile("sync 0; nop; nop");

	// Index_Invalidate_I
	for (idx = 0x80000000; idx <= 0x80004000; idx += 16) // all of I-cache
		asm volatile("cache 0, 0(%0); cache 0, 0(%0)" : : "r"(idx));

	asm volatile("nop; nop; nop; nop; nop"); // ehb may be nop on this core
}

void _flush_cache(void* start, void* end) {
    // note: params are ignored and *all* the cache is cleared instead.
	// this seems to produce the most stable behavior for running dynarec code.
    full_cache_flush();
}

void *_malloc_r(struct _reent *r, size_t size) {
    return frontend_functions.malloc(size);
}

void _free_r(struct _reent *r, void *ptr) {
    frontend_functions.free(ptr);
}

void *_calloc_r(struct _reent *r, size_t n, size_t size) {
    return frontend_functions.calloc(n, size);
}

void *_realloc_r(struct _reent *r, void *ptr, size_t size) {
    return frontend_functions.realloc(ptr, size);
}

int	stat(const char *path, struct stat *sbuf) {
    xlog("stat called\n");
    return frontend_functions.stat(path, sbuf);
}

int fstat(int fd, struct stat *sbuf) {
    return frontend_functions.fstat(fd, sbuf);
}

int kill(pid_t pid, int sig) {
    return frontend_functions.kill(pid, sig);
}

pid_t getpid(void) {
    return frontend_functions.getpid();
}

int gettimeofday(struct timeval *tv, void *tz) {
    return frontend_functions.gettimeofday(tv, tz);
}

//      FreeRTOS Functions      //
TickType_t xTaskGetTickCount(void) {
    return frontend_functions.xTaskGetTickCount();
}

//      I/O Operations      //
int open(const char *pathname, int flags, ...) {
    mode_t mode = 0;

    // If O_CREAT flag is set, we need to handle the mode argument
    if (flags & O_CREAT) {
        va_list args;
        va_start(args, flags);
        mode = va_arg(args, mode_t);
        va_end(args);
    }

    return frontend_functions.open(pathname, flags, mode);
}

int close(int fd) {
    return frontend_functions.close(fd);
}

int write(int fd, const void *buf, size_t count) {
    return frontend_functions.write(fd, buf, count);
}

int read(int fd, void *buf, size_t count) {
    return frontend_functions.read(fd, buf, count);
}

int isatty(int fd) {
    return frontend_functions.isatty(fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return frontend_functions.lseek(fd, offset, whence);
}

int link(const char *__path1, const char *__path2) {
    (void)__path1;
    (void)__path2;

    return -1;
}

int unlink(const char *__path) {
    return frontend_functions.unlink(__path);
}

//      Dirent      //
DIR *opendir(const char *path) {
    return frontend_functions.opendir(path);
}

int closedir(DIR *dir) {
    return frontend_functions.closedir(dir);
}

struct dirent *readdir(DIR *dir){
    return frontend_functions.readdir(dir);
}