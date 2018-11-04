/*
 * cdinstall.c - code to set up cdrom installs
 *
 * Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003  Red Hat, Inc.
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

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <newt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <asm/types.h>
#include <limits.h>
#include <linux/cdrom.h>
#include <stdio.h>

#include "kickstart.h"
#include "loader.h"
#include "loadermisc.h"
#include "log.h"
#include "lang.h"
#include "modules.h"
#include "method.h"
#include "cdinstall.h"
#include "mediacheck.h"
#include "windows.h"

#include "../isys/imount.h"
#include "../isys/isys.h"
#include "../syslinux/com32/include/fcntl.h"

/* boot flags */
extern uint64_t flags;

/* ejects the CD device the device node points at */
static void ejectCdrom(char *device) {
    int ejectfd;

    if (!device) return;
    logMessage(INFO, "ejecting %s...",device);
    if ((ejectfd = open(device, O_RDONLY | O_NONBLOCK, 0)) >= 0) {
        ioctl(ejectfd, CDROM_LOCKDOOR, 0);
        if (ioctl(ejectfd, CDROMEJECT, 0))
            logMessage(ERROR, "eject failed on device %s: %m", device);
        close(ejectfd);
    } else {
        logMessage(ERROR, "could not open device %s: %m", device);
    }
}

static char *cdrom_drive_status(int rc) {
    struct {
        int code;
        char *str;
    } status_codes[] =
        {
            { CDS_NO_INFO, "CDS_NO_INFO" },
            { CDS_NO_DISC, "CDS_NO_DISC" },
            { CDS_TRAY_OPEN, "CDS_TRAY_OPEN" },
            { CDS_DRIVE_NOT_READY, "CDS_DRIVE_NOT_READY" },
            { CDS_DISC_OK, "CDS_DISC_OK" },
            { CDS_AUDIO, "CDS_AUDIO" },
            { CDS_DATA_1, "CDS_DATA_1" },
            { CDS_DATA_2, "CDS_DATA_2" },
            { CDS_XA_2_1, "CDS_XA_2_1" },
            { CDS_XA_2_2, "CDS_XA_2_2" },
            { CDS_MIXED, "CDS_MIXED" },
            { INT_MAX, NULL },
        };
    int i;

    if (rc < 0)
        return strerror(-rc);

    for (i = 0; status_codes[i].code != INT_MAX; i++) {
        if (status_codes[i].code == rc)
            return status_codes[i].str;
    }
    return NULL;
}

static int waitForCdromTrayClose(int fd) {
    int rc;
    int prev = INT_MAX;

    do {
        char *status = NULL;
        rc = ioctl(fd, CDROM_DRIVE_STATUS, CDSL_CURRENT);
        if (rc < 0)
            rc = -errno;

        /* only bother to print the status if it changes */
        if (prev == INT_MAX || prev != rc) {
            status = cdrom_drive_status(rc);
            if (status != NULL) {
                logMessage(INFO, "drive status is .%s.", status);
            } else {
                logMessage(INFO, "drive status is unknown status code %d", rc);
            }
        }
        prev = rc;
        if (rc == CDS_DRIVE_NOT_READY)
            usleep(100000);
    } while (rc == CDS_DRIVE_NOT_READY);
    return rc;
}

static void closeCdromTray(char *device) {
    int fd;

    if (!device || !*device)
        return;

    logMessage(INFO, "closing CD tray on %s .", device);
    if ((fd = open(device, O_RDONLY | O_NONBLOCK, 0)) >= 0) {
        if (ioctl(fd, CDROMCLOSETRAY, 0)) {
            logMessage(ERROR, "closetray failed on device %s: %m", device);
        } else {
            waitForCdromTrayClose(fd);
            ioctl(fd, CDROM_LOCKDOOR, 1);
        }
        close(fd);
    } else {
        logMessage(ERROR, "could not open device %s: %m", device);
    }
}

/* Given cd device cddriver, this function will attempt to check its internal
 * checksum.
 */
static void mediaCheckCdrom(char *cddriver) {
    int rc;
    int first;

    first = 1;
    do {
        char *descr;
        char *tstamp;
        int ejectcd;

        /* init every pass */
        ejectcd = 0;
        descr = NULL;

        closeCdromTray(cddriver);

        /* if first time through, see if they want to eject the CD      */
        /* currently in the drive (most likely the CD they booted from) */
        /* and test a different disk.  Otherwise just test the disk in  */
        /* the drive since it was inserted in the previous pass through */
        /* this loop, so they want it tested.                           */
        if (first) {
            first = 0;
            rc = newtWinChoice(_("Media Check"), _("Test"), _("Eject Disc"),
                               _("Choose \"%s\" to test the disc currently in "
                                 "the drive, or \"%s\" to eject the disc and "
                                 "insert another for testing."), _("Test"),
                               _("Eject Disc"));

            if (rc == 2)
                ejectcd = 1;
        }

        if (!ejectcd) {
            /* XXX MSFFIXME: should check return code for error */
            readStampFileFromIso(cddriver, &tstamp, &descr);
            doMediaCheck(cddriver, descr);

            if (descr)
                free(descr);
        }

        ejectCdrom(cddriver);

        rc = newtWinChoice(_("Media Check"), _("Test"), _("Continue"),
                       _("If you would like to test additional media, "
                       "insert the next disc and press \"%s\". "
                       "Testing each disc is not strictly required, however "
                       "it is highly recommended.  Minimally, the discs should "
                       "be tested prior to using them for the first time. "
                       "After they have been successfully tested, it is not "
                       "required to retest each disc prior to using it again."),
                       _("Test"), _("Continue"));

        if (rc == 2) {
            closeCdromTray(cddriver);
            return;
        } else {
            continue;
        }
    } while (1);
}

/* output an error message when CD in drive is not the correct one */
/* Used by mountCdromStage2()                                      */
static void wrongCDMessage(void) {
    newtWinMessage(_("Error"), _("OK"),
                   _("The %s disc was not found "
                     "in any of your drives. Please insert "
                     "the %s disc and press %s to retry."),
                   getProductName(), getProductName(), _("OK"));
}

/* ask about doing media check */
static void queryCDMediaCheck(char *dev, char *location) {
    int rc;
    char *stage2loc;

    /* dont bother to test in automated installs */
    if (FL_KICKSTART(flags) && !FL_MEDIACHECK(flags))
        return;

    /* see if we should check image(s) */
    /* in rescue mode only test if they explicitly asked to */
    if (!FL_RESCUE(flags) || FL_MEDIACHECK(flags)) {
        startNewt();
        rc = newtWinChoice(_("Disc Found"), _("OK"), _("Skip"),
             _("To begin testing the media before installation press %s.\n\n"
               "Choose %s to skip the media test and start the installation."),
             _("OK"), _("Skip"));

        if (rc != 2) {
            /* We already mounted the CD earlier to verify there's at least a
             * stage2 image.  Now we need to unmount to perform the check, then
             * remount to pretend nothing ever happened.
             */
            umount(location);
            mediaCheckCdrom(dev);

            do {
                if (doPwMount(dev, location, "iso9660", "ro", NULL)) {
                    ejectCdrom(dev);
                    wrongCDMessage();
                    continue;
                }

                if (asprintf(&stage2loc, "%s/images/install.img",
                             location) == -1) {
                    logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                    abort();
                }

                if (access(stage2loc, R_OK)) {
                    free(stage2loc);
                    logMessage(CRITICAL, "%s: %d: unable %s %m", __func__, __LINE__, stage2loc);
                    umount(location);
                    ejectCdrom(dev);
                    wrongCDMessage();
                    continue;
                }

                free(stage2loc);
                break;
            } while (1);
        }
    }
}

/* Set up a CD/DVD drive to mount the stage2 image from.  If successful, the
 * stage2 image will be left mounted on /mnt/runtime.
 *
 * location:     Where to mount the media at (usually /mnt/stage2)
 * loaderData:   The usual, can be NULL if no info
 * interactive:  Whether or not to prompt about questions/errors
 * mediaCheck:   Do we run media check or not?
 */
static char *setupCdrom(char *location, struct loaderData_s *loaderData,
                        int interactive, int mediaCheck) {
    int i, rc;
    int stage2inram = 0;
    char *retbuf = NULL, *stage2loc, *stage2img;
    struct device ** devices;
    char *cddev = NULL;
    char *devwithpart = NULL;

    /*
     * In this step we try detect a CDROM media. If this detection
     * fails, we try a device disk dectection, usually an USB memory
     * stick with the installation ISO burned (with dd tool).
     * So it's a Usb Mass Storage device with iso9660 format.
     */
    devices = getDevices(DEVICE_CDROM);
    if (!devices) {
        logMessage(ERROR, "got to setupCdrom without a CD device");
        devices = getDevices(DEVICE_DISK);
        if (!devices) {
                logMessage(ERROR, "Not CD device or usb stick detected");
                return NULL;
        }
        logMessage(INFO, "USB Mass Storage stick detected");
    }

    if (asprintf(&stage2loc, "%s/images/install.img", location) == -1) {
        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
        abort();
    }

    logMessage(INFO, "%s: %d: stage2loc -> %s", __func__, __LINE__, stage2loc);

    /* JKFIXME: ASSERT -- we have a cdrom device when we get here */
    do {
        for (i = 0; devices[i]; i++) {
            char *tmp = NULL;
            int fd;

            logMessage(INFO, "Device: %s, Description: %s",
                    devices[i]->device, devices[i]->description);

            switch(devices[i]->type) {
                case DEVICE_ANY:
                    logMessage(INFO, "Device type ANY");
                    break;
                case DEVICE_NETWORK:
                    logMessage(INFO, "Device type NETWORK");
                    break;
                case DEVICE_CDROM:
                    logMessage(INFO, "Device type CDROM");
                    break;
                case DEVICE_DISK:
                    logMessage(INFO, "Device type DISK");
                    break;
                default:
                    logMessage(INFO, "Device type UNKNOW");
            }

            if (!devices[i]->device)
                continue;

            if (strncmp("/dev/", devices[i]->device, 5)) {
                if (asprintf(&tmp, "/dev/%s", devices[i]->device) == -1) {
                    logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                    abort();
                }

                free(devices[i]->device);
                devices[i]->device = tmp;
            }

            devwithpart = malloc(256);
            sprintf(devwithpart, "%s%s", devices[i]->device, "1");

            logMessage(INFO, "trying to mount CD device %s on %s",
                       devwithpart, location);

            if (!FL_CMDLINE(flags))
                winStatus(60, 3,
                        _("Scanning"),
                        _("Looking for installation images on CD device %s"),
                        devwithpart);
            else
                printf(_("Looking for installation images on CD device %s"), devices[i]->device);

            //fd = open(devices[i]->device, O_RDONLY | O_NONBLOCK);
            fd = open(devwithpart, O_RDONLY | O_NONBLOCK);
            if (fd < 0) {
                //logMessage(ERROR, "Couldn't open %s: %m", devices[i]->device);
                logMessage(ERROR, "Couldn't open %s: %m", devwithpart);
                if (!FL_CMDLINE(flags))
                    newtPopWindow();
                continue;
            }

            logMessage(INFO, "waitForCdromTrayClose %s", devwithpart);
            /*
            rc = waitForCdromTrayClose(fd);
            close(fd);
            switch (rc) {
                case CDS_NO_INFO:
                    logMessage(ERROR, "Drive tray reports CDS_NO_INFO");
                    break;
                case CDS_NO_DISC:
                    if (!FL_CMDLINE(flags))
                            newtPopWindow();
                    continue;
                case CDS_TRAY_OPEN:
                    logMessage(ERROR, "Drive tray reports open when it should be closed");
                    break;
                default:
                    break;
            }
             */

            if (!FL_CMDLINE(flags))
                newtPopWindow();

            if (!(rc=doPwMount(devwithpart, location, "iso9660", "ro", NULL))) {
                //cddev = devices[i]->device;
                cddev = devwithpart;
                logMessage(INFO, "doPwMount rc: %i", rc);

                if (!access(stage2loc, R_OK)) {
                    char *updpath;

                    logMessage(INFO, "doPwMount, acces");
                    if (mediaCheck)
                        //queryCDMediaCheck(devices[i]->device, location);
                        queryCDMediaCheck(devwithpart, location);

                    /* if in rescue mode lets copy stage 2 into RAM so we can */
                    /* free up the CD drive and user can have it avaiable to  */
                    /* aid system recovery.                                   */
                    if (FL_RESCUE(flags) && !FL_TEXT(flags) &&
                        totalMemory() > 128000) {
			            /*
			             * copy /mnt/stage2/images/install.img to
			             * /tmp/install.img
			             */
                        logMessage(INFO, "copyFile: %s to /tm/install.img",
                                stage2loc);

                        rc = copyFile(stage2loc, "/tmp/install.img");
                        stage2img = strdup("/tmp/install.img");
                        stage2inram = 1;
                    } else {
                        stage2img = strdup(stage2loc);
                        stage2inram = 0;
                    }

		            /*
		             * loopback mounting of stage-2 installer
		             */
                    rc = mountStage2(stage2img);
                    free(stage2img);

                    if (rc) {
                        logMessage(INFO, "mounting stage2 failed");
                        umount(location);
                        continue;
                    }

                    logMessage(INFO, "mounting stage2 done");

                    umount(location);
                    if (asprintf(&updpath, "%s/images/updates.img", location) == -1) {
                        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                        abort();
                    }

                    logMessage(INFO, "Looking for updates in %s", updpath);
                    copyUpdatesImg(updpath);
                    free(updpath);

                    if (asprintf(&updpath, "%s/images/product.img", location) == -1) {
                        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                        abort();
                    }

                    logMessage(INFO, "Looking for product in %s", updpath);
                    copyProductImg(updpath);
                    free(updpath);

                    /* if in rescue mode and we copied stage2 to RAM */
                    /* we can now unmount the CD                     */
                    if (FL_RESCUE(flags) && stage2inram) {
                        umount(location);
                    }

                    if (asprintf(&retbuf, "cdrom://%s:%s",
                                 devices[i]->device, location) == -1) {
                        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                        abort();
                    }
                } else {
                    logMessage(INFO, "doPwMount, unmounting");
                    /* this wasnt the CD we were looking for, clean up and */
                    /* try the next CD drive                               */
                    umount(location);
                }
            }
        }

        if (!retbuf) {
            if (interactive) {
                char * buf;

                if (asprintf(&buf, _("The %s disc was not found in any of your "
                                     "CDROM drives. Please insert the %s disc "
                                     "and press %s to retry."),
                    getProductName(), getProductName(), _("OK")) == -1) {
                        logMessage(CRITICAL, "%s: %d: %m", __func__, __LINE__);
                        abort();
                }

                ejectCdrom(cddev);
                rc = newtWinChoice(_("Disc Not Found"),
                                   _("OK"), _("Back"), buf, _("OK"));
                free(buf);
                if (rc == 2)
                    goto err;
            } else {
                /* we can't ask them about it, so just return not found */
                goto err;
            }
        }
    } while (!retbuf);

err:
    free(stage2loc);
    return retbuf;
}

/* try to find a install CD non-interactively */
char * findAnacondaCD(char *location) {
    /* JAVI: avoid mediacheck.
     return setupCdrom(location, NULL, 0, 1);*/
    return setupCdrom(location, NULL, 0, 0);
}

/* look for a CD and mount it.  if we have problems, ask */
char * mountCdromImage(struct installMethod * method,
                       char * location, struct loaderData_s * loaderData) {
    return setupCdrom(location, loaderData, 1, 1);
}

void setKickstartCD(struct loaderData_s * loaderData, int argc, char ** argv) {

    logMessage(INFO, "kickstartFromCD");
    loaderData->method = METHOD_CDROM;
}

int kickstartFromCD(char *kssrc) {
    int rc, i;
    char *p, *kspath;
    struct device ** devices;

    logMessage(INFO, "getting kickstart file from first CDROM");

    devices = getDevices(DEVICE_CDROM);
    if (!devices) {
        logMessage(ERROR, "No CDROM devices found!");
        return 1;
    }

    /* format is cdrom:[/path/to/ks.cfg] */
    kspath = "";
    p = strchr(kssrc, ':');
    if (p)
        kspath = p + 1;

    if (!p || strlen(kspath) < 1)
        kspath = "/ks.cfg";

    for (i=0; devices[i]; i++) {
        if (!devices[i]->device)
            continue;

        rc = getKickstartFromBlockDevice(devices[i]->device, kspath);
        if (rc == 0)
            return 0;
    }

    startNewt();
    newtWinMessage(_("Error"), _("OK"),
                   _("Cannot find kickstart file on CDROM."));
    return 1;
}

/* vim:set shiftwidth=4 softtabstop=4 et */
