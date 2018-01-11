/*
 * log.c - logging functionality
 *
 * Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002  Red Hat, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author(s): Erik Troan <ewt@redhat.com>
 *            Matt Wilson <msw@redhat.com>
 *            Michael Fulbright <msf@redhat.com>
 *            Jeremy Katz <katzj@redhat.com>
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

static FILE * tty_logfile = NULL;
static FILE * file_logfile = NULL;
static int minLevel = INFO;

static void printLogHeader(int level, FILE *outfile) {
    time_t current_time = time(NULL);
    struct tm *t = gmtime (&current_time);

    switch (level) {
        case DEBUGLVL:
            fprintf (outfile, "%02d:%02d:%02d DEBUG   : ", t->tm_hour,
                     t->tm_min, t->tm_sec);
            break;

        case INFO:
            fprintf (outfile, "%02d:%02d:%02d INFO    : ", t->tm_hour,
                     t->tm_min, t->tm_sec);
            break;

        case WARNING:
            fprintf (outfile, "%02d:%02d:%02d WARNING : ", t->tm_hour,
                     t->tm_min, t->tm_sec);
            break;

        case ERROR:
            fprintf (outfile, "%02d:%02d:%02d ERROR   : ", t->tm_hour,
                     t->tm_min, t->tm_sec);
            break;

        case CRITICAL:
            fprintf (outfile, "%02d:%02d:%02d CRITICAL: ", t->tm_hour,
                     t->tm_min, t->tm_sec);
            break;
    }
}

void logMessageV(int level, const char * s, va_list ap) {

    /* Only log to the screen things that are above the minimum level. */
    if (tty_logfile && level >= minLevel) {
        va_list apc;

        va_copy(apc, ap);

        printLogHeader(level, tty_logfile);
        vfprintf(tty_logfile, s, apc);
        fprintf(tty_logfile, "\n");
        fflush(tty_logfile);

        va_end(apc);
    }

    /* But log everything to the file. */
    if (file_logfile) {
        va_list apc;

        va_copy(apc, ap);

        printLogHeader(level, file_logfile);
        vfprintf(file_logfile, s, apc);
        fprintf(file_logfile, "\n");
        fflush(file_logfile);

        va_end(apc);
    }
}

void logMessage(int level, const char * s, ...) {
    va_list args;

    va_start(args, s);
    logMessageV(level, s, args);
    va_end(args);
}

int tty_logfd = -1;
int file_logfd = -1;

void openLog(int useLocal) {
    int flags;

    if (!useLocal) {
        tty_logfile = fopen("/dev/tty3", "w");
        file_logfile = fopen("/tmp/anaconda.log", "w");
    } else {
        tty_logfile = NULL;
        file_logfile = fopen("debug.log", "w");
    }

    if (tty_logfile) {
        tty_logfd = fileno(tty_logfile);
        flags = fcntl(tty_logfd, F_GETFD, 0) | FD_CLOEXEC;
        fcntl(tty_logfd, F_SETFD, flags);
    }

    if (file_logfile) {
        file_logfd = fileno(file_logfile);
        flags = fcntl(file_logfd, F_GETFD, 0) | FD_CLOEXEC;
        fcntl(file_logfd, F_SETFD, flags);
    }
}

void closeLog(void) {
    if (tty_logfile)
        fclose(tty_logfile);

    if (file_logfile)
        fclose(file_logfile);
}

/* set the level.  higher means you see more verbosity */
void setLogLevel(int level) {
    minLevel = level;
}

int getLogLevel(void) {
    return minLevel;
}

/* vim:set shiftwidth=4 softtabstop=4: */
