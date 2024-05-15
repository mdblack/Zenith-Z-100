//Margaret Black
//2018 - 2024

/*****************************************************************************
 * 8085 processor emulator                                                   *
 * Created:     Margaret Black                                               *
 * Copyright:   (C) 2018-2024 Margaret Black  blackmd@gmail.com              *
 *****************************************************************************/

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation                                           *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

typedef struct Z_100 Z100;
typedef struct
{
	int A, B, C, D, E, H, L, SP, PC;
	int c, p, ac, z, s, i, m75, m65, m55;
	int flags;
	int interrupt_deferred, interrupts;
	int halted;

	int opcode, value, immediate, cycles;
	char* name;

	// signal for external device data read request (used for FD-1797 Floppy Disk controller
	int ready_;

	int wait_state;

	unsigned char* memory;

	Z100* z100;
} P8085;

void doInstruction8085();
void reset8085();
void throwInterrupt8085(int);
