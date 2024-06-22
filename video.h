//Margaret Black
//2018-2024

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


// The Z-100 screen resolution is 640 pixels wide by 225 pixels in height
#define VWIDTH 640
#define VHEIGHT 225

typedef struct
{
	unsigned int vram[0x10000*3];

	unsigned int registers[18];
	int registerPointer;

	int io,controlA,controlB,addressLatch;
	int redenabled, blueenabled, greenenabled, flashenabled;
	int redcopy,greencopy,bluecopy;
	int vramenabled;
	int cursorh,cursorl,cursors,cursore;
	int shift;
	int x,y;

	int esc;
	int escape1,escape2;
	int escline,esccolumn;
} Video;

Video* newVideo();

unsigned int* generateScreen();
void renderScreen(Video*,unsigned int* pixels);

unsigned int readVideo(Video*,unsigned int addr);
void writeVideo(Video*,unsigned int addr, unsigned int data);
void drawASCII(unsigned int*);
void resetVideo(Video*,unsigned int*)
;
void videoSetChar(Video*,int x, int y, int code);
void videoWrite(Video*,int);
