//Margaret Black
//2020-2024

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


#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

#include "mainboard.h"

void scheduleTrap(int,int);
pthread_mutex_t fifo_mutex;

void doclick()
{
	printf("key click\n");
	beep(10);
}

void dobeep()
{
	printf("beep\n");
	beep(200);
	// ring system bell with "BELL" character
	// printf("\a");
}


void keyboardCommandWrite(Keyboard* k, unsigned int command)
{
	switch(command)
	{
		case 0x00:
			keyboardReset(k);
			break;
		case 0x01:
			k->autoRepeatOn=1;
			break;
		case 0x02:
			k->autoRepeatOn=0;
			break;
		case 0x03:
			k->keyClickOn=1;
			break;
		case 0x04:
			k->keyClickOn=0;
			break;
		case 0x05:
			k->fifoHead=k->fifoTail=0;
			break;
		case 0x06:
			doclick();
			break;
		case 0x07:
			dobeep();
			break;
		case 0x08:
			k->keyboardEnabled=1;
			break;
		case 0x09:
			k->keyboardEnabled=0;
			break;
		case 0x0a:
			k->ASCIImode=0;
			break;
		case 0x0b:
			k->ASCIImode=1;
			break;
		case 0x0c:
			k->interruptsEnabled=1;
			break;
		case 0x0d:
			k->interruptsEnabled=0;
			break;
	}
}

unsigned int keyboardStatusRead(Keyboard* k)
{
	unsigned int kpr=0;
	unsigned int kda=0;
	if (k->fifoHead!=k->fifoTail)
		kda=1;
	return (kpr<<1)|kda;
}

unsigned int keyboardDataRead(Keyboard* k)
{
	unsigned int rval=0;
	pthread_mutex_lock(&fifo_mutex);

	if(k->fifoHead==k->fifoTail)
	{
		rval = k->dataReg;
		k->dataReg=0;
	}
	else
	{
		rval=k->dataReg=k->fifo[k->fifoHead];
		k->fifoHead=(k->fifoHead+1)%17;
	}
	pthread_mutex_unlock(&fifo_mutex);

	return rval;
}

void keyaction(Keyboard* k, int code)
{
	code=code&0xff;
	printf("Incoming code from GdkEventKey to kb device buffer: %x\n", code);
	if(code==0x03)
	{
		k->controlPressed=1;
		printf("Control key\n");
		return;
	}
	if(code==0xe1) {printf("Ignoring shift\n");return;}
	if(k->controlPressed==1)
	{
		code&=0x1f;
		k->controlPressed=0;
	}
// handle UP key press
if(code == 0xa5){
//substitute ctrl-k
code=(char)0xb;
}
// handle DOWN key press
if(code == 0xa6){
//substitute ctrl-j
code=(char)0xa;
}
// handle RIGHT key press
if(code == 0xa7){
//substitute ctrl-l
code=(char)0xc;
}
// handle LEFT key press
if(code == 0xa8){
//substitute ctrl-h
code=(char)0x8;
}

	pthread_mutex_lock(&fifo_mutex);
	k->fifo[k->fifoTail]=code;
	k->fifoTail=(k->fifoTail+1)%17;

	k->requestInterrupt=1;
	printf("key interrupt requested\n");
	pthread_mutex_unlock(&fifo_mutex);
}

Keyboard* newKeyboard()
{
	Keyboard* k=(Keyboard*)malloc(sizeof(Keyboard));
	k->capsLock=0;
	k->dataReg=0;
	k->interruptsEnabled=0;
	k->controlPressed=0;
	keyboardReset(k);
	return k;
}

void keyboardReset(Keyboard* k)
{
	k->autoRepeatOn=1;
	k->keyClickOn=1;
	k->keyboardEnabled=1;
	k->fifoHead=k->fifoTail=0;
	k->ASCIImode=1;
	k->requestInterrupt=0;
}

//tables:
const char* keynames[]={
"0","1","2","3","4","5","6","7","8","9",
")","!","@","#","$","%","^","&","*","(",
"A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z",
"BACKSPACE","TAB","LINEFEED","RETURN","ESC","SPACE",
"QUOTE","<","_",">","?",":","+","-","|","}","~",
"'",",","-",".","/",";","=","[","\\","]","`",
"DELETE","ENTER","HELP",
"F0","F1","F2","F3","F4","F5","F6","F7","F8","F9","F10","F11","F12",
"DCHR","DLINE",
"ICHR","ILINE",
"UP","DOWN","RIGHT","LEFT","HOME","BREAK",
};
unsigned int keytable[]={
0,1,2,3,4,5,6,7,8,9,
0,1,2,3,4,5,6,7,8,9,
10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
36,37,38,39,40,41,
42,43,44,45,46,47,48,49,50,51,52,
42,43,44,45,46,47,48,49,50,51,52,
53,54,55,
56,57,58,59,60,61,62,63,64,65,66,67,68,
69,70,
69,70,
71,72,73,74,75,76,
};

//not shifted, shifted, control, controlshift, capslock, down, up
unsigned int keycodes[100][7]={
	// keys ['0'/')'] through ['9'/'(']
	{ 0x30, 0x29, 0x30, 0x29, 0, 0x5b, 0xdb},	// '0'/')''
	{ 0x31, 0x21, 0x31, 0x21, 0, 0x57, 0xd7},
	{ 0x32, 0x40, 0x32, 0x40, 0, 0x56, 0xd6},
	{ 0x33, 0x23, 0x33, 0x23, 0, 0x55, 0xd5},
	{ 0x34, 0x24, 0x34, 0x24, 0, 0x54, 0xd4},
	{ 0x35, 0x25, 0x35, 0x25, 0, 0x53, 0xd3},
	{ 0x36, 0x5e, 0x36, 0x5e, 0, 0x52, 0xd2},
	{ 0x37, 0x26, 0x37, 0x26, 0, 0x51, 0xd1},
	{ 0x38, 0x2a, 0x38, 0x2a, 0, 0x50, 0xd0},
	{ 0x39, 0x28, 0x39, 0x28, 0, 0x5a, 0xda},	// '9'/'('

	// keys ['A'] through ['Z']
	{ 0x61, 0x41, 0x01, 0x41, 1, 0x07, 0x87},	// A
	{ 0x62, 0x42, 0x02, 0x42, 1, 0x13, 0x93},
	{ 0x63, 0x43, 0x03, 0x43, 1, 0x15, 0x95},
	{ 0x64, 0x44, 0x04, 0x44, 1, 0x05, 0x85},
	{ 0x65, 0x45, 0x05, 0x45, 1, 0x0d, 0x8d},
	{ 0x66, 0x46, 0x06, 0x46, 1, 0x04, 0x84},
	{ 0x67, 0x47, 0x07, 0x47, 1, 0x03, 0x83},
	{ 0x68, 0x48, 0x08, 0x48, 1, 0x02, 0x82},
	{ 0x69, 0x49, 0x09, 0x49, 1, 0x08, 0x88},
	{ 0x6a, 0x4a, 0x0a, 0x4a, 1, 0x01, 0x81},
	{ 0x6b, 0x4b, 0x0b, 0x4b, 1, 0x00, 0x80},
	{ 0x6c, 0x4c, 0x0c, 0x4c, 1, 0x10, 0x90},
	{ 0x6d, 0x4d, 0x0d, 0x4d, 1, 0x11, 0x91},
	{ 0x6e, 0x4e, 0x0e, 0x4e, 1, 0x12, 0x92},
	{ 0x6f, 0x4f, 0x0f, 0x4f, 1, 0x19, 0x99},
	{ 0x70, 0x50, 0x10, 0x50, 1, 0x1a, 0x9a},
	{ 0x71, 0x51, 0x11, 0x51, 1, 0x0f, 0x8f},
	{ 0x72, 0x52, 0x12, 0x52, 1, 0x0c, 0x8c},
	{ 0x73, 0x53, 0x13, 0x53, 1, 0x06, 0x86},
	{ 0x74, 0x54, 0x14, 0x54, 1, 0x0b, 0x8b},
	{ 0x75, 0x55, 0x15, 0x55, 1, 0x09, 0x89},
	{ 0x76, 0x56, 0x16, 0x56, 1, 0x14, 0x84},
	{ 0x77, 0x57, 0x17, 0x57, 1, 0x0e, 0x8e},
	{ 0x78, 0x58, 0x18, 0x58, 1, 0x16, 0x86},
	{ 0x79, 0x59, 0x19, 0x59, 1, 0x0a, 0x8a},
	{ 0x7a, 0x5a, 0x1a, 0x5a, 1, 0x17, 0x87}, // Z

	{ 0x08, 0x08, 0x08, 0x08, 0, 0x5f, 0xdf},	// BACKSPACE
	{ 0x09, 0x09, 0x09, 0x09, 0, 0x4e, 0xce},	// TAB
	{ 0x0a, 0x0a, 0x0a, 0x0a, 0, 0x44, 0xc4},	// LINE FEED
	{ 0x0d, 0x0d, 0x0d, 0x0d, 0, 0x4c, 0xcc},	// RETURN
	{ 0x1b, 0x1b, 0x1b, 0x1b, 0, 0x4f, 0xcf},	// ESC
	{ 0x20, 0x20, 0x20, 0x20, 0, 0x45, 0xc5},	// SPACE

	{ 0x27, 0x22, 0x27, 0x22, 0, 0x48, 0xc8},	// ' "
	{ 0x2c, 0x3c, 0x2c, 0x3c, 0, 0x4d, 0xcd},	// , <
	{ 0x2d, 0x5f, 0x2d, 0x5f, 0, 0x5c, 0xdc},	// - _
	{ 0x2e, 0x3e, 0x2e, 0x3e, 0, 0x4a, 0xca},	// . >
	{ 0x2f, 0x3f, 0x2f, 0x3f, 0, 0x4b, 0xcb},	// / ?
	{ 0x3b, 0x3a, 0x3b, 0x3a, 0, 0x49, 0xc9},	// ; :
	{ 0x3d, 0x2b, 0x3d, 0x2b, 0, 0x5d, 0xdd},	// = +
	{ 0x5b, 0x7b, 0x5b, 0x7b, 0, 0x59, 0xd9},	// [ {
	{ 0x5c, 0x7c, 0x5c, 0x7c, 0, 0x43, 0xc3},	// \ |
	{ 0x5d, 0x7d, 0x5d, 0x7d, 0, 0x58, 0xd8},	// ] }
	{ 0x60, 0x7e, 0x60, 0x7e, 0, 0x5e, 0xde},	// ` ~

	{ 0x7f, 0x7f, 0x7f, 0x7f, 0, 0x42, 0xc2},	// DELETE
	{ 0x8d, 0xcd, 0x8d, 0xcd, 0, 0x38, 0xb8},	// ENTER
	{ 0x95, 0xd5, 0x95, 0xc5, 0, 0x46, 0xc6},	// HELP

	{ 0x96, 0xd6, 0x96, 0xd6, 0, 0x27, 0xa7},	// F0
	{ 0x97, 0xd7, 0x97, 0xd7, 0, 0x26, 0xa6},	// F1
	{ 0x98, 0xd8, 0x98, 0xd8, 0, 0x25, 0xa5},	// F2
	{ 0x99, 0xd9, 0x99, 0xd9, 0, 0x24, 0xa4},	// F3
	{ 0x9a, 0xda, 0x9a, 0xda, 0, 0x23, 0xa3},	// F4
	{ 0x9b, 0xdb, 0x9b, 0xdb, 0, 0x22, 0xa2},	// F5
	{ 0x9c, 0xdc, 0x9c, 0xdc, 0, 0x21, 0xa1},	// F6
	{ 0x9d, 0xdd, 0x9d, 0xdd, 0, 0x20, 0xa0},	// F7
	{ 0x9e, 0xde, 0x9e, 0xde, 0, 0x29, 0xa9},	// F8
	{ 0x9f, 0xdf, 0x9f, 0xdf, 0, 0x2a, 0xaa},	// F9
	{ 0xa0, 0xe0, 0xa0, 0xe0, 0, 0x2b, 0xab},	// F10
	{ 0xa1, 0xe1, 0xa1, 0xe1, 0, 0x2c, 0xac},	// F11
	{ 0xa2, 0xe2, 0xa2, 0xe2, 0, 0x2d, 0xad},	// F12

	{ 0xa3, 0xe3, 0xa3, 0xe3, 0, 0x2e, 0xae},	// INSERT CHARACTER/DELETE CHARACTER
	{ 0xa4, 0xe4, 0xa4, 0xe4, 0, 0x2f, 0xaf},	// INSERT LINE/DELETE LINE

	{ 0xa5, 0xe5, 0xa5, 0xe5, 0, 0x3b, 0xbb},	// UP
	{ 0xa6, 0xe6, 0xa6, 0xe6, 0, 0x3a, 0xba},	// DOWN
	{ 0xa7, 0xe7, 0xa7, 0xe7, 0, 0x33, 0xb3},	// RIGHT
	{ 0xa8, 0xe8, 0xa8, 0xe8, 0, 0x3f, 0xbf},	// LEFT

	{ 0xa9, 0xe9, 0xa9, 0xe9, 0, 0x37, 0xb7},	// HOME
	{ 0xaa, 0xea, 0xaa, 0xea, 0, 0x47, 0xc7},	// BREAK

	{ 0xad, 0xed, 0xad, 0xed, 0, 0x39, 0xb9}, // - (keypad)
	{ 0xae, 0xee, 0xae, 0xee, 0, 0x40, 0xc0},	// . (keypad)

	{ 0xb0, 0xf0, 0xb0, 0xf0, 0, 0x41, 0xc1}, // 0 (keypad)
	{ 0xb1, 0xf1, 0xb1, 0xf1, 0, 0x34, 0xb4}, // 1 (keypad)
	{ 0xb2, 0xf2, 0xb2, 0xf2, 0, 0x3c, 0xbc}, // 2 (keypad)
	{ 0xb3, 0xf3, 0xb3, 0xf3, 0, 0x30, 0xb0}, // 3 (keypad)
	{ 0xb4, 0xf4, 0xb4, 0xf4, 0, 0x35, 0xb5}, // 4 (keypad)
	{ 0xb5, 0xf5, 0xb5, 0xf5, 0, 0x3d, 0xbd}, // 5 (keypad)
	{ 0xb6, 0xf6, 0xb6, 0xf6, 0, 0x31, 0xb1}, // 6 (keypad)
	{ 0xb7, 0xf7, 0xb7, 0xf7, 0, 0x36, 0xb6}, // 7 (keypad)
	{ 0xb8, 0xf8, 0xb8, 0xf8, 0, 0x3e, 0xbe}, // 8 (keypad)
	{ 0xb9, 0xf9, 0xb9, 0xf9, 0, 0x32, 0xb2}, // 9 (keypad)

	{ 0x00, 0x00, 0x00, 0x00, 0, 0x60, 0xe0},	// FAST REPEAT
	{ 0x00, 0x00, 0x00, 0x00, 0, 0x61, 0xe1},	// CAPS LOCK
	{ 0x00, 0x00, 0x00, 0x00, 0, 0x62, 0xe2},	// SHIFT (right)
	{ 0x00, 0x00, 0x00, 0x00, 0, 0x63, 0xe3},	// CTRL
	{ 0x00, 0x00, 0x00, 0x00, 0, 0x64, 0xe4},	// SHIFT (left)
	{ 0x00, 0x00, 0x00, 0x00, 0, 0x00, 0x00},	// RESET
};
