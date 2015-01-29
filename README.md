# xf86-input-synlx40

*Clickpad driver for Lenovo XX40(T540/T440/X240/E440 etc) series laptops,
it is based on xf86-input-synaptics 1.8.1-1 driver. Some features have
been added and some have been discarded.*

## Features ##
* Multitouch Support
* Synclient & Syndaemon Compatible
* Two Finger Scroll
* Adjustable Button Layout
* Tap and Hold Gesture

## Configuration Options ##
```
Run 'synclient -l' for the list of all available options
```

### Buttons Layout ###
*All values are in percent. (See clickpad.jpg)*

<img src="/clickpad.jpg">

* **BottomButtonsHeight**  - The height of bottom buttons. 0=NO bottom buttons.
* **BottomButtonsSepPos**  - Bottom buttons separator position, from the
left edge to the center of the separator.
* **BottomButtonsSepWidth**  - Bottom buttons separator width, 0=NO separator.
```
To make the bottom area into a ginormous right button one could do:
Option "BottomButtonsSepPos" "0"
Option "BottomButtonsSepWidth" "0"
```

* **TopButtonsHeight**  - The height of top buttons.
* **TopButtonsMiddleWidth**  - The width of the middle button.

*Default values for the top buttons correspond closely with the markings
on the clickpad*

### Misc. Settings ###

* **TapAnywhere**  - Disables/Enables taps(left button) in the "Move/Scroll" area.
*Note: Clicks are NOT allowed in this area.*
* **TapHoldGesture**  - Tap and Hold Gesture timeout, 0 to disable.
* **TwoFingerScrollFingerSize**  - Mostly internal setting for when scroll
originates in a button area, but if you have unusually large/small fingers try
adjusting this.
* **MinTapPressure**  - How hard to hit the click pad for a tap to register.
```
evtest /dev/input/event[DEVICE_NUMBER]|grep ABS_MT_PRESSURE
To see what your clickpad reports.
```

### Other Settings ###
All other available setting have been keep from the original synaptics
driver **google 'man synaptics'** for info.

**Pressure Motion** related options will have no effect as of now. I kept them
in there because I like the idea and once I have sometime I'll add it.


## Why was this made ?##

Because I could not stand the new clickpad that Lenovo put into an otherwise good laptop.
