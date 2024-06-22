//Margaret Black
//2018-2024
//Joseph Matta
//2020

// *This file implements the Z100 Main board.
// Hardware and glue logic existant on the main board is implemented here

/* mainBoard.c */

/*****************************************************************************
 * This program is free software. You can redistribute it and / or modify it *
 * under the terms of the GNU General Public License version 2 as  published *
 * by the Free Software Foundation.                                          *
 *                                                                           *
 * This program is distributed in the hope  that  it  will  be  useful,  but *
 * WITHOUT  ANY   WARRANTY,   without   even   the   implied   warranty   of *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU  General *
 * Public License for more details.                                          *
 *****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <gtk/gtk.h>
#include "mainboard.h"
#include "screen.h"
#include "name.h"
#include "debug.h"

#ifdef RPI
#include<pigpio.h>
#endif


// floppy a and b file names
char* image_name_a=NULL;
char* image_name_b=NULL;

FILE* printer_in=NULL;
FILE* printer_out=NULL;

//cpm addons
#define SECTOR_SIZE 128
#define SECTORS_PER_TRACK 26
unsigned int sector,track,sector_ram_low,sector_ram_high,drive,sector_count;

unsigned char* disk;
unsigned char* diskb;
unsigned int disksize=0,disksizeb=0;

pthread_t emulator_thread;      // this thread object runs the main emulator thread
volatile sig_atomic_t pauseSimulation;	// allows user to debug and single-step

unsigned int* pixels;   // holds the state of each pixel on the screen

Z100* z100object;

void initcpmdisks(Z100* c)
{
       FILE* cpmdisk=fopen(image_name_a,"rb");
        if(cpmdisk==NULL)
        {
                printf("can't find %s\n",image_name_a);
                return;
        }
        fseek(cpmdisk,0,SEEK_END);
        disksize=ftell(cpmdisk);
        rewind(cpmdisk);
        disk=(unsigned char*)malloc(sizeof(unsigned char)*disksize);
        fread(disk,1,disksize,cpmdisk);
        fclose(cpmdisk);

        cpmdisk=fopen(image_name_b,"rb");
        if(cpmdisk!=NULL)
        {
                fseek(cpmdisk,0,SEEK_END);
                disksizeb=ftell(cpmdisk);
                rewind(cpmdisk);
                diskb=(unsigned char*)malloc(sizeof(unsigned char)*disksizeb);
                fread(diskb,1,disksizeb,cpmdisk);
                fclose(cpmdisk);
        }
}

Z100* newComputer()
{
	Z100* computer=(Z100*)malloc(sizeof(Z100));
	z100object=computer;
        //create a keyboard
	computer->keyboard=newKeyboard();
        //video handles CRT controller 68A45 and video parallel port
	computer->video=newVideo();
	computer->p8088=new8088(computer);
	assignCallbacks8088(computer->p8088, z100_memory_read,z100_memory_write,z100_port_read,z100_port_write);

	computer->p8085.z100=computer;

	// create two interrupt controller objects "master" and "secondary"
	// need secondary to 1. pass the self-test and 2. access JWD1797 floppy controller
	computer->e8259_master=e8259_new("MASTER",computer);
	computer->e8259_secondary=e8259_new("SECONDARY",computer);
	e8259_reset(computer->e8259_master);
	e8259_reset(computer->e8259_secondary);

	//setup master int controller to cause 8088 interrupt on trap pin (connect interrupt controller to 8088 processor) 
	e8259_set_int_fct(computer->e8259_master, NULL, interruptFunctionCall);
        // setup secondary int controller to cause IR3 pin on master to go high 
	e8259_set_int_fct(computer->e8259_secondary, NULL, cascadeInterruptFunctionCall);

	// make a timer object
	computer->e8253 = e8253_new(computer);
	e8253_set_gate(computer->e8253, 0, 1);
	e8253_set_gate(computer->e8253, 1, 1);
	e8253_set_gate(computer->e8253, 2, 1);

	//set OUT 0 and OUT 2 functions to triggar IRQ2 on the master PIC
	//OUT 0 also clocks timer channel 1 in the Z-100
	e8253_set_out_fct(computer->e8253, 0, NULL, timer_out_0);
	e8253_set_out_fct(computer->e8253, 1, NULL, timer_out_1);
	e8253_set_out_fct(computer->e8253, 2, NULL, timer_out_2);

	//set up floppy controller
	computer->jwd1797 = newJWD1797(computer->e8259_secondary,computer);

	// this will be the S101 Switch - selects functions to be run during start-up
	// and master reset. (Page 2.8 in Z100 technical manual for pin defs)

	//this setting starts at the hand prompt
	computer->switch_s101_FF = 0b00000000;
	//this setting boots automatically
	if(AUTOBOOT==1)
		computer->switch_s101_FF = 0b00001000;
	// processor swap ports
	computer->processor_swap_port_FE = 0b00000000;
	// memory control latch
	computer->memory_control_latch_FC = 0b00000000;
	// io_diag_port
	computer->io_diag_port_F6 = 0b11111111;

	for(unsigned int addr=0; addr < RAM_SIZE; addr++)
		computer->ram[addr]=0;

	return computer;
}

void reset(Z100* computer)
{
	//reset the processors
	reset8085(&computer->p8085);
	reset8088(computer->p8088);

	// load initial disk file in reset function
	resetJWD1797(computer->jwd1797);

	resetVideo(computer->video,pixels);

	//set the counters
	computer->instructions_done = 0;
	computer->last_instruction_cycles = 0;
	computer->last_instruction_time_us = 0.0;
	computer->total_time_elapsed = 0.0;
	computer->vsync_timer_cycle_count = 0;
	computer->vsync_timer_overage = 0;
	computer->e8253_timer_cycle_count = 0;
	computer->e8253_timer_overage = 0;
	// set processor selection to 8085
	computer->active_processor = PR8085;

	computer->serialloopback=0;
	computer->int6set=0;

	//temp set ROM romOption to 0 - make the ROM appear to be repeated throughout memory
	computer->romOption=0;
//	computer->romOption=1;
	//setting killParity to 0 here actually enables the parity checking circuitry
	//setting this to 1 is saying "yes, kill (disable) the parity che>
	//in the actual hardware, bit 5 of the memory control port would be set t>
	//to disable parity ckecking
	computer->killParity = 0;
	//setting this zeroParity variable to zero forces the zero parity like in the hardware
	computer->zeroParity = 0;
	// this holds the result of calculating a byte parity
	computer->byteParity = 0;


	e8259_reset(computer->e8259_master);
	e8259_reset(computer->e8259_secondary);
	//this setting starts at the hand prompt
	computer->switch_s101_FF = 0b00000000;
	if(AUTOBOOT==1)
	//this setting boots automatically
		computer->switch_s101_FF = 0b00001000;

	// processor swap ports
	computer->processor_swap_port_FE = 0b00000000;
	// memory control latch
	computer->memory_control_latch_FC = 0b00000000;
	// io_diag_port
	computer->io_diag_port_F6 = 0b11111111;
}

void z100singleinstruction(Z100* computer)
{
/*if(computer->p8085.PC==0x3c)
{
        for(unsigned int i=0x80; i<0x400; i++)
        {
                z100_memory_write(i,0,computer);
        }
        for(unsigned int i=0x480; i<=0xffff; i++)
        {
                z100_memory_write(i,0,computer);
        }
printf("Cleared all memory\r\n");
exit(0);
}
*/
	//is there a keyboard trap requested?
	if(computer->keyboard->requestInterrupt==1 && computer->active_processor==PR8088)
	{
//		printf("keyboard interrupt request received, calling trap\n");
		computer->keyboard->requestInterrupt=0;
		if(computer->p8088->CS!=0xfc01)
			trap(computer->p8088,6+64,1);
	}
	//step the active processor one cycle
	if(computer->active_processor==PR8085)
		handle8085InstructionCycle(computer);
	else if(computer->active_processor==PR8088)
		handle8088InstructionCycle(computer);

	// calculate virtual time passed with the last instruction execution
	updateElapsedVirtualTime(computer);

	//simulate VSYNC interrupt on I6 (keyboard video display and light pen int)
	//roughly every 10,000 instructions - This satisfies BIOS diagnostics,
	//but the interrupt routine is not used to update the Z-100 display
	simulateVSYNCInterrupt(computer);

	//clock the 8253 timer - this should be ~ every 4 microseconds
	//- according to page 2.80 of Z-100 technical manual, the timer is
	//clocked by a 250kHz clock (every 20 cycles of the 5 Mhz main clock)
	//The e8253 timer is incremented based on the last instruction cycle count.
	handle8253TimerClockCycle(computer);

	//cycle the JWD1797. The JWD1797 is driven by a 1 MHz clock in the Z-100.
	//Thus, it should be cycled every 1 microsecond or every five CPU (5 MHz)
	//cycles. Instead, time slices added to the internal JWD1797 timer
	//mechanisms will be determined by how many cycles the previous instruction took.
	doJWD1797Cycle(computer->jwd1797,computer->last_instruction_time_us);

	// update the screen every 100,000 instructions
	updateZ100Screen(computer);

	if(computer->active_processor==PR8085)
		bp_exec_check(computer->p8085.PC);
	else if(computer->active_processor==PR8088)
		bp_exec_check(((computer->p8088->CS<<4)+computer->p8088->IP)&0xfffff);
}

//main emulation loop: run the processors
void z100mainloop(Z100* computer)
{

	printf("Start running processors\n\n");

	//run the processors forever
	while(1)
	{
		//give the debugger a chance to run if invoked
		if(pauseSimulation==1)
			handleDebug(computer);
		//do a single instruction
		z100singleinstruction(computer);
	}
}

void handle8088InstructionCycle(Z100* c)
{
	c->p8088->ready_x86_ = c->jwd1797->drq;
	doInstruction8088(c->p8088);

	if(c->p8088->wait_state_x86==0)
	{
		c->instructions_done++;
		c->last_instruction_cycles=c->p8088->cycles;
/*
		P8088* p8088=c->p8088;
               printf("IP = %X, opcode = %X, inst = %s\n",
                        p8088->IP,p8088->opcode,p8088->name_opcode);
                printf("value1 = %X, value2 = %X, result = %X cycles = %d\n",
                        p8088->operand1,p8088->operand2,p8088->op_result,p8088->cycles);
                printf("AL = %X, AH = %X, BL = %X, BH = %X, CL = %X, CH = %X, DL = %X, DH = %X\n"
                        "SP = %X, BP = %X, DI = %X, SI = %X\n"
                        "CS = %X, SS = %X, DS = %X, ES = %X\n",
                        p8088->AL, p8088->AH,p8088->BL,p8088->BH,p8088->CL,p8088->CH,p8088->DL,
                        p8088->DH,p8088->SP,p8088->BP,p8088->DI,p8088->SI,p8088->CS,p8088->SS,
                        p8088->DS,p8088->ES);
                printf("carry = %X, parity = %X, aux_carry = %X, zero = %X, sign = %X\n",
                        p8088->c,p8088->p,p8088->ac,p8088->z,p8088->s);
                printf("trap = %X, int = %X, dir = %X, overflow = %X\n",
                        p8088->t,p8088->i,p8088->d,p8088->o);
*/
	}
	//processor is in a wait state
	//- no instruction done
	//- 1 clock cycle passes (200 ns for 5 MHz clock)
	else
	{
		c->last_instruction_cycles = 1;
	}
}

void handle8085InstructionCycle(Z100* c)
{
	// update data request signal from JWD1797 drq pin
	c->p8085.ready_ = c->jwd1797->drq;
	doInstruction8085(&c->p8085);
	// if processor is NOT in a wait state
	if(c->p8085.wait_state == 0)
	{
		c->instructions_done++;
		c->last_instruction_cycles = c->p8085.cycles;
/*
		P8085 p8085=c->p8085;
               printf("PC = %X, opcode = %X, inst = %s\n",p8085.PC,p8085.opcode,p8085.name);
                printf("A = %X, B = %X, C = %X, D = %X, E = %X, H = %X, L = %X, SP = %X\n",
                        p8085.A, p8085.B, p8085.C, p8085.D, p8085.E, p8085.H, p8085.L, p8085.SP);
                printf("carry = %X, parity = %X, aux_carry = %X, zero = %X, sign = %X\n",
                        p8085.c, p8085.p, p8085.ac, p8085.z, p8085.s);
                printf("i = %X, m75 = %X, m65 = %X, m55 = %X\n",
                        p8085.i, p8085.m75, p8085.m65, p8085.m55);
*/
	}
	//processor is in a wait state
	//- no instruction done
	//- 1 clock cycle passes (200 ns for 5 MHz clock)
	else
	{
		c->last_instruction_cycles = 1;
	}
}

void updateElapsedVirtualTime(Z100* c)
{
	// number of cycles for last instruction * 0.2 microseconds (5 MHz clock)
	c->last_instruction_time_us = c->last_instruction_cycles * 0.2;
	// increment the time total_time_elapsed based on the last instruction time
	c->total_time_elapsed += c->last_instruction_time_us;
}

void simulateVSYNCInterrupt(Z100* c)
{
	// update VSYNC cycle count
	c->vsync_timer_cycle_count += c->last_instruction_cycles;

	if(c->vsync_timer_cycle_count >= VSYNC_TIMER_CYCLE_LIMIT)
	{
		// this will account for any additional cycles not used for this clock cycle
		c->vsync_timer_overage = c->vsync_timer_cycle_count - VSYNC_TIMER_CYCLE_LIMIT;
		// set the irq6 pin on the master 8259 int controller to high
		e8259_set_irq6(c->e8259_master, 1);
		c->int6set=1;
		c->vsync_timer_cycle_count = c->vsync_timer_overage;
	}

	// reset VSYNC INT if pulse high
	if(c->int6set == 1)
	{
		// set the irq6 pin to low
		e8259_set_irq6(c->e8259_master, 0);
		c->int6set = 0;
	}
}

void handle8253TimerClockCycle(Z100* c)
{
	// update timer cycle count
	c->e8253_timer_cycle_count += c->last_instruction_cycles;
	if(c->e8253_timer_cycle_count >= E8253_TIMER_CYCLE_LIMIT)
	{
		// this will account for any additional cycles not used for this clock cycle
		c->e8253_timer_overage = c->e8253_timer_cycle_count - E8253_TIMER_CYCLE_LIMIT;
		e8253_clock(c->e8253, 1);
		c->e8253_timer_cycle_count = c->e8253_timer_overage;
	}
}

void updateZ100Screen(Z100* c)
{
	if(c->instructions_done%100000 == 0)
	{
		//update pixel array using current VRAM state using renderScreen() function from video.c
		renderScreen(c->video, pixels);
		// draw pixels to the GTK window using display() function from screen.c
		display();
	}
}

void interruptFunctionCall(void* v, int number, Z100* c)
{
	if(number == 0)
	{
		return;
	}
	int irq = e8259_inta(c->e8259_master, c->e8259_secondary);
	trap(c->p8088, irq, 0);
}

void cascadeInterruptFunctionCall(void* v, int number, Z100* c)
{
	if(number == 0)
	{
		return;
	}
	printf("SECONDARY 8259 PIC INT SIGNAL to MASTER 8259 PIC IRQ3\n");
	e8259_set_irq3(c->e8259_master, 1);
}

// since the Z-100 timer circuitry clocks the 8253 channel 1 timer with the
// output of the channel 0 timer, this function clocks timer channel 1 when
// channel 0's out goes high (Z-100 Technical Manual (Hardware) - Page 10.8)

void timer_out_0(void* v, int number, Z100* c)
{
	e8259_set_irq2(c->e8259_master, 1);
	e8253_cascade_clock_ch1(c->e8253, 1);
}

void timer_out_1(void* v, int number, Z100* c)
{
	printf("timer out 1\n");
}
void timer_out_2(void* v, int number, Z100* c)
{
	printf("timer out 2\n");
	e8259_set_irq2(c->e8259_master, 1);
}
//
//		================================
//		***** READ/WRITE FUNCTIONS *****
//		================================
//
// these are called from the processors

unsigned int z100_memory_read(unsigned int addr, Z100* c)
{
	unsigned int return_value = 0x00;

	// based on memory mode adjust memory read location
	switch(c->romOption)
	{
		case 0:
		//this is the ROM memory configuration that makes the code in ROM appear
		//to be in all of memory, therefore any address is moded by 0x4000
		//(size of ROM)
			return_value = c->rom[addr&(ROM_SIZE-1)]&0xff;
			break;

		case 1:
		// the ROM appears at the top of every 64K page of memory
			if((addr&0xffff) > 65536-ROM_SIZE)
				return_value = c->rom[addr&(ROM_SIZE-1)]&0xff;
			else if (addr < RAM_SIZE)
				return_value = c->ram[addr]&0xff;
			else if (addr >= 0xc0000 && addr <=0xeffff)
				return_value = c->video->vram[addr-0xc0000]&0xff;
			else
				return_value = 0xff;
			break;

		case 2:
		//this is the ROM memory configuration that makes the code in ROM appear
		//to be at the top of the first megabyte of memory.
			if(addr < RAM_SIZE)
				return_value = c->ram[addr]&0xff;
			else if (addr >= 0xf8000)
				return_value = c->rom[addr&(ROM_SIZE-1)]&0xff;
			else if (addr >= 0xc0000 && addr <= 0xeffff)
				return_value = c->video->vram[addr-0xc0000]&0xff;
			else
				return_value = 0xff;
			break;

		case 3:
			// ROM is disabled - just read from RAM
			if(addr < RAM_SIZE)
				return_value = c->ram[addr]&0xff;
			else if (addr >= 0xc0000 && addr <= 0xeffff)
				return_value = c->video->vram[addr-0xc0000]&0xff;
			else
				return_value = 0xff;
			break;
	}

	// calculate parity of byte read (return_value) *** SHOULD THIS BE WHEN A BYTE IS WRITTEN?
	c->byteParity = getParity(return_value);

	// if we force a zero parity, enabled parity errors (by setting kill parity to false - 0),
	// and get an odd parity from byte read from memory location
	// -> throw parity interrupt by writing a 1 to line 0 of IRQ input.
	if(c->zeroParity==1 && c->killParity==0 && c->byteParity==1)
	{
		// write 1 to IRQ line 0 (pin 0)
		e8259_set_irq0(c->e8259_master, 1);
	}

	bp_read_check(addr);
	return return_value;
}

void z100_memory_write(unsigned int addr, unsigned char data, Z100* c)
{
 	if(addr < RAM_SIZE)
	{
		// write data to RAM at address
		c->ram[addr] = data&0xff;
	}
	else if(addr >= 0xc0000 && addr <= 0xeffff)
	{
		// write data to video memory portion of RAM at address
		// video object has its own virtual RAM to hold video data
		c->video->vram[addr - 0xc0000] = data&0xff;
	}
	bp_write_check(addr);
}

void z100_memory_must_write(unsigned int addr, unsigned char data, Z100* c)
{
	if(addr>=0xf0000)
		c->rom[addr]=data&0xff;
	else
		z100_memory_write(addr,data,c);
}

int receive_ready(Z100* c)
{
	if(!keyboardStatusRead(c->keyboard))
		return 0;
	return 1;
}

unsigned char receive(Z100* c)
{
	while(!keyboardStatusRead(c->keyboard))
	{
		if(pauseSimulation==1) return 0;
	}
	return keyboardDataRead(c->keyboard)&0x7f;
}

unsigned int z100_port_read(unsigned int address, Z100* c)
{
	unsigned char return_value;

	// the appropriate port reads will happen here
	// read port based on incoming port address
	switch(address)
	{

		//cpm emulator ports
                case 2:
                        return receive(c);
                //read status
                case 3:
                        if(receive_ready(c))
                                return 1;
                        else
                                return 0;
                //disk status
                case 0xf:
                        return 0xff;


		//ports AA-AF are for the Winchester hard drive
		//this isn't implemented so we return dummy values

		// Z-217 Secondary Winchester Drive Controller Status Port (0xAA)
		case 0xAA:
		// printf("reading from Z-217 Secondary Winchester Drive Controller Status Port %X [NO HARDWARE IMPLEMENTATION]\n", address);
      		// this returned status value is hardcoded to indicate the absence of the
      		//Winchester Drive Controller S-100 card */
			return_value = 0xfe;
			break;
		// Z-217 Secondary Winchester Drive Controller Command Port (0xAB)
		case 0xAB:
		// printf("reading from Z-217 Secondary Winchester Drive Controller Command Port %X [NO HARDWARE IMPLEMENTATION]\n", address);
		//this returned command value is hardcoded to indicate the absence of the Winchester Drive controller 
			return_value = 0x0;
			break;
		// Z-217 Primary Winchester Drive Controller Status Port (0xAE)
		case 0xAE:
		// printf("reading from Z-217 Primary Winchester Drive Controller Status Port %X [NO HARDWARE IMPLEMENTATION]\n", address);
		//this returned status value is hardcoded to indicate the absence of the
		//Winchester Drive Controller S-100 card 
			return_value = 0xfe;
			break;
		// Z-217 Primary Winchester Drive Controller Command Port (0xAF)
		case 0xAF:
		// printf("reading from Z-217 Primary Winchester Drive Controller Command Port %X [NO HARDWARE IMPLEMENTATION]\n", address);
      		//this returned command value is hardcoded to indicate the absence of the Winchester Drive controller 
			return_value = 0x0;
			break;

		// FD179X-02 Floppy Disk Formatter/Controller (0xB0-0xB5)
		case 0xB0:
		// Z-207 Primary Floppy Drive Controller Status Port
			return_value = readJWD1797(c->jwd1797, address);
			break;
		case 0xB1:
		// Z-207 Primary Floppy Drive Controller Track Port
			return_value = readJWD1797(c->jwd1797, address);
			break;
		case 0xB2:
		// Z-207 Primary Floppy Drive Controller Sector Port
			return_value = readJWD1797(c->jwd1797, address);
			break;
		case 0xB3:
		// Z-207 Primary Floppy Drive Controller Data Port
			return_value = readJWD1797(c->jwd1797, address);
			break;
		case 0xB4:
		// Z-207 Primary Floppy Drive Controller CNTRL Control Port
			return_value = readJWD1797(c->jwd1797, address);
			break;
		case 0xB5:
		// Z-207 Primary Floppy Drive Controller CNTRL Status Port
			return_value = readJWD1797(c->jwd1797, address);
			break;

		// Video Commands - 68A21 parallel port
		case 0xD8:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// Video Command Control - 68A21 parallel port
		case 0xD9:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// Video RAM Mapping Module Data - 68A21 parallel port
		case 0xDA:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// Video RAM Mapping Module Control - 68A21 parallel port
		case 0xDB:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// CRT Controller 68A45 Register Select port 0xDC
		case 0xDC:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// CRT Controller 68A45 Register Value port 0xDD
		case 0xDD:
			return_value = readVideo(c->video, address)&0xff;
			break;

		// parallel port (68A21 - Peripheral Interface Adapter (PIA))
		// intended for printer output
		// ports 0xE0-0xE3 ***WHY SHOULD ALL THE 68A21 PORT READS RETURN 0x40??**
		case 0xE0:
		// general data port
			// hardcoded return value
			return_value = 0x40;
			break;
		case 0xE1:
		// general control port
			return_value = 0x40;
			break;
		case 0xE2:
		// printer data port
			return_value = 0x40;
			break;
		case 0xE3:
		// printer control port
			return_value = 0x40;
			break;

		//8253 timer ports
		case 0xE4:
			// 8253 timer counter 0
		case 0xE5:
			// 8253 timer counter 1
		case 0xE6:
			// 8253 timer counter 2
		case 0xE7:
			// 8253 timer control port
			return_value = e8253_get_uint8(c->e8253, address&3)&0xff;
			break;

		case 0xe8:
		case 0xea:
		case 0xec:
		case 0xee:
			return_value=0x0;
			break;

		//Serial ports
		case 0xe9:
			return_value=0xff;
			break;
		case 0xEB:
			// Serial A (printer port)
			return_value = c->serialloopback;
			break;

		case 0xEF:
			// Serial B (modem port)
			//dummy value
			return_value = 0x00;
			break;

		// read IRR register of 8259 secondary interrupt controller (PIC)
		case 0xF0:
		case 0xF1:
			return_value = e8259_get_uint8(c->e8259_secondary, address&1)&0xff;
			break;
		// read IRR register of 8259 master interrupt controller (PIC)
		case 0xF2:
		case 0xF3:
			return_value = e8259_get_uint8(c->e8259_master, address&1)&0xff;
			break;

		// keyboard data
		case 0xF4:
			return_value = keyboardDataRead(c->keyboard);
			break;

		// keyboard status
		case 0xF5:
			return_value = keyboardStatusRead(c->keyboard);
			break;

		// IO_DIAG port F6
		case 0xF6:
			return_value = c->io_diag_port_F6;
			break;

		case 0xFB:
		// 8253 timer status port
			return_value = e8253_get_status(c->e8253)&0xff;
			break;

		case 0xFC:
		// memory control latch port FC;
			return_value = c->memory_control_latch_FC;
			break;
		//high-address latch implemented
		case 0xfd:
			printf("Reading from high address latch\n");
			exit(0);
			break;

		case 0xFE:
		// processor swap port FE;
			return_value = c->processor_swap_port_FE;
			break;

		// S101 DIP Switch - (Page 2.8 in Z100 technical manual for pin defs)
		case 0xFF:
			return_value = c->switch_s101_FF;
			break;

		default:
			printf("READING FROM UNIMPLEMENTED PORT %X\n",address);
	  		return_value = 0x00;
			break;
	}
	bp_in_check(address);
	return return_value;
}

void z100_port_write(unsigned int address, unsigned char data, Z100* c)
{
	switch(address) 
	{
		//2-f cpm emulator ports


		case 2:
	                printf("%c",data);
//			videoSetChar(c->video,0,0,data);
			videoWrite(c->video,data);
			display();
			break;
               //disk handler
                case 0x9:
                        //addr_low
                        sector_ram_low=data;
                        break;
                case 0xa:
                        //addr_high
                        sector_ram_high=data;
                        break;
                case 0xb:
                        //sector is 128 bytes
                        sector_count=data;
                        break;
                //track
                case 0xc:
                        track=data;
                        break;
                //sector
                case 0xd:
                        sector=data;
                        break;
                case 0xe:
                        drive=data;
                case 0xf:
                        //0x20 is read disk
                        //0x30 is write disk
                        sector_count=1;
                        if(data==0x20)
                        {
                                unsigned int rawsector = sector+track*SECTORS_PER_TRACK-1;
                                unsigned int addr=(sector_ram_high<<8)|sector_ram_low;
                                if(drive==0)
                                {
                                        for(int i=0; i<SECTOR_SIZE*sector_count; i=i+1)
                                        {
                                                if(rawsector*SECTOR_SIZE+i<disksize && addr+i<65536)
						{
							z100_memory_write(addr+i,disk[rawsector*SECTOR_SIZE+i],c);
//							printf("reading from cpm: %x, writing in memory %x: %x\n",
//								disk[rawsector*SECTOR_SIZE+i],
//								addr+i,
//								z100_memory_read(addr+i,c));
						}
                                        }
                                }
                                else if(drive==1 && disksizeb>0)
                                {
                                        for(int i=0; i<SECTOR_SIZE*sector_count; i=i+1)
                                        {
                                                if(rawsector*SECTOR_SIZE+i<disksizeb && addr+i<65536)
							z100_memory_write(addr+i,diskb[rawsector*SECTOR_SIZE+i],c);
                                        }
                                }
                        }
                       else if(data==0x30)
                        {
                                unsigned int rawsector = sector+track*SECTORS_PER_TRACK-1;
                                unsigned int addr=(sector_ram_high<<8)|sector_ram_low;
                                for(int i=0; i<SECTOR_SIZE*sector_count; i=i+1)
                                {
                                        if(drive==0)
                                        {
                                                if(rawsector*SECTOR_SIZE+i<disksize && addr+i<65536)
                                                {
                                                        disk[rawsector*SECTOR_SIZE+i]=z100_memory_read(addr+i,c);
						}
                                        }
                                        else if (drive==1 && disksizeb>0)
                                        {
                                                if(rawsector*SECTOR_SIZE+i<disksizeb && addr+i<65536)
                                                {
                                                        diskb[rawsector*SECTOR_SIZE+i]=z100_memory_read(addr+i,c);
                                                }
                                        }
                                }
				if(drive==0 && image_name_a[0]!='_')
				{
						FILE* cpmdisk=fopen(image_name_a,"wb");
						for(int i=0; i<disksize; i++)
							fprintf(cpmdisk,"%c",disk[i]);
						fclose(cpmdisk);
				}
				if(drive==1 && image_name_b[0]!='_')
				{
						FILE* cpmdisk=fopen(image_name_b,"wb");
						for(int i=0; i<disksize; i++)
							fprintf(cpmdisk,"%c",disk[i]);
						fclose(cpmdisk);
				}
                        }
                        break;



		//No winchester drive present
		case 0xAA:
		case 0xAB:
		case 0xAE:
		case 0xAF:
			break;

		//FD179X-02 Floppy Disk Formatter/Controller (0xB0-0xB5)
		case 0xB0:
		// Z-207 Primary Floppy Drive Controller Command Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;
		case 0xB1:
		// Z-207 Primary Floppy Drive Controller Track Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;
		case 0xB2:
		// Z-207 Primary Floppy Drive Controller Sector Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;
		case 0xB3:
		// Z-207 Primary Floppy Drive Controller Data Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;
		case 0xB4:
		// Z-207 Primary Floppy Drive Controller CNTRL Control Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;
		case 0xB5:
		// Z-207 Primary Floppy Drive Controller CNTRL Status Port
			writeJWD1797(c->jwd1797, address, data&0xff);
			break;

		// Video Commands - 68A21 parallel port
		case 0xD8:
printf("d8 write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// Video Command Control - 68A21 parallel port
		case 0xD9:
printf("d9 write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// Video RAM Mapping Module Data - 68A21 parallel port
		case 0xDA:
printf("da write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// Video RAM Mapping Module Control - 68A21 parallel port
		case 0xDB:
printf("db write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// CRT Controller 68A45 Register Select port 0xDC
		case 0xDC:
printf("dc write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// CRT Controller 68A45 Register Value port 0xDD
		case 0xDD:
printf("dd write %x\n",data);
			writeVideo(c->video, address, data&0xff);
			break;
		// parallel port (68A21 - Peripheral Interface Adapter (PIA))
		// ports 0xE0-0xE3
		case 0xE0:
			// general data port
			break;
		case 0xE1:
			// general control port
			break;
		case 0xE2:
			// printer data port
			if(printer_out==NULL)
				printf("PRINTER: %c\n",data);
			else
			{
				fprintf(printer_out,"%c",data);
				fflush(printer_out);
			}
			break;
		case 0xE3:
			// printer control port
			break;
		case 0xE4:
			e8253_set_uint8(c->e8253, address&3, data&0xff);
			break;
		case 0xE5:
			// 8253 timer counter 1
			e8253_set_uint8(c->e8253, address&3, data&0xff);
			break;
		case 0xE6:
			// 8253 timer counter 2
			e8253_set_uint8(c->e8253, address&3, data&0xff);
			break;
		case 0xE7:
			// 8253 timer control port
			e8253_set_uint8(c->e8253, address&3, data&0xff);
			break;
		case 0xe8:
			if(printer_out==NULL)
				printf("E8: %c\n",data);
			else
			{
				fprintf(printer_out,"%c",data);
				fflush(printer_out);
			}
			break;
		case 0xea:
		case 0xee:
		case 0xec:
			break;
		case 0xEB:
			// Serial A (printer port)
			printf("writing %X to Serial A printer port %X\n", data, address);
			c->serialloopback = data;
			break;
		case 0xEF:
			// Serial B (modem port)
			printf("writing %X to Serial B modem port %X\n", data, address);
			c->serialloopback = data;
			break;
		case 0xFB:
			// 8253 timer status port
			e8253_set_status(c->e8253, data&0xff);
			break;
			// memory control latch port FC;
		case 0xFC:
		{
			c->memory_control_latch_FC = data;
			//extract bits 3 and 2 to determine which ROM memory configuration will be set
			int bit2 = (data >> 2) & 0x01;
			int bit3 = (data >> 3) & 0x01;
			if (bit3 == 0 && bit2 == 0)
			{
				c->romOption = 0;
			}
			else if (bit3 == 0 && bit2 == 1)
			{
				c->romOption = 1;
			}
			else if (bit3 == 1 && bit2 == 0)
			{
				c->romOption = 2;
			}
			else if (bit3 == 1 && bit2 == 1)
			{
				c->romOption = 3;
			}
			// check the zero parity mode bit by examining bit 4 -
			// if the zero party bit is 0, the parity for every byte is forced to zero
			// -- this mode is used to force a parity error to check the parity logic
			c->zeroParity = ((data>>4)&1)==0;
			// check the kill parity bit by examing bit 5 -
			// if this bit is a 0, the parity checking circuitry is trurned off
			int tokillparity = ((data>>5)&1)==0;
			// if parity checking circuitry is enabled with a 1 written to bit 5 and
			// killParity variable is a 1 which means the circuitry is off (actual hardware
			// memory control port bit 5 = 0)
			if(!tokillparity && c->killParity)
			{
				printf("CLEAR PARITY ERROR\n");
				printf("CS = %X IP = %X\n", c->p8088->CS, c->p8088->IP);
				e8259_set_irq0(c->e8259_master, 0);
			}
			c->killParity = tokillparity;
			break;
		}
		// secondary interrupt controller (8259) ports F0 and F1
		case 0xF0:
		case 0xF1:
			e8259_set_uint8(c->e8259_secondary, address&1, data);
			break;
		// master interrupt controller (8259) ports F2 and F3
		case 0xF2:
		case 0xF3:
			e8259_set_uint8(c->e8259_master, address&1, data);
			break;
		// keyboard command port F5
		case 0xF5:
			keyboardCommandWrite(c->keyboard, data);
			break;
		// IO_DIAG port F6
		case 0xF6:
			// printf("writing %X to IO_DIAG port %X\n", data, address);
			c->io_diag_port_F6 = data;
			break;
		//high-address latch implemented
		case 0xfd:
			printf("Writing to high address latch %x\n",data);
			exit(0);
			break;
		// processor switch port FE
		case 0xFE:
			c->processor_swap_port_FE = data;
			printf("Value %X written to processor_swap_port_FE port at address %X\n",data, address);
			// select proper processor based on 7th bit in port data
			// if the 7th bit is 1, select 8088 processor
			if(((c->processor_swap_port_FE >> 7) & 0x01) == 0x01)
			{
				c->active_processor = PR8088;
			}
			// if the 7th bit is 0, select 8085 processor
			else if (((c->processor_swap_port_FE >> 7) & 0x01) == 0x00)
			{
				c->active_processor = PR8085;
			}
			// generate interrupt signal (I1 on master 8259A) if bit 1 is set high
			if(((c->processor_swap_port_FE >> 1) & 0x01) == 0x01)
			{
				e8259_set_irq1(c->e8259_master, 1);
			}
			// force a swap to the 8088 if bit 0 is high. This option is used when
			// an interrupt is called when the 8085 is active but the interrupt must
			// be handled by the 8088. Otherwise (if bit 0 is low) interrupts are handled
			// by whatever processor is active.
			if(((c->processor_swap_port_FE) & 0x01) == 0x01)
			{
				c->active_processor = PR8088;
			}
			break;
		// S101 DIP Switch - (Page 2.8 in Z100 technical manual for pin defs)
		// NOTE: since this is a PHYSICAL DIP switch - this case should never be called
		case 0xFF:
			printf("ERROR: CAN NOT WRITE to DIP_switch_s101_FF port at address %X\n",address);
			break;
		// unimplemented port
		default:
			printf("WRITING %X TO UNIMPLEMENTED PORT %X\n", data, address);
			break;
	}
	bp_out_check(address);
}

int getParity(unsigned int data)
{
	return ((data&1)!=0) ^ ((data&2)!=0) ^ ((data&4)!=0) ^ ((data&8)!=0) ^
	((data&16)!=0) ^ ((data&32)!=0) ^ ((data&64)!=0) ^ ((data&128)!=0) ^
	((data&256)!=0) ^ ((data&512)!=0) ^ ((data&1024)!=0) ^ ((data&2048)!=0) ^
	((data&4096)!=0) ^ ((data&8192)!=0) ^ ((data&16384)!=0) ^ ((data&32768)!=0);
}
// open the bin file for the Z100 ROM and load it into the rom array
void loadrom(Z100* c, char* fname)
{
	FILE* f = fopen(fname,"rb");
	if(f==NULL)
	{
		printf("Can't find BIOS ROM image %s\n",fname);
		exit(0);
	}
	for(int i = 0; i < ROM_SIZE; i++) 
	{
		c->rom[i] = fgetc(f);
	}
	fclose(f);
}

//invoked when ctrl-c pressed at console
void onCtrlC(int c)
{
	pauseS();
//	debug_start(z100object);

	struct sigaction act;
	act.sa_handler=onCtrlC;
	sigaction(SIGINT, &act, NULL);

}
void pauseS()
{
	pauseSimulation=1;
	debug_start(z100object);
	printf("PAUSED\n");
}
void unpause()
{
	pauseSimulation=0;
	printf("UNPAUSED\n");
}
void togglePause()
{
	if(pauseSimulation==0)
		pauseS();
	else
		unpause();
}
int getpause()
{
	return pauseSimulation;
}

void setLED(int LED, int state)
{
#ifdef RPI
	if(state==1)
		gpioWrite(LED,PI_HIGH);
	else if(state==0)
		gpioWrite(LED,PI_LOW);
#endif
}

void beep(int ms)
{
#ifdef RPI
	for(int i=0; i<ms; i++)
	{
		gpioWrite(SPEAKER,PI_HIGH);
		time_sleep(0.001/2.0);
		gpioWrite(SPEAKER,PI_LOW);
		time_sleep(0.001/2.0);
	}
#endif
}

void stepclick()
{
#ifdef RPI
	for(int i=0; i<1; i++)
	{
		gpioWrite(SPEAKER,PI_HIGH);
		time_sleep(0.02);
		gpioWrite(SPEAKER,PI_LOW);
		time_sleep(0.02);
	}
#endif
}

void initGPIO()
{
#ifdef RPI
	gpioInitialise();
	gpioSetMode(LED_DRIVE_A,PI_OUTPUT);
	gpioSetMode(LED_DRIVE_B,PI_OUTPUT);
	gpioWrite(LED_DRIVE_A,PI_LOW);
	gpioWrite(LED_DRIVE_B,PI_LOW);
	gpioSetMode(SPEAKER,PI_OUTPUT);
	gpioWrite(SPEAKER,PI_LOW);
#endif
}



void z100_main()
{
	//construct all the parts of the Z-100
	Z100* z100=newComputer();

	// load Z100 monitor ROM from bin file
	loadrom(z100,"zrom_444_276_1.bin");

	//pass the Z100 object to the screen
	screenSetComputer(z100);

	initcpmdisks(z100);

	//reset all components
	reset(z100);

//uncomment to bypass the rom for cpm
//z100_port_write(0xfe,0xff,z100);
//z100_port_write(0xfc,0x04,z100);
//z100->p8088->CS=0;
//z100->p8088->IP=0x400;
//prefetch_flush(z100->p8088);
//z100->p8085.PC=0x3c;

//for(int i=0; i<128; i++)
//	z100->ram[i+0x400]=disk[i];

	//and go!
	z100mainloop(z100);
}

// emulator thread function
void* mainBoardThread(void* arg)
{
	// start the z100_main function defined here in mainBoard.c. This starts the
	// actual emulation program.
	z100_main();
}


int main(int argc, char* argv[]) {

	//if no command line arguments, use IMAGE_NAME as the disk image
	//otherwise use 1 or 2 arguments as disks A and B
	image_name_a=IMAGE_NAME;
	image_name_b="empty.img";
	if(argc>1)
	{
		image_name_a=argv[1];
		printf("%s\n",image_name_a);
		if(argc>2)
			image_name_b=argv[2];
	}

	initGPIO();

	//contributed by porkypiggy64: this enables ctrl-c to pause the simulation
	struct sigaction act;
	act.sa_handler=onCtrlC;
	sigaction(SIGINT, &act, NULL);

	printf("\n\n%s\n\n",
		" ===========================================\n"
		" |\tZENITH Z-100 EMULATOR\t\t   |\n"
		" |\tby: Margaret Black, Joe Matta\t   |\n"
		" |\t8/2020 - 5/2024\t\t\t   |\n"
		" ===========================================\n");

	debug_init();
	// allocate memory for array and initialize each pixel element to 0
	// (uses generateScreen() function from video.c to set up the pixel array)
	pixels = generateScreen();
	// call initialization function defined in screen.c to set up a gtk window
	screenInit(&argc, &argv);
	// start main emulator thread
	pthread_create(&emulator_thread, NULL, mainBoardThread, NULL);
	// start GTK window thread
	screenLoop();
}
