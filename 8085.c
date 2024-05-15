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


#include <stdio.h>
#include "mainboard.h"

unsigned int z100_memory_read(unsigned int,Z100*);
void z100_memory_write(unsigned int, unsigned char,Z100*);
unsigned int z100_port_read(unsigned int,Z100*);
void z100_port_write(unsigned int, unsigned char,Z100*);
int pr8085_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num);

//unsigned int ram[0x10000];

void reset8085(P8085* p8085)
{
	p8085->PC=0;
	p8085->z=p8085->ac=p8085->c=1;
	p8085->s=p8085->p=0;
	p8085->i=0;
	p8085->halted=0;
	p8085->wait_state=0;
}

unsigned int fetch(P8085* p8085)
{
	// unsigned int r=ram[p8085->PC];
	// call mainboard function here...r=p8085->memory
	// unsigned int r=p8085->memory[p8085->PC];
	unsigned int r=z100_memory_read(p8085->PC,p8085->z100);
	p8085->PC=(p8085->PC+1)&0xffff;
	return r;
}

void memory_write_8085(P8085* p8085, unsigned int address, unsigned int data)
{
	//	printf("memory write: %x = %x\n",address,data);
	// call mainboard function here...
	z100_memory_write(address, data, p8085->z100);
	// p8085->memory[address]=data;

}

unsigned int memory_read_8085(P8085* p8085, unsigned int address)
{
	// printf("%s\n", "called memory_read_8085");
	// return p8085->memory[address];
	// call mainboard function here...
	unsigned int data;
	data = z100_memory_read(address, p8085->z100);
	return data;
}

void port_write(P8085* p8085, unsigned int address, unsigned char data)
{
	z100_port_write(address,data,p8085->z100);
}

unsigned char port_read(P8085* p8085, unsigned int address)
{
	return z100_port_read(address,p8085->z100);
}

void check_wait_state(P8085* p8085) {
	// get current and (PC) next (PC+1) byte from memory
	unsigned int pre_check_byte_1=z100_memory_read(p8085->PC,p8085->z100);
	unsigned int pre_check_byte_2=z100_memory_read(p8085->PC+1,p8085->z100);
	/* mainBoard.c function - glue logic for FD-1797 Floppy Disk Controller */
	if(pr8085_FD1797WaitStateCondition(pre_check_byte_1, pre_check_byte_2) &&
		p8085->ready_ == 0) {
		p8085->wait_state = 1;
	}
	else {
		p8085->wait_state = 0;
	}
}

void halt(P8085* p8085)
{
	p8085->halted = 1;
}

void sim(P8085* p8085, unsigned int value)
{
	p8085->i=(value>>3)&1;
	p8085->m75=(value>>2)&1;
	p8085->m65=(value>>1)&1;
	p8085->m55=(value>>0)&1;
}

unsigned int rim(P8085* p8085)
{
	//sid,i7,i6,i5,ie,m7,m6,m5
	return (0<<7)|(0<<6)|(0<<5)|(0<<4)|(p8085->i<<3)|(p8085->m75<<2)|(p8085->m65<<1)|(p8085->m55<<0);
}

void doInstruction8085(P8085* p8085)
{
	if(p8085->halted) {p8085->cycles=1; return;}

	check_wait_state(p8085);
	if(p8085->wait_state) {return;}

	// if not halted or wait_state, continue with executing instruction
	unsigned int cyc=0;
	unsigned int m=0,t=0;
	unsigned int value,value2,result;
	char* name;

	unsigned int opcode=fetch(p8085);
	// stuff opcode into instance variable "opcode" contained in the 8085 header file
	p8085->opcode = opcode;
	// printf("%X", opcode);

	switch(opcode)
	{
		case 0x00: name="nop"; m=1; t=4; break;
		case 0x01: name="lxi b"; m=3; t=10;
			p8085->C=fetch(p8085); p8085->B=fetch(p8085);
			break;
		case 0x02: name="stax b"; m=2; t=7;
			memory_write_8085(p8085,(p8085->B<<8)|p8085->C, (unsigned char)p8085->A);
			break;
		case 0x03: name="inx b"; m=1; t=6;
			value=(((p8085->B<<8)|p8085->C)+1)&0xffff;
			p8085->B=value>>8; p8085->C=value&0xff;
			break;
		case 0x04: name="inr b"; m=1; t=4;
			value=p8085->B;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->B=value&0xff;
			break;
		case 0x05: name="dcr b"; m=1; t=4;
			value=p8085->B;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->B=value&0xff;
			break;
		case 0x06: name="mvi b"; m=2; t=7;
			p8085->B=fetch(p8085);
			break;
		case 0x07: name="rlc";  m=1; t=4;
			p8085->c=(p8085->A&0x80)!=0?1:0;
			p8085->A<<=1;
			p8085->A|=p8085->c;
			p8085->A&=0xff;
			break;
		case 0x09: name="dad b"; m=3; t=10;
			value=((p8085->H<<8)|p8085->L);
			value2=((p8085->B<<8)|p8085->C);
			result=value+value2;
			p8085->c=(result&0x10000)!=0?1:0;
			p8085->H=(result&0xffff)>>8; p8085->L=result&0xff;
			break;
		case 0x0a: name="ldax b"; m=2; t=7;
			p8085->A=memory_read_8085(p8085,(p8085->B<<8)|p8085->C)&0xff;
			break;
		case 0x0b: name="dcx b"; m=1; t=6;
			value=(((p8085->B<<8)|p8085->C)-1)&0xffff;
			p8085->B=value>>8; p8085->C=value&0xff;
			break;
		case 0x0c: name="inr c"; m=1; t=4;
			value=p8085->C;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->C=value&0xff;
			break;
		case 0x0d: name="dcr c"; m=1; t=4;
			value=p8085->C;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->C=value&0xff;
			break;
		case 0x0e: name="mvi c"; m=2; t=7;
			p8085->C=fetch(p8085);
			break;
		case 0x0f: name="rrc"; m=1; t=4;
			value=p8085->A;
			p8085->c=value&1;
			value>>=1;
			value|=(p8085->c<<7);
			p8085->A=value&0xff;
			break;
		case 0x11: name="lxi d"; m=3; t=10;
			p8085->E=fetch(p8085); p8085->D=fetch(p8085);
			break;
		case 0x12: name="stax d"; m=2; t=7;
			memory_write_8085(p8085,(p8085->D<<8)|p8085->E, (unsigned char)p8085->A);
			break;
		case 0x13: name="inx d"; m=1; t=6;
			value=(((p8085->D<<8)|p8085->E)+1)&0xffff;
			p8085->D=value>>8; p8085->E=value&0xff;
			break;
		case 0x14: name="inr d"; m=1; t=4;
			value=p8085->D;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->D=value&0xff;
			break;
		case 0x15: name="dcr d"; m=1; t=4;
			value=p8085->D;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->D=value&0xff;
			break;
		case 0x16: name="mvi d"; m=2; t=7;
			p8085->D=fetch(p8085);
			break;
		case 0x17: name="ral"; m=1; t=4;
			value=p8085->c;
			p8085->c=((p8085->A&0x80)!=0)?1:0;
			p8085->A<<=1;
			p8085->A|=value;
			p8085->A&=0xff;
			break;
		case 0x19: name="dad d"; m=3; t=10;
			value=((p8085->H<<8)|p8085->L);
			value2=((p8085->D<<8)|p8085->E);
			result=value+value2;
			p8085->c=(result&0x10000)!=0?1:0;
			p8085->H=(result&0xffff)>>8; p8085->L=result&0xff;
			break;
		case 0x1a: name="ldax d"; m=2; t=7;
			p8085->A=memory_read_8085(p8085,(p8085->D<<8)|p8085->E)&0xff;
			break;
		case 0x1b: name="dcx d"; m=1; t=6;
			value=(((p8085->D<<8)|p8085->E)-1)&0xffff;
			p8085->D=value>>8; p8085->E=value&0xff;
			break;
		case 0x1c: name="inr e"; m=1; t=4;
			value=p8085->E;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->E=value&0xff;
			break;
		case 0x1d: name="dcr e"; m=1; t=4;
			value=p8085->E;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->E=value&0xff;
			break;
		case 0x1e: name="mvi e"; m=2; t=7;
			p8085->E=fetch(p8085);
			break;
		case 0x1f: name="rar"; m=1; t=4;
			value=p8085->c;
			p8085->c=p8085->A&1;
			p8085->A>>=1;
			p8085->A|=(value<<7);
			p8085->A&=0xff;
			break;
		case 0x20: name="rim"; m=1; t=4;
			p8085->A=rim(p8085);
			break;
		case 0x21: name="lxi h"; m=3; t=10;
			p8085->L=fetch(p8085); p8085->H=fetch(p8085);
			break;
		case 0x22: name="shld"; m=5; t=16;
			value=fetch(p8085); value2=fetch(p8085);
			value=(value2<<8)|value;
			memory_write_8085(p8085,value,(unsigned char)p8085->L);
			value=(value+1)&0xffff;
			memory_write_8085(p8085,value,(unsigned char)p8085->H);
			break;
		case 0x23: name="inx h"; m=1; t=6;
			value=(((p8085->H<<8)|p8085->L)+1)&0xffff;
			p8085->H=value>>8; p8085->L=value&0xff;
			break;
		case 0x24: name="inr h"; m=1; t=4;
			value=p8085->H;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->H=value&0xff;
			break;
		case 0x25: name="dcr h"; m=1; t=4;
			value=p8085->H;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->H=value&0xff;
			break;
		case 0x26: name="mvi h"; m=2; t=7;
			p8085->H=fetch(p8085);
			break;
		case 0x27: name="daa"; m=1; t=4;
			result=0; value2=0; value=p8085->A;
			if(p8085->A>0x99 || p8085->c==1)
			{
				result=0x60; value2=1;
			}
			if((p8085->A&0xf)>9 || p8085->ac==1)
				result|=6;
			p8085->A+=result;
			p8085->c=p8085->A>255;
			p8085->A&=0xff;
			p8085->ac=((value^p8085->A)&0x10)!=0?1:0;
			p8085->s=(p8085->A&0x80)!=0? 1:0;
			p8085->z=p8085->A==0?1:0;
			value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0x29: name="dad h"; m=3; t=10;
			value=((p8085->H<<8)|p8085->L);
			value2=((p8085->H<<8)|p8085->L);
			result=value+value2;
			p8085->c=(result&0x10000)!=0?1:0;
			p8085->H=(result&0xffff)>>8; p8085->L=result&0xff;
			break;
		case 0x2a: name="lhld"; m=5; t=16;
			value=fetch(p8085); value2=fetch(p8085);
			value=(value2<<8)|value;
			p8085->L=memory_read_8085(p8085,value)&0xff;
			value=(value+1)&0xffff;
			p8085->H=memory_read_8085(p8085,value)&0xff;
			break;
		case 0x2b: name="dcx h"; m=1; t=6;
			value=(((p8085->H<<8)|p8085->L)-1)&0xffff;
			p8085->H=value>>8; p8085->L=value&0xff;
			break;
		case 0x2c: name="inr l"; m=1; t=4;
			value=p8085->L;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->L=value&0xff;
			break;
		case 0x2d: name="dcr l"; m=1; t=4;
			value=p8085->L;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->L=value&0xff;
			break;
		case 0x2e: name="mvi l"; m=2; t=7;
			p8085->L=fetch(p8085);
			break;
		case 0x2f: name="cma"; m=1; t=4;
			p8085->A=(~p8085->A)&0xff;
			break;
		case 0x30: name="sim"; m=1; t=4;
			sim(p8085,p8085->A);
			break;
		case 0x31: name="lxi sp"; m=3; t=10;
			value=fetch(p8085); value2=fetch(p8085);
			p8085->SP=((value2&0xff)<<8)|(value&0xff);
			break;
		case 0x32: name="sta"; m=3; t=13;
			value=fetch(p8085);
			value|=fetch(p8085)<<8;
			memory_write_8085(p8085,value,(unsigned char)p8085->A);
			break;
		case 0x33: name="inx sp"; m=1; t=6;
			p8085->SP=(p8085->SP+1)&0xffff;
			break;
		case 0x34: name="inr m"; m=3; t=10;
			value2=(p8085->H<<8)|p8085->L;
			value=memory_read_8085(p8085,value2)&0xff;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			value=value&0xff;
			memory_write_8085(p8085,value2,(unsigned char)value);
			break;
		case 0x35: name="dcr m"; m=3; t=10;
			value2=(p8085->H<<8)|p8085->L;
			value=memory_read_8085(p8085,value2)&0xff;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			value=value&0xff;
			memory_write_8085(p8085,value2,(unsigned char)value);
			break;
		case 0x36: name="mvi m"; m=3; t=10;
			value=fetch(p8085)&0xff;
			memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)value);
			break;
		case 0x37: name="stc"; m=1; t=4;
			p8085->c=1;
			break;
		case 0x39: name="dad sp"; m=3; t=10;
			value=((p8085->H<<8)|p8085->L);
			value2=p8085->SP;
			result=value+value2;
			p8085->c=(result&0x10000)!=0?1:0;
			p8085->H=(result&0xffff)>>8; p8085->L=result&0xff;
			break;
		case 0x3a: name="lda"; m=3; t=13;
			value=fetch(p8085);
			value|=fetch(p8085)<<8;
			p8085->A=memory_read_8085(p8085,value)&0xff;
			break;
		case 0x3b: name="dcx sp"; m=1; t=6;
			p8085->SP=(p8085->SP-1)&0xffff;
			break;
		case 0x3c: name="inr a"; m=1; t=4;
			value=p8085->A;
			value++;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->A=value&0xff;
			break;
		case 0x3d: name="dcr a"; m=1; t=4;
			value=p8085->A;
			value--;
			p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->ac=(value&0xf)==0xf?1:0;
			p8085->s=((value&0x80)!=0)?1:0;
			p8085->z=((value&0xff)==0)?1:0;
			p8085->A=value&0xff;
			break;
		case 0x3e: name="mvi a"; m=2; t=7;
			p8085->A=fetch(p8085);
			break;
		case 0x3f: name="cmc"; m=1; t=4;
			p8085->c=1-p8085->c;
			break;
		case 0x40: name="mov b b"; m=1; t=4; p8085->B=p8085->B; break;
		case 0x41: name="mov b c"; m=1; t=4; p8085->B=p8085->C; break;
		case 0x42: name="mov b d"; m=1; t=4; p8085->B=p8085->D; break;
		case 0x43: name="mov b e"; m=1; t=4; p8085->B=p8085->E; break;
		case 0x44: name="mov b h"; m=1; t=4; p8085->B=p8085->H; break;
		case 0x45: name="mov b l"; m=1; t=4; p8085->B=p8085->L; break;
		case 0x46: name="mov b M"; m=2; t=7; p8085->B=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x47: name="mov b a"; m=1; t=4; p8085->B=p8085->A; break;
		case 0x48: name="mov c b"; m=1; t=4; p8085->C=p8085->B; break;
		case 0x49: name="mov c c"; m=1; t=4; p8085->C=p8085->C; break;
		case 0x4a: name="mov c d"; m=1; t=4; p8085->C=p8085->D; break;
		case 0x4b: name="mov c e"; m=1; t=4; p8085->C=p8085->E; break;
		case 0x4c: name="mov c h"; m=1; t=4; p8085->C=p8085->H; break;
		case 0x4d: name="mov c l"; m=1; t=4; p8085->C=p8085->L; break;
		case 0x4e: name="mov c M"; m=2; t=7; p8085->C=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x4f: name="mov c a"; m=1; t=4; p8085->C=p8085->A; break;
		case 0x50: name="mov d b"; m=1; t=4; p8085->D=p8085->B; break;
		case 0x51: name="mov d c"; m=1; t=4; p8085->D=p8085->C; break;
		case 0x52: name="mov d d"; m=1; t=4; p8085->D=p8085->D; break;
		case 0x53: name="mov d e"; m=1; t=4; p8085->D=p8085->E; break;
		case 0x54: name="mov d h"; m=1; t=4; p8085->D=p8085->H; break;
		case 0x55: name="mov d l"; m=1; t=4; p8085->D=p8085->L; break;
		case 0x56: name="mov d M"; m=2; t=7; p8085->D=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x57: name="mov d a"; m=1; t=4; p8085->D=p8085->A; break;
		case 0x58: name="mov e b"; m=1; t=4; p8085->E=p8085->B; break;
		case 0x59: name="mov e c"; m=1; t=4; p8085->E=p8085->C; break;
		case 0x5a: name="mov e d"; m=1; t=4; p8085->E=p8085->D; break;
		case 0x5b: name="mov e e"; m=1; t=4; p8085->E=p8085->E; break;
		case 0x5c: name="mov e h"; m=1; t=4; p8085->E=p8085->H; break;
		case 0x5d: name="mov e l"; m=1; t=4; p8085->E=p8085->L; break;
		case 0x5e: name="mov e M"; m=2; t=7; p8085->E=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x5f: name="mov e a"; m=1; t=4; p8085->E=p8085->A; break;
		case 0x60: name="mov h b"; m=1; t=4; p8085->H=p8085->B; break;
		case 0x61: name="mov h c"; m=1; t=4; p8085->H=p8085->C; break;
		case 0x62: name="mov h d"; m=1; t=4; p8085->H=p8085->D; break;
		case 0x63: name="mov h e"; m=1; t=4; p8085->H=p8085->E; break;
		case 0x64: name="mov h h"; m=1; t=4; p8085->H=p8085->H; break;
		case 0x65: name="mov h l"; m=1; t=4; p8085->H=p8085->L; break;
		case 0x66: name="mov h M"; m=2; t=7; p8085->H=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x67: name="mov h a"; m=1; t=4; p8085->H=p8085->A; break;
		case 0x68: name="mov l b"; m=1; t=4; p8085->L=p8085->B; break;
		case 0x69: name="mov l c"; m=1; t=4; p8085->L=p8085->C; break;
		case 0x6a: name="mov l d"; m=1; t=4; p8085->L=p8085->D; break;
		case 0x6b: name="mov l e"; m=1; t=4; p8085->L=p8085->E; break;
		case 0x6c: name="mov l h"; m=1; t=4; p8085->L=p8085->H; break;
		case 0x6d: name="mov l l"; m=1; t=4; p8085->L=p8085->L; break;
		case 0x6e: name="mov l M"; m=2; t=7; p8085->L=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x6f: name="mov l a"; m=1; t=4; p8085->L=p8085->A; break;
		case 0x70: name="mov M b"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->B); break;
		case 0x71: name="mov M c"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->C); break;
		case 0x72: name="mov M d"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->D); break;
		case 0x73: name="mov M e"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->E); break;
		case 0x74: name="mov M h"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->H); break;
		case 0x75: name="mov M l"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->L); break;
		case 0x76: name="hlt"; m=2; t=7; halt(p8085); break;
		case 0x77: name="mov M a"; m=2; t=7; memory_write_8085(p8085,(p8085->H<<8)|p8085->L,(unsigned char)p8085->A); break;
		case 0x78: name="mov a b"; m=1; t=4; p8085->A=p8085->B; break;
		case 0x79: name="mov a c"; m=1; t=4; p8085->A=p8085->C; break;
		case 0x7a: name="mov a d"; m=1; t=4; p8085->A=p8085->D; break;
		case 0x7b: name="mov a e"; m=1; t=4; p8085->A=p8085->E; break;
		case 0x7c: name="mov a h"; m=1; t=4; p8085->A=p8085->H; break;
		case 0x7d: name="mov a l"; m=1; t=4; p8085->A=p8085->L; break;
		case 0x7e: name="mov a M"; m=2; t=7; p8085->A=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff; break;
		case 0x7f: name="mov a a"; m=1; t=4; p8085->A=p8085->A; break;
		case 0x80: name="add b"; m=1; t=4;
			value2=p8085->B;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x81: name="add c"; m=1; t=4;
			value2=p8085->C;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x82: name="add d"; m=1; t=4;
			value2=p8085->D;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x83: name="add e"; m=1; t=4;
			value2=p8085->E;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x84: name="add h"; m=1; t=4;
			value2=p8085->H;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x85: name="add l"; m=1; t=4;
			value2=p8085->L;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x86: name="add M"; m=2; t=7;
			value2=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x87: name="add a"; m=1; t=4;
			value2=p8085->A;
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x88: name="adc b"; m=1; t=4;
			value2=p8085->B;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x89: name="adc c"; m=1; t=4;
			value2=p8085->C;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8a: name="adc d"; m=1; t=4;
			value2=p8085->D;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8b: name="adc e"; m=1; t=4;
			value2=p8085->E;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8c: name="adc h"; m=1; t=4;
			value2=p8085->H;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8d: name="adc l"; m=1; t=4;
			value2=p8085->L;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8e: name="adc M"; m=2; t=7;
			value2=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x8f: name="adc a"; m=1; t=4;
			value2=p8085->A;
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf)+p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x90: name="sub b"; m=1; t=4;
			value2=p8085->B;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x91: name="sub c"; m=1; t=4;
			value2=p8085->C;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x92: name="sub d"; m=1; t=4;
			value2=p8085->D;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x93: name="sub e"; m=1; t=4;
			value2=p8085->E;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x94: name="sub h"; m=1; t=4;
			value2=p8085->H;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x95: name="sub l"; m=1; t=4;
			value2=p8085->L;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x96: name="sub M"; m=2; t=7;
			value2=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x97: name="sub a"; m=1; t=4;
			value2=p8085->A;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x98: name="sbb b"; m=1; t=4;
			value2=p8085->B;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x99: name="sbb c"; m=1; t=4;
			value2=p8085->C;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9a: name="sbb d"; m=1; t=4;
			value2=p8085->D;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9b: name="sbb e"; m=1; t=4;
			value2=p8085->E;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9c: name="sbb h"; m=1; t=4;
			value2=p8085->H;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9d: name="sbb l"; m=1; t=4;
			value2=p8085->L;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9e: name="sbb M"; m=2; t=7;
			value2=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0x9f: name="sbb a"; m=1; t=4;
			value2=p8085->A;
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf)-p8085->c)&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0xa0: name="ana b"; m=1; t=4;
			p8085->A&=p8085->B;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa1: name="ana c"; m=1; t=4;
			p8085->A&=p8085->C;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa2: name="ana d"; m=1; t=4;
			p8085->A&=p8085->D;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa3: name="ana e"; m=1; t=4;
			p8085->A&=p8085->E;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa4: name="ana h"; m=1; t=4;
			p8085->A&=p8085->H;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa5: name="ana l"; m=1; t=4;
			p8085->A&=p8085->L;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa6: name="ana M"; m=2; t=7;
			p8085->A&=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa7: name="ana a"; m=1; t=4;
			p8085->A&=p8085->A;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa8: name="xra b"; m=1; t=4;
			p8085->A^=p8085->B;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xa9: name="xra c"; m=1; t=4;
			p8085->A^=p8085->C;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xaa: name="xra d"; m=1; t=4;
			p8085->A^=p8085->D;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xab: name="xra e"; m=1; t=4;
			p8085->A^=p8085->E;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xac: name="xra h"; m=1; t=4;
			p8085->A^=p8085->H;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xad: name="xra l"; m=1; t=4;
			p8085->A^=p8085->L;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xae: name="xra M"; m=2; t=7;
			p8085->A^=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xaf: name="xra a"; m=1; t=4;
			p8085->A^=p8085->A;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb0: name="ora b"; m=1; t=4;
			p8085->A|=p8085->B;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb1: name="ora c"; m=1; t=4;
			p8085->A|=p8085->C;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb2: name="ora d"; m=1; t=4;
			p8085->A|=p8085->D;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb3: name="ora e"; m=1; t=4;
			p8085->A|=p8085->E;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb4: name="ora h"; m=1; t=4;
			p8085->A|=p8085->H;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb5: name="ora l"; m=1; t=4;
			p8085->A|=p8085->L;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb6: name="ora M"; m=2; t=7;
			p8085->A|=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb7: name="ora a"; m=1; t=4;
			p8085->A|=p8085->A;
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb8: name="cmp b"; m=1; t=4;
			value2=p8085->B;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xb9: name="cmp c"; m=1; t=4;
			value2=p8085->C;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xba: name="cmp d"; m=1; t=4;
			value2=p8085->D;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xbb: name="cmp e"; m=1; t=4;
			value2=p8085->E;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xbc: name="cmp h"; m=1; t=4;
			value2=p8085->H;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xbd: name="cmp l"; m=1; t=4;
			value2=p8085->L;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xbe: name="cmp M"; m=2; t=7;
			value2=memory_read_8085(p8085,(p8085->H<<8)|p8085->L)&0xff;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xbf: name="cmp a"; m=1; t=4;
			value2=p8085->A;
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xc0: name="rnz";
			if(p8085->z==0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xc1: name="pop b"; m=3; t=10;
			p8085->C=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->B=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			break;
		case 0xc2: name="jnz";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->z==0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xc3: name="jmp";
			p8085->PC=fetch(p8085)|(fetch(p8085)<<8);
			break;
		case 0xc4: name="cnz";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->z==0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xc5: name="push b"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->B);
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->C);
			break;
		case 0xc6: name="adi"; m=2; t=7;
			value2=fetch(p8085);
			result=p8085->A+value2;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0xc7: name="rst 0"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0;
			break;
		case 0xc8: name="rz";
			if(p8085->z!=0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xc9: name="ret";
			m=3; t=10;
			value=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->PC=value;
			break;
		case 0xca: name="jz";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->z!=0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xcc: name="cz";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->z!=0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xcd: name="call";
			value=fetch(p8085)|(fetch(p8085)<<8);
			m=5; t=18;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=value;
			break;
		case 0xce: name="aci"; m=2; t=7;
			value2=fetch(p8085);
			result=p8085->A+value2+p8085->c;
			p8085->ac=(((p8085->A&0xf)+(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0xcf: name="rst 1"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x8;
			break;
		case 0xd0: name="rnc";
			if(p8085->c==0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xd1: name="pop d"; m=3; t=10;
			p8085->E=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->D=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			break;
		case 0xd2: name="jnc";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->c==0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xd3: name="out"; m=3; t=10;
			value=fetch(p8085);
			port_write(p8085,value,(unsigned char)p8085->A);
			break;
		case 0xd4: name="cnc";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->c==0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xd5: name="push d"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->D);
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->E);
			break;
		case 0xd6: name="sui"; m=2; t=7;
			value2=fetch(p8085);
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0xd7: name="rst 2"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x10;
			break;
		case 0xd8: name="rc";
			if(p8085->c!=0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xda: name="jc";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->c!=0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xdb: name="in"; m=3; t=10;
			value=fetch(p8085);
			p8085->A=port_read(p8085,value)&0xff;
			break;
		case 0xdc: name="cc";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->c!=0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xde: name="sbi";
			value2=fetch(p8085);
			result=p8085->A-value2-p8085->c;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			p8085->A=result&0xff;
			break;
		case 0xdf: name="rst 3"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x18;
			break;
		case 0xe0: name="rpo";
			if(p8085->p==0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xe1: name="pop h"; m=3; t=10;
			p8085->L=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->H=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			break;
		case 0xe2: name="jpo";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->p==0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xe3: name="xthl"; m=5; t=16;
			value=p8085->L;
			p8085->L=memory_read_8085(p8085,p8085->SP)&0xff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)value);
			value=p8085->H;
			p8085->H=memory_read_8085(p8085,(p8085->SP+1)&0xffff)&0xff;
			memory_write_8085(p8085,(p8085->SP+1)&0xffff,(unsigned char)value);
			break;
		case 0xe4: name="cpo";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->p==0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xe5: name="push h"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->H);
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->L);
			break;
		case 0xe6: name="ani"; m=2; t=7;
			p8085->A&=fetch(p8085);
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=1; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xe7: name="rst 4"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x20;
			break;
		case 0xe8: name="rpe";
			if(p8085->p!=0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xe9: name="pchl"; m=1; t=6;
			p8085->PC=(p8085->H<<8)|p8085->L;
			break;
		case 0xea: name="jpe";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->p!=0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xeb: name="xchg"; m=1; t=4;
			value=p8085->D;
			p8085->D=p8085->H;
			p8085->H=value;
			value=p8085->E;
			p8085->E=p8085->L;
			p8085->L=value;
			break;
		case 0xec: name="cpe";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->p!=0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xee: name="xri"; m=2; t=7;
			p8085->A^=fetch(p8085);
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xef: name="rst 5"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x28;
			break;
		case 0xf0: name="rp";
			if(p8085->s==0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xf1: name="pop psw"; m=3; t=10;
			value=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->A=memory_read_8085(p8085,p8085->SP)&0xff;
			p8085->SP=(p8085->SP+1)&0xffff;
			p8085->s=(value>>7)&1; p8085->z=(value>>6)&1; p8085->ac=(value>>4)&1;
			p8085->p=(value>>2)&1; p8085->c=(value>>0)&1;
			break;
		case 0xf2: name="jp";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->s==0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xf3: name="di"; m=1; t=4;
			p8085->i=0;
			break;
		case 0xf4: name="cp";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->s==0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xf5: name="push psw"; m=3; t=12;
			value=(p8085->c<<0)|(1<<1)|(p8085->p<<2)|(0<<3)|(p8085->ac<<4)|(0<<5)|(p8085->z<<6)|(p8085->s<<7);
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)p8085->A);
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)value);
			break;
		case 0xf6: name="ori"; m=2; t=7;
			p8085->A|=fetch(p8085);
			p8085->s=(p8085->A&0x80)!=0?1:0; p8085->z=(p8085->A==0)?1:0; p8085->ac=0; p8085->c=0; value=p8085->A; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xf7: name="rst 6"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x30;
			break;
		case 0xf8: name="rm";
			if(p8085->s!=0)
			{
				m=3; t=12;
				value=memory_read_8085(p8085,p8085->SP)&0xff;
				p8085->SP=(p8085->SP+1)&0xffff;
				value|=(memory_read_8085(p8085,p8085->SP)&0xff)<<8;
				p8085->SP=(p8085->SP+1)&0xffff;
				p8085->PC=value;
			}
			else { m=1; t=6; }
			break;
		case 0xf9: name="sphl"; m=1; t=6;
			p8085->SP=(p8085->H<<8)|p8085->L;
			break;
		case 0xfa: name="jm";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->s!=0)
			{
				m=3; t=10;
				p8085->PC=value;
			}
			else { m=2; t=7; };
			break;
		case 0xfb: name="ei"; m=1; t=4;
			p8085->i=1;
			break;
		case 0xfc: name="cm";
			value=fetch(p8085)|(fetch(p8085)<<8);
			if(p8085->s!=0)
			{
				m=5; t=18;
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
				p8085->SP=(p8085->SP-1)&0xffff;
				memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
				p8085->PC=value;
			}
			else { m=2; t=9; }
			break;
		case 0xfe: name="cpi"; m=2; t=7;
			value2=fetch(p8085);
			result=p8085->A-value2;
			p8085->ac=(((p8085->A&0xf)-(value2&0xf))&0x10)!=0? 1:0;
			p8085->c=(result&0x100)!=0?1:0;
			p8085->s=(result&0x80)!=0?1:0;
			p8085->z=(result&0xff)==0?1:0;
			value=result; p8085->p=(((value>>7)^(value>>6)^(value>>5)^(value>>4)^(value>>3)^(value>>2)^(value>>1)^(value))&1)==0? 1:0;
			break;
		case 0xff: name="rst 7"; m=3; t=12;
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)((p8085->PC>>8)&0xff));
			p8085->SP=(p8085->SP-1)&0xffff;
			memory_write_8085(p8085,p8085->SP,(unsigned char)(p8085->PC&0xff));
			p8085->PC=0x38;
			break;
	}

	p8085->name=name;
	p8085->cycles=cyc;
}
