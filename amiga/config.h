/* VoR for AmigaOS: what configure would have found (args.c and the others
 * include <config.h>). The AmiCradle Team. GPL 2 or later, like VoR. */
#define PACKAGE_NAME "vor"
#define PACKAGE_STRING "vor 0.5.8"
#define PACKAGE_TARNAME "vor"
#define PACKAGE_VERSION "0.5.8"
#define VERSION "0.5.8"
#define HAVE_LIBSDL_IMAGE 1
#define HAVE_LIBSDL_MIXER 1
#define HAVE_STDBOOL_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_UNISTD_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_STRDUP 1
#define HAVE_NANOSLEEP 1
/* libnix has nanosleep() (main.c's tiny_sleep) but its <time.h> does not
   declare it */
#include <time.h>
int nanosleep(const struct timespec *, struct timespec *);
