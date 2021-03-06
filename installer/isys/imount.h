/*
 * imount.h
 *
 * Copyright (C) 2007  Red Hat, Inc.  All rights reserved.
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
 */

#ifndef H_IMOUNT
#define H_IMOUNT

#define IMOUNT_ERR_ERRNO	1
#define IMOUNT_ERR_OTHER	2

#include <sys/mount.h>		/* for umount() */

#define IMOUNT_RDONLY  1
#define IMOUNT_BIND    2
#define IMOUNT_REMOUNT 4

int doPwMount(char *dev, char *where, char *fs, char *options, char **err);
int doMultiMount(char *dev, char *where, char **fstypes, char *options, char **err);
int mkdirChain(char * origChain);

#endif
