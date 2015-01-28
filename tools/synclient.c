/*
 * Copyright © 2014 Sergey Mosin
 *
 * This driver is based on xf86-input-synaptics 1.8.1-1 driver. It is
 * geared towards Lenovo XX40(T540/T440/X240/E440 etc) series laptops.
 * Some features have been added and some have been discarded. See below
 * for original license, authors and contributors.
 *
 * - Sergey Mosin <serge@sergem.org>
 *
 * ----------------------------------------------------------
 *
 * Copyright © 2002-2005,2007 Peter Osterlund
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of Red Hat
 * not be used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.  Red
 * Hat makes no representations about the suitability of this software
 * for any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * THE AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:
 *      Peter Osterlund (petero2@telia.com)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <math.h>
#include <limits.h>

#include <X11/Xdefs.h>
#include <X11/Xatom.h>
#include <X11/extensions/XI.h>
#include <X11/extensions/XInput.h>
#include "synaptics-properties.h"

#ifndef XATOM_FLOAT
#define XATOM_FLOAT "FLOAT"
#endif

#define SYN_MAX_BUTTONS 12
#define SBR_MIN 10
#define SBR_MAX 1000

union flong {                   /* Xlibs 64-bit property handling madness */
    long l;
    float f;
};

enum ParaType {
    PT_INT,
    PT_BOOL,
    PT_DOUBLE
};

struct Parameter {
    char *name;                 /* Name of parameter */
    enum ParaType type;         /* Type of parameter */
    double min_val;             /* Minimum allowed value */
    double max_val;             /* Maximum allowed value */
    char *prop_name;            /* Property name */
    int prop_format;            /* Property format (0 for floats) */
    int prop_offset;            /* Offset inside property */
};

static struct Parameter params[] = {
    {"FingerLow",             PT_INT,    0, 255,   SYNAPTICS_PROP_FINGER,	32,	0},
    {"FingerHigh",            PT_INT,    0, 255,   SYNAPTICS_PROP_FINGER,	32,	1},
    {"MaxTapTime",            PT_INT,    0, 1000,  SYNAPTICS_PROP_TAP_TIME,	32,	0},
    {"MaxTapMove",            PT_INT,    0, 2000,  SYNAPTICS_PROP_TAP_MOVE,	32,	0},
    {"VertScrollDelta",       PT_INT,    -1000, 1000,  SYNAPTICS_PROP_SCROLL_DISTANCE,	32,	0},
    {"HorizScrollDelta",      PT_INT,    -1000, 1000,  SYNAPTICS_PROP_SCROLL_DISTANCE,	32,	1},
    {"VertTwoFingerScroll",   PT_BOOL,   0, 1,     SYNAPTICS_PROP_SCROLL_TWOFINGER,	8,	0},
    {"HorizTwoFingerScroll",  PT_BOOL,   0, 1,     SYNAPTICS_PROP_SCROLL_TWOFINGER,	8,	1},
    {"MinSpeed",              PT_DOUBLE, 0, 255.0,   SYNAPTICS_PROP_SPEED,	0, /*float */	0},
    {"MaxSpeed",              PT_DOUBLE, 0, 255.0,   SYNAPTICS_PROP_SPEED,	0, /*float */	1},
    {"AccelFactor",           PT_DOUBLE, 0, 1.0,   SYNAPTICS_PROP_SPEED,	0, /*float */	2},
    {"TouchpadOff",           PT_INT,    0, 2,     SYNAPTICS_PROP_OFF,		8,	0},
    {"PressureMotionMinZ",    PT_INT,    1, 255,   SYNAPTICS_PROP_PRESSURE_MOTION,	32,	0},
    {"PressureMotionMaxZ",    PT_INT,    1, 255,   SYNAPTICS_PROP_PRESSURE_MOTION,	32,	1},
    {"PressureMotionMinFactor", PT_DOUBLE, 0, 10.0,SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR,	0 /*float*/,	0},
    {"PressureMotionMaxFactor", PT_DOUBLE, 0, 10.0,SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR,	0 /*float*/,	1},
    {"GrabEventDevice",       PT_BOOL,   0, 1,     SYNAPTICS_PROP_GRAB,	8,	0},
    {"HorizHysteresis",       PT_INT,    0, 10000, SYNAPTICS_PROP_NOISE_CANCELLATION, 32,	0},
    {"VertHysteresis",        PT_INT,    0, 10000, SYNAPTICS_PROP_NOISE_CANCELLATION, 32,	1},
    {"ClickPad",              PT_BOOL,   0, 1,     SYNAPTICS_PROP_CLICKPAD,	8,	0},

	{"BottomButtonsHeight",		PT_INT,		0, 100,		SYNAPTICS_PROP_BOTTOM_BUTTONS,	32,	0},
	{"BottomButtonsSepPos",		PT_INT,		0, 100,		SYNAPTICS_PROP_BOTTOM_BUTTONS,	32,	1},
	{"BottomButtonsSepWidth",	PT_INT,		0, 100,		SYNAPTICS_PROP_BOTTOM_BUTTONS,	32,	2},
	{"TopButtonsHeight",		PT_INT,		0, 100,		SYNAPTICS_PROP_TOP_BUTTONS,	32,	0},
	{"TopButtonsMiddleWidth",	PT_INT,		0, 100,		SYNAPTICS_PROP_TOP_BUTTONS,	32,	1},
	{"TwoFingerScrollFingerSize",	PT_INT,		0, 100,	SYNAPTICS_PROP_SCROLL_TWOFINGER_FINGER_SIZE,	32,	0},
	{"MinTapPressure",			PT_INT,		1, 255,		SYNAPTICS_PROP_TAP_EXTRAS,	32,	0},
	{"TapAnywhere",				PT_INT,		0,	1,		SYNAPTICS_PROP_TAP_EXTRAS,	32,	1},
	{"TapHoldGuesture",			PT_INT,		0,	30000,	SYNAPTICS_PROP_TAP_EXTRAS,	32,	2},

    { NULL, 0, 0, 0, 0 }
};

static double
parse_cmd(char *cmd, struct Parameter **par)
{
    char *eqp = strchr(cmd, '=');

    *par = NULL;

    if (eqp) {
        int j;
        int found = 0;

        *eqp = 0;
        for (j = 0; params[j].name; j++) {
            if (strcasecmp(cmd, params[j].name) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            double val = atof(&eqp[1]);

            *par = &params[j];

            if (val < (*par)->min_val)
                val = (*par)->min_val;
            if (val > (*par)->max_val)
                val = (*par)->max_val;

            return val;
        }
        else {
            printf("Unknown parameter %s\n", cmd);
        }
    }
    else {
        printf("Invalid command: %s\n", cmd);
    }

    return 0;
}

/** Init display connection or NULL on error */
static Display *
dp_init()
{
    Display *dpy = NULL;
    XExtensionVersion *v = NULL;
    Atom touchpad_type = 0;
    Atom synaptics_property = 0;
    int error = 0;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Failed to connect to X Server.\n");
        error = 1;
        goto unwind;
    }

    v = XGetExtensionVersion(dpy, INAME);
    if (!v->present ||
        (v->major_version * 1000 + v->minor_version) <
        (XI_Add_DeviceProperties_Major * 1000 +
         XI_Add_DeviceProperties_Minor)) {
        fprintf(stderr, "X server supports X Input %d.%d. I need %d.%d.\n",
                v->major_version, v->minor_version,
                XI_Add_DeviceProperties_Major, XI_Add_DeviceProperties_Minor);
        error = 1;
        goto unwind;
    }

    /* We know synaptics sets XI_TOUCHPAD for all the devices. */
    touchpad_type = XInternAtom(dpy, XI_TOUCHPAD, True);
    if (!touchpad_type) {
        fprintf(stderr, "XI_TOUCHPAD not initialised.\n");
        error = 1;
        goto unwind;
    }

    //~ synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_EDGES, True);
    synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_FINGER, True);
    if (!synaptics_property) {
        fprintf(stderr, "Couldn't find synaptics properties. No synaptics "
                "driver loaded?\n");
        error = 1;
        goto unwind;
    }

 unwind:
    XFree(v);
    if (error && dpy) {
        XCloseDisplay(dpy);
        dpy = NULL;
    }
    return dpy;
}

static XDevice *
dp_get_device(Display * dpy)
{
    XDevice *dev = NULL;
    XDeviceInfo *info = NULL;
    int ndevices = 0;
    Atom touchpad_type = 0;
    Atom synaptics_property = 0;
    Atom *properties = NULL;
    int nprops = 0;
    int error = 0;

    touchpad_type = XInternAtom(dpy, XI_TOUCHPAD, True);
    //~ synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_EDGES, True);
    synaptics_property = XInternAtom(dpy, SYNAPTICS_PROP_FINGER, True);
    info = XListInputDevices(dpy, &ndevices);

    while (ndevices--) {
        if (info[ndevices].type == touchpad_type) {
            dev = XOpenDevice(dpy, info[ndevices].id);
            if (!dev) {
                fprintf(stderr, "Failed to open device '%s'.\n",
                        info[ndevices].name);
                error = 1;
                goto unwind;
            }

            properties = XListDeviceProperties(dpy, dev, &nprops);
            if (!properties || !nprops) {
                fprintf(stderr, "No properties on device '%s'.\n",
                        info[ndevices].name);
                error = 1;
                goto unwind;
            }

            while (nprops--) {
                if (properties[nprops] == synaptics_property)
                    break;
            }
            if (!nprops) {
                fprintf(stderr, "No synaptics properties on device '%s'.\n",
                        info[ndevices].name);
                error = 1;
                goto unwind;
            }

            break;              /* Yay, device is suitable */
        }
    }

 unwind:
    XFree(properties);
    XFreeDeviceList(info);
    if (!dev)
        fprintf(stderr, "Unable to find a synaptics device.\n");
    else if (error && dev) {
        XCloseDevice(dpy, dev);
        dev = NULL;
    }
    return dev;
}

static void
dp_set_variables(Display * dpy, XDevice * dev, int argc, char *argv[],
                 int first_cmd)
{
    int i;
    double val;
    struct Parameter *par;
    Atom prop, type, float_type;
    int format;
    unsigned char *data;
    unsigned long nitems, bytes_after;

    union flong *f;
    long *n;
    char *b;

    float_type = XInternAtom(dpy, XATOM_FLOAT, True);
    if (!float_type)
        fprintf(stderr, "Float properties not available.\n");

    for (i = first_cmd; i < argc; i++) {
        val = parse_cmd(argv[i], &par);
        if (!par)
            continue;

        prop = XInternAtom(dpy, par->prop_name, True);
        if (!prop) {
            fprintf(stderr, "Property for '%s' not available. Skipping.\n",
                    par->name);
            continue;

        }

        XGetDeviceProperty(dpy, dev, prop, 0, 1000, False, AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &data);

        if (type == None) {
            fprintf(stderr, "Property for '%s' not available. Skipping.\n",
                    par->name);
            continue;
        }

        switch (par->prop_format) {
        case 8:
            if (format != par->prop_format || type != XA_INTEGER) {
                fprintf(stderr, "   %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }
            b = (char *) data;
            b[par->prop_offset] = rint(val);
            break;
        case 32:
            if (format != par->prop_format ||
                (type != XA_INTEGER && type != XA_CARDINAL)) {
                fprintf(stderr, "   %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }
            n = (long *) data;
            n[par->prop_offset] = rint(val);
            break;
        case 0:                /* float */
            if (!float_type)
                continue;
            if (format != 32 || type != float_type) {
                fprintf(stderr, "   %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }
            f = (union flong *) data;
            f[par->prop_offset].f = val;
            break;
        }

        XChangeDeviceProperty(dpy, dev, prop, type, format,
                              PropModeReplace, data, nitems);
        XFlush(dpy);
    }
}

/* FIXME: horribly inefficient. */
static void
dp_show_settings(Display * dpy, XDevice * dev)
{
    int j;
    Atom a, type, float_type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *data;
    int len;

    union flong *f;
    long *i;
    char *b;

    float_type = XInternAtom(dpy, XATOM_FLOAT, True);
    if (!float_type)
        fprintf(stderr, "Float properties not available.\n");

    printf("Parameter settings:\n");
    for (j = 0; params[j].name; j++) {
        struct Parameter *par = &params[j];

        a = XInternAtom(dpy, par->prop_name, True);
        if (!a)
            continue;

        len =
            1 +
            ((par->prop_offset * (par->prop_format ? par->prop_format : 32) /
              8)) / 4;

        XGetDeviceProperty(dpy, dev, a, 0, len, False,
                           AnyPropertyType, &type, &format,
                           &nitems, &bytes_after, &data);
        if (type == None)
            continue;

        switch (par->prop_format) {
        case 8:
            if (format != par->prop_format || type != XA_INTEGER) {
                fprintf(stderr, "    %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }

            b = (char *) data;
            printf("    %-23s = %d\n", par->name, b[par->prop_offset]);
            break;
        case 32:
            if (format != par->prop_format ||
                (type != XA_INTEGER && type != XA_CARDINAL)) {
                fprintf(stderr, "    %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }

            i = (long *) data;
            printf("    %-23s = %ld\n", par->name, i[par->prop_offset]);
            break;
        case 0:                /* Float */
            if (!float_type)
                continue;
            if (format != 32 || type != float_type) {
                fprintf(stderr, "    %-23s = format mismatch (%d)\n",
                        par->name, format);
                break;
            }

            f = (union flong *) data;
            printf("    %-23s = %g\n", par->name, f[par->prop_offset].f);
            break;
        }

        XFree(data);
    }
}

static void
usage(void)
{
    fprintf(stderr, "Usage: synclient [-h] [-l] [-V] [-?] [var1=value1 [var2=value2] ...]\n");
    fprintf(stderr, "  -l List current user settings\n");
    fprintf(stderr, "  -V Print synclient version string and exit\n");
    fprintf(stderr, "  -? Show this help message\n");
    fprintf(stderr, "  var=value  Set user parameter 'var' to 'value'.\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    int c;
    int dump_settings = 0;
    int first_cmd;

    Display *dpy;
    XDevice *dev;

    if (argc == 1)
        dump_settings = 1;

    /* Parse command line parameters */
    while ((c = getopt(argc, argv, "lV?")) != -1) {
        switch (c) {
        case 'l':
            dump_settings = 1;
            break;
        case 'V':
            printf("%s\n", VERSION);
            exit(0);
        case '?':
        default:
            usage();
        }
    }

    first_cmd = optind;
    if (!dump_settings && first_cmd == argc)
        usage();

    dpy = dp_init();
    if (!dpy || !(dev = dp_get_device(dpy)))
        return 1;

    dp_set_variables(dpy, dev, argc, argv, first_cmd);
    if (dump_settings)
        dp_show_settings(dpy, dev);

    XCloseDevice(dpy, dev);
    XCloseDisplay(dpy);

    return 0;
}
