/* Bare-metal (dartos) dirent shim: frontend_functions.c implements opendir/readdir
   against the exact struct dirent layout below (d_type first, d_name after).
   On Linux/PC hosts (glibc/musl) the OS struct dirent has a DIFFERENT layout
   (d_name at offset ~19), so the real system <dirent.h> must be used - never this shim. */
#if defined(__linux__)
#include_next <dirent.h>
#else
#ifndef _DIRENT_H
#define _DIRENT_H

#ifdef __cplusplus
extern "C"
{
#endif

#define DTYPE_UNKNOWN             0
#define DTYPE_DIRECTORY           4
#define DTYPE_FILE                8
#define DTYPE_LINK                10

#define DT_UNKNOWN                DTYPE_UNKNOWN
#define DT_REG                    DTYPE_FILE
#define DT_DIR                    DTYPE_DIRECTORY
#define DT_LNK                    DTYPE_LINK

#define DIRENT_ISFILE(type)      ((type) == DTYPE_FILE)
#define DIRENT_ISDIRECTORY(type) ((type) == DTYPE_DIRECTORY)

struct dirent
{
	  unsigned char  d_type;
	  char     d_name[255];
};

typedef void DIR;

DIR *opendir(const char *path);
int closedir(DIR *dir);
struct dirent *readdir(DIR *dir);

#ifdef __cplusplus
}
#endif

#endif /* _DIRENT_H */
#endif /* !__linux__ */
