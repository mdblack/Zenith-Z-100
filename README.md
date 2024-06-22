This Z-100 emulator is entirely written in C and can be compiled on a Linux system with gcc by typing make and ./z100 [imagename]  The screen visualization requires the gtk graphics library.

The core emulator was constructed by Margaret Black and Joseph Matta.  The 8088 and 8085 emulators are by Margaret Black, the WD1797 floppy controller by Joseph Matta.  The 8259 PIC and 8253 PIT emulators are by Hampa Hug.  The debugger was contributed by "PorkyPiggy63" and revised by Margaret Black

As of 6/2024, the following components are working:
- The computer boots to Z-DOS versions 1, 2, and 3 and handles commands.  Most Z-DOS utilities run.
- CP/M boots using a custom 2.2 port and most utilities run.  The ISEA assembler and SCRED editor work correctly 
- ZBASIC loads, runs, and saves programs
- The ZEDIT text editor, GALAHAD word processor, and Multiplan spreadsheet are all working without errors
- FORMAT works and disks can be made bootable
- MASM and Microsoft Pascal compile programs
- A debugger allows stepping, unassembling, breakpoints, memory/port editing, and virtual disks to be swapped at runtime
- The emulator runs on a Raspberry Pi

