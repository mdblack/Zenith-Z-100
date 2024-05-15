This Z-100 emulator is entirely written in C and can be compiled on a Linux system with gcc by typing make and ./z100 [imagename]  The screen visualization requires the gtk graphics library.

The core emulator was constructed by Margaret Black and Joseph Matta.  The 8088 and 8085 emulators are by Margaret Black, the WD1797 floppy controller by Joseph Matta.  The 8259 PIC and 8253 PIT emulators are by Hampa Hug.  The debugger was contributed by "PorkyPiggy63" and revised by Margaret Black

As of 5/2024, the following components are working:
- The computer boots to Z-DOS versions 1, 2, and 3 and handles commands
- ZBASIC loads, runs, and saves programs
- The ZEDIT text editor, GALAHAD word processor, and Multiplan spreadsheet are all working without errors
- FORMAT works and disks can be made bootable
- A debugger allows stepping, unassembling, breakpoints, memory/port editing, and virtual disks to be swapped at runtime

The following components are not yet working correctly:
- MASM and Microsoft Pascal work intermittently.  This is likely because 8088 instruction 0x82 hasn't been implemented yet
- Attempts to boot CP/M, as well as running the DSKCOMP and DSKCOPY command cause the emulator to freeze in a track verification error loop
- Z-DOS enters a loop waiting for a Timer 2 interrupt that is somehow masked.  Removing one instruction from Z-DOS sidesteps this error.  Also the keyboard only works in Z-DOS if the keyboard interrupts call traps directly rather than through the PIC
 
