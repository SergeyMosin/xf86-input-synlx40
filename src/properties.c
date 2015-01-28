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
 * Copyright © 2008-2012 Red Hat, Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xorg-server.h>
#include "xf86Module.h"

#include <X11/Xatom.h>
#include <xf86.h>
#include <xf86Xinput.h>
#include <exevents.h>

#include "synapticsstr.h"
#include "synaptics-properties.h"

#ifndef XATOM_FLOAT
#define XATOM_FLOAT "FLOAT"
#endif

#ifndef XI_PROP_PRODUCT_ID
#define XI_PROP_PRODUCT_ID "Device Product ID"
#endif

#ifndef XI_PROP_DEVICE_NODE
#define XI_PROP_DEVICE_NODE "Device Node"
#endif

static Atom float_type;

Atom prop_finger = 0;
Atom prop_tap_time = 0;
Atom prop_tap_move = 0;
Atom prop_clickpad = 0;
Atom prop_scrolldist = 0;
Atom prop_scrolltwofinger = 0;
Atom prop_speed = 0;
Atom prop_edgemotion_pressure = 0;
Atom prop_edgemotion_speed = 0;
Atom prop_edgemotion_always = 0;
Atom prop_off = 0;
Atom prop_pressuremotion = 0;
Atom prop_pressuremotion_factor = 0;
Atom prop_grab = 0;
Atom prop_capabilities = 0;
Atom prop_resolution = 0;
Atom prop_noise_cancellation = 0;
Atom prop_product_id = 0;
Atom prop_device_node = 0;

Atom prop_bottom_buttons = 0;
Atom prop_top_buttons = 0;
Atom prop_scroll_twofinger_finger_size = 0;
Atom prop_tap_extras = 0;


static Atom
InitTypedAtom(DeviceIntPtr dev, char *name, Atom type, int format, int nvalues,
              int *values)
{
    int i;
    Atom atom;
    uint8_t val_8[9];           /* we never have more than 9 values in an atom */
    uint16_t val_16[9];
    uint32_t val_32[9];
    pointer converted;

    for (i = 0; i < nvalues; i++) {
        switch (format) {
        case 8:
            val_8[i] = values[i];
            break;
        case 16:
            val_16[i] = values[i];
            break;
        case 32:
            val_32[i] = values[i];
            break;
        }
    }

    switch (format) {
    case 8:
        converted = val_8;
        break;
    case 16:
        converted = val_16;
        break;
    case 32:
        converted = val_32;
        break;
    }

    atom = MakeAtom(name, strlen(name), TRUE);
    XIChangeDeviceProperty(dev, atom, type, format, PropModeReplace, nvalues,
                           converted, FALSE);
    XISetDevicePropertyDeletable(dev, atom, FALSE);
    return atom;
}

static Atom
InitAtom(DeviceIntPtr dev, char *name, int format, int nvalues, int *values)
{
    return InitTypedAtom(dev, name, XA_INTEGER, format, nvalues, values);
}

static Atom
InitFloatAtom(DeviceIntPtr dev, char *name, int nvalues, float *values)
{
    Atom atom;

    atom = MakeAtom(name, strlen(name), TRUE);
    XIChangeDeviceProperty(dev, atom, float_type, 32, PropModeReplace,
                           nvalues, values, FALSE);
    XISetDevicePropertyDeletable(dev, atom, FALSE);
    return atom;
}

void
InitDeviceProperties(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;
    SynapticsParameters *para = &priv->synpara;
    int values[9];              /* we never have more than 9 values in an atom */
    float fvalues[4];           /* never have more than 4 float values */

    float_type = XIGetKnownProperty(XATOM_FLOAT);
    if (!float_type) {
        float_type = MakeAtom(XATOM_FLOAT, strlen(XATOM_FLOAT), TRUE);
        if (!float_type) {
            xf86IDrvMsg(pInfo, X_ERROR, "Failed to init float atom. "
                        "Disabling property support.\n");
            return;
        }
    }

    values[0] = para->finger_low;
    values[1] = para->finger_high;
    values[2] = 0;

    prop_finger = InitAtom(pInfo->dev, SYNAPTICS_PROP_FINGER, 32, 3, values);
    prop_tap_time =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_TAP_TIME, 32, 1, &para->tap_time);
    prop_tap_move =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_TAP_MOVE, 32, 1, &para->tap_move);

    prop_clickpad =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_CLICKPAD, 8, 1, &para->clickpad);

    values[0] = para->scroll_dist_vert;
    values[1] = para->scroll_dist_horiz;
    prop_scrolldist =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_SCROLL_DISTANCE, 32, 2, values);

    values[0] = para->scroll_twofinger_vert;
    values[1] = para->scroll_twofinger_horiz;
    prop_scrolltwofinger =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_SCROLL_TWOFINGER, 8, 2, values);

    fvalues[0] = para->min_speed;
    fvalues[1] = para->max_speed;
    fvalues[2] = para->accl;
    fvalues[3] = 0;
    prop_speed = InitFloatAtom(pInfo->dev, SYNAPTICS_PROP_SPEED, 4, fvalues);

    prop_off =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_OFF, 8, 1, &para->touchpad_off);

    values[0] = para->press_motion_min_z;
    values[1] = para->press_motion_max_z;
    prop_pressuremotion =
        InitTypedAtom(pInfo->dev, SYNAPTICS_PROP_PRESSURE_MOTION, XA_CARDINAL,
                      32, 2, values);

    fvalues[0] = para->press_motion_min_factor;
    fvalues[1] = para->press_motion_max_factor;

    prop_pressuremotion_factor =
        InitFloatAtom(pInfo->dev, SYNAPTICS_PROP_PRESSURE_MOTION_FACTOR, 2,
                      fvalues);

    prop_grab =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_GRAB, 8, 1,
                 &para->grab_event_device);

    // TODO: size???
    values[0] = priv->has_left;
    values[1] = FALSE; // priv->has_middle;
    values[2] = FALSE; // priv->has_right;
    values[3] = FALSE; // priv->has_double;
    values[4] = FALSE; // priv->has_triple;
    values[5] = priv->has_pressure;
    values[6] = priv->has_width;
    prop_capabilities =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_CAPABILITIES, 8, 7, values);

    values[0] = para->resolution_vert;
    values[1] = para->resolution_horiz;
    prop_resolution =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_RESOLUTION, 32, 2, values);

    values[0] = para->hyst_x;
    values[1] = para->hyst_y;
    prop_noise_cancellation = InitAtom(pInfo->dev,
                                       SYNAPTICS_PROP_NOISE_CANCELLATION, 32, 2,
                                       values);

    values[0] = para->bottom_buttons_height;
	values[1] = para->bottom_buttons_sep_pos;
	values[2] = para->bottom_buttons_sep_width;
	prop_bottom_buttons = InitAtom(pInfo->dev,
                                       SYNAPTICS_PROP_BOTTOM_BUTTONS, 32, 3,
                                       values);

	values[0] = para->top_buttons_height;
	values[1] = para->top_buttons_middle_width;
	prop_top_buttons = InitAtom(pInfo->dev,
                                       SYNAPTICS_PROP_TOP_BUTTONS, 32, 2,
                                       values);

    prop_scroll_twofinger_finger_size =
        InitAtom(pInfo->dev, SYNAPTICS_PROP_SCROLL_TWOFINGER_FINGER_SIZE, 32, 1,
                 &para->scroll_twofinger_finger_size);

	values[0] = para->tap_pressure;
	values[1] = para->tap_anywhere;
	values[2] = para->tap_hold;
	prop_tap_extras = InitAtom(pInfo->dev,
                                       SYNAPTICS_PROP_TAP_EXTRAS, 32, 3,
                                       values);


    /* only init product_id property if we actually know them */
    if (priv->id_vendor || priv->id_product) {
        values[0] = priv->id_vendor;
        values[1] = priv->id_product;
        prop_product_id =
            InitAtom(pInfo->dev, XI_PROP_PRODUCT_ID, 32, 2, values);
    }

    if (priv->device) {
        prop_device_node =
            MakeAtom(XI_PROP_DEVICE_NODE, strlen(XI_PROP_DEVICE_NODE), TRUE);
        XIChangeDeviceProperty(pInfo->dev, prop_device_node, XA_STRING, 8,
                               PropModeReplace, strlen(priv->device),
                               (pointer) priv->device, FALSE);
        XISetDevicePropertyDeletable(pInfo->dev, prop_device_node, FALSE);
    }

}
void SetCoordsFromPercent(InputInfoPtr pInfo, int flag){
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;
    SynapticsParameters *pars = &priv->synpara;

    int width = abs(priv->maxx - priv->minx);
    int height = abs(priv->maxy - priv->miny);

	if(!flag || flag==1){
		pars->no_button_max_y=(100-pars->bottom_buttons_height)/100.0 * height + priv->miny;
		pars->bottom_left_btn_rx=(pars->bottom_buttons_sep_pos-pars->bottom_buttons_sep_width/2) / 100.0 * width + priv->minx;
		pars->bottom_right_btn_lx=(pars->bottom_buttons_sep_pos+pars->bottom_buttons_sep_width/2) / 100.0 * width + priv->minx;
	}
	if(!flag || flag==2){
		pars->no_button_min_y=pars->top_buttons_height/100.0 * height + priv->miny;
		pars->top_mid_lx=(50-pars->top_buttons_middle_width/2) / 100.0 * width + priv->minx;
		pars->top_mid_rx=(50+pars->top_buttons_middle_width/2) / 100.0 * width + priv->minx;
	}
	if(!flag || flag==3) pars->finger_radius=pars->scroll_twofinger_finger_size/100.0 * width;
}


int
SetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
            BOOL checkonly)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;
    SynapticsParameters *para = &priv->synpara;
    SynapticsParameters tmp;


    /* If checkonly is set, no parameters may be changed. So just let the code
     * change temporary variables and forget about it. */
    if (checkonly) {
        tmp = *para;
        para = &tmp;
    }

    if (property == prop_finger) {
        INT32 *finger;

        if (prop->size != 3 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        finger = (INT32 *) prop->data;
        if (finger[0] > finger[1])
            return BadValue;

        para->finger_low = finger[0];
        para->finger_high = finger[1];
    }

    else if (property == prop_tap_extras) {
        INT32 *tapextras;

        if (prop->size != 3 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        tapextras = (INT32 *) prop->data;

		if(tapextras[1]>2) return BadValue;

		para->tap_pressure = tapextras[0];
		para->tap_anywhere = tapextras[1];
		para->tap_hold = tapextras[2];

    }
    else if (property == prop_scroll_twofinger_finger_size) {

        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        para->scroll_twofinger_finger_size = *(INT32 *) prop->data;

		SetCoordsFromPercent(pInfo,3);
    }
    else if (property == prop_top_buttons) {
        INT32 *tbtns;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        tbtns = (INT32 *) prop->data;

		para->top_buttons_height = tbtns[0];
		para->top_buttons_middle_width = tbtns[1];

		SetCoordsFromPercent(pInfo,2);

    }
    else if (property == prop_bottom_buttons) {
        INT32 *bbtns;

        if (prop->size != 3 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        bbtns = (INT32 *) prop->data;

		para->bottom_buttons_height = bbtns[0];
		para->bottom_buttons_sep_pos = bbtns[1];
		para->bottom_buttons_sep_width = bbtns[2];

		SetCoordsFromPercent(pInfo,1);

    }
    else if (property == prop_tap_time) {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        para->tap_time = *(INT32 *) prop->data;

    }
    else if (property == prop_tap_move) {
        if (prop->size != 1 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        para->tap_move = *(INT32 *) prop->data;
    }
    else if (property == prop_clickpad) {
        BOOL value;

        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return BadMatch;

        value = *(BOOL *) prop->data;
        //~ if (!para->clickpad && value && !prop_softbutton_areas)
            //~ InitSoftButtonProperty(pInfo);
        //~ else if (para->clickpad && !value && prop_softbutton_areas) {
            //~ XIDeleteDeviceProperty(dev, prop_softbutton_areas, FALSE);
            //~ prop_softbutton_areas = 0;
        //~ }

        para->clickpad = *(BOOL *) prop->data;
    }
    else if (property == prop_scrolldist) {
        INT32 *dist;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        dist = (INT32 *) prop->data;
        if (dist[0] == 0 || dist[1] == 0)
            return BadValue;

        if (para->scroll_dist_vert != dist[0]) {
            para->scroll_dist_vert = dist[0];
            SetScrollValuator(dev, priv->scroll_axis_vert, SCROLL_TYPE_VERTICAL,
                              para->scroll_dist_vert, 0);
        }
        if (para->scroll_dist_horiz != dist[1]) {
            para->scroll_dist_horiz = dist[1];
            SetScrollValuator(dev, priv->scroll_axis_horiz,
                              SCROLL_TYPE_HORIZONTAL, para->scroll_dist_horiz,
                              0);
        }
    }
    else if (property == prop_scrolltwofinger) {
        CARD8 *twofinger;

        if (prop->size != 2 || prop->format != 8 || prop->type != XA_INTEGER)
            return BadMatch;

        twofinger = (BOOL *) prop->data;
        para->scroll_twofinger_vert = twofinger[0];
        para->scroll_twofinger_horiz = twofinger[1];
    }
    else if (property == prop_speed) {
        float *speed;

        if (prop->size != 4 || prop->format != 32 || prop->type != float_type)
            return BadMatch;

        speed = (float *) prop->data;
        para->min_speed = speed[0];
        para->max_speed = speed[1];
        para->accl = speed[2];
    }
    else if (property == prop_off) {
        CARD8 off;

        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return BadMatch;

        off = *(CARD8 *) prop->data;

        if (off > 2)
            return BadValue;

        para->touchpad_off = off;
    }
    else if (property == prop_pressuremotion) {
        CARD32 *press;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_CARDINAL)
            return BadMatch;

        press = (CARD32 *) prop->data;
        if (press[0] > press[1])
            return BadValue;

        para->press_motion_min_z = press[0];
        para->press_motion_max_z = press[1];
    }
    else if (property == prop_pressuremotion_factor) {
        float *press;

        if (prop->size != 2 || prop->format != 32 || prop->type != float_type)
            return BadMatch;

        press = (float *) prop->data;
        if (press[0] > press[1])
            return BadValue;

        para->press_motion_min_factor = press[0];
        para->press_motion_max_factor = press[1];
    }
    else if (property == prop_grab) {
        if (prop->size != 1 || prop->format != 8 || prop->type != XA_INTEGER)
            return BadMatch;

        para->grab_event_device = *(BOOL *) prop->data;
    }
    else if (property == prop_capabilities) {
        /* read-only */
        return BadValue;
    }
    else if (property == prop_resolution) {
        /* read-only */
        return BadValue;
    }
    else if (property == prop_noise_cancellation) {
        INT32 *hyst;

        if (prop->size != 2 || prop->format != 32 || prop->type != XA_INTEGER)
            return BadMatch;

        hyst = (INT32 *) prop->data;
        if (hyst[0] < 0 || hyst[1] < 0)
            return BadValue;
        para->hyst_x = hyst[0];
        para->hyst_y = hyst[1];
    }
    else if (property == prop_product_id || property == prop_device_node)
        return BadValue;        /* read-only */

    return Success;
}
