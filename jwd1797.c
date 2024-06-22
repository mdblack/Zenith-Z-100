// WD1797 Implementation
// By: Joe Matta
// email: jmatta1980@hotmail.com
// November 2020
// Copyright (C) 2020

// expanded by Margaret Black, 2024
// email: blackmd@gmail.com

// jwd1797.c

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

// Notes:
/* clock is assumed to be 1 MHz for single and double density mini-floppy disks
	(5.25" ) */
/* Z-DOS disks are 360k disks - 40 cylinders/2 heads (sides)/9 sectors per track/
	512 bytes per sector */
// A 300 RPM motor speed is also assumed
// NO write protect functionality
// the wd1797 has a 1MHz clock in the Z100 for 5.25" floppy

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include "e8259.h"
//#include "jwd1797.h"
#include "mainboard.h"
#include "name.h"

/* TIMINGS (microseconds) */
// index hole pulses should last for a minimum of 20 microseconds (WD1797 docs)
#define INDEX_HOLE_PULSE_US 100.0
// head load timing (this can be set from 30-100 ms, depending on drive)
// set to 45 ms (45,000 us)
#define HEAD_LOAD_TIMING_LIMIT 55.0*1000
// verify time is 30 milliseconds for a 1MHz clock
#define VERIFY_HEAD_SETTLING_LIMIT 30.0*1000
// E (15 ms delay) for TYPE II and III commands (30 ms (30*1000 us) for 1 MHz clock)
#define E_DELAY_LIMIT 30.0*1000

/* COUNTS */
// when non-busy status and HLD high, reset HLD after 15 index pulses
#define HLD_IDLE_INDEX_COUNT_LIMIT 15
/* number of bytes after ID field search encounters four 0x00 bytes. This
 	should be 16 bytes according to WD-1797 docs. After 16 bytes the search
	for the next ID field starts over. */
#define ID_FIELD_SEARCH_LIMIT 16
/* In Double Density Disks, if 43 bytes pass before Data AM is found, INTRQ */
#define DATA_AM_SEARCH_LIMIT 43

// DOS disk format (bytes per format section and byte written for each section)
#define GAP4A_LENGTH 80
#define GAP4A_BYTE 0x4E

#define SYNC_LENGTH 12
#define SYNC_BYTE 0x00

#define INDEX_AM_PREFIX_LENGTH 3
#define INDEX_AM_PREFIX_BYTE 0xC2
#define INDEX_AM_LENGTH 1
#define INDEX_AM_BYTE 0xFC

#define GAP1_LENGTH 50
#define GAP1_BYTE 0x4E

#define ID_AM_PREFIX_LENGTH 3
#define ID_AM_PREFIX_BYTE 0xA1
#define ID_AM_LENGTH 1
#define ID_AM_BYTE 0xFE

#define CYLINDER_LENGTH 1
#define HEAD_LENGTH 1
#define SECTOR_LENGTH 1
#define SECTOR_SIZE_LENGTH 1

#define CRC_LENGTH 2
#define CRC_BYTE 0x01	// PLACEHOLDER; CRC is not actually calulated or written

#define GAP2_LENGTH 22
#define GAP2_BYTE 0x4E

#define DATA_AM_PREFIX_LENGTH 3
#define DATA_AM_PREFIX_BYTE 0xA1
#define DATA_AM_LENGTH 1
#define DATA_AM_BYTE 0xFB

#define GAP3_LENGTH 54
#define GAP3_BYTE 0x4E

#define GAP4B_LENGTH 598
#define GAP4B_BYTE 0x4E

/* INTRQ (pin connected to secondary PIC IRQ0 in the Z100) is set to high at the
  completion of every command and when a force interrupt condition is met. It is
  reset (set to low) when the status register is read or when the commandRegister
  is loaded with a new command */
extern char* image_name_a;
extern char* image_name_b;

char* disk_content_array;
int header[512*3]={0xeb,0x1c,0x0,0x1,0x0,0x2,0x2,0x1,0x0,0x2,0x70,0x0,0x80,0x2,0x9,0x8,0xa,0x0,0x1,0x3,0x0,0x1,0x18,0xb0,0x0,0x0,0xa0,0xf,0x88,0x1c,0xe,0x1f,0xb8,0x0,0x4,0x8e,0xc0,0xb9,0x0,0x3c,0xbe,0x0,0x4,0x8b,0xfe,0xfc,0xf3,0xa4,0xea,0x35,0x4,0x0,0x4,0x8c,0xc8,0x8e,0xd8,0x33,0xdb,0x8e,0xc3,0x26,0x8e,0x6,0xfe,0x3,0x26,0x8a,0x1e,0x9,0x0,0x89,0x1e,0x17,0x4,0x26,0x8a,0x1e,0x5a,0x0,0x8,0x1e,0x16,0x4,0x8e,0xc0,0xb0,0x8,0xe6,0xfc,0x8b,0x1e,0x13,0x4,0x8a,0xe,0xe,0x4,0xd3,0xe3,0xf6,0x6,0x16,0x4,0x4,0x75,0x13,0x80,0x3e,0x12,0x4,0x2,0x7d,0xc,0xe8,0x5d,0x1,0xa8,0x8,0x74,0x5,0x80,0xe,0x15,0x4,0xc,0xb9,0xb,0x0,0x8d,0xb7,0x0,0x4,0xbf,0xeb,0x4,0xf3,0xa6,0x75,0x41,0x8b,0x87,0x1a,0x4,0x2d,0x2,0x0,0x8a,0xe,0x12,0x4,0xd3,0xe0,0x3,0x6,0x10,0x4,0xa3,0xf,0x5,0x8b,0x87,0x1c,0x4,0x8b,0xe,0x4,0x4,0x49,0x3,0xc1,0x8a,0xe,0xe,0x4,0xd3,0xe8,0xa3,0x11,0x5,0xbb,0xe,0x5,0xb0,0x2,0xe8,0x5b,0x0,0x72,0x16,0x8c,0xc8,0x5,0x40,0x0,0x8e,0xd8,0xbe,0x3,0x0,0xea,0x0,0x0,0x40,0x0,0xbb,0xf6,0x4,0xb5,0xd,0xeb,0x5,0xbb,0x1,0x5,0xb5,0xd,0x8a,0x7,0x53,0x51,0x9a,0x19,0x0,0x1,0xfe,0x59,0x5b,0x43,0xfe,0xcd,0x75,0xf0,0xeb,0xfe,0x49,0x4f,0x20,0x20,0x20,0x20,0x20,0x20,0x53,0x59,0x53,0xd,0xa,0x4e,0x6f,0x20,0x53,0x79,0x73,0x74,0x65,0x6d,0xd,0xa,0x49,0x2f,0x4f,0x20,0x65,0x72,0x72,0x6f,0x72,0xd,0xa,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x40,0x0,0x26,0x8b,0x47,0x1,0xa3,0xfc,0x5,0xa1,0xfc,0x5,0xf6,0x36,0xf,0x4,0xfe,0xc4,0x32,0xd2,0xf6,0x6,0x15,0x4,0x1,0x74,0x6,0xd0,0xe8,0x73,0x2,0xb2,0x2,0x88,0x16,0xfb,0x5,0x8a,0x2e,0x19,0x4,0xa2,0x19,0x4,0xe8,0xae,0x0,0xa0,0x1d,0x4,0xe8,0x7a,0x0,0xf6,0x6,0x15,0x4,0x4,0x74,0x11,0xa0,0x19,0x4,0xe8,0x9b,0x0,0x8a,0xc5,0xe8,0x8c,0x0,0xa0,0x1d,0x4,0xe8,0x62,0x0,0xe8,0x55,0x0,0x8a,0xc4,0xe8,0x83,0x0,0xa0,0x16,0x4,0xc,0x40,0xe8,0x6d,0x0,0x8b,0xe,0x4,0x4,0x6,0x26,0xc4,0x7f,0x5,0xfc,0xa0,0x1c,0x4,0xa,0x6,0xfb,0x5,0x9c,0xfa,0x8b,0x16,0x17,0x4,0x83,0xc2,0x3,0x52,0x8b,0x16,0x17,0x4,0xee,0x5a,0xec,0xaa,0xe2,0xfc,0xa0,0x16,0x4,0xe8,0x42,0x0,0xe8,0x27,0x0,0x9d,0x7,0x84,0xc0,0x74,0x2,0xf9,0xc3,0xff,0x6,0xfc,0x5,0x26,0x89,0x7f,0x5,0x26,0xff,0x4f,0x3,0x74,0x3,0xe9,0x65,0xff,0xc3,0x51,0x8b,0xe,0x1a,0x4,0x49,0x75,0xfd,0x59,0xc3,0xe8,0x1d,0x0,0xe8,0x8,0x0,0xa8,0x1,0x74,0xf9,0xe8,0x6,0x0,0xc3,0xba,0x5,0x0,0xeb,0x2,0x33,0xd2,0x3,0x16,0x17,0x4,0xec,0xc3,0xba,0x4,0x0,0xeb,0x11,0x33,0xd2,0xeb,0xd,0xba,0x1,0x0,0xeb,0x8,0xba,0x2,0x0,0xeb,0x3,0xba,0x3,0x0,0x3,0x16,0x17,0x4,0xee,0xc3,0x0,0x0,0x0,0xa5,0x5a,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xff,0xff,0xff,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0x0,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5,0xe5};

JWD1797* newJWD1797(e8259_t* pic, Z100* c) {
	JWD1797* jwd_controller = (JWD1797*)malloc(sizeof(JWD1797));
	jwd_controller->pic=pic;
	jwd_controller->z100=c;
	return jwd_controller;
}

void resetJWD1797(JWD1797* jwd_controller) {
	jwd_controller->dataShiftRegister = 0b00000000;
	jwd_controller->dataRegister = 0b00000000;
	jwd_controller->trackRegister = 0b00000000;
	jwd_controller->sectorRegister = 0b00000000;
	jwd_controller->commandRegister = 0b00000000;
	jwd_controller->statusRegister = 0b00000000;
	jwd_controller->CRCRegister = 0b00000000;
	jwd_controller->controlLatch = 0b00000000;
	jwd_controller->controlStatus = 0b00000000;

	jwd_controller->disk_img_index_pointer = 0;
	jwd_controller->rotational_byte_pointer = 2500;	// start at a few bytes before 0 index
	jwd_controller->rw_start_byte = 0;

	// jwd_controller->ready = 0;	// start drive not ready
	// jwd_controller->stepDirection = 0;	// start direction step out -> track 00

	jwd_controller->currentCommandName = "";
	jwd_controller->currentCommandType = 0;

	// TYPE I command bits
	jwd_controller->stepRate = 0;	// bits 0 and 1 determine the step rate
	jwd_controller->verifyFlag = 0;
	jwd_controller->headLoadFlag = 0;
	jwd_controller->trackUpdateFlag = 0;
	// TYPE II and III command bits
	jwd_controller->dataAddressMark = 0;
	jwd_controller->updateSSO = 0;
	jwd_controller->delay15ms = 0;
	jwd_controller->swapSectorLength = 0;
	jwd_controller->multipleRecords = 0;
	// TYPE IV command (forced interrupt) conditions
	jwd_controller->interruptNRtoR = 0;
	jwd_controller->interruptRtoNR = 0;
	jwd_controller->interruptIndexPulse = 0;
	jwd_controller->interruptImmediate = 0;
	// command step controls
	jwd_controller->command_action_done = 0;
	jwd_controller->command_done = 0;
	jwd_controller->head_settling_done = 0;
	jwd_controller->verify_operation_active = 0;
	jwd_controller->verify_operation_done = 0;
	jwd_controller->e_delay_done = 0;
	jwd_controller->start_byte_set = 0;

	jwd_controller->terminate_command = 0;

	jwd_controller->master_timer = 0.0;
	jwd_controller->index_pulse_timer = 0.0;
	jwd_controller->index_encounter_timer = 0.0;
	jwd_controller->step_timer = 0.0;
	jwd_controller->verify_head_settling_timer = 0.0;
	jwd_controller->e_delay_timer = 0.0;
	jwd_controller->assemble_data_byte_timer = 0.0;
	jwd_controller->rotational_byte_read_limit = 0; // NANOSECONDS
	jwd_controller->rotational_byte_read_timer = 0; // NANOSECONDS
	jwd_controller->rotational_byte_read_timer_OVR = 0; // NANOSECONDS
	jwd_controller->HLD_idle_reset_timer = 0.0;
	jwd_controller->HLT_timer = 0.0;
	jwd_controller->read_track_bytes_read = 0;

	jwd_controller->index_pulse_pin = 0;
	jwd_controller->ready_pin = 1;	// make drive ready immediately after reset
	jwd_controller->tg43_pin = 0;
	jwd_controller->HLD_pin = 0;
	jwd_controller->HLT_pin = 0;
	jwd_controller->not_track00_pin = 0;
	jwd_controller->direction_pin = 0;
	jwd_controller->sso_pin = 0;
	// jwd_controller-> test_not_pin;

	jwd_controller->delayed_HLD = 0;
	jwd_controller->HLT_timer_active = 0;
	jwd_controller->HLD_idle_index_count = 0;

	jwd_controller->drq = 0;
	jwd_controller->intrq = 0;
	jwd_controller->not_master_reset = 1;

	jwd_controller->current_track = 0;

	jwd_controller->cylinders = 0; // (tracks per side)
	jwd_controller->num_heads = 0;
	jwd_controller->sectors_per_track = 0;
	jwd_controller->sector_length = 0;

	jwd_controller->disk_img_file_size = 0;

	jwd_controller->formattedDiskArray = NULL;
	jwd_controller->actual_num_track_bytes = 0;

	jwd_controller->bytesToWrite=0;
	jwd_controller->sectorToWrite=(unsigned char*)malloc(1024);

	jwd_controller->new_byte_read_signal_ = 0;
	jwd_controller->track_start_signal_ = 0;

	jwd_controller->zero_byte_counter = 0;
	jwd_controller->a1_byte_counter = 0;
	jwd_controller->verify_index_count = 0;
	jwd_controller->address_mark_search_count = 0;
	jwd_controller->id_field_found = 0;
	jwd_controller->id_field_data_array_pt = 0;
	jwd_controller->id_field_data_collected = 0;
	jwd_controller->data_a1_byte_counter = 0;
	jwd_controller->data_mark_search_count = 0;
	jwd_controller->data_mark_found = 0;
	/* collects ID Field data
	  (0: cylinders, 1: head, 2: sector, 3: sector len, 4: CRC1, 5: CRC2)
		initialize all to 0x00 */
	for(int i = 0; i < 6; i++) {jwd_controller->id_field_data[i] = 0x00;}
	jwd_controller->ID_data_verified = 0;
	jwd_controller->intSectorLength = 0;
	jwd_controller->all_bytes_inputted = 0;
	jwd_controller->IDAM_byte_count = 0;
	jwd_controller->start_track_read_ = 0;

	// control latch initializations
	jwd_controller->wait_enabled = 0;

	// disk_content_array = diskImageToCharArray("z-dos-1.img", jwd_controller);
	// TEST disk image to array function
	// printByteArray(disk_content_array, 368640);

	/* make a formatted disk array from the disk data payload image file.
	 	will be held in jwd_controller->formattedDiskArray */
	//assembleFormattedDiskArray(jwd_controller, "Z_DOS_ver1.bin");
	assembleFormattedDiskArray(jwd_controller, image_name_a);

}

// this is a helper function to display numbers in binary format
void print_bin8_representation(unsigned char val) {
  for(int i=7; i>=0; i--)  {
    printf("%d",(val>>i)&1);
  }
  printf("\n");
}

void printByteArray(unsigned char *array, int size) {
  for (int i = 0; i < size; i++) {
      if (i > 0) printf(" ");
      printf("%02X", array[i]);
  }
  printf("\n");
}

// read data from wd1797 according to port
unsigned int readJWD1797(JWD1797* jwd_controller, unsigned int port_addr) {
	// printf("\nRead ");
	// printf("%s%X\n\n", " from wd1797/port: ", port_addr);

	unsigned int r_val = 0;

	switch(port_addr) {
		// status reg port
		case 0xb0:
			r_val = jwd_controller->statusRegister;
			// clear interrupt
			jwd_controller->intrq = 0;
			e8259_set_irq0 (jwd_controller->pic, 0);
			// clear all forced interrupt flags except INTERRUPT IMMEDIATE (0xD8)
			jwd_controller->interruptNRtoR = 0;
			jwd_controller->interruptRtoNR = 0;
			jwd_controller->interruptIndexPulse = 0;
			jwd_controller->terminate_command = 0;
			break;
		// track reg port
		case 0xb1:
			r_val = jwd_controller->trackRegister;
			break;
		// sector reg port
		case 0xb2:
			r_val = jwd_controller->sectorRegister;
			break;
		// data reg port
		case 0xb3:
			r_val = jwd_controller->dataRegister;
			/* if there is a byte waiting to be read from the data register
				(DRQ pin high) because of a READ operation */
			if((jwd_controller->currentCommandName == "READ SECTOR" ||
				jwd_controller->currentCommandName == "READ ADDRESS" ||
				jwd_controller->currentCommandName == "READ TRACK")
			 	&& jwd_controller->drq) {
				// reset data request line and status bit
				jwd_controller->drq = 0;
				jwd_controller->statusRegister &= 0b11111101;
			}
			break;
		// control latch reg port (write)
		case 0xb4:
			printf(" ** WARNING: Reading from WD1797 control latch port 0xB4 (write only)!\n");
			r_val = jwd_controller->controlLatch;
			break;
		// controller status port (read)
		case 0xb5:
			// printf("reading from WD1797 control status port 0xB5\n");
			r_val = jwd_controller->controlStatus;
			break;
		default:
			printf("%X is an invalid port!\n", port_addr);
	}
	return r_val;
}

// write data to wd1797 based on port address
void writeJWD1797(JWD1797* jwd_controller, unsigned int port_addr, unsigned int value) {
	// printf("\nWrite ");
	// print_bin8_representation(value);
	// printf("%s%X\n\n", " to wd1797/port: ", port_addr);
	switch(port_addr) {
		// command reg port
		case 0xb0:
			jwd_controller->commandRegister = value;
			// reset INTRQ when command register is written to - clear interrupt
			jwd_controller->intrq = 0;
			e8259_set_irq0 (jwd_controller->pic, 0);
			// clear all forced interrupt flags except INTERRUPT IMMEDIATE (0xD8)
			jwd_controller->interruptNRtoR = 0;
			jwd_controller->interruptRtoNR = 0;
			jwd_controller->interruptIndexPulse = 0;
			jwd_controller->terminate_command = 0;
			/* clear INTERRUPT IMMEDIATE flag ONLY if forced int 0xD0 command is
				received */
			if(value == 0xD0) {
				jwd_controller->interruptImmediate = 0;
			}
			doJWD1797Command(jwd_controller);
			break;
		// track reg port
		case 0xb1:
			jwd_controller->trackRegister = value;
			break;
		// sector reg port
		case 0xb2:
			jwd_controller->sectorRegister = value;
			break;
		// data reg port
		case 0xb3:
			jwd_controller->dataRegister = value;
			if(jwd_controller->currentCommandName=="WRITE SECTOR")
			{
//printf("MJB: writing %x to [%d] b3\n",value,jwd_controller->bytesToWrite);
//printf("\t%s %d\n",jwd_controller->currentCommandName,jwd_controller->drq);
				if(jwd_controller->bytesToWrite>=jwd_controller->sector_length)
//if(jwd_controller->bytesToWrite>=512)
					printf("MJB: OVERFLOW ERROR %d\n",jwd_controller->bytesToWrite);
				else
					jwd_controller->sectorToWrite[jwd_controller->bytesToWrite++]=value;
			}

			if((jwd_controller->currentCommandName == "WRITE SECTOR" ||
				jwd_controller->currentCommandName == "WRITE TRACK")
			 	&& jwd_controller->drq) {
				// reset data request line and status bit
				jwd_controller->drq = 0;
				jwd_controller->statusRegister &= 0b11111101;
			}
			break;
		// control latch port
		case 0xb4:
			printf("Writing to WD1797 control port 0xB4 (ONLY wait_enabled option)\n");
			jwd_controller->controlLatch = value;
			// set wait enabled option according to bit 6
			jwd_controller->wait_enabled = (jwd_controller->controlLatch >> 6) & 1;
			if(jwd_controller->wait_enabled) {
				printf("%s\n", "** FD-1797 Wait Enabled **");
			}
			break;
		// controller status port
		case 0xb5:
			printf(" ** WARNING: Writing to WD1797 status port 0xB5 (read only)!\n");
			break;
		default:
			printf("%X is an invalid port!\n", port_addr);
	}
}

/* main program will add the amount of calculated time from the previous
	instruction to the internal WD1797 timers */
void doJWD1797Cycle(JWD1797* w, double us) {
	w->master_timer += us;	// @@@ DEBUG clock @@@

	/* update status register bit 7 (NOT READY) based on inverted not_master_reset
		or'd with inverted ready_pin (ALL COMMANDS) */
	if(((!w->ready_pin | !w->not_master_reset)&1) == 0) {
		w->statusRegister &= 0b01111111; // reset NOT READY bit
	}
	else {w->statusRegister |= 0b10000000;} // set NOT READY bit

	// update not_track00_pin
	// check track and set not_track00_pin accordingly
	if(w->current_track == 0) {w->not_track00_pin = 0;}
	else {w->not_track00_pin = 1;}

	/* check if there is a forced intr 0xD0
		(NO INTRQ - terminate command immediately) */
	if(w->terminate_command) {
		// is there a command currently running?
		if(!w->command_done) {	// YES
			// terminate command
			w->command_done = 1;
			// reset BUSY status bit ONLY - other status bits are unchanged
			w->statusRegister &= 0b11111110;
		}
		else {									// NO command running
			/* reset busy status and clear SEEK ERROR and CRC ERROR bits
				(reflect TYPE I status) */
			w->statusRegister &= 0b11100110; // reset NOT READY bit
			// change command type to I in order to update TYPE I status bits
			w->currentCommandType = 1;
		}
	}

	/* check if there is a forced intr 0xD8 (INTRQ & terminate command immediately)
		(can be combined with other conditions) */
	if(w->interruptImmediate) {
		// is there a command currently running?
		if(!w->command_done) {	// YES
			// terminate command
			w->command_done = 1;
			// reset BUSY status bit ONLY - other status bits are unchanged
			w->statusRegister &= 0b11111110;
			// generate interrupt
			w->intrq = 1;
			e8259_set_irq0 (w->pic, 1);
		}
		else {	// NO command running
			/* reset busy status and clear SEEK ERROR and CRC ERROR bits
				(reflect TYPE I status) */
			w->statusRegister &= 0b11100110; // reset NOT READY bit
			// change command type to I in order to update TYPE I status bits
			w->currentCommandType = 1;
			// generate interrupt
			w->intrq = 1;
			e8259_set_irq0 (w->pic, 1);
		}
	}

	// reset new byte signal every WD1797 clock cycle
	w->new_byte_read_signal_ = 0;
	// clock the rotational byte timer using NANOSECONDS from mainBoard
	w->rotational_byte_read_timer += ((int)(us*1000.0));
	// is it time to advance to the next rotational byte?
	if(w->rotational_byte_read_timer >= w->rotational_byte_read_limit) {
		// calculate overage for incoming time from mainBoard.c
		w->rotational_byte_read_timer_OVR =
			w->rotational_byte_read_timer - w->rotational_byte_read_limit;
		// advance to next rotational byte (go to 0 if back to start of track)
		w->rotational_byte_pointer =
			(w->rotational_byte_pointer + 1) % w->actual_num_track_bytes;
		// when the first byte of the track is read, signal the start of the index pulse
		if(w->rotational_byte_pointer == 0) {
			w->track_start_signal_ = 1;
			// printf("%s\n", "Beginning of Track");
			// command execution idle - clock HLD idle index counter
			if((w->statusRegister & 1) == 0) {w->HLD_idle_index_count++;}
			else {w->HLD_idle_index_count = 0;}
			// clock verify timeout counter
			if(w->verify_operation_active) {w->verify_index_count++;}
			else {w->verify_index_count = 0;}
		}
		/* make new byte read signal (internal) go high. This signals that a new
			rotational byte has been encountered */
		w->new_byte_read_signal_ = 1;
		// reset timer to include overage
		w->rotational_byte_read_timer = w->rotational_byte_read_timer_OVR;
	}

	/* is it the start of a new track (rising edge of IP? = track_start_signal_)
		-- with a IP forced interrupt? */
	if(w->track_start_signal_ && w->interruptIndexPulse) {
		printf("%s\n", "IP interrupt condition met..");
		// is there a command currently running?
		if(!w->command_done) {	// YES
			// terminate command
			w->command_done = 1;
			// reset BUSY status bit ONLY - other status bits are unchanged
			w->statusRegister &= 0b11111110;
			// generate interrupt
			w->intrq = 1;
			e8259_set_irq0 (w->pic, 1);
		}
		else {	// NO command running
			/* reset busy status and clear SEEK ERROR and CRC ERROR bits
				(reflect TYPE I status) */
			w->statusRegister &= 0b11100110; // reset NOT READY bit
			// change command type to I in order to update TYPE I status bits
			w->currentCommandType = 1;
			// generate interrupt
			w->intrq = 1;
			e8259_set_irq0 (w->pic, 1);
		}
	}

	handleIndexPulse(w, us);

	handleHLTTimer(w, us);

	if(w->currentCommandType == 1) {
		// Type I status bit 5 (S5) will be set if HLD and HLT pins are high
		if(w->HLD_pin && w->HLT_pin) {w->statusRegister |= 0b00100000;}
		// clear TYPE I status bit 5 (S5) if HLD and HLT both not high
		else {w->statusRegister &= 0b11011111;}
		// update type I status bit 2 (S2) for track 00 status
		if(!w->not_track00_pin) {w->statusRegister |= 0b00000100;}
		// clear TYPE I status bit 2 (S2) if not on track 00
		else {w->statusRegister &= 0b11111011;}
	}

	/* update DATA REQUEST status bit (S1) for type II and III commands based on
		DRQ pin */
	if(w->currentCommandType == 2 || w->currentCommandType == 3) {
		// Type II/III status bit 1 (S1) will be set if DRQ is high
		if(w->drq) {w->statusRegister |= 0b00000010;}
		// ...and cleared if DRQ is low
		else {w->statusRegister &= 0b11111101;}
	}

	// check if command is still active and do command step if so...
	if(!w->command_done) {
		commandStep(w, us);
	}
	// HLD pin will reset if drive is not busy and 15 index pulses happen
	handleHLDIdle(w);

	/* update control status */
	updateControlStatus(w);
}

/* WD1797 accepts 11 different commands - this function will register the
	command and set all paramenters associated with it */
void doJWD1797Command(JWD1797* w) {
	// if the 4 high bits are 0b1101, the command is a force interrupt
	if(((w->commandRegister>>4) & 15) == 13) {setupForcedIntCommand(w); return;}

	// if not TYPE IV (forced interrupt), get busy status bit from status register (bit 0)
	int busy = w->statusRegister & 1;
	// check busy status
	if(busy) {printBusyMsg(); return;}	// do not run command if busy

	/* determine if command in command register is a TYPE I command by checking
		if the 7 bit is a zero (noly TYPE I commands have a zero (0) in the 7 bit) */
	if(((w->commandRegister>>7) & 1) == 0) {
		setupTypeICommand(w);
		setTypeICommand(w);
	}
	/* Determine if command in command register is TYPE II
		 by checking the highest 3 bits. The two TYPE II commands have either 0b100
		 (Read Sector) or 0b101 (Write Sector) as the high 3 bits
		 **NOTE: TYPE II commands assume that the target sector has been previously
		 loaded into the sector register */
	else if(((w->commandRegister>>5) & 7) < 6) {
		printf("TYPE II Command in WD1797 command register..\n");
		setupTypeIICommand(w);
		setTypeIICommand(w);

		w->bytesToWrite=0;
	}
	/* Determine if command in command register is TYPE III
		 by checking the highest 3 bits. TYPE III commands have a higher value
		 then 5 in their shifted 3 high bits */
	else if(((w->commandRegister>>5) & 7) > 5) {
		printf("TYPE III Command in WD1797 command register..\n");
		setupTypeIIICommand(w);
		setTypeIIICommand(w);
	}
	// check command register error
	else {
		printf("%s\n", "Something went wrong! BAD COMMAND BITS in COMMAND REG!");
	}
}

// execute command step if a command is active (not done)
// us is the time that passed since the last CPU instruction
void commandStep(JWD1797* w, double us) {
	/* do what needs to be done based on which command is still active and based
		on the timers */

	if(w->currentCommandType == 1) {
		// check if comand action is still ongoing...
		if(!w->command_action_done) {

			if(w->currentCommandName == "RESTORE") {
				// check TR00 pin (this pin is updated in doJWD1797Cycle)
				if(!w->not_track00_pin) {	// indicates r/w head is over track 00
					w->trackRegister = 0;
					w->command_action_done = 1;	// indicate end of command action
					printf("%s\n", "RESTORED HEAD TO TRACK 00 - command action DONE");
					return;
				}
				// not at track 00 - increment step timer
				else {
					w->step_timer += us;
					/* check step timer - has it completed one step according to the step rate?
						Step rates are in milliseconds (ms), so step rate must be multipled by 1000
						to change it to microseconds (us). */
					if(w->step_timer >= (w->stepRate*1000)) {
						w->direction_pin = 0;
						w->current_track--;
						if((w->controlLatch&0x3)<=1) stepclick();

						// step the disk image index down track bytes
						// w->disk_img_index_pointer -= (w->sector_length * w->sectors_per_track);
						// reset step timer
						w->step_timer = 0.0;
					}
				}
			}	// END RESTORE

			else if(w->currentCommandName == "SEEK") {
				/* check if track register == data register (SEEK command assumes that
					the data register contains the target track) */
				if(w->trackRegister == w->dataRegister) {	// SEEK found the target track
					w->command_action_done = 1;	// indicate end of command action
					printf("%s\n", "SEEK found target track - command action DONE");
					return;
				}
				else if(w->trackRegister > w->dataRegister) {	// must step out
					w->step_timer += us;
					if(w->step_timer >= (w->stepRate*1000)) {
						w->direction_pin = 0;
						w->current_track--;
						if((w->controlLatch&0x3)<=1) stepclick();

						// step the disk image index down track bytes
						// w->disk_img_index_pointer -= (w->sector_length * w->sectors_per_track);
						// update track register with current track
						w->trackRegister = w->current_track;
						// reset step timer
						w->step_timer = 0.0;
					}
				}
				else if(w->trackRegister < w->dataRegister) {	// must step in
					w->step_timer += us;
					if(w->step_timer >= (w->stepRate*1000)) {
						w->direction_pin = 1;
						w->current_track++;
						if((w->controlLatch&0x3)<=1) stepclick();

						// step the disk image index up track bytes
						// w->disk_img_index_pointer += (w->sector_length * w->sectors_per_track);
						// update track register with current track
						w->trackRegister = w->current_track;
						// reset step timer
						w->step_timer = 0.0;
					}
				}
			}	// END SEEK

			else if(w->currentCommandName == "STEP") {
				/* check if direction is step out with track already at TRACK 00
					(can not go to -1 track) */
				if(w->not_track00_pin == 0 && w->direction_pin == 0) {
					// update track register to 0 regardless of track update flag
					w->trackRegister = 0;
					w->command_action_done = 1;	// indicate end of command action
					printf("\n%s\n\n", "STEP - command action DONE (tried to step to track -1)");
					return;
				}
				// check if step would put head past the number of tracks on the disk
				else if((w->current_track == (w->cylinders - 1)) && w->direction_pin == 1) {
					w->command_action_done = 1;
					printf("\n%s\n\n", "STEP - command action DONE (tried to step past track limit)");
					return;
				}
				else {
					w->step_timer += us;
					/* check step timer - has it completed one step according to the step rate?
						Step rates are in milliseconds (ms), so step rate must be multipled by 1000
						to change it to microseconds (us). */
					if(w->step_timer >= (w->stepRate*1000)) {
						// step track according to direction_pin
						if(w->direction_pin == 0) {
							w->current_track--;
							if((w->controlLatch&0x3)<=1) stepclick();
						}
						else if(w->direction_pin == 1) {
							w->current_track++;
							if((w->controlLatch&0x3)<=1) stepclick();
						}
						// update track register if track update flag is high
						if(w->trackUpdateFlag) {w->trackRegister = w->current_track;}
						// reset step timer
						w->step_timer = 0.0;
						w->command_action_done = 1;	// indicate end of command action
						printf("%s\n", "STEP - command action DONE");
						return;
					}
				}
			}	// END STEP

			else if(w->currentCommandName == "STEP-IN") {
				if((w->current_track == (w->cylinders - 1))) {
					w->command_action_done = 1;
					printf("\n%s\n\n", "STEP-IN - command action DONE (tried to step past track limit)");
					return;
				}
				w->step_timer += us;
				/* check step timer - has it completed one step according to the step rate?
					Step rates are in milliseconds (ms), so step rate must be multipled by 1000
					to change it to microseconds (us). */
				if(w->step_timer >= (w->stepRate*1000)) {
					// step track according to direction_pin
					w->current_track++;
					// w->disk_img_index_pointer += (w->sector_length * w->sectors_per_track);
					// update track register if track update flag is high
					if(w->trackUpdateFlag) {w->trackRegister = w->current_track;}
					// reset step timer
					w->step_timer = 0.0;
					w->command_action_done = 1;	// indicate end of command action
					printf("%s\n", "STEP-IN - command action DONE");
					return;
				}
			}

			else if(w->currentCommandName == "STEP-OUT") {
				if(w->not_track00_pin == 0) {
					// update track register to 0 regardless of track update flag
					w->trackRegister = 0;
					w->command_action_done = 1;	// indicate end of command action
					printf("\n%s\n\n", "STEP-OUT - command action DONE (tried to step to track -1)");
					if((w->controlLatch&0x3)<=1) stepclick();
					return;
				}
				else {
					w->step_timer += us;
					/* check step timer - has it completed one step according to the step rate?
						Step rates are in milliseconds (ms), so step rate must be multipled by 1000
						to change it to microseconds (us). */
					if(w->step_timer >= (w->stepRate*1000)) {
						// step track according to direction_pin
						w->current_track--;
						if((w->controlLatch&0x3)<=1) stepclick();
						// w->disk_img_index_pointer -= (w->sector_length * w->sectors_per_track);
						// update track register if track update flag is high
						if(w->trackUpdateFlag) {w->trackRegister = w->current_track;}
						// reset step timer
						w->step_timer = 0.0;
						w->command_action_done = 1;	// indicate end of command action
						printf("%s\n", "STEP - command action DONE");
						return;
					}
				}
			} // END STEP-OUT

		}	// END command action section

		// ----------------------------------------------------

		/* after all steps are done (reached track 00 in the case of RESTORE)
			take care of post command varifications and delays */
		else if(w->command_action_done) {

			// take care of delayed HLD
			if(w->delayed_HLD && w->HLD_pin == 0) {
				w->HLT_timer_active = 1;
				w->HLT_timer = 0.0;
				w->HLD_pin = 1;
				// one shot from HLD pin resets HLT pin
				w->HLT_pin = 0;
				// w->HLD_idle_reset_timer = 0.0;
				// reset delayed HLD flag
				w->delayed_HLD = 0;
			}

			// if NO headload or yes headload and no verify
			if(!w->verifyFlag) {
				// no 30 ms verification delay and HLT is not sampled - command is done
				w->command_done = 1;
				w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
				// w->HLD_idle_reset_timer = 0.0;
				// generate interrupt
				w->intrq = 1;
				e8259_set_irq0 (w->pic, 1);
				printf("%s\n", "command type I complete");
				return;
			}

			// VERIFY still waiting on verify head settling...
			else if(w->verifyFlag) {
				typeIVerifySequence(w, us);
			}	// END VERIFY sequence
		}	// END verify/head settling phase

	}	// END TYPE I command

	else if(w->currentCommandType == 2) {
		// stall here until E delay clock has expired, if engaged
		if(handleEDelay(w, us) == 0) {return;}
		// sample HLT pin - do not continue with command if HLT pin has not engaged
		if(w->HLT_pin == 0) {return;}
		updateTG43Signal(w);
		// ID Address mark verification
		if(!w->ID_data_verified) {
			w->verify_operation_active = 1;
			typeIICmdIDVerify(w);
			// if ID data has not been verified, do not continue type II cmd
			return;
		}
		// verify op is done
		w->verify_operation_active = 0;

		// READ SECTOR
		if(w->currentCommandName == "READ SECTOR") {
			// ID address mark data is valid.. now look for Data Address mark (DATA AM)
			if(!w->data_mark_found) {
				if(w->new_byte_read_signal_) {
					dataAddressMarkSearch(w);
				}
				return;
			}


			// ?? after DATA AM found, put reacord type in status bit 5 ??

			// check if there is a new byte to read.. (ie. "assembled in DSR")
			if(w->data_mark_found && !w->all_bytes_inputted) {
				// is there a new byte in the DR
				if(w->new_byte_read_signal_) {
					/* did computer read the last data byte in the DR? If DRQ is still high,
						it did not; set lost data bit in status */
					if(w->drq == 1) {w->statusRegister |= 0b00000100;}
					// last byte was read (DRQ = 0) reset lost data bit
					else {w->statusRegister &= 0b11111011;}
					// read current byte into data register
					w->dataRegister = getFDiskByte(w);
					printf("%X ", w->dataRegister);
					// set drq and status drq status bit
					w->drq = 1;
					w->statusRegister |= 0b00000010;
					// decrement data field byte counter
					w->intSectorLength--;
					// have all bytes in data field been read?
					if(w->intSectorLength == 0) {w->all_bytes_inputted = 1;}
					return;
				}
				return;
			}

			// check CRC *** the next two bytes..
			//...

			// check multiple records flag
			if(w->multipleRecords) {
				w->sectorRegister++;
				// check if number of sectors have been exceeded
				if(w->sectorRegister > w->sectors_per_track) {
					// command is done
					w->command_done = 1;
					w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
					// w->HLD_idle_reset_timer = 0.0;
					// assume verification operation is successful - generate interrupt
					w->intrq = 1;
					e8259_set_irq0 (w->pic, 1);
					return;
				}
				// if sector number not out of bounds, find next sector
				else {
					w->verify_index_count = 0;
					w->ID_data_verified = 0;
					w->zero_byte_counter = 0;
					w->address_mark_search_count = 0;	/* after 16 bytes (MFM) */
					w->a1_byte_counter = 0;	// look for three 0xA1 bytes
					w->id_field_found = 0;
					w->id_field_data_array_pt = 0;
					w->id_field_data_collected = 0;
					w->data_a1_byte_counter = 0;	// counter for 0xA1 bytes for data field
					w->data_mark_search_count = 0;
					w->data_mark_found = 0;
					w->all_bytes_inputted = 0;
					return;
				}
				printf("%s\n", "ERROR: SOMETHING WENT WRONG WITH READING MULTIPLE SECTORS");
				return;
			}
			if((w->controlLatch&0x3)==0)
			{
				printf("DRIVE LIGHT A OFF\n");
				setLED(LED_DRIVE_A,0);
			}
			else if((w->controlLatch&0x3)==1)
			{
				printf("DRIVE LIGHT B OFF\n");
				setLED(LED_DRIVE_B,0);
			}
			// command is done
			w->command_done = 1;
			w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
			// w->HLD_idle_reset_timer = 0.0;
			// assume verification operation is successful - generate interrupt
			w->intrq = 1;
			e8259_set_irq0 (w->pic, 1);
			return;
		} // END READ SECTOR

		// WRITE SECTOR (*** NOT IMPLEMENTED - command completes without executing ***)
		else if(w->currentCommandName == "WRITE SECTOR") {
			printf("%s\n", "@@ ** WD-1797 WRITE SECTOR NOT IMPLEMENTED! ** @@");

					printf("WRITE-DATA %X ", w->dataRegister);
			writeSector(w);
			printAllRegisters(w);
			if((w->controlLatch&0x3)==0)
			{
				printf("DRIVE LIGHT A OFF\n");
				setLED(LED_DRIVE_A,0);
			}
			else if((w->controlLatch&0x3)==1)
			{
				printf("DRIVE LIGHT B OFF\n");
				setLED(LED_DRIVE_B,0);
			}

			// command is done
			w->command_done = 1;
			w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
			// w->HLD_idle_reset_timer = 0.0;
			// assume verification operation is successful - generate interrupt
			 w->intrq = 1;
			 e8259_set_irq0 (w->pic, 1);
			return;
		}	// END WRITE SECTOR

	} // END TYPE II command

	else if(w->currentCommandType == 3) {

		// do delay if E set and delay not done yet
		if(w->e_delay_done == 0 && w->delay15ms) {
			// clock the e delay timer
			w->e_delay_timer += us;
			// check if E delay timer has reached limit
			if(w->e_delay_timer >= E_DELAY_LIMIT) {
				w->e_delay_done = 1;
				w->e_delay_timer = 0.0;
			}
			return;	// delay still in progess - do not continue with command
		}
		// check HLT
		if(w->HLT_pin == 0) {return;}
		updateTG43Signal(w);

		if(w->currentCommandName == "READ ADDRESS") {
			/* if ID address mark has not been found yet, verify active so that
				index timeout count is incremented in doJWD1797Cycle() */
			if(!w->id_field_found) {
				w->verify_operation_active = 1;	// verify operation = IDAM detection
				// new byte available?
				if(w->new_byte_read_signal_) {
					// continue search for IDAM...
					IDAddressMarkSearch(w);
				}
				// check if index pass timed out..
				verifyIndexTimeout(w, 6);
				return;
			}
			else {w->verify_operation_active = 0;}

			// still collecting IDAM bytes.. new byte available?
			if(w->IDAM_byte_count < 6) {
				if(w->new_byte_read_signal_) {
					w->dataRegister = getFDiskByte(w);
					w->id_field_data[w->IDAM_byte_count] = w->dataRegister;
					w->drq = 1;
					w->statusRegister |= 0b00000010;
					w->IDAM_byte_count++;
					return;
				}
				return;
			}
			// tansfer track IDAM data byte to sector register
			w->sectorRegister = w->id_field_data[0];
			if(verifyCRC(w)) {
				return;
			}
			else {
				// command is done
				w->command_done = 1;
				w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
				// w->HLD_idle_reset_timer = 0.0;
				// assume verification operation is successful - generate interrupt
				w->intrq = 1;
				e8259_set_irq0 (w->pic, 1);
				// reset HLD idle timer
				return;
			}
		}

		else if(w->currentCommandName == "READ TRACK") {
			// is there an index pulse?
			if(w->index_pulse_pin) {
				w->start_track_read_ = 1;
			}
			// wait for index pulse
			if(!w->start_track_read_) {
				return;
			}
			// new byte available?
			if(w->new_byte_read_signal_) {
				/* is there an index pulse? Wait until after GAP 4a has passed (80 x 0x4E)
					before starting to look for another index pulse */
				if((w->read_track_bytes_read > 80) && (w->index_pulse_pin)) {
					// command is done
					w->command_done = 1;
					w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
					// w->HLD_idle_reset_timer = 0.0;
					// assume verification operation is successful - generate interrupt
					w->intrq = 1;
					e8259_set_irq0 (w->pic, 1);
					// reset HLD idle timer
					return;
				}
				/* did computer read the last data byte in the DR? If DRQ is still high,
					it did not; set lost data bit in status */
				if(w->drq == 1) {w->statusRegister |= 0b00000100;}
				// last byte was read (DRQ = 0) reset lost data bit
				else {w->statusRegister &= 0b11111011;}
				// read current byte into data register
				w->dataRegister = getFDiskByte(w);
				// read track takes up a new byte
				w->read_track_bytes_read++;
				// set drq and status drq status bit
				w->drq = 1;
				w->statusRegister |= 0b00000010;
				return;
			}
		}

		else if(w->currentCommandName == "WRITE TRACK") {
			printf("%s\n", "@@ ** WD-1797 WRITE TRACK NOT IMPLEMENTED! ** @@");

					printf("WRITE-TRACK %X ", w->dataRegister);

			// command is done
			w->command_done = 1;
			w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
			// w->HLD_idle_reset_timer = 0.0;
			// assume verification operation is successful - generate interrupt
			 w->intrq = 1;
			 e8259_set_irq0 (w->pic, 1);
			return;
		}
	} // END TYPE III command

}	// END general command step


/*
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
	+++++++++++++++++ HELPER FUNCTIONS ++++++++++++++++++++++++++++++++++++++++++
	+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
*/

void setupTypeICommand(JWD1797* w) {
	// printf("TYPE I Command in WD1797 command register..\n");
	w->currentCommandType = 1;
	w->command_action_done = 0;
	w->command_done = 0;
	w->head_settling_done = 0;
	w->step_timer = 0.0;
	w->verify_operation_active = 0;
	w->verify_index_count = 0;
	w->zero_byte_counter = 0;
	w->address_mark_search_count = 0;	/* after 16 bytes (MFM) */
	w->a1_byte_counter = 0;	// look for three 0xA1 bytes
	w->id_field_found = 0;
	w->id_field_data_array_pt = 0;
	w->id_field_data_collected = 0;
	// set appropriate status bits for type I command to start
	typeIStatusReset(w);
	// establish step rate options (in ms) for 1MHz clock (only used with TYPE I cmds)
	int rates[] = {6, 12, 20, 30};
	// get rate bits
	int rateBits = w->commandRegister & 3;
	// set flags according to command bits
	w->stepRate = rates[rateBits];
	w->verifyFlag = (w->commandRegister>>2) & 1;
	w->headLoadFlag = (w->commandRegister>>3) & 1;
	// HLD set according to V and h flags of type I command
	if(!w->headLoadFlag && !w->verifyFlag) {w->HLD_pin = 0;}
	else if(w->headLoadFlag && !w->verifyFlag && w->HLD_pin == 0) {
		w->HLT_timer_active = 1;
		w->HLT_timer = 0.0;
		w->HLD_pin = 1;
		// one shot from HLD pin resets HLT pin
		w->HLT_pin = 0;
		w->HLD_idle_reset_timer = 0.0;
	}
	else if(!w->headLoadFlag && w->verifyFlag) {w->delayed_HLD = 1;}
	else if(w->headLoadFlag && w->verifyFlag && w->HLD_pin == 0) {
		w->HLT_timer_active = 1;
		w->HLT_timer = 0.0;
		w->HLD_pin = 1;
		// one shot from HLD pin resets HLT pin
		w->HLT_pin = 0;
		w->HLD_idle_reset_timer = 0.0;
	}
	// initialize command type I timer
	// w->command_typeI_timer = 0.0;
	// add appropriate time based on V flag (1 MHz clock) 30,000 us
	// if(w->verifyFlag) {w->command_typeI_timer += 30*1000;}
}

void setupTypeIICommand(JWD1797* w) {
	// NOTE: assume Sector register has the target sector number
	w->currentCommandType = 2;
	w->command_done = 0;
	w->e_delay_done = 0;
	w->start_byte_set = 0;	// ??
	w->verify_operation_active = 0;
	w->verify_index_count = 0;
	w->ID_data_verified = 0;
	w->intSectorLength = 0;
	w->zero_byte_counter = 0;
	w->address_mark_search_count = 0;	/* after 16 bytes (MFM) */
	w->a1_byte_counter = 0;	// look for three 0xA1 bytes
	w->id_field_found = 0;
	w->id_field_data_array_pt = 0;
	w->id_field_data_collected = 0;
	w->data_a1_byte_counter = 0;	// counter for 0xA1 bytes for data field
	w->data_mark_search_count = 0;
	w->data_mark_found = 0;
	w->all_bytes_inputted = 0;

	// set busy status
	w->statusRegister |= 0b00000001;
	// reset DRQ line
	w->drq = 0;
	// reset drq/lost data/record not found/bits 5, 6 in status register
	w->statusRegister &= 0b10001001;
	// reset INT request line
	w->intrq = 0;
	e8259_set_irq0 (w->pic, 0);

	// sample READY input from DRIVE
	if(!w->ready_pin) {
		printf("\n%s\n\n", "DRIVE NOT READY! Command cancelled");
		w->command_done = 1;
		w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
		// ** generate interrupt **
		w->intrq = 1; // MUST SEND INTERRUPT to secondary int controller also...
		e8259_set_irq0 (w->pic, 1);
		return; // do not execute command
	}

	/* set TYPE II flags */
	w->updateSSO = (w->commandRegister>>1) & 1;	// U
	w->delay15ms = (w->commandRegister>>2) & 1;	// E
	w->swapSectorLength = (w->commandRegister>>3) & 1;	// L (1 for IBM format)
	w->multipleRecords = (w->commandRegister>>4) & 1; // m

	// update SSO (side select) line
	if(w->currentCommandType == 2 || w->currentCommandType == 3) {
		w->sso_pin = w->updateSSO? 1:0;
	}

	if(w->HLD_pin == 0) {
		// set HLD pin
		w->HLT_timer_active = 1;
		w->HLT_timer = 0.0;
		w->HLD_pin = 1;
		// one shot from HLD pin resets HLT pin
		w->HLT_pin = 0;
		w->HLD_idle_reset_timer = 0.0;
	}
	w->e_delay_timer = 0.0;
}

void setupTypeIIICommand(JWD1797* w) {
	w->currentCommandType = 3;
	w->command_done = 0;
	w->e_delay_done = 0;
	w->id_field_found = 0;
	// set busy status
	w->statusRegister |= 1;
	/* reset status bits 2 (lost data), 4 (record not found),
		5 (record type/write fault) */
	w->statusRegister &= 0b11001011;

	// sample READY input from DRIVE
	if(!w->ready_pin) {
		printf("\n%s\n\n", "DRIVE NOT READY! Command cancelled");
		w->command_done = 1;
		w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
		// ** generate interrupt **
		w->intrq = 1; // MUST SEND INTERRUPT to secondary int controller also...
		e8259_set_irq0 (w->pic, 1);
		return; // do not execute command
	}

	/* set TYPE III flags */
	w->updateSSO = (w->commandRegister>>1) & 1;
	w->sso_pin = w->updateSSO;
	w->delay15ms = (w->commandRegister>>2) & 1;

	if(w->HLD_pin == 0) {
		// set HLD pin
		w->HLT_timer_active = 1;
		w->HLT_timer = 0.0;
		w->HLD_pin = 1;
		// one shot from HLD pin resets HLT pin
		w->HLT_pin = 0;
		// w->HLD_idle_reset_timer = 0.0;
	}
	w->e_delay_timer = 0.0;
}

void setupForcedIntCommand(JWD1797* w) {
	/* %%%%%% DEBUG/TESTING - clear all interrupt flags here  */
	// w->interruptNRtoR = 0;
	// w->interruptRtoNR = 0;
	// w->interruptIndexPulse = 0;
	// w->interruptImmediate = 0;
	// %%%%%%% DEBUG ABOVE ^^^^
	printf("TYPE IV Command in WD1797 command register (Force Interrupt)..\n");
	// w->currentCommandType = 4;
	// w->currentCommandName = "FORCED INTR";
	/* get the interrupt condition bits (I0-I3) -
	the lowest 4 bits of the interrupt command */
	unsigned char int_condition_bits = w->commandRegister & 0b00001111;
	// set interrupt condition(s)
	if(int_condition_bits & 1) {
		printf("%s\n", "INTRQ on NOT READY to READY transition");
		w->interruptNRtoR = 1;
	}
	if((int_condition_bits>>1) & 1) {
		printf("%s\n", "INTRQ on READY to NOT READY transition");
		w->interruptRtoNR = 1;
	}
	if((int_condition_bits>>2) & 1) {
		printf("%s\n", "INTRQ on INDEX PULSE");
		w->interruptIndexPulse = 1;
	}
	if((int_condition_bits>>3) & 1) {
		printf("%s\n", "INTRQ and IMMEDIATE INTERRUPT");
		w->interruptImmediate = 1;
	}
	if(int_condition_bits == 0) {
		printf("%s\n", "NO INTRQ and TERMINATE COMMAND IMMEDIATELY");
		w->terminate_command = 1;
	}
}

void setTypeICommand(JWD1797* w) {
	// get 4 high bits of command register to determine the specific command
	int highBits = ((w->commandRegister>>4) & 15);	// examine 4 high bits

	if(highBits < 2) { // RESTORE or SEEK command
		if((highBits&1) == 0) {	// RESTORE command
			w->currentCommandName = "RESTORE";
			printf("%s command in WD1797 command register\n", w->currentCommandName);
		}
		else if((highBits&1) == 1) {	// SEEK command
			w->currentCommandName = "SEEK";
			printf("%s command in WD1797 command register\n", w->currentCommandName);
			// update Track Register with current track
			w->trackRegister = w->current_track;
		}
		// check error
		else {
			printf("%s\n", "Something went wrong! Cannot determine RESTORE or SEEK!");
		}
	}
	else { // STEP, STEP-IN or STEP-OUT commands
		// set track register update flag
		w->trackUpdateFlag = (w->commandRegister>>4) & 1;
		// determine which command by examining highest three bits of cmd reg
		int cmdID = (w->commandRegister>>5) & 7;
		if(cmdID == 1) {	// STEP
			w->currentCommandName = "STEP";
			printf("%s command in WD1797 command register\n", w->currentCommandName);
		}
		else if(cmdID == 2) {	//STEP-IN
			w->currentCommandName = "STEP-IN";
			w->direction_pin = 1;
			printf("%s command in WD1797 command register\n", w->currentCommandName);
		}
		else if(cmdID == 3)  {	// STEP-OUT
			w->currentCommandName = "STEP-OUT";
			w->direction_pin = 0;
			printf("%s command in WD1797 command register\n", w->currentCommandName);
		}
		// check error
		else {
			printf("%s\n", "Something went wrong! Cannot determine which TYPE I STEP command!");
		}
	}
}

void reloadDisk(JWD1797* w)
{
	if((w->controlLatch&0x3)==0)
		assembleFormattedDiskArray(w, image_name_a);
	if((w->controlLatch&0x3)==1)
		assembleFormattedDiskArray(w, image_name_b);
}

void setTypeIICommand(JWD1797* w) {
	// determine which command by examining highest three bits of cmd reg
	int cmdID = (w->commandRegister>>5) & 7;
	// check if READ SECTOR (high 3 bits == 0b100)
	if(cmdID == 4) {

		if((w->controlLatch&0x3)==0)
			assembleFormattedDiskArray(w, image_name_a);
		if((w->controlLatch&0x3)==1)
		{
			if(image_name_b!=NULL)
				assembleFormattedDiskArray(w, image_name_b);
		}

		w->currentCommandName = "READ SECTOR";
		printf("%s command in WD1797 command register\n", w->currentCommandName);

		if((w->controlLatch&0x3)==0)
		{
			printf("DRIVE LIGHT A ON\n");
			setLED(LED_DRIVE_A,1);
		}
		else if((w->controlLatch&0x3)==1)
		{
			printf("DRIVE LIGHT B ON\n");
			setLED(LED_DRIVE_B,1);
		}
	}
	else if(cmdID == 5) {
		w->currentCommandName = "WRITE SECTOR";
		
		printf("%s command in WD1797 command register\n", w->currentCommandName);
		if((w->controlLatch&0x3)==0)
		{
			printf("DRIVE LIGHT A ON\n");
			setLED(LED_DRIVE_A,1);
		}
		else if((w->controlLatch&0x3)==1)
		{
			printf("DRIVE LIGHT B ON\n");
			setLED(LED_DRIVE_B,1);
		}
		// set Data Address Mark flag
		w->dataAddressMark = w->commandRegister & 1;
	}
	// check error
	else {
		printf("%s\n", "Something went wrong! Cannot determine which TYPE II command!");
	}
}

void setTypeIIICommand(JWD1797* w) {
	// determine which command by examining highest 4 bits of cmd reg
	int cmdID = (w->commandRegister>>4) & 15;
	// READ ADDRESS
	if(cmdID == 12) {
		w->currentCommandName = "READ ADDRESS";
		printf("%s command in WD1797 command register\n", w->currentCommandName);
		w->IDAM_byte_count = 0;	// count to collect IDAM bytes
	}
	// READ TRACK
	else if(cmdID == 14) {
		w->currentCommandName = "READ TRACK";
		printf("%s command in WD1797 command register\n", w->currentCommandName);
		w->start_track_read_ = 0;
		w->read_track_bytes_read = 0;
		if((w->controlLatch&0x3)==0)
			assembleFormattedDiskArray(w, image_name_a);
		if((w->controlLatch&0x3)==1)
			assembleFormattedDiskArray(w, image_name_b);
	}
	// WRITE TRACK
	else if(cmdID == 15) {
		w->currentCommandName = "WRITE TRACK";
		printf("%s command in WD1797 command register\n", w->currentCommandName);
	}
	// check error
	else {
		printf("%s\n", "Something went wrong! Cannot determine which TYPE III command!");
	}
}

void typeIStatusReset(JWD1797* w) {
	// set BUSY bit
	w->statusRegister |= 0b00000001;
	// reset CRC ERROR, SEEK ERROR bits
	w->statusRegister &= 0b11100111;
	// reset DRQ, INTRQ pins
	w->drq = 0;
	// w->intrq = 0;
}

void printBusyMsg() {
	printf("%s\n", "Cannot execute command placed into command register!");
	printf("%s\n", "Another command is currently processing! (BUSY STATUS)");
}

void updateTG43Signal(JWD1797* w) {
	// update TG43 signal
	if(w->current_track > 43) {w->tg43_pin = 1;}
	else if(w->current_track <= 43) {w->tg43_pin = 0;}
}

/* helper function to compute CRC */
void computeCRC(int initialValue, int* bytes, int len, int* result) {
	unsigned short initial=(unsigned short)initialValue;
	unsigned short temp,a;
	unsigned short table[256];
	unsigned short poly=4129;

	for(int i=0; i<256; i++) {
		temp=0;
		a=(unsigned short)(i<<8);
		for(int j=0; j<8; j++) {
			if (((temp^a)&0x8000)!=0)
				temp=(unsigned short)((temp<<1)^poly);
			else
				temp<<=1;
		}
		table[i]=temp;
	}
	unsigned short crc=initial;
	for(int i=0; i<len; i++) {
		crc = (unsigned short)((crc<<8)^table[((crc>>8)^(0xff & bytes[i]))]);
	}
	result[0]=crc & 0xff;
	result[1]=(crc>>8)&0xff;
}

void printAllRegisters(JWD1797* w) {
	printf("\n%s\n", "WD1797 Registers:");
	printf("%s", "Status: ");
	print_bin8_representation(w->statusRegister);
	printf("\n%s", "Command: ");
	print_bin8_representation(w->commandRegister);
	printf("\n%s", "Sector: ");
	print_bin8_representation(w->sectorRegister);
	printf("\n%s", "Track: ");
	print_bin8_representation(w->trackRegister);
	printf("\n%s", "Data: ");
	print_bin8_representation(w->dataRegister);
	printf("\n%s", "DataShift: ");
	print_bin8_representation(w->dataShiftRegister);
	printf("\n%s", "CRC: ");
	print_bin8_representation(w->CRCRegister);
}

void printCommandFlags(JWD1797* w) {
	if(w->currentCommandType == 1) {
		printf("%s\n", "-- TYPE I COMMAND FLAGS --");
		printf("%s%d\n", "StepRate: ", w->stepRate);
		printf("%s%d\n", "Verify: ", w->verifyFlag);
		printf("%s%d\n", "HeadLoad: ", w->headLoadFlag);
		printf("%s%d\n", "TrackUpdate: ", w->trackUpdateFlag);
	}
	else if(w->currentCommandType == 2 || w->currentCommandType == 3) {
		printf("%s\n", "-- TYPE II/III COMMAND FLAGS --");
		printf("%s%d\n", "DataAddressMark: ", w->dataAddressMark);
		printf("%s%d\n", "UpdateSSO: ", w->updateSSO);
		printf("%s%d\n", "Delay15ms: ", w->delay15ms);
		printf("%s%d\n", "SwapSectorLength: ", w->swapSectorLength);
		printf("%s%d\n", "MultipleRecords: ", w->multipleRecords);
	}
	else if(w->currentCommandType == 4) {
		printf("%s\n", "-- TYPE IV COMMAND FLAGS/INTERRUPT CONDITIONS --");
		printf("%s%d\n", "InterruptNRtoR: ", w->interruptNRtoR);
		printf("%s%d\n", "InterruptRtoNR: ", w->interruptRtoNR);
		printf("%s%d\n", "InterruptIndexPulse: ", w->interruptIndexPulse);
		printf("%s%d\n", "InterruptImmediate: ", w->interruptImmediate);
	}
	else {
		printf("%s\n", "INVALID COMMAND TYPE!");
	}
}

void handleIndexPulse(JWD1797* w, double time) {
	// printf("%f\n", w->index_pulse_timer);
	// printf("%d\n", w->track_start_signal_);
	// beginning of track encountered and IP timer has not been set
	if(w->track_start_signal_ == 1) {
		w->track_start_signal_ = 0;
		w->index_pulse_pin = 1;
		w->index_pulse_timer = INDEX_HOLE_PULSE_US;
	}
	// only decrement index pulse timer if index pulse is high (1)
	if(w->index_pulse_pin) {
		w->index_pulse_timer -= time;
		// set IP status if TYPE I command is active
		if(w->currentCommandType == 1) {w->statusRegister |= 0b00000010;}
	}
	if(w->index_pulse_timer <= 0.0) {
		w->index_pulse_pin = 0;
		// clear IP status if TYPE I command is active
		if(w->currentCommandType == 1) {w->statusRegister &= 0b11111101;}
		// reset index pulse timer
	}
}

void handleHLDIdle(JWD1797* w) {
	// check to see if HLD must be reset because of idle
	if(w->HLD_idle_index_count >= HLD_IDLE_INDEX_COUNT_LIMIT) {
		w->HLD_pin = 0;
		w->HLD_idle_index_count = 0;
	}
	// if busy, make sure timer starts at 0.0 for start of next IDLE TIME count
	if(w->statusRegister & 1) {
		w->HLD_idle_index_count = 0;
	}
}

void handleHLTTimer(JWD1797* w, double time) {
	// clock HLT delay timer if active
	if(w->HLT_timer_active) {
		w->HLT_timer += time;
		// set HLT pin if timer expired
		if(w->HLT_timer >= HEAD_LOAD_TIMING_LIMIT) {
			w->HLT_pin = 1;
			// reset timer
			w->HLT_timer = 0.0;
			w->HLT_timer_active = 0;
		}
	}
}

void updateControlStatus(JWD1797* w) {
	// set INTRQ bit 0 and DRQ bit 7
	w->controlStatus = (w->intrq & 1) | ((0x01 & 1) << 1) | ((w->drq & 1) << 7);
}

int substring(char* a, int alen, char* s, int slen)
{
	for(int i=0; i<alen; i++)
	{
		int match=1;
		for(int j=0; j<slen; j++)
		{
			if(i+j>=alen)
			{
				match=0;
				break;
			}
			if(a[i+j]!=s[j])
			{
				match=0;
				break;
			}
		}
		if(match==1) return i;
	}
	return -1;
}

// http://www.cplusplus.com/reference/cstdio/fread/
unsigned char* diskImageToCharArray(char* fileName, JWD1797* w) {
	FILE* disk_img;
	unsigned long diskFileSize;
	size_t check_result;
	unsigned char* diskFileArray;
  	// open current file (disk in drive)
	if(fileName==NULL) return NULL;
  	disk_img = fopen(fileName, "rb");
	if(disk_img==NULL)
	{
		return NULL;
	}

	// obtain disk image file size in bytes
	fseek(disk_img, 0, SEEK_END);
	w->disk_img_file_size = ftell(disk_img);
	rewind(disk_img);
	// allocate memory to handle array for entire disk image
	diskFileArray = (unsigned char*) malloc(sizeof(char) * w->disk_img_file_size);
	/* copy disk image file into array buffer
		("check_result" variable makes sure all expected bytes are copied) */
	check_result = fread(diskFileArray, 1, w->disk_img_file_size, disk_img);
	if(check_result != w->disk_img_file_size) {
		printf("%s\n", "ERROR Converting disk image");
	}
	else {
		printf("\n%s\n", "disk image file converted to char array successfully!");
	}
	fclose(disk_img);

	patchOS(diskFileArray,w->disk_img_file_size);

	return diskFileArray;
}

/* establishes a char array (w->formattedDiskArray) that contains the (IBM)
	format bytes and the disk .img data bytes. The returned array will approximate
	the actual bytes on a 5.25" DS/DD (double side/double density) floppy disk */
void assembleFormattedDiskArray(JWD1797* w, char* fileName) {
	// first, get the payload byte data from the disk image file as an array
	unsigned char* sectorPayloadDataBytes = diskImageToCharArray(fileName, w);

	if(sectorPayloadDataBytes==NULL)
	{	sectorPayloadDataBytes = (unsigned char*) malloc(sizeof(char) * 327680);
		for(int i=0; i<327680; i++) sectorPayloadDataBytes[i]=0;
		for(int i=0; i<512*3; i++) sectorPayloadDataBytes[i]=header[i];
	}

	int l=strlen(fileName);
	if(fileName[l-4]=='.' && fileName[l-3]=='c' && fileName[l-2]=='p' && fileName[l-1]=='m')
	{
	w->num_heads=1;
	w->sectors_per_track=8;
	w->sector_length=0x800/4;
//	int total_sectors=20*8*4;
	int total_sectors=15*8*4;
	w->cylinders = total_sectors/w->sectors_per_track/w->num_heads;	// 0-39

	initcpmdisks(w->z100);
	}
	else
	{
	/* set disk attributes based on disk image file (For exmaple,
		40 tracks/9 sectors per track/512 bytes per sector for 360k z-dos disk)
		These are dynamically set according to the loader disk paramenter table.
		(page 10.18 - Z100 Technical Manual – Hardware) */
	w->num_heads = (sectorPayloadDataBytes[0x15]&1) + 1;	// 0-1
	printf("%s%d\n", "number of sides (heads): ", w->num_heads);
	w->sectors_per_track = sectorPayloadDataBytes[0xF];	// 1-9 (sectors start on 1)
	printf("%s%d\n", "sectors per track: ", w->sectors_per_track);
	w->sector_length = sectorPayloadDataBytes[0x4] | (sectorPayloadDataBytes[0x5]<<8);
	printf("%s%d\n", "sector length (bytes): ", w->sector_length);
	int total_sectors = sectorPayloadDataBytes[0xC] | (sectorPayloadDataBytes[0xD]<<8);
	printf("%s%d\n", "total number of sectors on disk: ", total_sectors);
	w->cylinders = total_sectors/w->sectors_per_track/w->num_heads;	// 0-39
	printf("%s%d\n", "cylinders (tracks per side): ", w->cylinders);
	}


	/* determine how many actual bytes (including format bytes) each track is
		This will be used for rotational byte pointing while the disk is spinning */
	w->actual_num_track_bytes = GAP4A_LENGTH + SYNC_LENGTH
		+ INDEX_AM_PREFIX_LENGTH + INDEX_AM_LENGTH + GAP1_LENGTH
		+ (w->sectors_per_track * (SYNC_LENGTH + ID_AM_PREFIX_LENGTH
		+ ID_AM_LENGTH + CYLINDER_LENGTH + HEAD_LENGTH + SECTOR_LENGTH
		+ SECTOR_SIZE_LENGTH + CRC_LENGTH + GAP2_LENGTH + SYNC_LENGTH
		+ DATA_AM_PREFIX_LENGTH + DATA_AM_LENGTH + w->sector_length
		+ CRC_LENGTH + GAP3_LENGTH)) + GAP4B_LENGTH;
	printf("%s%d\n", "Formatted bytes per track: ", w->actual_num_track_bytes);

	/* calculate byte rotation time in ns (for a 300 rpm disk, one rotation takes
		200,000,000 nanoseconds) */
	unsigned long raw_rotational_byte_read_limit =
		(unsigned long)(200000000/w->actual_num_track_bytes);
	/* the raw rotational byte read limit is moded by 200 because the smallest
		incoming time slice from the main Z-100 processor loop is 0.2 microseconds.
		This is because one cycle of the 5Mhz clock speed takes 0.2 microseconds. */
	w->rotational_byte_read_limit = raw_rotational_byte_read_limit -
		(raw_rotational_byte_read_limit%200);
	printf("%s%d\n", "rotational byte read limit (ns): ", w->rotational_byte_read_limit);

	// now, get the total amount of bytes for the entire formatted disk
	unsigned long formatted_disk_size = (w->cylinders * w->num_heads) * w->actual_num_track_bytes;

	unsigned char fDiskArray[formatted_disk_size];
	w->formattedDiskArray = fDiskArray;

	unsigned long formattedDiskIndexPointer = 0;
	unsigned long sectorPayloadArrayIndexPointer = 0;

	/* ** start making formatted disk array ** */

	// for each cylinder
	for(int cyl = 0; cyl < w->cylinders; cyl++) {

		// for each head
		for(int h = 0; h < w->num_heads; h++) {

			// write GAP4A
			for(int ct = 0; ct < GAP4A_LENGTH; ct++) {
				// write GAP4A_BYTE
				w->formattedDiskArray[formattedDiskIndexPointer] = GAP4A_BYTE;
				formattedDiskIndexPointer++;
			}
			// write SYNC
			for(int ct = 0; ct < SYNC_LENGTH; ct++) {
				// write GAP4A_BYTE
				w->formattedDiskArray[formattedDiskIndexPointer] = SYNC_BYTE;
				formattedDiskIndexPointer++;
			}
			// write IAM prefix
			for(int ct = 0; ct < INDEX_AM_PREFIX_LENGTH; ct++) {
				// write GAP4A_BYTE
				w->formattedDiskArray[formattedDiskIndexPointer] = INDEX_AM_PREFIX_BYTE;
				formattedDiskIndexPointer++;
			}
			// write IAM
			w->formattedDiskArray[formattedDiskIndexPointer] = INDEX_AM_BYTE;
			formattedDiskIndexPointer++;
			// write GAP1
			for(int ct = 0; ct < GAP1_LENGTH; ct++) {
				// write GAP1_BYTE
				w->formattedDiskArray[formattedDiskIndexPointer] = GAP1_BYTE;
				formattedDiskIndexPointer++;
			}

			// for each sector
			for(int s = 1; s < w->sectors_per_track + 1; s++) {
				// write SYNC
				for(int ct = 0; ct < SYNC_LENGTH; ct++) {
					// write SYNC
					w->formattedDiskArray[formattedDiskIndexPointer] = SYNC_BYTE;
					formattedDiskIndexPointer++;
				}
				// write IDAM prefix
				for(int ct = 0; ct < ID_AM_PREFIX_LENGTH; ct++) {
					// write IDAM prefix byte
					w->formattedDiskArray[formattedDiskIndexPointer] = ID_AM_PREFIX_BYTE;
					formattedDiskIndexPointer++;
				}
				// write IDAM byte
				w->formattedDiskArray[formattedDiskIndexPointer] = ID_AM_BYTE;
				formattedDiskIndexPointer++;
				// write cylinder byte (track)
				w->formattedDiskArray[formattedDiskIndexPointer] = cyl;
				formattedDiskIndexPointer++;
				// write head byte (side)
				w->formattedDiskArray[formattedDiskIndexPointer] = h;
				formattedDiskIndexPointer++;
				// write sector byte
				w->formattedDiskArray[formattedDiskIndexPointer] = s;
				formattedDiskIndexPointer++;
				// write sector length byte
				int s_length_byte = 0x00;
				switch (w->sector_length) {
					case 128:
						s_length_byte = 0x00;
						break;
					case 256:
						s_length_byte = 0x01;
						break;
					case 512:
						s_length_byte = 0x02;
						break;
					case 1024:
						s_length_byte = 0x03;
						break;
					case 2048:
						s_length_byte = 0x04;
						break;
					default:
						printf("%s\n", "ERROR: Non-standard sector length!");
				}
				w->formattedDiskArray[formattedDiskIndexPointer] = s_length_byte;
				formattedDiskIndexPointer++;
				// write 2 placeholder CRC bytes (0x01 X 2)
				for(int ct = 0; ct < CRC_LENGTH; ct++) {
					w->formattedDiskArray[formattedDiskIndexPointer] = CRC_BYTE;
					formattedDiskIndexPointer++;
				}
				// write GAP2
				for(int ct = 0; ct < GAP2_LENGTH; ct++) {
					// write GAP2_BYTE
					w->formattedDiskArray[formattedDiskIndexPointer] = GAP2_BYTE;
					formattedDiskIndexPointer++;
				}
				// write SYNC
				for(int ct = 0; ct < SYNC_LENGTH; ct++) {
					// write GAP4A_BYTE
					w->formattedDiskArray[formattedDiskIndexPointer] = SYNC_BYTE;
					formattedDiskIndexPointer++;
				}
				// write DATA AM prefix
				for(int ct = 0; ct < DATA_AM_PREFIX_LENGTH; ct++) {
					// write DATA AM prefix byte
					w->formattedDiskArray[formattedDiskIndexPointer] = DATA_AM_PREFIX_BYTE;
					formattedDiskIndexPointer++;
				}
				// write DATA AM byte
				w->formattedDiskArray[formattedDiskIndexPointer] = DATA_AM_BYTE;
				formattedDiskIndexPointer++;
				// write the data payload
				for(int ct = 0; ct < w->sector_length; ct++) {
					w->formattedDiskArray[formattedDiskIndexPointer] =
						sectorPayloadDataBytes[sectorPayloadArrayIndexPointer];
					formattedDiskIndexPointer++;
					sectorPayloadArrayIndexPointer++;
				}
				// write 2 placeholder CRC bytes (0x01 X 2)
				for(int ct = 0; ct < CRC_LENGTH; ct++) {
					w->formattedDiskArray[formattedDiskIndexPointer] = CRC_BYTE;
					formattedDiskIndexPointer++;
				}
				// write GAP3
				for(int ct = 0; ct < GAP3_LENGTH; ct++) {
					// write GAP3_BYTE
					w->formattedDiskArray[formattedDiskIndexPointer] = GAP3_BYTE;
					formattedDiskIndexPointer++;
				}

			}	// END SECTOR LOOP

			// write GAP 4B
			for(int ct = 0; ct < GAP4B_LENGTH; ct++) {
				// write GAP4B_BYTE
				w->formattedDiskArray[formattedDiskIndexPointer] = GAP4B_BYTE;
				formattedDiskIndexPointer++;
			}

		}	// END HEAD LOOP

	} // END CYLINDER LOOP

	/* * * DEBUG * * */
 	// printByteArray(w->formattedDiskArray, 1500);

	if(fileName[l-4]=='.' && fileName[l-3]=='c' && fileName[l-2]=='p' && fileName[l-1]=='m')
	{
		initcpmdisks(w->z100);
	}
}

/* returns the actual byte on the formatted disk (formatted disk array)
	based on the rotational byte position, actual track (w->current_track),
	and side select/head (w->sso_pin) */
unsigned char getFDiskByte(JWD1797* w) {
	// advance 2 tracks worth of formatted bytes for each cylinder (track)
	long r_byte_pt = w->current_track * (w->actual_num_track_bytes * 2);
	// what head (side)? Head == 1? Add 1 formatted track's worth of bytes
	r_byte_pt += w->actual_num_track_bytes * w->sso_pin;
	// now add where the head is located in the rotation
	r_byte_pt += w->rotational_byte_pointer;

	return w->formattedDiskArray[r_byte_pt];
}

void handleVerifyHeadSettleDelay(JWD1797* w, double us) {
	// if verify head settling has not occurred yet...
	if(!w->head_settling_done) {
		w->verify_head_settling_timer += us;
		// check if verify head settling is timed out
		if(w->verify_head_settling_timer >= VERIFY_HEAD_SETTLING_LIMIT) {
			// reset timer
			w->verify_head_settling_timer = 0.0;
			w->head_settling_done = 1;
		}
	}	// END verify head settling delay
}

/* returns 1 if verify sequnce times out after 5 index holes have passed while
 	looking for an ID address mark. Otherwise, retunrs 0. X paramenter is the
	limit that is checked. */
int verifyIndexTimeout(JWD1797* w, int x) {
	// check if X index holes have passed
	if(w->verify_index_count >= x) {
		printf("%s\n", "VERIFY INDEX TIMED OUT!");
		w->verify_operation_active = 0;
		// command is done
		w->command_done = 1;
		// reset (clear) busy status bit
		w->statusRegister &= 0b11111110;
		// set SEEK ERROR/RECORD NOT FOUND bit
		w->statusRegister |= 0b00010000;
		// w->HLD_idle_reset_timer = 0.0;
		// assume verification operation is successful - generate interrupt
		w->intrq = 1;
		e8259_set_irq0 (w->pic, 1);
		// reset HLD idle timer
		return 1;
	}
	return 0;
}

// returns 1 if ID address mark is found, otherwise returns 0
int IDAddressMarkSearch(JWD1797* w) {
	// get byte based on rotational byte and current track
	unsigned char incoming_byte = getFDiskByte(w);
	// look for four 0x00 bytes in a row (MFM)
	if(w->zero_byte_counter < 4) {
		if(incoming_byte == 0x00) {
			// printf("%s\n", "INCREMENTING 0x00 COUNT...");
			w->zero_byte_counter++;
		}
		else {w->zero_byte_counter = 0;}
		return 0;
	}
	// look for 3 0xA1 bytes in a row before 16 bytes pass (MFM)
	if(w->a1_byte_counter < ID_AM_PREFIX_LENGTH) {
		if(incoming_byte == 0xA1) {w->a1_byte_counter++; return 0;}
		else {	// not 0xA1
			w->a1_byte_counter = 0;
			w->address_mark_search_count++;
			// have 16 bytes passed without finding 3 consecutive 0xA1 (ID field)?
			if(w->address_mark_search_count >= ID_FIELD_SEARCH_LIMIT) {
					w->zero_byte_counter = 0;
					w->address_mark_search_count = 0;
					w->a1_byte_counter = 0;
					return 0;
			}
			return 0;
		}
		return 0;
	}
	// look for 0xFE - if so, IDAM has been found
	if(w->id_field_found == 0 && incoming_byte == 0xFE) {
		w->id_field_found = 1;
		return 1;
	}
	// 4 x 0x00, 3 x 0xA1, but no 0xFE - start search from the beginning..
	w->zero_byte_counter = 0;
	w->address_mark_search_count = 0;
	w->a1_byte_counter = 0;
	w->id_field_found = 0;
	return 0;
}

/* collect ID field data into w->id_field_data[6]. Returns 1 when complete,
 	returns 0 until [TRACK, HEAD, SECTOR, LENGTH, CRC1, CRC2] collection is
	complete */
int collectIDFieldData(JWD1797* w) {
	// collect bytes into array
	if(w->id_field_data_array_pt < 6) {
		w->id_field_data[w->id_field_data_array_pt] = getFDiskByte(w);
		w->id_field_data_array_pt++;
		return 0;
	}
	w->id_field_data_collected = 1;
	return 1;
}

/* this function will check the track ID against w->trackRegister after the ID
	field has been collected in a verify operation - will return 1 if matched,
	0 otherwise. */
int verifyTrackID(JWD1797* w) {
	if(w->trackRegister == w->id_field_data[0]) {
		printf("\n%s\n\n", "TRACK VERIFIED!!");
		return 1;
	}
	// track ID field != track register - keep searching
	else {
		w->zero_byte_counter = 0;
		w->address_mark_search_count = 0;
		w->a1_byte_counter = 0;
		w->id_field_found = 0;
		w->id_field_data_collected = 0;
		w->id_field_data_array_pt = 0;
		return 0;
	}
}

/* this function will check the sector ID against w->sectorRegister after the ID
	field has been collected in a verify operation - will return 1 if matched,
	0 otherwise. */
int verifySectorID(JWD1797* w) {
	if(w->sectorRegister == w->id_field_data[2]) {
		printf("\n%s\n\n", "SECTOR VERIFIED!!");
		return 1;
	}
	else {
		w->zero_byte_counter = 0;
		w->address_mark_search_count = 0;
		w->a1_byte_counter = 0;
		w->id_field_found = 0;
		w->id_field_data_collected = 0;
		w->id_field_data_array_pt = 0;
		return 0;
	}
}

/* this function will check the head ID against w->sso_pin after the ID
	field has been collected in a verify operation - will return 1 if matched,
	0 otherwise. */
int verifyHeadID(JWD1797* w) {
	if(w->updateSSO == w->id_field_data[1]) {
		printf("\n%s\n\n", "HEAD/SIDE VERIFIED!!");
		return 1;
	}
	else {
		w->zero_byte_counter = 0;
		w->address_mark_search_count = 0;
		w->a1_byte_counter = 0;
		w->id_field_found = 0;
		w->id_field_data_collected = 0;
		w->id_field_data_array_pt = 0;
		return 0;
	}
}

/* this function is a stand-in for the CRC check. CRC is not actually
 	computed (yet). The CRC bytes are always set to 0x01 when creating the
	formatted disk array. Thus, this function simply checks if the last
	two bytes in the collected ID Address Data array, which respresent the
	CRC bytes, are 0x01. */
int verifyCRC(JWD1797* w) {
	// do the two CRC bytes equal the TEMP values of 0x01? (TEMP!!)
	if(w->id_field_data[4] == 0x01 && w->id_field_data[5] == 0x01) {
		printf("\n%s\n\n", "CRC VERIFIED!!");
		// reset CRC error status
		w->statusRegister &= 0b11110111;
		w->verify_operation_active = 0;
		// command is done
		w->command_done = 1;
		w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
		// w->HLD_idle_reset_timer = 0.0;
		// assume verification operation is successful - generate interrupt
		w->intrq = 1;
		e8259_set_irq0 (w->pic, 1);
		// reset HLD idle timer
		return 1;
	}
	else {
		w->zero_byte_counter = 0;
		w->address_mark_search_count = 0;
		w->a1_byte_counter = 0;
		w->id_field_found = 0;
		w->id_field_data_collected = 0;
		w->id_field_data_array_pt = 0;
		// set CRC error in TYPE I status
		w->statusRegister |= 0b00001000;
		return 0;
	}
}

int verifyCRCTypeII(JWD1797* w) {
	// do the two CRC bytes equal the TEMP values of 0x01? (TEMP!!)
	if(w->id_field_data[4] == 0x01 && w->id_field_data[5] == 0x01) {
		printf("\n%s\n\n", "CRC VERIFIED!!");
		// reset CRC error status
		w->statusRegister &= 0b11110111;
		w->verify_operation_active = 0;
		return 1;
	}
	else {
		w->zero_byte_counter = 0;
		w->address_mark_search_count = 0;
		w->a1_byte_counter = 0;
		w->id_field_found = 0;
		w->id_field_data_collected = 0;
		w->id_field_data_array_pt = 0;
		// set CRC error in TYPE I status
		w->statusRegister |= 0b00001000;
		return 0;
	}
}

/* verify delay timer, wait for HLT, index hole timout check,
	search for ID field, track ID/track register compare, CRC check */
void typeIVerifySequence(JWD1797* w, double us) {
	// if verify delay (30ms - 1 MHz clock), w->head_settling_done = 1
	handleVerifyHeadSettleDelay(w, us);
	// head settling time is done. Wait for HLT pin to go high if not already.
	if(w->head_settling_done) {
		// is HLT pin high?
		if(w->HLT_pin) {
			// (head settling time has passed and the HLT pin is high)
			// do verification operation...
			w->verify_operation_active = 1;
			// check if 5 index holes have passed
			if(verifyIndexTimeout(w, 5)) {return;}
			// new byte available to read
			if(w->new_byte_read_signal_) {
				// search for ID Address field if not already found
				if(!w->id_field_found) {
					// if still searching.. return
					IDAddressMarkSearch(w); return;
				}
				/* collect ID field data when ID field found and not already
					collected */
				if(!w->id_field_data_collected) {collectIDFieldData(w); return;}
			}	// END READ new byte
			if(w->id_field_data_collected) {
				/* check track register against track ID data -
					if 1 is returned - track verified, continue to CRC checks
					0 returned - track not verified - start search over */
				if(verifyTrackID(w)) {verifyCRC(w);}
				/* check CRC bytes (CRC circuitry NOT implemented yet)
					CRC bytes are temporerily set to 0x01 in the formatted disk array */
			}
		}	// END verify operation
	}	// END verify head settling (30ms - 1MHz)
}

/* handles the E delay timer for type II and III commands -- returns 0 if
	clock is still active, 1 if delay clock has expired */
int handleEDelay(JWD1797* w, double us) {
	// do delay if E set and delay not done yet
	if(w->e_delay_done == 0 && w->delay15ms) {
		// clock the e delay timer
		w->e_delay_timer += us;
		// check if E delay timer has reached limit
		if(w->e_delay_timer >= E_DELAY_LIMIT) {
			w->e_delay_done = 1;
			w->e_delay_timer = 0.0;
			return 1;	// delay clock expired
		}
		return 0;	// delay still in progess - do not continue with command
	}
	return 1;	// delay option not active/delay clock expired
}

/* this function verifies that an ID address mark has been found, the track
 	ID matched the TR, the sector ID matches the SR, the head ID matched the
	side select, saves the sector length, and checks the CRC bytes.
	Returns 0 if the process times out because of an idex time out or the verify
	operation was not successful. Returns 1 if verified.
	The ID_data_verified flag is used to signal that the verification process has
	been successful. */
int typeIICmdIDVerify(JWD1797* w) {
	/* check if 5 index holes have passed -
		if 1 is returned, process timed out, 0 -> continue with verification */
	if(verifyIndexTimeout(w, 5)) {return 0;}
	// new byte available to read
	if(w->new_byte_read_signal_) {
		// search for ID mark
		if(!w->id_field_found) {
			IDAddressMarkSearch(w);
			return 0;
		}
		if(!w->id_field_data_collected) {
			collectIDFieldData(w);
			return 0;
		}
	}
	if(w->id_field_data_collected) {
		printf("id_field_data ");
		printByteArray(w->id_field_data, 6);
		int track_verified = verifyTrackID(w);
		if(!track_verified) {printf("TRACK NOT VERIFIED\n");
			printf("Track reg %d\n",w->trackRegister);
return 0;
		}
		int sector_verified = verifySectorID(w);
		if(!sector_verified) {printf("SECTOR NOT VERIFIED\n"); return 0;}
		int head_verified = verifyHeadID(w);
		if(!head_verified) {printf("HEAD NOT VERIFIED\n"); 
return 0;
		}
		// extract sector length from ID Field
		w->intSectorLength = getSectorLengthFromID(w);
		// printf("%d\n", w->intSectorLength);
		if(!verifyCRCTypeII(w)) 
		{
			printf("CRC NOT VERIFIED\n");
			return 0;
		}
		// ID data is valid..
		w->ID_data_verified = 1;
		return 1;
	}
}

/* this function reads the sector length field of the IDAM data and extracts
	the actual integer sector length */
int getSectorLengthFromID(JWD1797* w) {
	switch (w->id_field_data[3]) {
		case 0x00:
			return 128;
			break;
		case 0x01:
			return 256;
			break;
		case 0x02:
			return 512;
			break;
		case 0x03:
			return 1024;
			break;
		case 0x04:
			return 2048;
			break;
		default:
			printf("%s\n", "ERROR: Non-standard sector length!");
	}
}

// returns 1 if ID address mark is found, otherwise returns 0
int dataAddressMarkSearch(JWD1797* w) {
	// get byte based on rotational byte and current track
	unsigned char incoming_byte = getFDiskByte(w);
	// look for 3 0xA1 bytes in a row
	if(w->data_a1_byte_counter < DATA_AM_PREFIX_LENGTH) {
		if(incoming_byte == 0xA1) {w->data_a1_byte_counter++; return 0;}
		else {	// not 0xA1
			w->data_a1_byte_counter = 0;
			w->data_mark_search_count++;
			// have 43 bytes passed without finding 3 consecutive 0xA1 and 0xFB (DATA AM)?
			if(w->data_mark_search_count >= DATA_AM_SEARCH_LIMIT) {
					// interrupt and terminate command..
					w->command_done = 1;
					w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
					w->statusRegister |= 0b00010000;	// set record-not found bit
					// ** generate interrupt **
					w->intrq = 1; // MUST SEND INTERRUPT to secondary int controller also...
					e8259_set_irq0 (w->pic, 1);
					return 0;
			}
			return 0;
		}
		return 0;
	}
	// look for 0xFB - if so, IDAM has been found
	if(w->data_mark_found == 0 && incoming_byte == 0xFB) {
		w->data_mark_found = 1;
		return 1;
	}
	// the 3 0xA1 bytes were not followed by 0xFB - DATA AM not found
	else {
		w->data_a1_byte_counter = 0;
		w->data_mark_search_count++;
		// have 43 bytes passed without finding 3 consecutive 0xA1 and 0xFB (DATA AM)?
		if(w->data_mark_search_count >= DATA_AM_SEARCH_LIMIT) {
				// interrupt and terminate command..
				w->command_done = 1;
				w->statusRegister &= 0b11111110;	// reset (clear) busy status bit
				w->statusRegister |= 0b00010000;	// set record-not found bit
				// ** generate interrupt **
				w->intrq = 1; // MUST SEND INTERRUPT to secondary int controller also...
				e8259_set_irq0 (w->pic, 1);
				return 0;
		}
		return 0;
	}
	return 0;
}

void writeSector(JWD1797* w)
{
	char* name;
	if((w->controlLatch&0x3)==0)
		name=image_name_a;
	if((w->controlLatch&0x3)==1)
		name=image_name_b;
	if(name==NULL) return;

	if(name[0]=='_')
	{
		printf("\nNot writing sector: disk %s is write protected\n",name);
		return;
	}
	printf("\nPreparing to write sector to disk %s\n",name);

	printf("Sector number = %d, head = %d, track = %d\n",w->id_field_data[2],w->id_field_data[1],w->id_field_data[0]);
	printf("cylinders %d heads %d s/t %d slength %d\n",w->cylinders,w->num_heads,w->sectors_per_track,w->sector_length);
	unsigned int offset = w->id_field_data[0]*w->num_heads*w->sectors_per_track*w->sector_length;
	offset+=w->id_field_data[1]*w->sectors_per_track*w->sector_length;
	offset+=(w->id_field_data[2]-1)*w->sector_length;
	printf("Offset = %x\n",offset);
	printf("Data: ");
	int i;
	for(i=0; i<w->sector_length; i++)
	{
		printf("%x ",w->sectorToWrite[i]);
	}
	printf("\n");
	printf("saving to image %s\n",name);
	FILE* f;
	f=fopen(name,"r+b");
	if(f==NULL)
	{
		f=fopen(name,"wb");
		for(int i=0; i<512*3; i++)
			fputc(header[i],f);
		for(int i=0; i<327680-512*3; i++)
			fputc(0,f);
		fclose(f);
		f=fopen(name,"r+b");
		printf("created image file %s\n",name);
	}
	fseek(f,offset,SEEK_SET);
	for(i=0; i<w->sector_length; i++)
		fputc(w->sectorToWrite[i],f);
	fclose(f);

	printf("reloading disk %s\n",name);
	assembleFormattedDiskArray(w, name);
	printf("\n\n");
}

int pr8085_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num)
{
	// if 8085 "in" instruction and reading from WD1797 data register (port 0xB3)
	if((opCode == 0xdb) && (port_num == 0xb3))
	{
		return 1;
	}
	return 0;
}

int pr8088_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num)
{
	// if 8088 "in" instruction and reading from WD1797 data register (port 0xB3)
	if(((opCode == 0xe4) || (opCode == 0xe5) || (opCode == 0xec) || (opCode == 0xed)) && (port_num == 0xb3)) 
	{
		return 1;
	}
	return 0;
}

void fD1797DebugOutput(JWD1797* jwd1797) {
	// DEBUG FD-1797 Floppy Disk Controller
	printf("%s%lu\n", "JWD1797 ROTATIONAL BYTE POINTER: ",
		jwd1797->rotational_byte_pointer);
	printf("%s%d\n", "JWD1797 ROTATIONAL BYTE TIMER (ns): ",
		jwd1797->rotational_byte_read_timer);
	printf("%s%d\n", "JWD1797 ROTATIONAL BYTE TIMER OVR (ns): ",
		jwd1797->rotational_byte_read_timer_OVR);
	printf("%s%02X\n", "Current Byte: ", getFDiskByte(jwd1797));
	printf("%s%f\n", "HEAD LOAD Timer: ", jwd1797->HLT_timer);
	printf("%s%f\n", "E Delay Timer: ", jwd1797->e_delay_timer);
	printf("%s", "FD-1797 Status Reg.: " );
	print_bin8_representation(jwd1797->statusRegister);
	printf("%s", "Disk ID Field Data: " );
	printByteArray(jwd1797->id_field_data, 6);
	printf("%s", "data a1 byte count: ");
  printf("%d\n", jwd1797->data_a1_byte_counter);
  printf("%s", "data AM search count: ");
  printf("%d\n", jwd1797->data_mark_search_count);
  printf("%s", "Data AM found: ");
  printf("%d\n", jwd1797->data_mark_found);
  printf("%s", "Sector length count: ");
  printf("%d\n", jwd1797->intSectorLength);
  printf("%s%d\n", "DRQ: ", jwd1797->drq);
  printf("%s%02X\n", "DATA REGISTER: ", jwd1797->dataRegister);
  printf("%s", "SECTOR REGISTER: ");
  print_bin8_representation(jwd1797->sectorRegister);
  printf("%s", "");
  printf("%s", "TYPE II STATUS REGISTER: ");
  print_bin8_representation(jwd1797->statusRegister);
  printf("%s\n", "");
}

//OS patches
void patchOS(char* diskFileArray,int size)
{
	//check if Z-DOS v1.02 boot disk: look for EB 1C at 0, then 49 4F at 600 (IO.SYS), then 30 32 at 1490 (1.02)
	if((diskFileArray[0]&0xff)==0xeb && (diskFileArray[1]&0xff)==0x1c && (diskFileArray[0x600]&0xff)==0x49 && (diskFileArray[0x601]&0xff)==0x4f && (diskFileArray[0x1490]&0xff)==0x30 && (diskFileArray[0x1491]&0xff)==0x32)
	{
		//disable the timer 2 loop that hangs up Z-DOS
		diskFileArray[0x22d6]=0;
		diskFileArray[0x22d7]=0;
	}
	//check if Z-DOS v1.10 boot disk: look for EB 1C at 0, then 49 4F at 600 (IO.SYS), then 31 30 at 1490 (1.10)
	else if((diskFileArray[0]&0xff)==0xeb && (diskFileArray[1]&0xff)==0x1c && (diskFileArray[0x600]&0xff)==0x49 && (diskFileArray[0x601]&0xff)==0x4f && (diskFileArray[0x1490]&0xff)==0x31 && (diskFileArray[0x1491]&0xff)==0x30)
	{
		//disable the timer 2 loop that hangs up Z-DOS
		diskFileArray[0x264c]=0;
		diskFileArray[0x264d]=0;
	}
	//check if Z-DOS v2.18 boot disk: look for EB 1f 00 02 at 0, then 49 4F at 600 (IO.SYS), then 32 32 (2.22) at a361 a362
	else if((diskFileArray[0]&0xff)==0xeb && (diskFileArray[1]&0xff)==0x1f && (diskFileArray[2]&0xff)==0x00 && (diskFileArray[3]&0xff)==0x02 && (diskFileArray[0x600]&0xff)==0x49 && (diskFileArray[0x601]&0xff)==0x4f && (diskFileArray[0xa361]&0xff)==0x31 && (diskFileArray[0xa362]&0xff)==0x38)
	{
		//disable the timer 2 loop that hangs up Z-DOS
		diskFileArray[0x2003]=0;
		diskFileArray[0x2004]=0;
	}
	//check if Z-DOS v2.22 boot disk: look for EB 1f 00 02 at 0, then 49 4F at 600 (IO.SYS), then 32 32 (2.22) at a361 a362
	else if((diskFileArray[0]&0xff)==0xeb && (diskFileArray[1]&0xff)==0x1f && (diskFileArray[2]&0xff)==0x00 && (diskFileArray[3]&0xff)==0x02 && (diskFileArray[0x600]&0xff)==0x49 && (diskFileArray[0x601]&0xff)==0x4f && (diskFileArray[0xa361]&0xff)==0x32 && (diskFileArray[0xa362]&0xff)==0x32)
	{
		//disable the timer 2 loop that hangs up Z-DOS
		diskFileArray[0x2004]=0;
		diskFileArray[0x2005]=0;
	}
	//check if Z-DOS v3.10 boot disk: look for EB 1f 00 03 at 0, then 49 4F at 600 (IO.SYS)
	else if((diskFileArray[0]&0xff)==0xeb && (diskFileArray[1]&0xff)==0x1f && (diskFileArray[2]&0xff)==0x00 && (diskFileArray[3]&0xff)==0x03 && (diskFileArray[0x600]&0xff)==0x49 && (diskFileArray[0x601]&0xff)==0x4f)
	{
		//disable the timer 2 loop that hangs up Z-DOS
		diskFileArray[0x20ab]=0;
		diskFileArray[0x20ac]=0;
	}

	//search for ZBASIC code sequence:
		//this occurs at 0724:AA69 in memory
	unsigned char zbasic[16]={0x74,0x0c,0x5f,0x98,0x2b,0xf8,0x57,0x8b,0xd0,0xe8,0x22,0xec,0xeb,0xe8,0xbb,0x1c};
	int zloc=substring(diskFileArray,size,zbasic,16);
	if(zloc!=-1)
	{
		//patch 74(jz) to 75(jnz) to allow GOTOs to print out
		diskFileArray[zloc]=0x75;
	}
}
