#include "config.h"

#if HAVE_GRP_H
#include <grp.h>
#endif
#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#endif
#if HAVE_STRINGS_H
#include <strings.h>
#endif
#if HAVE_LIMITS_H
#include <limits.h>
#endif

int initgroups(const char *name, gid_t basegid)
{
#ifdef HAVE_SETGROUPS
#ifndef NGROUPS_MAX
#define NGROUPS_MAX 16
#endif

    gid_t groups[NGROUPS_MAX];
    struct group *g;
    int index = 0;

    setgrent();

    groups[index++] = basegid;

    while (index < NGROUPS_MAX && ((g = getgrent()) != NULL)) {
       if (g->gr_gid != basegid) {
        char **names;

        for (names = g->gr_mem; *names != NULL; ++names) {

       if (!strcmp(*names, name))
            groups[index++] = g->gr_gid;

           }
       }
    }

    endgrent();

    return setgroups(index, groups);

#else

    return 0;

#endif /* def HAVE_SETGROUPS */
}

