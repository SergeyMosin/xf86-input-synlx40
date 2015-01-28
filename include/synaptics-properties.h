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
 * Copyright © 2008 Red Hat, Inc.
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
 * Authors: Peter Hutterer
 */

#ifndef _SYNAPTICS_PROPERTIES_H_
#define _SYNAPTICS_PROPERTIES_H_

/**
 * Properties exported by the synaptics driver. These properties are
 * recognized by the driver and will change its behavior when modified.
 * For a description of what each property does, see synaptics.h.
 */

/* 32 bit, 4 values, left, right, top, bottom */
//~ #define SYNAPTICS_PROP_EDGES "Synaptics Edges"

/* 32 bit, 3 values, low, high, <deprecated> */
#define SYNAPTICS_PROP_FINGER "Synaptics Finger"

/* 32 bit */
#define SYNAPTICS_PROP_TAP_TIME "Synaptics Tap Time"

/* 32 bit */
#define SYNAPTICS_PROP_TAP_MOVE "Synaptics Tap Move"

/* 8 bit (BOOL) */
#define SYNAPTICS_PROP_CLICKPAD "Synaptics ClickPad"

/* 32 bit, 2 values, vert, horiz */
#define SYNAPTICS_PROP_SCROLL_DISTANCE "Synaptics Scrolling Distance"

/* 8 bit (BOOL), 2 values, vertical, horizontal */
#define SYNAPTICS_PROP_SCROLL_TWOFINGER "Synaptics Two-Finger Scrolling"

/* FLOAT, 4 values, min, max, accel, <deprecated> */
#define SYNAPTICS_PROP_SPEED "Synaptics Move Speed"

/* 8 bit, valid values (0, 1, 2) */
#define SYNAPTICS_PROP_OFF "Synaptics Off"

/* CARD32, 2 values, min, max */
#define SYNAPTICS_PROP_PRESSURE_MOTION "Synaptics Pressure Motion"

/* FLOAT, 2 values, min, max */
#define SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR "Synaptics Pressure Motion Factor"

/* 8 bit (BOOL) */
#define SYNAPTICS_PROP_GRAB "Synaptics Grab Event Device"

/* 8 bit (BOOL), 7 values (read-only), has_left, has_middle, has_right,
 * has_double, has_triple, has_pressure, has_width */
#define SYNAPTICS_PROP_CAPABILITIES "Synaptics Capabilities"

/* 32 bit unsigned, 2 values, vertical, horizontal in units/millimeter */
#define SYNAPTICS_PROP_RESOLUTION "Synaptics Pad Resolution"

/* 32 Bit Integer, 2 values, horizontal hysteresis, vertical hysteresis */
#define SYNAPTICS_PROP_NOISE_CANCELLATION "Synaptics Noise Cancellation"


/* 32 bit, 3 values, height, separator position, separator width */
#define SYNAPTICS_PROP_BOTTOM_BUTTONS "Synaptics Bottom Buttons"

/* 32 bit, 2 values, height, middle button width */
#define SYNAPTICS_PROP_TOP_BUTTONS "Synaptics Top Buttons"

/* 32 bit */
#define SYNAPTICS_PROP_SCROLL_TWOFINGER_FINGER_SIZE "Synaptics 2FS Finger Box Size"

/* 32 bit, 3 values, pressure, tap anywhere(0,1,2), tap hold timeout */
#define SYNAPTICS_PROP_TAP_EXTRAS "Synaptics Tap Extras"

#endif                          /* _SYNAPTICS_PROPERTIES_H_ */
