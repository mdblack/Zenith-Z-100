//Margaret Black
//2018 - 2024

/*****************************************************************************
 * 8088 processor emulator                                                   *
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



#define PREFETCH_SIZE 4
#define INTERRUPTLISTSIZE 100
typedef struct Z_100 Z100;
typedef void (*store_function) (unsigned int addr, unsigned char val,Z100*);
typedef unsigned int (*load_function) (unsigned int addr,Z100*);


typedef struct
{
	unsigned int AL, AH, BL, BH, CL, CH, DL, DH, SP, BP, DI, SI, IP;
	unsigned int CS, SS, DS, ES;
	unsigned int c, p, ac, z, s, t, i, d, o;

	int interrupt_deferred, interrupts;
	int halt;
	int enable_interrupts;
	int ready_x86_;
	int wait_state_x86;

	unsigned int opcode, value, operand1, operand2, immediate, cycles;
	int op_result;
	const char* name_opcode;

	unsigned char* memory;

	unsigned int prefetch[PREFETCH_SIZE];
	int prefetch_counter;

	store_function memory_write_x86;
	store_function port_write_x86;
	load_function memory_read_x86;
	load_function port_read_x86;

	int interruptlist[INTERRUPTLISTSIZE];
	int interruptlistoldest;
	int interruptlistyoungest;

	unsigned int CSbeforeflush;
	unsigned int IPbeforeflush;

	int wait_state_enabled;

	Z100* z100;
} P8088;

P8088* new8088(Z100*);
void doInstruction8088(P8088*);
void reset8088(P8088*);
void trap(P8088*,unsigned int,int);
void prefetch_flush(P8088*);

void assignCallbacks8088(P8088*,load_function,store_function,load_function,store_function);
