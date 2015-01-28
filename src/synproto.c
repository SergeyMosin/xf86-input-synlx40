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
 * Copyright © 2012 Canonical, Ltd.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "synproto.h"
#include "synapticsstr.h"

struct SynapticsHwState *
SynapticsHwStateAlloc(SynapticsPrivate * priv)
{
    struct SynapticsHwState *hw;

    hw = calloc(1, sizeof(struct SynapticsHwState));
    if (!hw)
        return NULL;

	hw->touches = calloc(MAX_TP, sizeof(struct TouchData));
	if(!hw->touches){
		free(hw);
		return NULL;
	}

    return hw;
}

void
SynapticsHwStateFree(struct SynapticsHwState **hw)
{
    if (!*hw)
        return;

	free((*hw)->touches);
    free(*hw);
    *hw = NULL;
}

void
SynapticsCopyHwState(struct SynapticsHwState *dst,
                     const struct SynapticsHwState *src)
{
	dst->left=src->left;
	dst->ev_time=src->ev_time;
    memcpy(dst->touches, src->touches, MAX_TP * sizeof(struct TouchData));

}

void
SynapticsResetHwState(struct SynapticsHwState *hw)
{
    struct TouchData *hwt = hw->touches;
    int i;
	hw->left=FALSE;
	hw->ev_time=0;
	for(i=0;i<MAX_TP;i++){
		hwt->slot_state=SLOTSTATE_EMPTY;
		hwt->x=0;
		hwt->y=0;
		hwt->z=0;
		hwt->millis=0;
		hwt++;
	}
}

void
SynapticsResetTouchHwState(struct SynapticsHwState *hw, Bool set_slot_empty)
{

	struct TouchData *hwt = hw->touches;
	int i;
    for (i = 0; i < MAX_TP; i++) {
        switch (hwt->slot_state){
        case SLOTSTATE_OPEN:
        case SLOTSTATE_OPEN_EMPTY:
        case SLOTSTATE_UPDATE:
            hwt->slot_state = set_slot_empty ? SLOTSTATE_EMPTY : SLOTSTATE_OPEN_EMPTY;
            break;
        default:
            hwt->slot_state = SLOTSTATE_EMPTY;
            break;
        }
        hwt++;
    }
}
