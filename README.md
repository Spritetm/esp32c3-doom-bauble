Doom on an ESP32-C3 based Christmas tree bauble
===============================================

This uses the GBA Doom from https://github.com/doomhack/GBADoom to put Doom on a tiny Christmas
bauble. More information is on https://spritesmods.com/?art=doom-bauble .

Directories
-----------

- case: contains OpenSCAD case designs. 
- esp32c3-doom: Contains the firmware. This needs ESP-IDF 5.0 or higher to compile.
- pcb - KiCad PCB designs

Case
----

You should be able to print the case on a FDM ('hot plastic squirter'-type) machine, 
but it'll look better when printed on a SLA/LCD/DLP ('ultra-violet goo hardener'-type)
printer. I masked it with masking tape afterwards and painted it with model paint
(specifically Tamiya TS-81 and TS-06 rattle-can paint).

PCB
---
The PCB designs and schematics are in Kicad format. Look at the schematics for hints 
for components not on the BOM. Note that the 'headers' on the PCB are supposed to have
wires directly soldered to it; space doesn't really allow for actual headers.

Firmware
--------

To build this, you need esp-idf 5.0 or higher. (I used git commit 4c1ff6016a, for reference.) You
also need to make sure you have 8MiB or more flash on your ESP32C3. You can use the standard 
esp-idf building & flashing procedure ('idf.py flash'). Note that you also need to flash the Doom
WAD to the flash. The flash-doom-wad.sh shellscript gives an example on how to do that with the
included doom1gba.wad file; using this will give you the shareware version of Doom (which includes
all the levels of Episode 1).

It is possible to use a different Doom WAD as a basis for this, but you need to manually convert 
it first. Tools and information for that is in the esp32c3-doom/GbaWadUtil. Most importantly, you
probably want to convert any music to IMF format and re-record demos before running GbaWadUtil
to generate the final WAD to be flashed.

Note that this Doom version is not compatible with demos that are included in existing WAD files; 
while they will play, they will likely desync early on. As such, you may want to re-record the
demo files. There's a rudimentary setup for this in esp32c3-doom/main/sdl which can be used to
compile this version of Doom for SDL input and output. After that, you can edit d_main.c and
recompile to enable demo recording of one map.

Note on input devices
---------------------

The ESP32C3 will connect to any BT-LE device that supports HID and is set to pairing mode. 
BT Classic devices aren't supported nor detected. Do note that connecting to an input 
device is not fast. Wait about 10 seconds after putting a device into pairing mode before 
using it.

Joystick has been tested and seems to work OK (although the input is digital only, no analog
movement is supported)

Mouse works, but again: inputs get flattened to digital-only. You may need a mouse 
with >=3 buttons to start the game (mouse wheel press / middle button = start)

Keyboard is implemented but entirely untested. The keymapping should be: z and x for 
shoot & open, a and s for left and right strafe, tab for select, enter for start, cursor 
keys for movement.

In general: weapon selection is hold open + use the strafe keys.
