/*
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
 */

#ifndef	_SYNAPTICSSTR_H_
#define _SYNAPTICSSTR_H_

#include "synproto.h"

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) < 18
#define LogMessageVerbSigSafe xf86MsgVerb
#endif

#if GET_ABI_MAJOR(ABI_XINPUT_VERSION) > 19
#define NO_DRIVER_SCALING 1
#elif GET_ABI_MAJOR(ABI_XINPUT_VERSION) == 19 && GET_ABI_MINOR(ABI_XINPUT_VERSION) >= 2
/* as of 19.2, the server takes device resolution into account when scaling
   relative events from abs device, so we must not scale in synaptics. */
#define NO_DRIVER_SCALING 1
#endif

#ifdef DBG
#undef DBG
#endif

#ifdef DEBUG
#define DBG(verb, ...) \
    xf86MsgVerb(X_INFO, verb, __VA_ARGS__)
#else
#define DBG(verb, msg, ...)     /* */
#endif

/******************************************************************************
 *		Definitions
 *					structs, typedefs, #defines, enums
 *****************************************************************************/
#define SYN_MAX_BUTTONS 12      /* Max number of mouse buttons */

#define MAX_TP 2 /* Max track points*/

static const int INT_SHIFT = sizeof(int)*CHAR_BIT-1;

enum OffState {
    TOUCHPAD_ON = 0,
    TOUCHPAD_OFF = 1,
    TOUCHPAD_TAP_OFF = 2,
};

typedef struct _SynapticsTouchAxis {
    const char *label;
    int min;
    int max;
    int res;
} SynapticsTouchAxisRec;


enum TouchpadModel {
    MODEL_UNKNOWN = 0,
    MODEL_SYNAPTICS,
    MODEL_ALPS,
    MODEL_APPLETOUCH,
    MODEL_ELANTECH,
    MODEL_UNIBODY_MACBOOK
};

enum TouchOrigin{
	TO_CLOSED=-2,
	TO_BTN_GAP=-1,
	TO_NO_CLICK=0,
	TO_LEFT_CLICK=1,
	TO_MIDDLE_CLICK=2,
	TO_RIGHT_CLICK=4
};

enum VertArea{
	VA_NO,		// not defined
	VA_TOP,		// top button area
	VA_MID,		// no click area
	VA_BOT		// buttom bnt area
};

enum TapState{
	TS_NONE,
	TS_WAIT,		// waiting for to fire click - timer is ON
	TS_THG_WAIT,		// button down fired - timer is ON to switch state to TS_THG
	TS_THG			// we are in THG mode - timer is OFF
};

struct ns_inf{ // perfinger tracking info
	int hist_x;
	int hist_y;
	int org_x;		// touch origin x
	int org_y;		// touch origin y
	CARD32 triple_click_timeout;
    int hyst_center_x;          /* center x of hysteresis */
    int hyst_center_y;          /* center y of hysteresis */
	enum TouchOrigin touch_origin;
	int vert_area;				// for scroll stuff;
	enum TapState tap_state;
	Bool tap_go;				// tap pressure breached
};

typedef struct _SynapticsParameters {
    int finger_low, finger_high; //, finger_press;  /* finger detection values in Z-values */
    int tap_time;
    int tap_move;               /* max. tapping time and movement in packets and coord. */
    Bool clickpad;              /* Device is a has integrated buttons */
    int scroll_dist_vert;       /* Scrolling distance in absolute coordinates */
    int scroll_dist_horiz;      /* Scrolling distance in absolute coordinates */
    Bool scroll_twofinger_vert; /* Enable/disable vertical two-finger scrolling */
    Bool scroll_twofinger_horiz;        /* Enable/disable horizontal two-finger scrolling */
    double min_speed, max_speed, accl;  /* movement parameters */
    int touchpad_off;           /* Switches the touchpad off
                                 * 0 : Not off
                                 * 1 : Off
                                 * 2 : Only tapping and scrolling off
                                 */
    int press_motion_min_z;     /* finger pressure at which minimum pressure motion factor is applied */
    int press_motion_max_z;     /* finger pressure at which maximum pressure motion factor is applied */
    double press_motion_min_factor;     /* factor applied on speed when finger pressure is at minimum */
    double press_motion_max_factor;     /* factor applied on speed when finger pressure is at maximum */
    Bool grab_event_device;     /* grab event device for exclusive use? */
    unsigned int resolution_horiz;      /* horizontal resolution of touchpad in units/mm */
    unsigned int resolution_vert;       /* vertical resolution of touchpad in units/mm */
    int hyst_x, hyst_y;         /* x and y width of hysteresis box */

    int bottom_buttons_height;				// default = 25%
	int bottom_buttons_sep_pos;				// default = 50%
	int bottom_buttons_sep_width;			// default = 2%
	// calculated
    int no_button_max_y;		// Bottom edge of no button area
    int bottom_left_btn_rx;		// right edge of bottom left button
    int bottom_right_btn_lx;	// left edge of bottom right button

	int top_buttons_height; 				// default = 15%
	int top_buttons_middle_width; 			// default = 16%
	// calculated
    int no_button_min_y;		// Bottom edge of no button area
	int top_mid_lx;				// left edge of top middle button
	int top_mid_rx;				// right edge of top middle button

	int scroll_twofinger_finger_size; 		//Finger Box Size, default = 18%
	// calculated
    int finger_radius;			// size of finger box for scrolling...

	int tap_pressure; 						// default = 50;
	int tap_anywhere;						// 0-disable, 1 - enable
	int tap_hold;							// Tap Hold Gesture - default timeOut=150 in ms/0-disable

} SynapticsParameters;

struct _SynapticsPrivateRec {
    SynapticsParameters synpara;        /* Default parameter settings, read from
                                           the X config file */
    struct SynapticsProtocolOperations *proto_ops;
    void *proto_data;           /* protocol-specific data */

    struct SynapticsHwState *hwState;

    const char *device;         /* device node */

    struct CommData comm;

    struct SynapticsHwState *local_hw_state;    /* used in place of local hw state variables */

    int hyst_center_x;          /* center x of hysteresis */
    int hyst_center_y;          /* center y of hysteresis */

    int scroll_delta_x;         /* accumulated horiz scroll delta */
	int scroll_delta_y;         /* accumulated vert scroll delta */

#ifndef NO_DRIVER_SCALING
    double horiz_coeff;         /* normalization factor for x coordintes */
    double vert_coeff;          /* normalization factor for y coordintes */
#endif

	int lastButtons;

    int minx, maxx, miny, maxy; /* min/max dimensions as detected */
    int minp, maxp, minw, maxw; /* min/max pressure and finger width as detected */
    int resx, resy;             /* resolution of coordinates as detected in units/mm */
    Bool has_left;              /* left button detected for this device */
    Bool has_pressure;          /* device reports pressure */
    Bool has_width;             /* device reports finger width */
    Bool has_semi_mt;           /* device is only semi-multitouch capable */

    enum TouchpadModel model;   /* The detected model */
    unsigned short id_vendor;   /* vendor id */
    unsigned short id_product;  /* product id */

    int scroll_axis_horiz;      /* Horizontal smooth-scrolling axis */
    int scroll_axis_vert;       /* Vertical smooth-scrolling axis */
    ValuatorMask *scroll_events_mask;   /* ValuatorMask for smooth-scrolling */

    Bool has_touch;             /* Device has multitouch capabilities */
    int max_touches;            /* Number of touches supported */
    int num_mt_axes;            /* Number of multitouch axes other than X, Y */
    SynapticsTouchAxisRec *touch_axes;  /* Touch axis information other than X, Y */

    int num_active_touches;     /* Number of active touches on device */

	struct ns_inf *ns_info;
	CARD32 btn_up_time; 				// when button was released;
	Bool go_scroll;

    OsTimerPtr timer;           /* for up/down-button repeat, tap processing, etc */
    CARD32 timer_time;
    int timer_click_mask;
    Bool timer_click_finish;

    int timer_delta_x;			// deltas while waiting to tap
    int timer_delta_y;

    int timer_y_scroll;			// cont y scroll

    CARD32 tap_start_time;		// let's call this tap_anywhere stabilizer timeout
};

#endif                          /* _SYNAPTICSSTR_H_ */
