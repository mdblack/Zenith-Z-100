//Margaret Black
//2018-2024
//Joseph Matta
//2020


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

//#define RPI 1

#define AUTOBOOT 0

#define ROM_SIZE 0x4000
#define RAM_SIZE 0x30000*2

// (20 cycles = 4 microseconds, EDIT - 8253 timer is connected to a 250kHz
//        clock according to page 2.80 Z-100 Technical manual (hardware) - clocked
//        every 120 cycles
#define E8253_TIMER_CYCLE_LIMIT 20
// 83,333 cycles = 16,666.6 microseconds, VSYNC occurs at 60 Hz
//        along with the display refresh rate
//        (page 4.46 Z-100 Technical manual (hardware))
#define VSYNC_TIMER_CYCLE_LIMIT 83333

#define LED_DRIVE_A 21
#define LED_DRIVE_B 20
#define SPEAKER 26

#include "8085.h"
#include "8088.h"
#include "e8253.h"
#include "e8259.h"
#include "jwd1797.h"
#include "keyboard.h"
#include "video.h"

enum Processor{PR8085,PR8088};

struct Z_100
{
	// rough clock to keep track of time passing (us) as instructions are executed
	double total_time_elapsed;
	unsigned long instructions_done;
	unsigned long breakAtInstruction;
	unsigned int vsync_timer_cycle_count;
	unsigned int vsync_timer_overage;
	unsigned int e8253_timer_cycle_count;
	unsigned int e8253_timer_overage;
	int last_instruction_cycles;
	double last_instruction_time_us;

	unsigned char switch_s101_FF;   // hardware jumpers
	unsigned char processor_swap_port_FE;   //identifies whether 8085 or 8088 is active
	unsigned char memory_control_latch_FC;

	unsigned char rom[ROM_SIZE];    //bios rom
	unsigned char ram[RAM_SIZE];    //main system ram

	int romOption;
	int killParity;
	int zeroParity;
	int byteParity;

	//temporary to make writes to serial appear as reads
	int serialloopback;
	int int6set;
	int io_diag_port_F6;

	//components:
	//two processors, keyboard, video, timer, interrupt controllers, floppy controllers
	P8085 p8085;
	P8088* p8088;
	Keyboard* keyboard;
	Video* video;
	e8253_t* e8253;
	e8259_t* e8259_master;
	e8259_t* e8259_secondary;
	JWD1797* jwd1797;

	enum Processor active_processor;
};

typedef struct Z_100 Z100;

void reset(Z100*);
void z100mainloop(Z100*);
void checkForTraps(Z100*);
void updateElapsedVirtualTime(Z100*);
void simulateVSYNCInterrupt(Z100*);
void handle8253TimerClockCycle(Z100*);
void updateZ100Screen(Z100*);
void interruptFunctionCall(void*, int, Z100*);
void cascadeInterruptFunctionCall(void*, int, Z100*);
void timer_out_0(void*, int, Z100*);
void timer_out_1(void*, int, Z100*);
void timer_out_2(void*, int, Z100*);
unsigned int z100_memory_read(unsigned int,Z100*);
void z100_memory_write(unsigned int, unsigned char,Z100*);
void z100_memory_must_write(unsigned int, unsigned char,Z100*);
unsigned int z100_port_read(unsigned int,Z100*);
void z100_port_write(unsigned int, unsigned char,Z100*);
int getParity(unsigned int);
void handle8088InstructionCycle(Z100*);
void handle8085InstructionCycle(Z100*);
int pr8088_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num);
int pr8085_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num);
void z100singleinstruction(Z100*);
void pauseS();
void unpause();
void togglePause();
int getpause();
void setLED(int LED, int state);
void beep(int ms);
void stepclick();
void initcpmdisks(Z100*);
