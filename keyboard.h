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

typedef struct
{
	unsigned char fifo[17];
	unsigned int dataReg;
	int fifoHead;
	int fifoTail;

	int autoRepeatOn;
	int keyClickOn;
	int keyboardEnabled;
	int ASCIImode;
	int interruptsEnabled;

	int capsLock;

	int controlPressed;

	int requestInterrupt;
} Keyboard;

void keyboardCommandWrite(Keyboard*,unsigned int);

unsigned int keyboardStatusRead(Keyboard*);

unsigned int keyboardDataRead(Keyboard*);

void keyboardReset(Keyboard*);

Keyboard* newKeyboard();

void click();
void beep();


//from UI
void keyaction(Keyboard*,char code);
void keydown(char* name);
void keyup(char* name);
