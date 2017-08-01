colord sensor driver for the hueyCOLOR
======================================

The hueyCOLOR is a *color sensor* found in the top-end P70 and P71 ThinkPads
from Lenovo. It is branded as a Pantone X-Rite sensor, and is similar in
protocol to the Huey and HueyPRO devices. The sensor is located in the palm-rest,
and so the laptop lid needs to be shut (and the display kept on) when showing
calibration patches.

The device has USB vendor ID of `0765` and a product ID of `5010` as is exposed
as a simple HID device with an 8-byte control transfer request then an 8-byte
interrupt transfer response.

Windows Behaviour
-----------------

In Microsoft Windows 10, the Pantone application prompts you to recalibrate your
display once per week. When you manually run the calibration wizard, it asks you
to choose your display temperature and also the gamma value for the curve,
defaulting to D65 whitepoint and 2.2 gamma.

It then asks you to shut the lid and uses a combination of flashing the Thinkpad
red-dot LED and using sound effects to show you the progress of the calibration.
By opening the lid a tiny fraction we can see the pattern is as follows:

 * Black offset
 * Red primary
 * Green primary
 * Blue primary
 * Red gamma ramp, 7 steps
 * Green gamma ramp, 7 steps
 * Blue gamma ramp, 7 steps

The USB traffic was intercepted for two runs, and these are dumped to the files
`huey-p70.csv` and `huey-p70-run2.csv`. These were further post-processed by the
`huey-p70.py` script to filter down and to help understand the protocol used.

Conclusions
-----------

From completely reverse engineering the protocol, we can show that the Pantone
X-Rite sensor in the palm-rest of the P70 is nothing more than a brightness
sensor with a display-specific primary correction matrix.

You can't actually get a RGB or XYZ color out of the sensor, the only useful
thing that it can do is linearize the VCGT ramps, and with only 7 measurements
for each channel I'm surprised it can do anything very useful at all.

Is it not known how the sensor and calibration tool can create an ICC profile
without hardcoding the primaries in the sensor EEPROM itself, and this is
probably what happens here. Whilst the sensor would be able to linearize a
display where the hardware-corrected backlight suddenly becomes non-linear,
it is completely unable to return a set of display primaries. Said another way,
the sensor can't tell the difference between a 100% red and 100% blue patch.

These findings also correlate with the findings from AnandTech[1] who say that
calibrating the display with the embedded sensor actually makes the LCD worse
when measuring saturation accuracy, whitepoint ***and*** grayscale accuracy.

If you're serious about color calibration, please don't use the built-in sensor,
and if you are buying a top-end Xeon system save a few dollars and don't bother
with the color sensor. For $20 extra Pantone could have used a calibrated XYZ
one-piece sensor from MAZeT, which would have given them a true "color sensor"
that could have sampled much quicker and with true XYZ-adapted readings.

The irony is of course, you can't even use the HueyCOLOR sensor as a ambient
light sensor. As the device is in the palm-rest, you frequently cover it with
your hand -- and any backlight adjustment would feed back into the sensor
causing the backlight to flash.

If you actually want to make a colord sensor driver for this hardware we'd need
to extend the capability bitfield to show it's only capable of brightness, and
also continue parsing the EEPROM to find the spectral sensitivities, but that's
not something that *I* think is useful to do.

Details about the hardware
==========================

EEPROM Map
----------

Unlike the well-documented EEPROM map for the Huey and HueyPro devices this
device has 1024 bytes of EEPROM, which looks to contain a serial number,
a CCMX-like structure and also the hardcoded display model. The majority of the
EEPROM is unused and set to `0x30`. The entire EEPROM is captured in the
`huey2.bin` file using the `huey-tool` command, as Windows 10 only queries
 about 80% of the address space, ignoring seemingly uninteresting chunks.

Command: Unlock
---------------

The device does not accept the Huey `UNLOCK` command and does not need manual
unlocking.

Command: GetStatus
------------------

The device always seems to respond with `"ECCM3 "`.

    input:   00 xx xx xx xx xx xx xx
    returns: 00 00 43 69 72 30 30 31
          status --^^^^^^^^^^^^^^^^^

Command: Read the EEPROM
-------------------------

The existing command `REGISTER_READ` now takes two bytes for the address and
returns 4 bytes of EEPROM on each request.

    input:   08 ff ff xx xx xx xx xx
                ^^-^^-- register address
    returns: 00 08 0b b8 01 02 03 04
         address --^^-^^ ^^-^^-^^-^^-- values

Command: Sample A Color
-----------------------

The Windows 10 application does not use the typical `MEASURE_RGB_ALT` and
`GET_GREEN` commands used in previous hardware revisions, but instead uses two
new commands `0x53` and `0x04`.

    input:   53 00 62 xx xx xx xx xx
                ^^-^^--- the amount of time to count
    returns: 00 53 00 00 01 02 xx xx
         value ----^^-^^-^^-^^

The value increases as the sample gets brighter, so we can guess this function
is counting pulses for a set amount of time.

    input:   04 00 70 xx xx xx xx xx
                ^^-^^--- the number of pulses to count
    returns: 00 04 00 00 01 02 xx xx
          value ---------^^-^^

The value increases as the sample gets darker, so we can guess this function
is counting a number of pulses.

[1] http://www.anandtech.com/show/10444/the-lenovo-thinkpad-p70-review-mobile-xeon-workstation/5
