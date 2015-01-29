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
 * Copyright © 1999 Henry Davies
 * Copyright © 2001 Stefan Gmeiner
 * Copyright © 2002 S. Lehner
 * Copyright © 2002 Peter Osterlund
 * Copyright © 2002 Linuxcare Inc. David Kennedy
 * Copyright © 2003 Hartwig Felger
 * Copyright © 2003 Jörg Bösner
 * Copyright © 2003 Fred Hucht
 * Copyright © 2004 Alexei Gilchrist
 * Copyright © 2004 Matthias Ihmig
 * Copyright © 2006 Stefan Bethge
 * Copyright © 2006 Christian Thaeter
 * Copyright © 2007 Joseph P. Skudlarek
 * Copyright © 2008 Fedor P. Goncharov
 * Copyright © 2008-2012 Red Hat, Inc.
 * Copyright © 2011 The Chromium OS Authors
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
 *      Joseph P. Skudlarek <Jskud@Jskud.com>
 *      Christian Thaeter <chth@gmx.net>
 *      Stefan Bethge <stefan.bethge@web.de>
 *      Matthias Ihmig <m.ihmig@gmx.net>
 *      Alexei Gilchrist <alexei@physics.uq.edu.au>
 *      Jörg Bösner <ich@joerg-boesner.de>
 *      Hartwig Felger <hgfelger@hgfelger.de>
 *      Peter Osterlund <petero2@telia.com>
 *      S. Lehner <sam_x@bluemail.ch>
 *      Stefan Gmeiner <riddlebox@freesurf.ch>
 *      Henry Davies <hdavies@ameritech.net> for the
 *      Linuxcare Inc. David Kennedy <dkennedy@linuxcare.com>
 *      Fred Hucht <fred@thp.Uni-Duisburg.de>
 *      Fedor P. Goncharov <fedgo@gorodok.net>
 *      Simon Thum <simon.thum@gmx.de>
 *
 * Trademarks are the property of their respective owners.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <xorg-server.h>
#include <unistd.h>
#include <misc.h>
#include <xf86.h>
#include <math.h>
#include <stdio.h>
#include <xf86_OSproc.h>
#include <xf86Xinput.h>
#include <exevents.h>

#include <X11/Xatom.h>
#include <X11/extensions/XI2.h>
#include <xserver-properties.h>
#include <ptrveloc.h>

#include "synapticsstr.h"
#include "synaptics-properties.h"

/*
 * We expect to be receiving a steady 80 packets/sec (which gives 40
 * reports/sec with more than one finger on the pad, as Advanced Gesture Mode
 * requires two PS/2 packets per report).  Instead of a random scattering of
 * magic 13 and 20ms numbers scattered throughout the driver, introduce
 * POLL_MS as 14ms, which is slightly less than 80Hz.  13ms is closer to
 * 80Hz, but if the kernel event reporting was even slightly delayed,
 * we would produce synthetic motion followed immediately by genuine
 * motion, so use 14.
 *
 * We use this to call back at a constant rate to at least produce the
 * illusion of smooth motion.  It works a lot better than you'd expect.
*/

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))
#define TIME_DIFF(a, b) ((int)((a)-(b)))

#define INPUT_BUFFER_SIZE 200

/*****************************************************************************
 * Forward declaration
 ****************************************************************************/
static int SynapticsPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags);
static void SynapticsUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags);
static Bool DeviceControl(DeviceIntPtr, int);
static void ReadInput(InputInfoPtr);
static void HandleState(InputInfoPtr, struct SynapticsHwState *);
static int ControlProc(InputInfoPtr, xDeviceCtl *);
static int SwitchMode(ClientPtr, DeviceIntPtr, int);
static int DeviceInit(DeviceIntPtr);
static int DeviceOn(DeviceIntPtr);
static int DeviceOff(DeviceIntPtr);
static int DeviceClose(DeviceIntPtr);
static Bool QueryHardware(InputInfoPtr);
static void ReadDevDimensions(InputInfoPtr);
#ifndef NO_DRIVER_SCALING
static void ScaleCoordinates(SynapticsPrivate * priv,
                             struct SynapticsHwState *hw);
static void CalculateScalingCoeffs(SynapticsPrivate * priv);
#endif
static void SanitizeDimensions(InputInfoPtr pInfo);

void InitDeviceProperties(InputInfoPtr pInfo);
void SetCoordsFromPercent(InputInfoPtr pInfo, int flag);

int SetProperty(DeviceIntPtr dev, Atom property, XIPropertyValuePtr prop,
                BOOL checkonly);


static CARD32 timerFunc(OsTimerPtr timer, CARD32 now, pointer arg);

const static struct {
    const char *name;
    struct SynapticsProtocolOperations *proto_ops;
} protocols[] = {
#ifdef BUILD_EVENTCOMM
    { "event", &event_proto_operations },
#endif
    { NULL, NULL }
};

InputDriverRec SYNAPTICS = {
    1,
    "synlx40",
    NULL,
    SynapticsPreInit,
    SynapticsUnInit,
    NULL,
    NULL,
#ifdef XI86_DRV_CAP_SERVER_FD
    XI86_DRV_CAP_SERVER_FD
#endif
};

static XF86ModuleVersionInfo VersionRec = {
    "synlx40",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
    ABI_CLASS_XINPUT,
    ABI_XINPUT_VERSION,
    MOD_CLASS_XINPUT,
    {0, 0, 0, 0}
};

static pointer
SetupProc(pointer module, pointer options, int *errmaj, int *errmin)
{
    xf86AddInputDriver(&SYNAPTICS, module, 0);
    return module;
}

_X_EXPORT XF86ModuleData synlx40ModuleData = {
    &VersionRec,
    &SetupProc,
    NULL
};

/*****************************************************************************
 *	Function Definitions
 ****************************************************************************/
static inline void
SynapticsCloseFd(InputInfoPtr pInfo)
{
    if (pInfo->fd > -1 && !(pInfo->flags & XI86_SERVER_FD)) {
        xf86CloseSerial(pInfo->fd);
        pInfo->fd = -1;
    }
}

/**
 * Fill in default dimensions for backends that cannot query the hardware.
* Eventually, we want the edges to be 1900/5400 for x, 1900/4000 for y.
 * These values are based so that calculate_edge_widths() will give us the
 * right values.
 *
 * The default values 1900, etc. come from the dawn of time, when men where
 * men, or possibly apes.
 */
static void
SanitizeDimensions(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;

    if (priv->minx >= priv->maxx) {
        priv->minx = 1615;
        priv->maxx = 5685;
        priv->resx = 0;

        xf86IDrvMsg(pInfo, X_PROBED,
                    "invalid x-axis range.  defaulting to %d - %d\n",
                    priv->minx, priv->maxx);
    }

    if (priv->miny >= priv->maxy) {
        priv->miny = 1729;
        priv->maxy = 4171;
        priv->resy = 0;

        xf86IDrvMsg(pInfo, X_PROBED,
                    "invalid y-axis range.  defaulting to %d - %d\n",
                    priv->miny, priv->maxy);
    }

    if (priv->minp >= priv->maxp) {
        priv->minp = 0;
        priv->maxp = 255;

        xf86IDrvMsg(pInfo, X_PROBED,
                    "invalid pressure range.  defaulting to %d - %d\n",
                    priv->minp, priv->maxp);
    }

    if (priv->minw >= priv->maxw) {
        priv->minw = 0;
        priv->maxw = 15;

        xf86IDrvMsg(pInfo, X_PROBED,
                    "invalid finger width range.  defaulting to %d - %d\n",
                    priv->minw, priv->maxw);
    }
}

static Bool
SetDeviceAndProtocol(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = pInfo->private;
    char *proto, *device;
    int i;

    proto = xf86SetStrOption(pInfo->options, "Protocol", NULL);
    device = xf86SetStrOption(pInfo->options, "Device", NULL);

    /* If proto is auto-dev, unset and let the code do the rest */
    if (proto && !strcmp(proto, "auto-dev")) {
        free(proto);
        proto = NULL;
    }

    for (i = 0; protocols[i].name; i++) {
        if ((!device || !proto) &&
            protocols[i].proto_ops->AutoDevProbe &&
            protocols[i].proto_ops->AutoDevProbe(pInfo, device))
            break;
        else if (proto && !strcmp(proto, protocols[i].name))
            break;
    }
    free(proto);
    free(device);

    priv->proto_ops = protocols[i].proto_ops;

    return (priv->proto_ops != NULL);
}

/* Area options support both percent values and absolute values. This is
 * awkward. The xf86Set* calls will print to the log, but they'll
 * also print an error if we request a percent value but only have an
 * int. So - check first for percent, then call xf86Set* again to get
 * the log message.
 */
static int
set_percent_option(pointer options, const char *optname,
                   const int range, const int offset, const int default_value)
{
    int result;
    double percent = xf86CheckPercentOption(options, optname, -1);

    if (percent >= 0.0) {
        percent = xf86SetPercentOption(options, optname, -1);
        result = percent / 100.0 * range + offset;
    } else
        result = xf86SetIntOption(options, optname, default_value);

    return result;
}

static void
set_default_parameters(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = pInfo->private;    /* read-only */
    pointer opts = pInfo->options;      /* read-only */
    SynapticsParameters *pars = &priv->synpara; /* modified */

    int horizScrollDelta, vertScrollDelta;      /* pixels */
    int tapMove;                /* pixels */
    double accelFactor;         /* 1/pixels */
    int fingerLow, fingerHigh;  /* pressure */
    int pressureMotionMinZ, pressureMotionMaxZ; /* pressure */
    Bool vertTwoFingerScroll, horizTwoFingerScroll;
    int horizResolution = 1;
    int vertResolution = 1;
    int width, height, diag, range;
    int horizHyst, vertHyst;
    int grab_event_device = 0;
    const char *source;

    /* The synaptics specs specify typical edge widths of 4% on x, and 5.4% on
     * y (page 7) [Synaptics TouchPad Interfacing Guide, 510-000080 - A
     * Second Edition, http://www.synaptics.com/support/dev_support.cfm, 8 Sep
     * 2008]. We use 7% for both instead for synaptics devices, and 15% for
     * ALPS models.
     * http://bugs.freedesktop.org/show_bug.cgi?id=21214
     *
     * If the range was autodetected, apply these edge widths to all four
     * sides.
     */

    width = abs(priv->maxx - priv->minx);
    height = abs(priv->maxy - priv->miny);
    diag = sqrt(width * width + height * height);

    /* Again, based on typical x/y range and defaults */
    horizScrollDelta = diag * .020;
    vertScrollDelta = diag * .020;
    tapMove = diag * .044;
    accelFactor = 200.0 / diag; /* trial-and-error */

    /* hysteresis, assume >= 0 is a detected value (e.g. evdev fuzz) */
    horizHyst = pars->hyst_x >= 0 ? pars->hyst_x : diag * 0.005;
    vertHyst = pars->hyst_y >= 0 ? pars->hyst_y : diag * 0.005;

    range = priv->maxp - priv->minp + 1;

    /* scaling based on defaults and a pressure of 256 */
    pressureMotionMinZ = priv->minp + range * (30.0 / 256);
//    pressureMotionMaxZ = priv->minp + range * (160.0 / 256);
    pressureMotionMaxZ = 90;

    range = priv->maxw - priv->minw + 1;

    /* Enable twofinger scroll if we can detect doubletap */
    vertTwoFingerScroll = FALSE;
    horizTwoFingerScroll = FALSE;

    /* Use resolution reported by hardware if available */
    if ((priv->resx > 0) && (priv->resy > 0)) {
        horizResolution = priv->resx;
        vertResolution = priv->resy;
    }

    /* set the parameters */
    pars->hyst_x =
        set_percent_option(opts, "HorizHysteresis", width, 0, horizHyst);
    pars->hyst_y =
        set_percent_option(opts, "VertHysteresis", height, 0, vertHyst);

    pars->finger_low = xf86SetIntOption(opts, "FingerLow", fingerLow);
    pars->finger_high = xf86SetIntOption(opts, "FingerHigh", fingerHigh);
    pars->tap_time = xf86SetIntOption(opts, "MaxTapTime", 180);
    pars->tap_move = xf86SetIntOption(opts, "MaxTapMove", tapMove);
    pars->clickpad = xf86SetBoolOption(opts, "ClickPad", pars->clickpad);       /* Probed */
    if (pars->clickpad)
    pars->scroll_dist_vert =
        xf86SetIntOption(opts, "VertScrollDelta", vertScrollDelta);
    pars->scroll_dist_horiz =
        xf86SetIntOption(opts, "HorizScrollDelta", horizScrollDelta);
    pars->scroll_twofinger_vert =
        xf86SetBoolOption(opts, "VertTwoFingerScroll", vertTwoFingerScroll);
    pars->scroll_twofinger_horiz =
        xf86SetBoolOption(opts, "HorizTwoFingerScroll", horizTwoFingerScroll);
    pars->touchpad_off = xf86SetIntOption(opts, "TouchpadOff", TOUCHPAD_ON);

    pars->press_motion_min_z =
        xf86SetIntOption(opts, "PressureMotionMinZ", pressureMotionMinZ);
    pars->press_motion_max_z =
        xf86SetIntOption(opts, "PressureMotionMaxZ", pressureMotionMaxZ);

    pars->min_speed = xf86SetRealOption(opts, "MinSpeed", 0.4);
    pars->max_speed = xf86SetRealOption(opts, "MaxSpeed", 0.7);
    pars->accl = xf86SetRealOption(opts, "AccelFactor", accelFactor);
    pars->press_motion_min_factor =
        xf86SetRealOption(opts, "PressureMotionMinFactor", 1.0);
    pars->press_motion_max_factor =
        xf86SetRealOption(opts, "PressureMotionMaxFactor", 10.0);

    /* Only grab the device by default if it's not coming from a config
       backend. This way we avoid the device being added twice and sending
       duplicate events.
      */
    source = xf86CheckStrOption(opts, "_source", NULL);
    if (source == NULL || strncmp(source, "server/", 7) != 0)
        grab_event_device = TRUE;
    pars->grab_event_device = xf86SetBoolOption(opts, "GrabEventDevice", grab_event_device);

    pars->resolution_horiz =
        xf86SetIntOption(opts, "HorizResolution", horizResolution);
    pars->resolution_vert =
        xf86SetIntOption(opts, "VertResolution", vertResolution);
    if (pars->resolution_horiz <= 0) {
        xf86IDrvMsg(pInfo, X_ERROR,
                    "Invalid X resolution, using 1 instead.\n");
        pars->resolution_horiz = 1;
    }
    if (pars->resolution_vert <= 0) {
        xf86IDrvMsg(pInfo, X_ERROR,
                    "Invalid Y resolution, using 1 instead.\n");
        pars->resolution_vert = 1;
    }

    pars->bottom_buttons_height=xf86SetIntOption(opts, "BottomButtonsHeight", 25);
	pars->bottom_buttons_sep_pos=xf86SetIntOption(opts, "BottomButtonsSepPos", 50);
	pars->bottom_buttons_sep_width=xf86SetIntOption(opts, "BottomButtonsSepWidth", 2);

	pars->top_buttons_height=xf86SetIntOption(opts, "TopButtonsHeight", 15);
	pars->top_buttons_middle_width=xf86SetIntOption(opts, "TopButtonsMiddleWidth", 16);

	pars->scroll_twofinger_finger_size=xf86SetIntOption(opts, "TwoFingerScrollFingerSize", 18);

	pars->tap_pressure = xf86SetIntOption(opts, "MinTapPressure", 50);
	pars->tap_anywhere = xf86SetIntOption(opts, "TapAnywhere", 0);
	pars->tap_hold = xf86SetIntOption(opts, "TapHoldGuesture", 160);

	SetCoordsFromPercent(pInfo,0);
}

static double
SynapticsAccelerationProfile(DeviceIntPtr dev,
                             DeviceVelocityPtr vel,
                             double velocity, double thr, double acc)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    SynapticsParameters *para = &priv->synpara;

    double accelfct;

    /*
     * synaptics accel was originally base on device coordinate based
     * velocity, which we recover this way so para->accl retains its scale.
     */
    velocity /= vel->const_acceleration;

    /* speed up linear with finger velocity */
    accelfct = velocity * para->accl;

    /* clip acceleration factor */
    if (accelfct > para->max_speed * acc)
        accelfct = para->max_speed * acc;
    else if (accelfct < para->min_speed)
        accelfct = para->min_speed;

    /* modify speed according to pressure */
    //~ if (priv->moving_state == MS_TOUCHPAD_RELATIVE) {
        //~ int minZ = para->press_motion_min_z;
        //~ int maxZ = para->press_motion_max_z;
        //~ double minFctr = para->press_motion_min_factor;
        //~ double maxFctr = para->press_motion_max_factor;
//~
        //~ if (priv->hwState->touches->z <= minZ) {
            //~ accelfct *= minFctr;
        //~ }
        //~ else if (priv->hwState->touches->z >= maxZ) {
            //~ accelfct *= maxFctr;
        //~ }
        //~ else {
            //~ accelfct *=
                //~ minFctr + (priv->hwState->touches->z - minZ) * (maxFctr -
                                                       //~ minFctr) / (maxZ - minZ);
        //~ }
    //~ }

    return accelfct;
}

static int
SynapticsPreInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
    SynapticsPrivate *priv;

    /* allocate memory for SynapticsPrivateRec */
    priv = calloc(1, sizeof(SynapticsPrivate));
    if (!priv)
        return BadAlloc;

    pInfo->type_name = XI_TOUCHPAD;
    pInfo->device_control = DeviceControl;
    pInfo->read_input = ReadInput;
    pInfo->control_proc = ControlProc;
    pInfo->switch_mode = SwitchMode;
    pInfo->private = priv;


    /* allocate now so we don't allocate in the signal handler */
    priv->timer = TimerSet(NULL, 0, 0, NULL, NULL);
    if (!priv->timer) {
        free(priv);
        return BadAlloc;
    }

	// alocate ns
	priv->ns_info=calloc(MAX_TP, sizeof(struct ns_inf));
    if (!priv->ns_info) {
        xf86IDrvMsg(pInfo, X_ERROR,
                    "Synaptics driver can't allocate memory\n");
        goto SetupProc_fail;
    }

    /* may change pInfo->options */
    if (!SetDeviceAndProtocol(pInfo)) {
        xf86IDrvMsg(pInfo, X_ERROR,
                    "Synaptics driver unable to detect protocol\n");
        goto SetupProc_fail;
    }

    priv->device = xf86FindOptionValue(pInfo->options, "Device");

    /* open the touchpad device */
    pInfo->fd = xf86OpenSerial(pInfo->options);
    if (pInfo->fd == -1) {
        xf86IDrvMsg(pInfo, X_ERROR, "Synaptics driver unable to open device\n");
        goto SetupProc_fail;
    }
    xf86ErrorFVerb(6, "port opened successfully\n");
    xf86IDrvMsg(pInfo, X_WARNING, "SynapticsPreInit fd: %d\n",pInfo->fd);

    /* initialize variables */
    priv->synpara.hyst_x = -1;
    priv->synpara.hyst_y = -1;

    priv->go_scroll=FALSE;
	priv->btn_up_time=0;
	priv->tap_start_time=0;

    /* read hardware dimensions */
    ReadDevDimensions(pInfo);

    set_default_parameters(pInfo);

    SynapticsParameters *pars = &priv->synpara;

#ifndef NO_DRIVER_SCALING
    CalculateScalingCoeffs(priv);
#endif

    priv->comm.buffer = XisbNew(pInfo->fd, INPUT_BUFFER_SIZE);

    if (!QueryHardware(pInfo)) {
        xf86IDrvMsg(pInfo, X_ERROR,
                    "Unable to query/initialize Synaptics hardware.\n");
        goto SetupProc_fail;
    }

    xf86ProcessCommonOptions(pInfo, pInfo->options);

    if (priv->comm.buffer) {
        XisbFree(priv->comm.buffer);
        priv->comm.buffer = NULL;
    }
    SynapticsCloseFd(pInfo);

    return Success;

 SetupProc_fail:
    SynapticsCloseFd(pInfo);

    if (priv->ns_info)
		free(priv->ns_info);

    if (priv->comm.buffer)
        XisbFree(priv->comm.buffer);
    free(priv->proto_data);
    free(priv->timer);
    free(priv);
    pInfo->private = NULL;
    return BadAlloc;
}

/*
 *  Uninitialize the device.
 */
static void
SynapticsUnInit(InputDriverPtr drv, InputInfoPtr pInfo, int flags)
{
    SynapticsPrivate *priv = ((SynapticsPrivate *) pInfo->private);

    if (priv && priv->timer)
        free(priv->timer);
	if (priv && priv->ns_info)
        free(priv->ns_info);
    if (priv && priv->proto_data)
        free(priv->proto_data);
    if (priv && priv->scroll_events_mask)
        valuator_mask_free(&priv->scroll_events_mask);
    free(pInfo->private);
    pInfo->private = NULL;
    xf86DeleteInput(pInfo, 0);

}

/*
 *  Alter the control parameters for the mouse. Note that all special
 *  protocol values are handled by dix.
 */
static void
SynapticsCtrl(DeviceIntPtr device, PtrCtrl * ctrl)
{
}

static int
DeviceControl(DeviceIntPtr dev, int mode)
{
    Bool RetValue;

    switch (mode) {
    case DEVICE_INIT:
        RetValue = DeviceInit(dev);
        break;
    case DEVICE_ON:
        RetValue = DeviceOn(dev);
        break;
    case DEVICE_OFF:
        RetValue = DeviceOff(dev);
        break;
    case DEVICE_CLOSE:
        RetValue = DeviceClose(dev);
        break;
    default:
        RetValue = BadValue;
    }

    xf86IDrvMsg(dev->public.devicePrivate, X_WARNING, "DeviceControl: rv: %d, mode: %d\n",RetValue,mode);

    return RetValue;
}

static int
DeviceOn(DeviceIntPtr dev)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);

    DBG(3, "Synaptics DeviceOn called\n");

    pInfo->fd = xf86OpenSerial(pInfo->options);
    if (pInfo->fd == -1) {
        xf86IDrvMsg(pInfo, X_WARNING, "cannot open input device\n");
        return !Success;
    }

    if (priv->proto_ops->DeviceOnHook &&
        !priv->proto_ops->DeviceOnHook(pInfo, &priv->synpara))
         goto error;

    priv->comm.buffer = XisbNew(pInfo->fd, INPUT_BUFFER_SIZE);
    if (!priv->comm.buffer)
        goto error;

    xf86FlushInput(pInfo->fd);

    /* reinit the pad */
    if (!QueryHardware(pInfo))
        goto error;

    xf86AddEnabledDevice(pInfo);
    dev->public.on = TRUE;

    return Success;

error:
    if (priv->comm.buffer) {
        XisbFree(priv->comm.buffer);
        priv->comm.buffer = NULL;
    }
    SynapticsCloseFd(pInfo);
    return !Success;
}

static void
SynapticsReset(SynapticsPrivate * priv)
{
    int i;

    SynapticsResetHwState(priv->hwState);
    SynapticsResetHwState(priv->local_hw_state);
    SynapticsResetHwState(priv->comm.hwState);

    priv->hyst_center_x = 0;
    priv->hyst_center_y = 0;
	priv->scroll_delta_x=0;
	priv->scroll_delta_y=0;

    priv->lastButtons = 0;
    priv->num_active_touches = 0;

    priv->go_scroll=FALSE;
	priv->btn_up_time=0;

    priv->timer_time=1;
    priv->timer_click_mask=0;
    priv->timer_click_finish=FALSE;

    priv->tap_start_time=0;

	memset(priv->ns_info, 0,MAX_TP*sizeof(struct ns_inf));

    for (i = 0; i < MAX_TP; i++)
		priv->ns_info[i].touch_origin=TO_CLOSED;
}

static int
DeviceOff(DeviceIntPtr dev)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    Bool rc = Success;

    xf86IDrvMsg(pInfo, X_WARNING, "DeviceOff called\n");

    DBG(3, "Synaptics DeviceOff called\n");

    if (pInfo->fd != -1) {
        TimerCancel(priv->timer);
        xf86RemoveEnabledDevice(pInfo);
        SynapticsReset(priv);

        if (priv->proto_ops->DeviceOffHook &&
            !priv->proto_ops->DeviceOffHook(pInfo))
            rc = !Success;
        if (priv->comm.buffer) {
            XisbFree(priv->comm.buffer);
            priv->comm.buffer = NULL;
        }
        SynapticsCloseFd(pInfo);
		xf86IDrvMsg(pInfo, X_WARNING, "DeviceOff fd!=-1\n");
    }
    dev->public.on = FALSE;
    xf86IDrvMsg(pInfo, X_WARNING, "DeviceOff: %d\n",rc);
    return rc;
}

static int
DeviceClose(DeviceIntPtr dev)
{
    Bool RetValue;
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;

    xf86IDrvMsg(pInfo, X_WARNING, "DeviceClose called\n");


    RetValue = DeviceOff(dev);
    TimerFree(priv->timer);
    priv->timer = NULL;
	free(priv->ns_info);
	priv->ns_info=NULL;

    free(priv->touch_axes);
    priv->touch_axes = NULL;

    SynapticsHwStateFree(&priv->hwState);
    SynapticsHwStateFree(&priv->local_hw_state);
    SynapticsHwStateFree(&priv->comm.hwState);

    xf86IDrvMsg(pInfo, X_WARNING, "DeviceClose: %d\n",RetValue);

    return RetValue;
}

static void
InitAxesLabels(Atom *labels, int nlabels, const SynapticsPrivate * priv)
{
    int i;

    memset(labels, 0, nlabels * sizeof(Atom));
    switch (nlabels) {
    default:
    case 4:
        labels[3] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_VSCROLL);
    case 3:
        labels[2] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_HSCROLL);
    case 2:
        labels[1] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_Y);
    case 1:
        labels[0] = XIGetKnownProperty(AXIS_LABEL_PROP_REL_X);
        break;
    }

    for (i = 0; i < priv->num_mt_axes; i++) {
        SynapticsTouchAxisRec *axis = &priv->touch_axes[i];
        int axnum = nlabels - priv->num_mt_axes + i;

        labels[axnum] = XIGetKnownProperty(axis->label);
    }
}

static void
InitButtonLabels(Atom *labels, int nlabels)
{
    memset(labels, 0, nlabels * sizeof(Atom));
    switch (nlabels) {
    default:
    case 7:
        labels[6] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_RIGHT);
    case 6:
        labels[5] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_HWHEEL_LEFT);
    case 5:
        labels[4] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_DOWN);
    case 4:
        labels[3] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_WHEEL_UP);
    case 3:
        labels[2] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_RIGHT);
    case 2:
        labels[1] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_MIDDLE);
    case 1:
        labels[0] = XIGetKnownProperty(BTN_LABEL_PROP_BTN_LEFT);
        break;
    }
}

static void
DeviceInitTouch(DeviceIntPtr dev, Atom *axes_labels)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    int i;

    if (priv->has_touch) {

        /* x/y + whatever other MT axes we found */
        if (!InitTouchClassDeviceStruct(dev, priv->max_touches,
                                        XIDependentTouch,
                                        2 + priv->num_mt_axes)) {
            xf86IDrvMsg(pInfo, X_ERROR,
                        "failed to initialize touch class device\n");
            priv->has_touch = 0;
            return;
        }

        for (i = 0; i < priv->num_mt_axes; i++) {
            SynapticsTouchAxisRec *axis = &priv->touch_axes[i];
            int axnum = 4 + i;  /* Skip x, y, and scroll axes */

            if (!xf86InitValuatorAxisStruct(dev, axnum, axes_labels[axnum],
                                            axis->min, axis->max, axis->res, 0,
                                            axis->res, Absolute)) {
                xf86IDrvMsg(pInfo, X_WARNING,
                            "failed to initialize axis %s, skipping\n",
                            axis->label);
                continue;
            }

            xf86InitValuatorDefaults(dev, axnum);
        }
    }
}

static int
DeviceInit(DeviceIntPtr dev)
{
    InputInfoPtr pInfo = dev->public.devicePrivate;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    Atom float_type, prop;
    float tmpf;
    unsigned char map[SYN_MAX_BUTTONS + 1];
    int i;
    int min, max;
    int num_axes = 2;
    Atom btn_labels[SYN_MAX_BUTTONS] = { 0 };
    Atom *axes_labels;
    DeviceVelocityPtr pVel;

    num_axes += 2;

    num_axes += priv->num_mt_axes;

    axes_labels = calloc(num_axes, sizeof(Atom));
    if (!axes_labels) {
        xf86IDrvMsg(pInfo, X_ERROR, "failed to allocate axis labels\n");
        return !Success;
    }

    InitAxesLabels(axes_labels, num_axes, priv);
    InitButtonLabels(btn_labels, SYN_MAX_BUTTONS);

    DBG(3, "Synaptics DeviceInit called\n");

    for (i = 0; i <= SYN_MAX_BUTTONS; i++)
        map[i] = i;

    dev->public.on = FALSE;

    InitPointerDeviceStruct((DevicePtr) dev, map,
                            SYN_MAX_BUTTONS,
                            btn_labels,
                            SynapticsCtrl,
                            GetMotionHistorySize(), num_axes, axes_labels);

    /*
     * setup dix acceleration to match legacy synaptics settings, and
     * etablish a device-specific profile to do stuff like pressure-related
     * acceleration.
     */
    if (NULL != (pVel = GetDevicePredictableAccelData(dev))) {
        SetDeviceSpecificAccelerationProfile(pVel,
                                             SynapticsAccelerationProfile);

        /* float property type */
        float_type = XIGetKnownProperty(XATOM_FLOAT);

        /* translate MinAcc to constant deceleration.
         * May be overridden in xf86InitValuatorDefaults */
        tmpf = 1.0 / priv->synpara.min_speed;

        xf86IDrvMsg(pInfo, X_CONFIG,
                    "(accel) MinSpeed is now constant deceleration " "%.1f\n",
                    tmpf);
        prop = XIGetKnownProperty(ACCEL_PROP_CONSTANT_DECELERATION);
        XIChangeDeviceProperty(dev, prop, float_type, 32,
                               PropModeReplace, 1, &tmpf, FALSE);

        /* adjust accordingly */
        priv->synpara.max_speed /= priv->synpara.min_speed;
        priv->synpara.min_speed = 1.0;

        /* synaptics seems to report 80 packet/s, but dix scales for
         * 100 packet/s by default. */
        pVel->corr_mul = 12.5f; /*1000[ms]/80[/s] = 12.5 */

        xf86IDrvMsg(pInfo, X_CONFIG, "(accel) MaxSpeed is now %.2f\n",
                    priv->synpara.max_speed);
        xf86IDrvMsg(pInfo, X_CONFIG, "(accel) AccelFactor is now %.3f\n",
                    priv->synpara.accl);

        prop = XIGetKnownProperty(ACCEL_PROP_PROFILE_NUMBER);
        i = AccelProfileDeviceSpecific;
        XIChangeDeviceProperty(dev, prop, XA_INTEGER, 32,
                               PropModeReplace, 1, &i, FALSE);
    }

    /* X valuator */
    if (priv->minx < priv->maxx) {
        min = priv->minx;
        max = priv->maxx;
    }
    else {
        min = 0;
        max = -1;
    }

    xf86InitValuatorAxisStruct(dev, 0, axes_labels[0], min, max,
			       priv->resx * 1000, 0, priv->resx * 1000,
			       Relative);
    xf86InitValuatorDefaults(dev, 0);

    /* Y valuator */
    if (priv->miny < priv->maxy) {
        min = priv->miny;
        max = priv->maxy;
    }
    else {
        min = 0;
        max = -1;
    }

    xf86InitValuatorAxisStruct(dev, 1, axes_labels[1], min, max,
			       priv->resy * 1000, 0, priv->resy * 1000,
			       Relative);
    xf86InitValuatorDefaults(dev, 1);


	// scroll valuators
    xf86InitValuatorAxisStruct(dev, 2, axes_labels[2], 0, -1, 0, 0, 0,
                               Relative);
    priv->scroll_axis_horiz = 2;
    xf86InitValuatorAxisStruct(dev, 3, axes_labels[3], 0, -1, 0, 0, 0,
                               Relative);
    priv->scroll_axis_vert = 3;
    priv->scroll_events_mask = valuator_mask_new(MAX_VALUATORS);
    if (!priv->scroll_events_mask) {
        free(axes_labels);
        return !Success;
    }

    SetScrollValuator(dev, priv->scroll_axis_horiz, SCROLL_TYPE_HORIZONTAL,
                      priv->synpara.scroll_dist_horiz, 0);
    SetScrollValuator(dev, priv->scroll_axis_vert, SCROLL_TYPE_VERTICAL,
                      priv->synpara.scroll_dist_vert, 0);

    DeviceInitTouch(dev, axes_labels);

    free(axes_labels);

    priv->hwState = SynapticsHwStateAlloc(priv);
    if (!priv->hwState)
        goto fail;

    priv->local_hw_state = SynapticsHwStateAlloc(priv);
    if (!priv->local_hw_state)
        goto fail;

    priv->comm.hwState = SynapticsHwStateAlloc(priv);

    InitDeviceProperties(pInfo);

    XIRegisterPropertyHandler(pInfo->dev, SetProperty, NULL, NULL);

    SynapticsReset(priv);

    return Success;

 fail:
    free(priv->local_hw_state);
    free(priv->hwState);
    return !Success;
}


static enum TouchOrigin
current_button_area_new(SynapticsParameters *para, int x, int y, struct ns_inf *pti)
{
		if(y<para->no_button_min_y){
			pti->vert_area=VA_TOP;
			if(x<para->top_mid_lx) return TO_LEFT_CLICK;
			else if(x>para->top_mid_rx) return TO_RIGHT_CLICK;
			else return TO_MIDDLE_CLICK;
		}else if (y>para->no_button_max_y){
			pti->vert_area=VA_BOT;
			if(x<para->bottom_left_btn_rx) return TO_LEFT_CLICK;
			else if(x>para->bottom_right_btn_lx) return TO_RIGHT_CLICK;
			else return TO_BTN_GAP;
		}else{
			pti->vert_area=VA_MID;
			return TO_NO_CLICK;
		}
}
static void inline
timerClick(InputInfoPtr pInfo, struct ns_inf *pti){
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    SynapticsParameters *para = &priv->synpara;

	if(priv->timer_click_finish){
		xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, FALSE, 0, 0);
		priv->timer_click_finish=FALSE;
		priv->timer_click_mask=0;
	}else if(priv->timer_delta_x<para->tap_move && priv->timer_delta_y<para->tap_move){
		xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, TRUE, 0, 0);
		pti->tap_state=TS_NONE;
		// 5 mills tap length
		priv->timer = TimerSet(priv->timer, 0, 5, timerFunc, pInfo);
		priv->timer_click_finish=TRUE;
		priv->timer_time=0;
	}

}

static void
post_scroll_events(const InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);

    valuator_mask_zero(priv->scroll_events_mask);

    if (priv->scroll_delta_y != 0) {
        valuator_mask_set_double(priv->scroll_events_mask,
                                 priv->scroll_axis_vert, priv->scroll_delta_y);
    }
    if (priv->scroll_delta_x != 0) {
        valuator_mask_set_double(priv->scroll_events_mask,
                                 priv->scroll_axis_horiz, priv->scroll_delta_x);
    }
    if (valuator_mask_num_valuators(priv->scroll_events_mask))
		xf86PostMotionEventM(pInfo->dev, FALSE, priv->scroll_events_mask);
}

static CARD32
timerFunc(OsTimerPtr timer, CARD32 now, pointer arg)
{
    InputInfoPtr pInfo = arg;
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    SynapticsParameters *para = &priv->synpara;
    struct ns_inf *pti = priv->ns_info;
    int sigstate;
	int i=0;

    sigstate = xf86BlockSIGIO();


	// finish click if needed;
	if(priv->timer_click_finish){
		timerClick(pInfo,NULL);
		i=1; //<-- so we don't go into THG loop
	};

	if(priv->timer_y_scroll){
		// hand cont. scroll

		if(priv->timer_y_scroll>0) priv->timer_y_scroll-=50;
		else priv->timer_y_scroll+=50;

		if(abs(priv->timer_y_scroll)>50){

			priv->scroll_delta_y=priv->timer_y_scroll;
			post_scroll_events(pInfo);
			priv->timer = TimerSet(priv->timer, 0, 64, timerFunc, pInfo);

		} else priv->timer_y_scroll=0;

		priv->scroll_delta_y=0;
		priv->timer_time=0;

	}else if(!i){
		// handle THG

		for(i=0;i<MAX_TP;i++){
			if(pti->tap_state==TS_WAIT){
				timerClick(pInfo,pti);
			}else if(pti->tap_state==TS_THG_WAIT){
				pti->tap_state=TS_THG; // we are now in THG mode
			}
			pti++;
		}

		priv->timer_time=0;

	}

    xf86UnblockSIGIO(sigstate);

    return 0;

}


static Bool
SynapticsGetHwState(InputInfoPtr pInfo, SynapticsPrivate * priv,
                    struct SynapticsHwState *hw)
{
    return priv->proto_ops->ReadHwState(pInfo, &priv->comm, hw);
}

/*
 *  called for each full received packet from the touchpad
 */
static void
ReadInput(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    struct SynapticsHwState *hw = priv->local_hw_state;
    int delay = 0;
    Bool newDelay = FALSE;

    SynapticsResetTouchHwState(hw, FALSE);

    while (SynapticsGetHwState(pInfo, priv, hw)) {

        SynapticsCopyHwState(priv->hwState, hw);

        HandleState(pInfo, hw);

		//~ xf86IDrvMsg(pInfo, X_INFO,"closed slot: %d\n",i);

        if(priv->timer_time){
			if(priv->timer_time>1) priv->timer = TimerSet(priv->timer, 0, priv->timer_time, timerFunc, pInfo);
			else TimerCancel(priv->timer);
			priv->timer_time=0;
		}
	}
}


/**
 * Applies hysteresis. center is shifted such that it is in range with
 * in by the margin again. The new center is returned.
 * @param in the current value
 * @param center the current center
 * @param margin the margin to center in which no change is applied
 * @return the new center (which might coincide with the previous)
 */
static int
hysteresis(int in, int center, int margin)
{
    int diff = in - center;

    if (abs(diff) <= margin) {
        diff = 0;
    }
    else if (diff > margin) {
        diff -= margin;
    }
    else if (diff < -margin) {
        diff += margin;
    }
    return center + diff;
}

static void
post_button_click(const InputInfoPtr pInfo, const int button)
{
    xf86PostButtonEvent(pInfo->dev, FALSE, button, TRUE, 0, 0);
    xf86PostButtonEvent(pInfo->dev, FALSE, button, FALSE, 0, 0);
}

static void
filter_jitter(SynapticsPrivate * priv, int *x, int *y, struct ns_inf *pti)
{
    SynapticsParameters *para = &priv->synpara;

    pti->hyst_center_x = hysteresis(*x, pti->hyst_center_x, para->hyst_x);
    pti->hyst_center_y = hysteresis(*y, pti->hyst_center_y, para->hyst_y);
    *x = pti->hyst_center_x;
    *y = pti->hyst_center_y;
}

/*
 * React on changes in the hardware state. This function is called every time
 * the hardware state changes.
 */
static void
HandleState(InputInfoPtr pInfo, struct SynapticsHwState *hw)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) (pInfo->private);
    SynapticsParameters *para = &priv->synpara;
    struct TouchData *hwt = hw->touches;
    struct ns_inf *pti = priv->ns_info;
    int dx = 0, dy = 0, buttons=0,id;
    int change;

	int temp=0;
	int new_two_down=0;

	int i;
	int x,y;
	enum TouchOrigin cba;
	int potential_click=0;

	priv->scroll_delta_y=0;
	priv->scroll_delta_x=0;

	// syndaemon
	if(para->touchpad_off==TOUCHPAD_OFF) return;

	for (i = 0; i < MAX_TP; i++) {

		hwt+=i;
		pti+=i;

		// slot is empty
		if(!hwt->slot_state) continue;

        if (hwt->slot_state == SLOTSTATE_CLOSE){

			// tap_anywhere option
			if(!pti->touch_origin) pti->touch_origin+=para->tap_anywhere;

			// handle tap
			if(para->touchpad_off!=TOUCHPAD_TAP_OFF && pti->tap_go &&
				(pti->tap_state==TS_THG || // <-- we are in THG mode
				pti->touch_origin>TO_NO_CLICK && // <-- first tap or second with timer ON
				(hw->ev_time - hwt->millis) < para->tap_time &&
				hw->ev_time > priv->btn_up_time && // <-- button click delay
				abs(pti->org_x-pti->hist_x)<para->tap_move &&
				abs(pti->org_y-pti->hist_y)<para->tap_move)){

				switch(pti->tap_state){
					case TS_NONE: // first tap release
						if(!para->tap_hold){
							post_button_click(pInfo, ffs(pti->touch_origin));
							break;
						}
						if(pti->triple_click_timeout>hw->ev_time)
							timerClick(pInfo,pti);
						else{
							priv->timer_click_mask=ffs(pti->touch_origin);
							priv->timer_time=para->tap_hold; // <-Start THG timer
							pti->tap_state=TS_WAIT;
							priv->timer_delta_x=0;
							priv->timer_delta_y=0;
						}
						pti->triple_click_timeout=0;
						break;
					case TS_WAIT: // <-- this gets switched by the timer(TS_NONE + clicked) or if we touch down(TS_THG_WAIT/TS_NONE)
					case TS_THG_WAIT: // released before timers switched into THG
						// need to double(posibly triple) tap here and
						pti->triple_click_timeout=hw->ev_time+para->tap_time;
						// do fist tap
						xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, TRUE, 0, 0);
						xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, FALSE, 0, 0);
						// start next tap
						xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, TRUE, 0, 0);
					case TS_THG: // released after THG or coming from TS_THG_WAIT
						xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, FALSE, 0, 0);
						if(!pti->triple_click_timeout) priv->timer_click_mask=0;
						priv->timer_time=1; // to be reset
						pti->tap_state=TS_NONE;
						//~ xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, TRUE, 0, 0);
						break;
				}
			}

			// clean up

			hwt->millis=0;
			pti->touch_origin=TO_CLOSED;
			pti->vert_area=0;
			pti->hist_x=0;
			pti->hist_y=0;
			pti->tap_go=FALSE;
			priv->go_scroll=FALSE;

			// clear tap_start_time / cont. scroll ?
			if(!priv->num_active_touches){
				if(para->tap_anywhere) priv->tap_start_time=0;
				if(priv->timer_y_scroll) priv->timer_time=20;
			}

			//~ xf86IDrvMsg(pInfo, X_INFO,"closed slot: %d\n",i);

			continue;
		}

		x=hwt->x;
		y=hwt->y;

		// if we don't know X & Y than assume potential left button and go to next finger
		if(!x || !y){
			potential_click|=1;
			continue;
		}

		// low pressure
		if(hwt->z < para->finger_low) continue;

		// At this point X, Y and Z are good

		filter_jitter(priv, &x, &y, pti);
		cba=current_button_area_new(para,x,y,pti);

		//set touch origin, history and new_two_down if new touch
		if(pti->touch_origin<TO_BTN_GAP){
			pti->touch_origin=cba;

			// set history so first deltas are 0's
			pti->hist_x=x;
			pti->hist_y=y;

			// if two down check for scroll later
			new_two_down=priv->num_active_touches;
		}

		//tap pressure reached handle THG mode
		if(!pti->tap_go && !priv->go_scroll && hwt->z > para->tap_pressure){
			pti->tap_go=TRUE;

			// move delay if tap_anywhere is enabled
			if(para->tap_anywhere && !priv->tap_start_time) priv->tap_start_time=hw->ev_time+120;

			// turn off cont. scroll if any
			if(priv->timer_y_scroll){
				priv->timer_y_scroll=0;
				priv->timer_time=1;
			}

			// set tap origin coords
			pti->org_x=x;
			pti->org_y=y;

			// THG stuff
			if(pti->tap_state==TS_WAIT){

				int tto;
				//~ =!pti->touch_origin?pti->touch_origin+para->tap_anywhere:pti->touch_origin;

				// tap_anywhere THG move restrict
				if(para->tap_anywhere && !pti->touch_origin &&
					priv->timer_delta_x<para->tap_move && priv->timer_delta_y<para->tap_move){
					tto=1;
				}else tto=pti->touch_origin;


				if(ffs(tto)==priv->timer_click_mask){ // <-- tap origin is the same
					xf86PostButtonEvent(pInfo->dev, FALSE, priv->timer_click_mask, TRUE, 0, 0);
					pti->tap_state=TS_THG_WAIT;
					priv->timer_time=para->tap_time; // set timer to change to TS_THG if tap time expires

				}else{ // <-- tap origing diferent
					timerClick(pInfo, pti);
					// set timer to reset
					// priv->timer_time=1;
				}
			}
		}

		//scroll delta
		priv->scroll_delta_x+=(x-pti->hist_x);
		priv->scroll_delta_y+=(y-pti->hist_y);

		// is move allowed
		if(!pti->touch_origin || (!cba && priv->num_active_touches<2)){
			// move deltas
			dx+=(x-pti->hist_x);
			dy+=(y-pti->hist_y);
		}else if(cba>0){
			// set potential_click if we are in a button area
			potential_click|=cba;
		}

		// set history
		pti->hist_x=x;
		pti->hist_y=y;
	}

	if(hw->left){
		// handle clicks ----

		buttons=priv->lastButtons;

		// ziro potential_click if priv->lastButtons
		potential_click&=-(priv->lastButtons==0);

		// buttons = potential_click if lastButtons==0
		buttons|=potential_click;

	}else if(priv->go_scroll ||
		(new_two_down==2 &&
		(priv->ns_info[0].vert_area==priv->ns_info[1].vert_area ||
		(abs(priv->ns_info[0].hist_x-priv->ns_info[1].hist_x)<para->finger_radius &&
		abs(priv->ns_info[0].hist_y-priv->ns_info[1].hist_y)<para->finger_radius)))){
		/* Handle Scroll IF
		* - already in the scroll mode
		* - new two finger touch and
		* - both fingers are in the same area or fingers are close together
		* */

		// if syndaemon allows scroll than scroll
		if(para->touchpad_off!=TOUCHPAD_TAP_OFF){
			int abs_sdy=abs(priv->scroll_delta_y);

			// scroll either y OR x
			temp=(abs_sdy-abs(priv->scroll_delta_x))>>INT_SHIFT;
			priv->scroll_delta_x&=temp;
			priv->scroll_delta_y&=~temp;


			//apply scroll Endable/Disable settings
			if(!para->scroll_twofinger_vert) priv->scroll_delta_y=0;
			if(!para->scroll_twofinger_horiz) priv->scroll_delta_x=0;

			post_scroll_events(pInfo);

			// cont. scroll if sdy>150
			priv->timer_y_scroll=priv->scroll_delta_y&((150-abs_sdy)>>INT_SHIFT);
		}

		priv->go_scroll=TRUE;

		// no move if scroll or attempt to scroll when TOUCHPAD_TAP_OFF
		temp=1;
	}


	// reuse x and y vars
	x=abs(dx);
	y=abs(dy);

	// no move if to much delta
	temp|=(x>400)|(y>400);
	// no motion if button was just clicked
	temp|=(hw->ev_time<priv->btn_up_time);

	// deal with tap_anywhere
	if(para->tap_anywhere && priv->tap_start_time>1){
		// stabilize tap - cancel move only if deltas are small and within the delay time (120ms)
		if (x>para->hyst_x || y>para->hyst_y) priv->tap_start_time=1;
		else if(hw->ev_time<priv->tap_start_time) temp=1;
		else priv->tap_start_time=1;
	}
	// post motion ----------
    if ((dx || dy) && !temp){

		// TGH stuff
		if((priv->ns_info[0].tap_state|priv->ns_info[1].tap_state)==1){
			priv->timer_delta_x+=dx;
			priv->timer_delta_y+=dy;
		}

		xf86PostMotionEvent(pInfo->dev, 0, 0, 2, dx, dy);
	}

	// post clicks ----------
    change = buttons ^ priv->lastButtons;
    while (change) {
        id = ffs(change);       /* number of first set bit 1..32 is returned */
        change &= ~(1 << (id - 1));
        xf86PostButtonEvent(pInfo->dev, FALSE, id, (buttons & (1 << (id - 1))), 0, 0);
    }

    // Save old buttons
    priv->lastButtons = buttons;
}

static int
ControlProc(InputInfoPtr pInfo, xDeviceCtl * control)
{
    DBG(3, "Control Proc called\n");
    return Success;
}

static int
SwitchMode(ClientPtr client, DeviceIntPtr dev, int mode)
{
    DBG(3, "SwitchMode called\n");

    return XI_BadMode;
}

static void
ReadDevDimensions(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;

    if (priv->proto_ops->ReadDevDimensions)
        priv->proto_ops->ReadDevDimensions(pInfo);

    SanitizeDimensions(pInfo);
}

static Bool
QueryHardware(InputInfoPtr pInfo)
{
    SynapticsPrivate *priv = (SynapticsPrivate *) pInfo->private;

    priv->comm.protoBufTail = 0;

    if (!priv->proto_ops->QueryHardware(pInfo)) {
        xf86IDrvMsg(pInfo, X_PROBED, "no supported touchpad found\n");
        if (priv->proto_ops->DeviceOffHook)
            priv->proto_ops->DeviceOffHook(pInfo);
        return FALSE;
    }

    return TRUE;
}

#ifndef NO_DRIVER_SCALING
static void
ScaleCoordinates(SynapticsPrivate * priv, struct SynapticsHwState *hw)
{
    int xCenter = (priv->synpara.left_edge + priv->synpara.right_edge) / 2;
    int yCenter = (priv->synpara.top_edge + priv->synpara.bottom_edge) / 2;

    //~ hw->x = (hw->x - xCenter) * priv->horiz_coeff + xCenter;
    //~ hw->y = (hw->y - yCenter) * priv->vert_coeff + yCenter;
}

void
CalculateScalingCoeffs(SynapticsPrivate * priv)
{
    int vertRes = priv->synpara.resolution_vert;
    int horizRes = priv->synpara.resolution_horiz;

    if ((horizRes > vertRes) && (horizRes > 0)) {
        priv->horiz_coeff = vertRes / (double) horizRes;
        priv->vert_coeff = 1;
    }
    else if ((horizRes < vertRes) && (vertRes > 0)) {
        priv->horiz_coeff = 1;
        priv->vert_coeff = horizRes / (double) vertRes;
    }
    else {
        priv->horiz_coeff = 1;
        priv->vert_coeff = 1;
    }
}
#endif
