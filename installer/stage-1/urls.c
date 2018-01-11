/*
 * urls.c - url handling code
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

#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <newt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>

#include "ftp.h"
#include "lang.h"
#include "loader.h"
#include "loadermisc.h"
#include "urls.h"
#include "log.h"
#include "windows.h"
#include "net.h"

/* boot flags */
extern uint64_t flags;

/* convert a url (ftp or http) to a ui */
int convertURLToUI(char *url, struct iurlinfo *ui) {
    char *chptr;

    memset(ui, 0, sizeof(*ui));
    
    if (!strncmp("ftp://", url, 6)) {
        ui->protocol = URL_METHOD_FTP;
        url += 6;
	
        /* There could be a username/password on here */
        if ((chptr = strchr(url, '@'))) {
            if ((chptr = strchr(url, ':'))) {
                *chptr = '\0';
                ui->login = strdup(url);
                url = chptr + 1;

                chptr = strchr(url, '@');
                *chptr = '\0';
                ui->password = strdup(url);
                url = chptr + 1;
            } else {
                chptr = strchr(url, '@');
                *chptr = '\0';
                ui->login = strdup(url);
                url = chptr + 1;
            }
        }
    } else if (!strncmp("http://", url, 7)) {
        ui->protocol = URL_METHOD_HTTP;
        url += 7;
    } else {
        logMessage(ERROR, "unknown url protocol '%s'", url);
        return -1;
    }
    
    /* url is left pointing at the hostname */
    chptr = strchr(url, '/');
    if (chptr != NULL) {
        *chptr = '\0';
        ui->address = strdup(url);
        url = chptr;
        *url = '/';
        ui->prefix = strdup(url);
    } else {
       ui->address = strdup(url);
       ui->prefix = strdup("/");
    }
    
    logMessage(DEBUGLVL, "url address %s", ui->address);
    logMessage(DEBUGLVL, "url prefix %s", ui->prefix);

    return 0;
}

static char * getLoginName(char * login, struct iurlinfo *ui) {
    int i;

    i = 0;
    /* password w/o login isn't useful */
    if (ui->login && strlen(ui->login)) {
	i += strlen(ui->login) + 5;
	if (strlen(ui->password))
	    i += 3*strlen(ui->password) + 5;

	if (ui->login || ui->password) {
	    login = malloc(i);
	    strcpy(login, ui->login);
	    if (ui->password) {
		char * chptr;
		char code[4];

		strcat(login, ":");
		for (chptr = ui->password; *chptr; chptr++) {
		    sprintf(code, "%%%2x", *chptr);
		    strcat(login, code);
		}
		strcat(login, "@");
	    }
	}
    }

    return login;
}

/* convert a UI to a URL, returns allocated string */
char *convertUIToURL(struct iurlinfo *ui) {
    char *login, *finalPrefix, *url, *p;

    if (!strcmp(ui->prefix, "/"))
	finalPrefix = "/.";
    else
	finalPrefix = ui->prefix;

    login = "";
    login = getLoginName(login, ui);

    url = malloc(strlen(finalPrefix) + 25 + strlen(ui->address) + strlen(login));

    /* sanitize url so we dont have problems like bug #101265 */
    /* basically avoid duplicate /'s                          */
    if (ui->protocol == URL_METHOD_HTTP) {
        for (p=finalPrefix; *p == '/' && *(p+1) && *(p+1) == '/'; p++);
        finalPrefix = p;
    }

    sprintf(url, "%s://%s%s%s", 
	    ui->protocol == URL_METHOD_FTP ? "ftp" : "http",
	    login, ui->address, finalPrefix);

    return url;
}

/* extraHeaders only applicable for http and used for pulling ks from http */
/* see ftp.c:httpGetFileDesc() for details */
/* set to NULL if not needed */
int urlinstStartTransfer(struct iurlinfo * ui, char *path,
                         char *extraHeaders, struct progressCBdata **data,
                         long long *size) {
    int fd, port;
    int family = -1;
    struct in_addr addr;
    struct in6_addr addr6;
    char *hostname, *portstr, *fileName;
    struct hostent *host;

    logMessage(INFO, "transferring %s://%s%s to a fd",
               ui->protocol == URL_METHOD_FTP ? "ftp" : "http",
               ui->address, path);

    splitHostname(ui->address, &hostname, &portstr);
    if (portstr == NULL) {
        port = -1;
    } else {
        errno = 0;
        port = strtol(portstr, NULL, 10);

        if ((errno == ERANGE && (port == LONG_MIN || port == LONG_MAX)) ||
            (errno != 0 && port == 0)) {
            logMessage(ERROR, "%s: %d: %m", __func__, __LINE__);
            abort();
        }
    }

    if (inet_pton(AF_INET, hostname, &addr) >= 1)
        family = AF_INET;
    else if (inet_pton(AF_INET6, hostname, &addr6) >= 1)
        family = AF_INET6;
    else {
        if ((host = gethostbyname(hostname)) == NULL) {
            logMessage(ERROR, "cannot determine address family of %s: %s",
                       hostname, hstrerror(h_errno));
            return -1;
        }
        else
            family = host->h_addrtype;
    }

    if (ui->protocol == URL_METHOD_FTP) {
        ui->ftpPort = ftpOpen(hostname, family,
                              ui->login ? ui->login : "anonymous", 
                              ui->password ? ui->password : "rhinstall@", 
                              port);
        if (ui->ftpPort < 0) {
            if (hostname) free(hostname);
            return -2;
        }

        fd = ftpGetFileDesc(ui->ftpPort, addr6, family, path, size);
        if (fd < 0) {
            close(ui->ftpPort);
            if (hostname) free(hostname);
            return -1;
        }
    } else {
        fd = httpGetFileDesc(hostname, port, path, extraHeaders, size);
        if (fd < 0) {
            if (portstr) free(portstr);
            return -1;
        }
    }

    fileName = strrchr(path, '/');

    if (FL_CMDLINE(flags)) {
        printf("%s %s...\n", _("Retrieving"), fileName+1);
    } else if (*size) {
        *data = winProgressBar(70, 5, _("Retrieving"), "%s %s...", _("Retrieving"),
                               fileName+1);
    } else {
        winStatus(70, 3, _("Retrieving"), "%s %s...", _("Retrieving"), fileName+1);
    }

    if (hostname) free(hostname);
    return fd;
}

int urlinstFinishTransfer(struct iurlinfo * ui, int fd, struct progressCBdata **data) {
    if (*data) free(*data);
    if (ui->protocol == URL_METHOD_FTP)
        close(ui->ftpPort);
    close(fd);

    if (!FL_CMDLINE(flags))
        newtPopWindow();

    return 0;
}

char * addrToIp(char * hostname) {
    struct in_addr ad;
    struct in6_addr ad6;
    char *ret;
    struct hostent *host;

    if ((ret = malloc(INET6_ADDRSTRLEN+1)) == NULL)
        return hostname;

    if (inet_ntop(AF_INET, &ad, ret, INET_ADDRSTRLEN) != NULL)
        return ret;
    else if (inet_ntop(AF_INET6, &ad6, ret, INET6_ADDRSTRLEN) != NULL)
        return ret;
    else if ((host = gethostbyname(hostname)) != NULL)
        return host->h_name;
    else
        return NULL;
}

int urlMainSetupPanel(struct iurlinfo * ui) {
    newtComponent form, okay, cancel, urlEntry;
    newtComponent answer, text;
    char *url = "";
    char * reflowedText = NULL;
    int width, height;
    newtGrid buttons, grid;
    char * chptr;
    char * buf = NULL;

    /* Populate the UI with whatever initial value we've got. */
    if (ui && ui->prefix)
        url = convertUIToURL(ui);

    buttons = newtButtonBar(_("OK"), &okay, _("Back"), &cancel, NULL);

    if (asprintf(&buf,
            _("Please enter the URL containing the %s installation image on your server."),
                 getProductName()) == -1) {
        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
        abort();
    }

    reflowedText = newtReflowText(buf, 47, 5, 5, &width, &height);
    free(buf);

    text = newtTextbox(-1, -1, width, height, NEWT_TEXTBOX_WRAP);
    newtTextboxSetText(text, reflowedText);
    free(reflowedText);

    urlEntry = newtEntry(22, 8, url, 60, (const char **) &url,
                         NEWT_ENTRY_SCROLL);

    grid = newtCreateGrid(1, 3);
    newtGridSetField(grid, 0, 0, NEWT_GRID_COMPONENT, text,
                     0, 0, 0, 1, 0, 0);
    newtGridSetField(grid, 0, 1, NEWT_GRID_COMPONENT, urlEntry,
                     0, 0, 0, 1, 0, 0);
    newtGridSetField(grid, 0, 2, NEWT_GRID_SUBGRID, buttons,
                     0, 0, 0, 0, 0, NEWT_GRID_FLAG_GROWX);

    form = newtForm(NULL, NULL, 0);
    newtGridAddComponentsToForm(grid, form, 1); 
    newtGridWrappedWindow(grid, _("URL Setup"));
    newtGridFree(grid, 1);

    do {
        answer = newtRunForm(form);
        if (answer != cancel) {
            if (!strlen(url)) {
                newtWinMessage(_("Error"), _("OK"),
                               _("You must enter a URL."));
                continue;
            }

            if (!strstr(url, "http://") && !strstr(url, "ftp://")) {
                newtWinMessage(_("Error"), _("OK"),
                               _("URL must be either an ftp or http URL"));
                continue;
            }

            /* Now split up the URL we were given into its components for
             * ease of checking.
             */
            if (convertURLToUI(url, ui) == -1)
                continue;

            if (!addrToIp(ui->address)) {
                newtWinMessage(_("Unknown Host"), _("OK"),
                        _("%s is not a valid hostname."), ui->address);
                continue;
            }
        }

        break;
    } while (1);

    if (answer == cancel) {
        newtFormDestroy(form);
        newtPopWindow();

        return LOADER_BACK;
    }

    /* Get rid of trailing /'s */
    chptr = ui->prefix + strlen(ui->prefix) - 1;
    while (chptr > ui->prefix && *chptr == '/') chptr--;
    chptr++;
    *chptr = '\0';

    newtFormDestroy(form);
    newtPopWindow();

    return 0;
}
