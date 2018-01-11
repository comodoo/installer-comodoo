/*
 * isys.h
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

#ifndef H_ISYS
#define H_ISYS

#define MIN_RAM			64000	    
#define MIN_GUI_RAM		192000
#if defined(__x86_64__) || defined(__ia64__) || defined(__s390x__) || defined(__ppc64__)
#define EARLY_SWAP_RAM		400000
#else
#define EARLY_SWAP_RAM		270000
#endif

int insmod(char * modName, char * path, char ** args);
int rmmod(char * modName);

/* returns 0 for true, !0 for false */
int fileIsIso(const char * file);

/* returns 1 if on an iSeries vio console, 0 otherwise */
int isVioConsole(void);

/* dasd functions */
char *getDasdPorts();
int isLdlDasd(char * dev);
int isUsableDasd(char *device);

#endif
