// Margaret Black
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


#include <stdlib.h>
#include <stdio.h>
#include "mainboard.h"

extern unsigned int* pixels;

const char font[]={0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x00,0x08,0x00,0x00,0x14,0x14,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x14,0x14,0x3e,0x14,0x3e,0x14,0x14,0x00,0x00,0x08,0x1e,0x28,0x1c,0x0a,0x3c,0x08,0x00,0x00,0x30,0x32,0x04,0x08,0x10,0x26,0x06,0x00,0x00,0x08,0x14,0x14,0x18,0x2a,0x24,0x1a,0x00,0x00,0x0c,0x08,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x08,0x10,0x10,0x10,0x08,0x04,0x00,0x00,0x10,0x08,0x04,0x04,0x04,0x08,0x10,0x00,0x00,0x00,0x08,0x2a,0x1c,0x2a,0x08,0x00,0x00,0x00,0x00,0x08,0x08,0x3e,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x08,0x10,0x00,0x00,0x00,0x00,0x3e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x1c,0x22,0x26,0x2a,0x32,0x22,0x1c,0x00,0x00,0x08,0x18,0x08,0x08,0x08,0x08,0x1c,0x00,0x00,0x1c,0x22,0x02,0x04,0x08,0x10,0x3e,0x00,0x00,0x3e,0x04,0x08,0x04,0x02,0x22,0x1c,0x00,0x00,0x04,0x0c,0x14,0x24,0x3e,0x04,0x04,0x00,0x00,0x3e,0x20,0x3c,0x02,0x02,0x22,0x1c,0x00,0x00,0x0c,0x10,0x20,0x3c,0x22,0x22,0x1c,0x00,0x00,0x3e,0x02,0x04,0x08,0x10,0x10,0x10,0x00,0x00,0x1c,0x22,0x22,0x1c,0x22,0x22,0x1c,0x00,0x00,0x1c,0x22,0x22,0x1e,0x02,0x04,0x18,0x00,0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00,0x00,0x00,0x18,0x18,0x00,0x18,0x18,0x08,0x10,0x00,0x02,0x04,0x08,0x10,0x08,0x04,0x02,0x00,0x00,0x00,0x00,0x3e,0x00,0x3e,0x00,0x00,0x00,0x00,0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00,0x00,0x1c,0x22,0x02,0x04,0x08,0x00,0x08,0x00,0x00,0x0c,0x12,0x26,0x2a,0x2e,0x20,0x1e,0x00,0x00,0x1c,0x22,0x22,0x3e,0x22,0x22,0x22,0x00,0x00,0x3c,0x22,0x22,0x3c,0x22,0x22,0x3c,0x00,0x00,0x1c,0x22,0x20,0x20,0x20,0x22,0x1c,0x00,0x00,0x38,0x24,0x22,0x22,0x22,0x24,0x38,0x00,0x00,0x3e,0x20,0x20,0x3c,0x20,0x20,0x3e,0x00,0x00,0x3e,0x20,0x20,0x3c,0x20,0x20,0x20,0x00,0x00,0x1c,0x22,0x20,0x26,0x22,0x22,0x1e,0x00,0x00,0x22,0x22,0x22,0x3e,0x22,0x22,0x22,0x00,0x00,0x1c,0x08,0x08,0x08,0x08,0x08,0x1c,0x00,0x00,0x0e,0x04,0x04,0x04,0x04,0x24,0x18,0x00,0x00,0x22,0x24,0x28,0x30,0x28,0x24,0x22,0x00,0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x3e,0x00,0x00,0x22,0x36,0x2a,0x2a,0x22,0x22,0x22,0x00,0x00,0x22,0x22,0x32,0x2a,0x26,0x22,0x22,0x00,0x00,0x1c,0x22,0x22,0x22,0x22,0x22,0x1c,0x00,0x00,0x3c,0x22,0x22,0x3c,0x20,0x20,0x20,0x00,0x00,0x1c,0x22,0x22,0x22,0x2a,0x24,0x1a,0x00,0x00,0x3c,0x22,0x22,0x3c,0x28,0x24,0x22,0x00,0x00,0x1c,0x22,0x20,0x1c,0x02,0x22,0x1c,0x00,0x00,0x3e,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00,0x22,0x22,0x22,0x22,0x22,0x22,0x1c,0x00,0x00,0x22,0x22,0x22,0x14,0x14,0x08,0x08,0x00,0x00,0x22,0x22,0x22,0x2a,0x2a,0x2a,0x14,0x00,0x00,0x22,0x22,0x14,0x08,0x14,0x22,0x22,0x00,0x00,0x22,0x22,0x14,0x08,0x08,0x08,0x08,0x00,0x00,0x3e,0x02,0x04,0x08,0x10,0x20,0x3e,0x00,0x00,0x0e,0x08,0x08,0x08,0x08,0x08,0x0e,0x00,0x00,0x40,0x20,0x10,0x08,0x04,0x02,0x01,0x00,0x00,0x38,0x08,0x08,0x08,0x08,0x08,0x38,0x00,0x00,0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x7f,0x00,0x00,0x18,0x08,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1c,0x02,0x1e,0x22,0x1e,0x00,0x00,0x20,0x20,0x3c,0x22,0x22,0x22,0x3c,0x00,0x00,0x00,0x00,0x1c,0x22,0x20,0x20,0x1c,0x00,0x00,0x02,0x02,0x1e,0x22,0x22,0x22,0x1e,0x00,0x00,0x00,0x00,0x1c,0x22,0x3e,0x20,0x1c,0x00,0x00,0x0c,0x12,0x10,0x38,0x10,0x10,0x10,0x00,0x00,0x00,0x00,0x1e,0x24,0x38,0x1c,0x22,0x1c,0x00,0x20,0x20,0x3c,0x22,0x22,0x22,0x22,0x00,0x00,0x08,0x00,0x18,0x08,0x08,0x08,0x1c,0x00,0x00,0x02,0x00,0x02,0x02,0x02,0x02,0x22,0x1c,0x00,0x20,0x20,0x24,0x28,0x34,0x22,0x22,0x00,0x00,0x18,0x08,0x08,0x08,0x08,0x08,0x1c,0x00,0x00,0x00,0x00,0x34,0x2a,0x2a,0x2a,0x2a,0x00,0x00,0x00,0x00,0x3c,0x22,0x22,0x22,0x22,0x00,0x00,0x00,0x00,0x1c,0x22,0x22,0x22,0x1c,0x00,0x00,0x00,0x00,0x3c,0x22,0x22,0x3c,0x20,0x20,0x00,0x00,0x00,0x1e,0x22,0x22,0x1e,0x02,0x02,0x00,0x00,0x00,0x2c,0x32,0x20,0x20,0x20,0x00,0x00,0x00,0x00,0x1c,0x20,0x1c,0x02,0x1c,0x00,0x00,0x10,0x10,0x38,0x10,0x10,0x12,0x0c,0x00,0x00,0x00,0x00,0x22,0x22,0x22,0x26,0x1a,0x00,0x00,0x00,0x00,0x22,0x22,0x22,0x14,0x08,0x00,0x00,0x00,0x00,0x22,0x22,0x2a,0x2a,0x14,0x00,0x00,0x00,0x00,0x22,0x14,0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x22,0x22,0x22,0x1e,0x02,0x1c,0x00,0x00,0x00,0x3e,0x04,0x08,0x10,0x3e,0x00,0x00,0x0c,0x10,0x10,0x20,0x10,0x10,0x0c,0x00,0x00,0x08,0x08,0x08,0x00,0x08,0x08,0x08,0x00,0x00,0x18,0x04,0x04,0x02,0x04,0x04,0x18,0x00,0x00,0x30,0x49,0x06,0x00,0x00,0x00,0x00,0x00};

Video* newVideo()
{
	Video* v;
	v=(Video*)malloc(sizeof(Video));
	v->redenabled=v->blueenabled=v->greenenabled=1;
	v->flashenabled=0;
	v->registerPointer=0;
	return v;
}

void resetVideo(Video* v, unsigned int* pixels)
{
	v->redenabled=v->blueenabled=v->greenenabled=1;
	v->flashenabled=0;
	v->registerPointer=0;
	v->shift=0;

	for(int y=0; y<VHEIGHT; y++)
	{
		for(int x=0; x<VWIDTH; x++)
		{
			// set each element in the array to 0. Each element represents one pixel
			// on the screen
			pixels[VWIDTH*y+x]=0;
		}
	}
	for(int i=0; i<0x10000*3; i++) v->vram[i]=0;
	for(int i=0; i<18; i++) v->registers[i]=0;
	v->controlA=v->controlB=v->addressLatch=v->io=0;
	v->redcopy=v->greencopy=v->bluecopy=0;
	v->vramenabled=0;
	v->cursorh=v->cursorl=0;
	v->x=0; v->y=3;
	v->esc=v->escape1=v->escape2=v->escline=v->esccolumn=0;
}

/*void attachInterrupt(Video* v, e8259_t* e8259)
{
}*/

unsigned int readVideo(Video* v, unsigned int addr)
{
	switch(addr)
	{
		case 0xd8:
			return v->io;
		case 0xd9:
			return v->controlA;
		case 0xda:
			return v->addressLatch;
		case 0xdb:
			return v->controlB;
		case 0xdc:
			return v->registerPointer;
		case 0xdd:
			return v->registers[v->registerPointer];
	}
	return 0;
}

void writeVideo(Video* v, unsigned int addr, unsigned int data)
{
	switch(addr)
	{
		case 0xd8:
			v->io=data;
			v->redenabled=(data&1)==0;
			v->greenenabled=(data&2)==0;
			v->blueenabled=(data&4)==0;
			v->flashenabled=(data&8)==0;
			v->redcopy=(data&16)==0;
			v->greencopy=(data&32)==0;
			v->bluecopy=(data&64)==0;
			v->vramenabled=(data&128)==0;
			break;
		case 0xd9:
			v->controlA=data;
			break;
		case 0xda:
			v->addressLatch=data;
//			if(data!=0) printf("address latch is now %d\n",data);

			int o=2048;
			for(int b=o; b<0x10000; b++)
			{
				v->vram[b-o]=v->vram[b];
				v->vram[b-o+0x10000]=v->vram[b+0x10000];
				v->vram[b-o+0x20000]=v->vram[b+0x20000];
			}
			v->shift++;
			break;
		case 0xdb:
			v->controlB=data;
			break;
		case 0xdc:
			if(data<18)
				v->registerPointer=data;
			break;
		case 0xdd:
			v->registers[v->registerPointer]=data;

			if(v->registerPointer==14)
				v->cursorh=data;
			if(v->registerPointer==15)
				v->cursorl=data;
			if(v->registerPointer==10)
				v->cursors=data;
			if(v->registerPointer==11)
				v->cursore=data;
printf("cursorh %d cursorl %d cursors %d cursore %d\n",v->cursorh,v->cursorl,v->cursors,v->cursore);
printf("\tcursor at (%d,%d) %d\n",(v->cursorh*256+v->cursorl)%80 , (v->cursorh*256+v->cursorl)/80, v->shift);
			break;
	}
//	renderScreen(v,pixels);
}

/* this function reads the VRAM in the Video object and writes the data as
	24-bit color pixel elements in the pixel array */
void renderScreen(Video* v, unsigned int* pixels) {

	// keep count of scan lines that are actully diplayed
	int displayed_scan_line = 0;
	/* between each set of 9 displayed scan lines for each character,
		there are addresses that consist of bytes that make up 7 scan lines that are
		not actually displayed. This causes an extra 7 lines for each of the total 25
		character rows. With 9 scan lines per character and the screen having a
		height of 25 characters, this equates to 225 scan lines. However, with the
		extra 7 lines per row of characters, there are actually a total of 400 scan
		lines of addresses (225 + (25 * 7)) */
	// printf("%s%d\n","Video Address Latch: ", v->addressLatch);

	int cursorpos=v->cursorh*256+v->cursorl;
	int cursorx=cursorpos%80;
	int cursory=cursorpos/80;
	int cursorpixelx=cursorx*8;
	int cursorpixely=cursory*9;
	int cursorpixelpos=cursorpixely*640+cursorpixelx;
//	for(int i=0; i<9; i++){for(int j=0; j<9; j++) {
//		pixels[(cursorpixelpos+j)%(640*255)]=0xff0000;
//	}}

//	int start=v->addressLatch/5;
	int start=v->shift;
	start=0;
	start+=16;
	for(int actual_scan_line = start; actual_scan_line < 225 + (25*7) + start; actual_scan_line++) {
//	for(int actual_scan_line = start; actual_scan_line < 225 + (25*7); actual_scan_line++) {

		// each character has a height of 9 scan lines
		// check if the actual_scan_line is a displayed_scan_line
		// only use every 0-8 scan lines out of each set of 0-15 actual scan lines
		// ** if lower 4 bits of row is 0 through 8 - (row&0xf = row%16)
		if((actual_scan_line&0xf) < 9) {

			/* start x at 0 (position of the current character) - VWIDTH/8 is the number
			 	of characters across an entire row -> 640/8 = 80.
			 	this is because each character is 8 pixels wide
			 	charXpos = "character X screen position"  */
			for(int charXpos = 0; charXpos < VWIDTH/8; charXpos++) {
				/* get the starting byte number in VRAM for the current scan line
					each actual scan line consists of 128 bytes of which only 80 are
					displayed.
			 		[actual_scan_lines << 7] is equivalent to [actual_scan_lines * 128]
					this advances the byte address by 128 for each scan line */
				int starting_byte_number = actual_scan_line<<7;
				/* get the offset for the byte address based on the current character
					position */
				int raw_addr = starting_byte_number + charXpos;
				raw_addr %= 0x10000;


				// cycle through each bit of each color plane's byte address
				for(int bit = 0; bit < 8; bit++) {
					// each color plane exists in three consecutive 64K (0x10000) VRAM pages
					// the value for blue, red, and green will be either 1 or 0
					int blue = (v->vram[raw_addr]>>bit)&1;
					if(!v->blueenabled) blue=0;
					int red = (v->vram[raw_addr+0x10000]>>bit)&1;
					if(!v->redenabled) red=0;
					int green = (v->vram[raw_addr+0x20000]>>bit)&1;
					if(!v->greenenabled) green=0;
					/* flash enabled mode turns all pixel colors on and ignores the
					contents of VRAM */
					if(v->flashenabled) { blue=0xff; red=0xff; green=0xff;}
					/* based on each pixel being a 0 or 1 (or 0xff in the case of flashenabled),
					 	set each color to an 8-bit value (either 0x00 or 0xff) */
					blue = blue==0? 0:0xff;
					red = red==0? 0:0xff;
					green = green==0? 0:0xff;
					// construct 24-bit color from 8-bit colors
					int twentyFourBitColor = (red<<16)|(green<<8)|(blue);
					// DEBUG
					// if(twentyFourBitColor > 0) {
					// 	// printf("%ld\n", sizeof(twentyFourBitColor));
					// 	printf("%X\n", red);
					// 	printf("%X\n", green);
					// 	printf("%X\n", blue);
					// 	printf("%06x\n", twentyFourBitColor);
					// }

					/* -- [charXpos * 8] advances the pixel index to the start of the next
						set of character bits for this scan line
						-- [7 - bit] fills in the bit data backwards - for example, for the
						first set of 8 bits, bit 0 is put into pixel array index 7, bit 1
						into pixel array index 6, bit 2 into index 5, and so on.
						-- [displayed_scan_line * VWIDTH] advances the index to the next scan
						line (this would advance the indexing by 640 each line) */
					pixels[(displayed_scan_line*VWIDTH) + ((charXpos*8)+(7-bit))] = twentyFourBitColor;
//if(displayed_scan_line==cursorpixely%225) 
//if(actual_scan_line==cursorpixely && charXpos==cursorx) 
//if(charXpos==cursorx && displayed_scan_line==(cursory-v->shift+3)*9)

//		pixels[(displayed_scan_line*VWIDTH) + ((charXpos*8)+(7-bit))] =0xffffff;
				}
			}
			// advance to next displayed scan line
			// this does not increment when a non-displayed scan line is read
			displayed_scan_line++;
		}
		for(int x=cursorpixelx; x<8+cursorpixelx; x++)
		{
			for(int y=(cursory-v->shift+2)*9+(v->cursors&0b1111); y<=(cursory-v->shift+2)*9+(v->cursore&0b1111); y++)
			{
				if(y*VWIDTH+x>=0 && y*VWIDTH+x<VWIDTH*VHEIGHT)
					pixels[y*VWIDTH+x]=0x00ff00;
			}
		}
	}
}

unsigned int* generateScreen()
{
	// make an usigned int array of size that takes into account the resolution
	// of the Z-100 screen (640 X 225 pixels). VWIDTH and VHEIGHT are defined
	// in video.h (VWIDTH = 640, VHEIGHT = 225).
	unsigned int* pixels = (unsigned int*)malloc((VWIDTH)*(VHEIGHT)*sizeof(unsigned int));
	for(int y=0; y<VHEIGHT; y++)
	{
		for(int x=0; x<VWIDTH; x++)
		{
			// set each element in the array to 0. Each element represents one pixel
			// on the screen
			pixels[VWIDTH*y+x]=0;
		}
	}
	// return the unsigned int array
	return pixels;
}

void drawASCII(unsigned int* pixels)
{
	printf("\33c");
	for(int y=0; y<VHEIGHT; y++)
	{
		for(int x=0; x<VWIDTH; x++)
		{
			if(pixels[VWIDTH*y+x]!=0)
				printf("*");
			else
				printf(" ");
		}
		printf("\n");
	}
}

void videoWrite(Video* v, int code)
{
	int x=v->x;
	int y=v->y;

	if(code>=0x20 && code<=0x7e && v->esc==0)
	{
		videoSetChar(v,x,y,code);
		if(x<79)
			x++;
	}
	else if (code==8)
	{
		if(x>0)
			x--;
	}
	else if (code=='\n')
	{
		if(y<24)
			y++;
		else
		{
			for(int yy=1; yy<=y+1; yy++)
			{
				for(int xx=0; xx<80; xx++)
				{
					unsigned int offsetold=yy*0x80*0x10+xx;
					unsigned int offsetnew=(yy-1)*0x80*0x10+xx;
					for(int i=0; i<9; i++)
					{
						v->vram[0x20000+i*0x80+offsetnew] = v->vram[0x20000+i*0x80+offsetold];
					}
				}
			}
		}
	}
	else if (code=='\r')
		x=0;
	else if (code==0x1b)
		v->esc=1;
	else if (v->esc==1)
	{
		if(code=='H')
		{
			v->esc=0;
		}
		else if (code=='Y')
		{
			v->esc=1;
			v->escape1='Y';
		}
		else if (code=='J' || code=='E')
		{
			for(unsigned int i=0x20000; i<=0x2ffff; i++)
				v->vram[i]=0;
			v->esc=0;
			x=0; y=1;
		}
		else
			v->esc=0;
	}
	else if (v->esc==2)
	{
		if(v->escape1=='Y')
		{
			v->escline=code-' ';
			v->esc=3;
		}
		else
			v->esc=0;
	}
	else if (v->esc==3)
	{
		if(v->escape1=='Y')
		{
			v->esccolumn=code-' ';
			x=v->esccolumn;
			y=v->escline;
			v->esc=0;
		}
		else
			v->esc=0;
	}


	int cursorpos=y*80+x;
	v->cursorh=cursorpos>>8;
	v->cursorl=cursorpos&255;

	v->y=y;
	v->x=x;
}

void videoSetChar(Video* v, int x, int y, int code)
{
	code=code-0x20;
	if(code<0 || code>0x7f)
		return;

	for(int i=code*9; i<code*9+9; i++)
	{
		for(int b=7; b>=0; b--)
		{
			int bb=(font[i]>>b)&1;
			if(bb==1)
				printf(".");
			else
				printf(" ");
		}
		printf("\n");
	}
	printf("\n");
	unsigned int offset=y*0x80*0x10+x;
	for(int i=0; i<9; i++)
	{
		unsigned int addr=0x20000+i*0x80+offset;
		if(addr<0x20000 || addr>0x2ffff)
			continue;
		v->vram[addr]=font[code*9+i];
	}

}
