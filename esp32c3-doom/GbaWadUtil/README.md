NOTE: This has been modified for the ESP32C3 version of Doom.

Changes are that demos are replaced by the files in the 'demo' directory, and the original .mus music is replaced by the .imf equivalent in the 'music' directory.

-Sprite_tm

Original README.md:


Utility to process Doom IWAD files for GBA Doom.

It can write either a new WAD file and/or a C Source file.

This is required to reduce the memory footprint of some of the data structures in Doom to get it to fit in 256Kb.

We will pre-calculate more fields so that the lumps stored in the WAD can be used directly from the ROM rather than having to load and convert them in memory.

For example:

In the WAD file a vertex is stored as two short values. (X and Y). When it is loaded into memory, it is shifted left by 16 to convert to 16.16 fixed point. By re-writing this lump and storing it in fixed point, we do not need to copy this into memory at runtime.

Usage: GbaWadUtil -in iwad_file_path -out gba_iwad_path -cfile iwad_cfile_path.


I plan to add PWAD support so that an IWAD and 1 or more PWADS can be converted to a GBA IWAD.

It is a command line util so it can be integrated into a build process.
