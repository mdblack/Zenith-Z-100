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



#define INTERRUPTLISTSIZE 100

#include <stdlib.h>
#include <stdio.h>
#include "mainboard.h"

void push(P8088* p8088, unsigned value);
unsigned int pop(P8088* p8088);
void memory_write_x86(P8088* p8088, unsigned int address, unsigned char data);
unsigned int memory_read_x86(P8088* p8088, unsigned int address);
void port_write_x86(P8088* p8088, unsigned int address, unsigned char data);
unsigned int port_read_x86(P8088* p8088, unsigned int address);
void prefetch_flush(P8088* p8088);
unsigned int fetch_x86(P8088* p8088);
void trap(P8088* p8088, unsigned int number,int);
void reset8088(P8088* p8088);
void undefined_instruction(P8088* p8088, int opcode);
void decode_ea(P8088* p8088);
unsigned int decode_rm_8(P8088* p8088);
unsigned int decode_rm_16(P8088* p8088);
unsigned int decode_reg_8(P8088* p8088);
unsigned int decode_reg_16(P8088* p8088);
void writeback_rm_8(P8088* p8088, unsigned int result);
void writeback_rm_16(P8088* p8088, unsigned int result);
void writeback_reg_8(P8088* p8088, unsigned int result);
void writeback_reg_16(P8088* p8088, unsigned int result);
unsigned int seg(P8088* p8088, unsigned int defaultseg);

P8088* new8088(Z100* c)
{
	P8088* cpu = (P8088*)malloc(sizeof(P8088));
	cpu->z100=c;
	cpu->wait_state_enabled=1;
	return cpu;
}

void assignCallbacks8088(P8088* p8088, load_function mem_rd, store_function mem_wr, load_function port_rd, store_function port_wr)
{
	p8088->memory_read_x86=mem_rd;
	p8088->memory_write_x86=mem_wr;
	p8088->port_read_x86=port_rd;
	p8088->port_write_x86=port_wr;
}

void memory_write_x86(P8088* p8088, unsigned int address, unsigned char data)
{
	// call mainboard memory write function - make sure mode is properly set
	p8088->memory_write_x86(address,data&0xff,p8088->z100);
	// p8088->memory[address]=data&0xff;
	// p8088->memory[address]=data;
	// printf("%s %X %X\n", "Memory write function called", address, data);
}

unsigned int memory_read_x86(P8088* p8088, unsigned int address)
{
	// call mainboard memory read function - make sure mode is properly set
	return p8088->memory_read_x86(address,p8088->z100);
	// return p8088->memory[address]&0xff;
	// return p8088->memory[address];
	// printf("%s %X\n", "Memory read function called", address);
}

void port_write_x86(P8088* p8088, unsigned int address, unsigned char data)
{
	// call mainboard port write function -
	p8088->port_write_x86(address,data&0xff,p8088->z100);
	// z100_port_write(address,data);
}

unsigned int port_read_x86(P8088* p8088, unsigned int address)
{
	// call mainboard port read function -
	return p8088->port_read_x86(address,p8088->z100);
	// return 0;
	// return z100_port_read(address);
}

void prefetch_flush(P8088* p8088)
{
	for(int i=0; i<PREFETCH_SIZE; i++)
		p8088->prefetch[i]=memory_read_x86(p8088,((p8088->CS<<4)+((p8088->IP+i)&0xffff))&0xfffff);
	p8088->prefetch_counter=0;
}

unsigned int fetch_x86(P8088* p8088)
{
	unsigned int f=p8088->prefetch[p8088->prefetch_counter];
	p8088->prefetch[p8088->prefetch_counter]=memory_read_x86(p8088,((p8088->CS<<4)+((p8088->IP+PREFETCH_SIZE)&0xffff))&0xfffff);
	p8088->prefetch_counter++;
	if(p8088->prefetch_counter>=PREFETCH_SIZE)
		p8088->prefetch_counter=0;
	p8088->IP = (p8088->IP+1)&0xffff;
	return f;
}

void interrupt(P8088* p8088, int number)
{
	unsigned int flags=(p8088->o<<11)|(p8088->d<<10)|(p8088->i<<9)|(p8088->t<<8)|(p8088->s<<7)|(p8088->z<<6)|(p8088->ac<<4)|(p8088->p<<2)|(p8088->c<<0);
	unsigned int cs=p8088->CS;
	unsigned int ip=p8088->IP;
	flags|=0xf002;
	p8088->CSbeforeflush=p8088->CS;
	p8088->IPbeforeflush=p8088->IP;
	p8088->IP=memory_read_x86(p8088,(number&0xff)<<2);
	p8088->IP|=memory_read_x86(p8088,((number&0xff)<<2)+1)<<8;
	p8088->CS=memory_read_x86(p8088,((number&0xff)<<2)+2);
	p8088->CS|=memory_read_x86(p8088,((number&0xff)<<2)+3)<<8;
	push(p8088,flags);
	push(p8088,cs);
	push(p8088,ip);
	p8088->i=0;
	p8088->t=0;
	prefetch_flush(p8088);
}

int getinterrupt(P8088* p8088)
{
	int n=p8088->interruptlist[p8088->interruptlistoldest];
	p8088->interruptlistoldest=(p8088->interruptlistoldest+1)%INTERRUPTLISTSIZE;
	return n;
}
int putinterrupt(P8088* p8088, int i)
{
	p8088->interruptlist[p8088->interruptlistyoungest]=i;
	p8088->interruptlistyoungest=(p8088->interruptlistyoungest+1)%INTERRUPTLISTSIZE;
}
int interruptswaiting(P8088* p8088)
{
	return p8088->interruptlistoldest!=p8088->interruptlistyoungest;
}

void trap(P8088* p8088, unsigned int number, int nonmaskable)
{
	// printf("%s\n", "** INTERRUPT SENT TO 8088 TRAP **");

	//the interrupt masking logic was contributed by
		//"PorkyPiggy64", 2024
	if(p8088->enable_interrupts==0)
	{
		if(p8088->i!=0 || nonmaskable)
			putinterrupt(p8088,number);
		return;
	}
	if(p8088->i!=0 || nonmaskable)
	{
		putinterrupt(p8088,number);
	}
	if(interruptswaiting(p8088))
	{
		int i=getinterrupt(p8088);
		interrupt(p8088,i);
	}
}

void handleOutstandingInterrupts(P8088* p8088)
{
	if(p8088->enable_interrupts!=0)
	{
		if(interruptswaiting(p8088))
		{
			int number=getinterrupt(p8088);
			interrupt(p8088,number);
		}
	}
}

void reset8088(P8088* p8088)
{
	p8088->CS=0xf000;
	p8088->IP=0xfff0;
	p8088->CSbeforeflush=p8088->CS;
	p8088->IPbeforeflush=p8088->IP;

	p8088->SS=p8088->ES=p8088->DS=p8088->SP=p8088->BP=p8088->SI=p8088->DI=0;
	p8088->AH=p8088->BH=p8088->CH=p8088->DH=p8088->AL=p8088->BL=p8088->CL=p8088->DL=0;
	p8088->c=p8088->p=p8088->ac=p8088->z=p8088->s=p8088->t=p8088->i=p8088->d=p8088->o=0;
	p8088->halt=0;
	p8088->name_opcode="";
	p8088->ready_x86_=0;
	p8088->wait_state_x86=0;
	p8088->interruptlistoldest=p8088->interruptlistyoungest=0;
	prefetch_flush(p8088);
}

unsigned int modrm, ea, mod, rm, disp, ea, eaUsesStack;
unsigned int segprefix,lockprefix,repprefix;
char* name_opcode;
unsigned int cycles;
int enable_interrupt=1;

void undefined_instruction(P8088* p8088, int opcode)
{
	printf("UNDEFINED INSTRUCTION %x\n",opcode);
//	trap(p8088,6);
	cycles+=50;
}

void decode_ea(P8088* p8088)
{
	modrm=fetch_x86(p8088);
	mod=(modrm>>6)&3;
	rm=modrm&7;
	disp=0;
	eaUsesStack=0;
	switch(mod)
	{
		case 0:
			disp=0;
			if(rm==6)
				disp=fetch_x86(p8088)|(fetch_x86(p8088)<<8);
			break;
		case 1:
			disp=fetch_x86(p8088);
			if((disp&0x80)!=0)
				disp|=0xff00;
			break;
		case 2:
			disp=fetch_x86(p8088)|(fetch_x86(p8088)<<8);
			break;
	}
	switch(mod)
	{
		case 0:
			switch(rm)
			{
				case 0: ea=((p8088->BH<<8)|p8088->BL)+p8088->SI+disp; break;
				case 1: ea=((p8088->BH<<8)|p8088->BL)+p8088->DI+disp; break;
				case 2: ea=p8088->BP+p8088->SI+disp; eaUsesStack=1; break;
				case 3: ea=p8088->BP+p8088->DI+disp; eaUsesStack=1; break;
				case 4: ea=p8088->SI+disp; break;
				case 5: ea=p8088->DI+disp; break;
				case 6: ea=disp; break;
				case 7: ea=((p8088->BH<<8)|p8088->BL)+disp; break;
			}
			ea&=0xffff;
			break;
		case 1: case 2:
			switch(rm)
			{
				case 0: ea=((p8088->BH<<8)|p8088->BL)+p8088->SI+disp; break;
				case 1: ea=((p8088->BH<<8)|p8088->BL)+p8088->DI+disp; break;
				case 2: ea=p8088->BP+p8088->SI+disp; eaUsesStack=1; break;
				case 3: ea=p8088->BP+p8088->DI+disp; eaUsesStack=1; break;
				case 4: ea=p8088->SI+disp; break;
				case 5: ea=p8088->DI+disp; break;
				case 6: ea=p8088->BP+disp; eaUsesStack=1; break;
				case 7: ea=((p8088->BH<<8)|p8088->BL)+disp; break;
			}
			ea&=0xffff;
			break;
	}
}

unsigned int decode_rm_8(P8088* p8088)
{
	if(mod==3)
	{
		switch(rm)
		{
			case 0: return p8088->AL;
			case 1: return p8088->CL;
			case 2: return p8088->DL;
			case 3: return p8088->BL;
			case 4: return p8088->AH;
			case 5: return p8088->CH;
			case 6: return p8088->DH;
			case 7: return p8088->BH;
		}
	}
	else
	{
		unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
		if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
		switch(segprefix)
		{
		case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
		case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
		case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
		case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
		}
		return memory_read_x86(p8088,addr);
	}
}

unsigned int decode_rm_16(P8088* p8088)
{
	if(mod==3)
	{
		switch(rm)
		{
			case 0: return (p8088->AH<<8)|p8088->AL;
			case 1: return (p8088->CH<<8)|p8088->CL;
			case 2: return (p8088->DH<<8)|p8088->DL;
			case 3: return (p8088->BH<<8)|p8088->BL;
			case 4: return p8088->SP;
			case 5: return p8088->BP;
			case 6: return p8088->SI;
			case 7: return p8088->DI;
		}
	}
	else
	{
		unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
		if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
		switch(segprefix)
		{
		case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
		case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
		case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
		case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
		}
		unsigned int valuemodrm=memory_read_x86(p8088,addr);
		valuemodrm|=(memory_read_x86(p8088,(addr+1)&0xfffff)<<8);
		return valuemodrm;
	}
}

unsigned int decode_reg_8(P8088* p8088)
{
	switch((modrm>>3)&7)
	{
		case 0: return p8088->AL;
		case 1: return p8088->CL;
		case 2: return p8088->DL;
		case 3: return p8088->BL;
		case 4: return p8088->AH;
		case 5: return p8088->CH;
		case 6: return p8088->DH;
		case 7: return p8088->BH;
	}
}

unsigned int decode_reg_16(P8088* p8088)
{
	switch((modrm>>3)&7)
	{
		case 0: return (p8088->AH<<8)|p8088->AL;
		case 1: return (p8088->CH<<8)|p8088->CL;
		case 2: return (p8088->DH<<8)|p8088->DL;
		case 3: return (p8088->BH<<8)|p8088->BL;
		case 4: return p8088->SP;
		case 5: return p8088->BP;
		case 6: return p8088->SI;
		case 7: return p8088->DI;
	}
}

void writeback_rm_8(P8088* p8088, unsigned int result)
{
	result&=0xff;
	if(mod==3)
	{
		switch(rm)
		{
			case 0: p8088->AL=result; break;
			case 1: p8088->CL=result; break;
			case 2: p8088->DL=result; break;
			case 3: p8088->BL=result; break;
			case 4: p8088->AH=result; break;
			case 5: p8088->CH=result; break;
			case 6: p8088->DH=result; break;
			case 7: p8088->BH=result; break;
		}
	}
	else
	{
		unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
		if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
		switch(segprefix)
		{
		case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
		case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
		case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
		case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
		}
		memory_write_x86(p8088,addr,result);
	}
}

void writeback_rm_16(P8088* p8088, unsigned int result)
{
	result&=0xffff;
	if(mod==3)
	{
		switch(rm)
		{
			case 0: p8088->AL=result&0xff; p8088->AH=(result>>8)&0xff;  break;
			case 1: p8088->CL=result&0xff; p8088->CH=(result>>8)&0xff;  break;
			case 2: p8088->DL=result&0xff; p8088->DH=(result>>8)&0xff;  break;
			case 3: p8088->BL=result&0xff; p8088->BH=(result>>8)&0xff;  break;
			case 4: p8088->SP=result; break;
			case 5: p8088->BP=result; break;
			case 6: p8088->SI=result; break;
			case 7: p8088->DI=result; break;
		}
	}
	else
	{
		int addr=((p8088->DS<<4)+ea)&0xfffff;
		if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
		switch(segprefix)
		{
		case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
		case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
		case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
		case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
		}
		memory_write_x86(p8088,addr,result&0xff);
		memory_write_x86(p8088,(addr+1)&0xfffff,(result>>8)&0xff);
	}
}

void writeback_reg_8(P8088* p8088, unsigned int result)
{
	result&=0xff;
	switch((modrm>>3)&7)
	{
		case 0: p8088->AL=result; break;
		case 1: p8088->CL=result; break;
		case 2: p8088->DL=result; break;
		case 3: p8088->BL=result; break;
		case 4: p8088->AH=result; break;
		case 5: p8088->CH=result; break;
		case 6: p8088->DH=result; break;
		case 7: p8088->BH=result; break;
	}
}

void writeback_reg_16(P8088* p8088, unsigned int result)
{
	result&=0xffff;
	switch((modrm>>3)&7)
	{
		case 0: p8088->AL=result&0xff; p8088->AH=(result>>8)&0xff; break;
		case 1: p8088->CL=result&0xff; p8088->CH=(result>>8)&0xff; break;
		case 2: p8088->DL=result&0xff; p8088->DH=(result>>8)&0xff; break;
		case 3: p8088->BL=result&0xff; p8088->BH=(result>>8)&0xff; break;
		case 4: p8088->SP=result; break;
		case 5: p8088->BP=result; break;
		case 6: p8088->SI=result; break;
		case 7: p8088->DI=result; break;
	}
}

unsigned int seg(P8088* p8088, unsigned int defaultseg)
{
	unsigned int addr=defaultseg;
	switch(segprefix)
	{
		case 0x26: addr=p8088->ES; break;
		case 0x2e: addr=p8088->CS; break;
		case 0x36: addr=p8088->SS; break;
		case 0x3e: addr=p8088->DS; break;
	}
	return addr;
}


void set_flags_add_8(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xff)==0)? 1:0;
	p8088->s = ((result&0x80)!=0)? 1:0;
	p8088->o = ( (result^value1) & (result^value2) & 0x80 )!=0? 1:0;
	p8088->ac = (((result^value1)^value2)&0x10)!=0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=(result&(~0xff))!=0? 1:0;
}

void set_flags_add_16(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xffff)==0)? 1:0;
	p8088->s = ((result&0x8000)!=0)? 1:0;
//	p8088->o = ((value1&0x8000)==(value2&0x8000) && (value1&0x8000)!=(result&0x8000))?1:0;
	p8088->o = ( (result^value1) & (result^value2) & 0x8000 )!=0? 1:0;
	p8088->ac = (((result^value1)^value2)&0x10)!=0? 1:0;
//	p8088->p=(((result>>15)^(result>>14)^(result>>13)^(result>>12)^(result>>11)^(result>>10)^(result>>9)^(result>>8)^(result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=(result&(~0xffff))!=0? 1:0;
}

void set_flags_sub_8(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xff)==0)? 1:0;
	p8088->s = ((result&0x80)!=0)? 1:0;
	p8088->o = ( (result^value1) & (value1^value2) & 0x80 )!=0? 1:0;
	p8088->ac = (((result^value1)^value2)&0x10)!=0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=(result&(~0xff))!=0? 1:0;
}

void set_flags_sub_16(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xffff)==0)? 1:0;
	p8088->s = ((result&0x8000)!=0)? 1:0;
	p8088->o = ( (result^value1) & (value1^value2) & 0x8000 )!=0? 1:0;
	p8088->ac = (((result^value1)^value2)&0x10)!=0? 1:0;
//	p8088->p=(((result>>15)^(result>>14)^(result>>13)^(result>>12)^(result>>11)^(result>>10)^(result>>9)^(result>>8)^(result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=(result&(~0xffff))!=0? 1:0;
}

void set_flags_logic_8(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xff)==0)? 1:0;
	p8088->s = ((result&0x80)!=0)? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=0;
	p8088->o=0;
}

void set_flags_logic_16(P8088* p8088, unsigned int value1, unsigned int value2, unsigned int result)
{
	p8088->z = ((result&0xffff)==0)? 1:0;
	p8088->s = ((result&0x8000)!=0)? 1:0;
//	p8088->p=(((result>>15)^(result>>14)^(result>>13)^(result>>12)^(result>>11)^(result>>10)^(result>>9)^(result>>8)^(result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->c=0;
	p8088->o=0;
}

void set_flags_zsp_8(P8088* p8088, unsigned int result)
{
	p8088->z = ((result&0xff)==0)? 1:0;
	p8088->s = ((result&0x80)!=0)? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
}

void set_flags_zsp_16(P8088* p8088, unsigned int result)
{
	p8088->z = ((result&0xffff)==0)? 1:0;
	p8088->s = ((result&0x8000)!=0)? 1:0;
//	p8088->p=(((result>>15)^(result>>14)^(result>>13)^(result>>12)^(result>>11)^(result>>10)^(result>>9)^(result>>8)^(result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
	p8088->p=(((result>>7)^(result>>6)^(result>>5)^(result>>4)^(result>>3)^(result>>2)^(result>>1)^result)&1)==0? 1:0;
}

void push(P8088* p8088, unsigned value)
{
	unsigned int address;
	p8088->SP = (p8088->SP - 1)&0xffff;
	address=((p8088->SS<<4)+p8088->SP)&0xfffff;
	memory_write_x86(p8088,address, (value>>8)&0xff);
	p8088->SP = (p8088->SP - 1)&0xffff;
	address=((p8088->SS<<4)+p8088->SP)&0xfffff;
	memory_write_x86(p8088,address, value&0xff);
}

unsigned int pop(P8088* p8088)
{
	unsigned int address,value;
	address=((p8088->SS<<4)+p8088->SP)&0xfffff;
	value=memory_read_x86(p8088,address);
	p8088->SP = (p8088->SP + 1)&0xffff;
	address=((p8088->SS<<4)+p8088->SP)&0xfffff;
	value|=(memory_read_x86(p8088,address)<<8);
	p8088->SP = (p8088->SP + 1)&0xffff;
	return value;
}

long int count=-1;

void check_wait_state_x86(P8088* p8088) {
	// get current and (PC) next (PC+1) byte from memory
	unsigned int pre_check_byte_1=memory_read_x86(p8088,((p8088->CS<<4)+((p8088->IP)&0xffff))&0xfffff);
	unsigned int pre_check_byte_2=0;
	// 0xe4 = IN AL<-imm8 | 0xe5 = IN AX<-imm8
	// port # = imm8 - get next memory byte for port address
	if((pre_check_byte_1 == 0xe4) || (pre_check_byte_1 == 0xe5)) {
		pre_check_byte_2=memory_read_x86(p8088,((p8088->CS<<4)+((p8088->IP+1)&0xffff))&0xfffff);
	}
	// 0xec = IN AL<-DX | 0xed = IN AX<-DX
	// port # = DX - get DX for port address
	else if((pre_check_byte_1 == 0xec) || (pre_check_byte_1 == 0xed)) {
		pre_check_byte_2=(p8088->DH<<8)|p8088->DL;
	}
	// mainBoard.c function - glue logic for FD-1797 Floppy Disk Controller
	if(p8088->wait_state_enabled==1 && pr8088_FD1797WaitStateCondition(pre_check_byte_1, pre_check_byte_2) && p8088->ready_x86_ == 0) {
			p8088->wait_state_x86 = 1;
		}
		else {
			p8088->wait_state_x86 = 0;
		}
}

void doInstruction8088(P8088* p8088)
{
	check_wait_state_x86(p8088);
	if(p8088->wait_state_x86)
	{
		return;
	}

	p8088->CSbeforeflush=p8088->CS;
	p8088->IPbeforeflush=p8088->IP;

	int isprefix,value1,value2,result;

	p8088->operand1=0;
	p8088->operand2=0;
	p8088->op_result=0;
	cycles=0;

	p8088->enable_interrupts=1;

	unsigned int opcode=fetch_x86(p8088);
	p8088->opcode = opcode;

	lockprefix=0;
	segprefix=0;
	repprefix=0;

	do
	{
		isprefix=0;
		switch(opcode)
		{
			case 0x26: case 0x2e: case 0x36: case 0x3e:
				segprefix=opcode;
				isprefix=1;
				opcode=fetch_x86(p8088);
				break;
			case 0xf0:
				lockprefix=opcode;
				isprefix=1;
				opcode=fetch_x86(p8088);
				break;
			case 0xf2: case 0xf3:
				repprefix=opcode;
				isprefix=1;
				opcode=fetch_x86(p8088);
				break;
		}
		cycles+=2;
	} while(isprefix);

	//---------------------------------------------execute------------------------------------------
	switch(opcode)
	{
		//add r/m8, r8
		case 0x00:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 + value2;
			set_flags_add_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="add r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//add r/m16, r16
		case 0x01:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 + value2;
			set_flags_add_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="add r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//add r8, r/m8
		case 0x02:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 + value2;
			set_flags_add_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="add r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//add r16, r/m16
		case 0x03:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 + value2;
			set_flags_add_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="add r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//add al, imm8
		case 0x04:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1+value2;
			set_flags_add_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="add al imm8";
			cycles+=4;
			break;

		//add ax, imm16
		case 0x05:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1+value2;
			set_flags_add_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="add ax imm16";
			cycles+=4;
			break;

		//push es
		case 0x06:
			push(p8088,p8088->ES);
			name_opcode="push es";
			cycles+=10;
			break;

		//pop es
		case 0x07:
			p8088->ES=pop(p8088);
			name_opcode="pop es";
			cycles+=8;
			break;

		//or r/m8, r8
		case 0x08:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 | value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="or r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//or r/m16, r16
		case 0x09:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 | value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="or r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//or r8, r/m8
		case 0x0a:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 | value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="or r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//or r16, r/m16
		case 0x0b:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 | value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="or r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//or al, imm8
		case 0x0c:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1|value2;
			set_flags_logic_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="or al imm8";
			cycles+=4;
			break;

		//or ax, imm16
		case 0x0d:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1|value2;
			set_flags_logic_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="or ax imm16";
			cycles+=4;
			break;

		//push cs
		case 0x0e:
			push(p8088,p8088->CS);
			name_opcode="push cs";
			cycles+=10;
			break;

		//pop cs
		case 0x0f:
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;
			p8088->CS=pop(p8088);
			prefetch_flush(p8088);
			name_opcode="pop cs";
			cycles+=8;
			break;

		//adc r/m8, r8
		case 0x10:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 + value2 + p8088->c;
			set_flags_add_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="adc r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//adc r/m16, r16
		case 0x11:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 + value2 + p8088->c;
			set_flags_add_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="adc r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//adc r8, r/m8
		case 0x12:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 + value2 + p8088->c;
			set_flags_add_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="adc r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//adc r16, r/m16
		case 0x13:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 + value2 + p8088->c;
			set_flags_add_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="adc r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//adc al, imm8
		case 0x14:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1+value2+p8088->c;
			set_flags_add_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="adc al imm8";
			cycles+=4;
			break;

		//adc ax, imm16
		case 0x15:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1+value2+p8088->c;
			set_flags_add_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="adc ax imm16";
			cycles+=4;
			break;

		//push ss
		case 0x16:
			push(p8088,p8088->SS);
			name_opcode="push ss";
			cycles+=10;
			break;

		//pop ss
		case 0x17:
			p8088->SS=pop(p8088);
			p8088->enable_interrupts=0;
			name_opcode="pop ss";
			cycles+=8;
			break;


		//sbb r/m8, r8
		case 0x18:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 - value2 - p8088->c;
			set_flags_sub_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="sbb r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//sbb r/m16, r16
		case 0x19:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 - value2 - p8088->c;
			set_flags_sub_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="sbb r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//sbb r8, r/m8
		case 0x1a:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 - value2 - p8088->c;
			set_flags_sub_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="sbb r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//sbb r16, r/m16
		case 0x1b:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 - value2 - p8088->c;
			set_flags_sub_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="sbb r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//sbb al, imm8
		case 0x1c:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1-value2-p8088->c;
			set_flags_sub_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="sbb al imm8";
			cycles+=4;
			break;

		//sbb ax, imm16
		case 0x1d:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1-value2-p8088->c;
			set_flags_sub_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="sbb ax imm16";
			cycles+=4;
			break;

		//push ds
		case 0x1e:
			push(p8088,p8088->DS);
			name_opcode="push ds";
			cycles+=10;
			break;

		//pop ds
		case 0x1f:
			p8088->DS=pop(p8088);
			name_opcode="pop ds";
			cycles+=8;
			break;

		//and r/m8, r8
		case 0x20:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 & value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="and r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//and r/m16, r16
		case 0x21:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 & value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="and r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//and r8, r/m8
		case 0x22:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 & value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="and r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//and r16, r/m16
		case 0x23:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 & value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="and r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//and al, imm8
		case 0x24:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1&value2;
			set_flags_logic_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="and al imm8";
			cycles+=4;
			break;

		//and ax, imm16
		case 0x25:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1&value2;
			set_flags_logic_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="and ax imm16";
			cycles+=4;
			break;

		//daa
		case 0x27:
			if((p8088->AL&0xf)>9 || p8088->ac==1)
			{
				p8088->AL+=6;
				if((p8088->AL&0xff00)!=0)
					p8088->c=1;
				p8088->ac=1;
			}
			else
				p8088->ac=0;
			if((p8088->AL&0xf0)>0x90 || p8088->c==1)
			{
				p8088->AL+=0x60;
				p8088->c=1;
			}
			else
				p8088->c=0;

			set_flags_zsp_8(p8088,p8088->AL);
			p8088->AL&=0xff;

			name_opcode="daa";
			cycles+=4;
			break;

		//sub r/m8, r8
		case 0x28:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 - value2;
			set_flags_sub_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="sub r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//sub r/m16, r16
		case 0x29:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 - value2;
			set_flags_sub_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="sub r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//sub r8, r/m8
		case 0x2a:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 - value2;
			set_flags_sub_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="sub r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//sub r16, r/m16
		case 0x2b:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 - value2;
			set_flags_sub_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="sub r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//sub al, imm8
		case 0x2c:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1-value2;
			set_flags_sub_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="sub al imm8";
			cycles+=4;
			break;

		//sub ax, imm16
		case 0x2d:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1-value2;
			set_flags_sub_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="sub ax imm16";
			cycles+=4;
			break;

		//das
		case 0x2f:
			if((p8088->AL&0xf)>9 || p8088->ac==1)
			{
				p8088->AL-=6;
				if((p8088->AL&0xff00)!=0)
					p8088->c=1;
				p8088->ac=1;
			}
			else
				p8088->ac=0;
			if((p8088->AL&0xf0)>0x90 || p8088->c==1)
			{
				p8088->AL-=0x60;
				p8088->c=1;
			}
			else
				p8088->c=0;

			set_flags_zsp_8(p8088,p8088->AL);
			p8088->AL&=0xff;

			name_opcode="das";
			cycles+=4;
			break;

		//xor r/m8, r8
		case 0x30:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 ^ value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_rm_8(p8088,result);
			name_opcode="xor r/m8 r8";
			cycles+=mod==3? 3:16;
			break;

		//xor r/m16, r16
		case 0x31:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 ^ value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_rm_16(p8088,result);
			name_opcode="xor r/m16 r16";
			cycles+=mod==3? 3:16;
			break;

		//xor r8, r/m8
		case 0x32:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 ^ value2;
			set_flags_logic_8(p8088,value1,value2,result);
			writeback_reg_8(p8088,result);
			name_opcode="xor r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//xor r16, r/m16
		case 0x33:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 ^ value2;
			set_flags_logic_16(p8088,value1,value2,result);
			writeback_reg_16(p8088,result);
			name_opcode="xor r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//xor al, imm8
		case 0x34:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1^value2;
			set_flags_logic_8(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			name_opcode="xor al imm8";
			cycles+=4;
			break;

		//xor ax, imm16
		case 0x35:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1^value2;
			set_flags_logic_16(p8088,value1,value2,result);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			name_opcode="xor ax imm16";
			cycles+=4;
			break;

		//aaa
		case 0x37:
			if((p8088->AL&0xf)>9 || p8088->ac==1)
			{
				p8088->AL+=6;
				p8088->AH+=1;
				p8088->ac=1;
				p8088->c=1;
			}
			else
			{
				p8088->ac=0;
				p8088->c=0;
			}
			p8088->AL&=0x0f;
			p8088->AH&=0xff;

			name_opcode="aaa";
			cycles+=8;
			break;

		//cmp r/m8, r8
		case 0x38:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 - value2;
			set_flags_sub_8(p8088,value1,value2,result);
			name_opcode="cmp r/m8 r8";
			cycles+=mod==3? 3:9;
			break;

		//cmp r/m16, r16
		case 0x39:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 - value2;
			set_flags_sub_16(p8088,value1,value2,result);
			name_opcode="cmp r/m16 r16";
			cycles+=mod==3? 3:9;
			break;

		//cmp r8, r/m8
		case 0x3a:
			decode_ea(p8088);
			value1=decode_reg_8(p8088);
			value2=decode_rm_8(p8088);
			result = value1 - value2;
			set_flags_sub_8(p8088,value1,value2,result);
			name_opcode="cmp r8 r/m8";
			cycles+=mod==3? 3:9;
			break;

		//cmp r16, r/m16
		case 0x3b:
			decode_ea(p8088);
			value1=decode_reg_16(p8088);
			value2=decode_rm_16(p8088);
			result = value1 - value2;
			set_flags_sub_16(p8088,value1,value2,result);
			name_opcode="cmp r16 r/m16";
			cycles+=mod==3? 3:9;
			break;

		//cmp al, imm8
		case 0x3c:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1-value2;
			set_flags_sub_8(p8088,value1,value2,result);
			name_opcode="cmp al imm8";
			cycles+=4;
			break;

		//cmp ax, imm16
		case 0x3d:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1-value2;
			set_flags_sub_16(p8088,value1,value2,result);
			name_opcode="cmp ax imm16";
			cycles+=4;
			break;

		//aas
		case 0x3f:
			if((p8088->AL&0xf)>9 || p8088->ac==1)
			{
				p8088->AL-=6;
				p8088->AH-=1;
				p8088->ac=1;
				p8088->c=1;
			}
			else
			{
				p8088->ac=0;
				p8088->c=0;
			}
			p8088->AL&=0x0f;
			p8088->AH&=0xff;

			name_opcode="aas";
			cycles+=8;
			break;

		//inc ax
		case 0x40:
			{
			unsigned int c=p8088->c;
			value1=(p8088->AH<<8)|p8088->AL;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->AH=(result>>8)&0xff;
			p8088->AL=result&0xff;
			cycles+=3;
			name_opcode="inc ax";
			}
			break;

		//inc cx
		case 0x41:
			{
			unsigned int c=p8088->c;
			value1=(p8088->CH<<8)|p8088->CL;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->CH=(result>>8)&0xff;
			p8088->CL=result&0xff;
			cycles+=3;
			name_opcode="inc cx";
			}
			break;

		//inc dx
		case 0x42:
			{
			unsigned int c=p8088->c;
			value1=(p8088->DH<<8)|p8088->DL;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->DH=(result>>8)&0xff;
			p8088->DL=result&0xff;
			cycles+=3;
			name_opcode="inc dx";
			}
			break;

		//inc bx
		case 0x43:
			{
			unsigned int c=p8088->c;
			value1=(p8088->BH<<8)|p8088->BL;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->BH=(result>>8)&0xff;
			p8088->BL=result&0xff;
			cycles+=3;
			name_opcode="inc bx";
			}
			break;

		//inc sp
		case 0x44:
			{
			unsigned int c=p8088->c;
			value1=p8088->SP;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->SP=result&0xffff;
			cycles+=3;
			name_opcode="inc sp";
			}
			break;

		//inc bp
		case 0x45:
			{
			unsigned int c=p8088->c;
			value1=p8088->BP;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->BP=result&0xffff;
			cycles+=3;
			name_opcode="inc bp";
			}
			break;

		//inc si
		case 0x46:
			{
			unsigned int c=p8088->c;
			value1=p8088->SI;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->SI=result&0xffff;
			cycles+=3;
			name_opcode="inc si";
			}
			break;

		//inc di
		case 0x47:
			{
			unsigned int c=p8088->c;
			value1=p8088->DI;
			result=value1+1;
			set_flags_add_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->DI=result&0xffff;
			cycles+=3;
			name_opcode="inc di";
			}
			break;

		//dec ax
		case 0x48:
			{
			unsigned int c=p8088->c;
			value1=(p8088->AH<<8)|p8088->AL;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->AH=(result>>8)&0xff;
			p8088->AL=result&0xff;
			cycles+=3;
			name_opcode="dec ax";
			}
			break;

		//dec cx
		case 0x49:
			{
			unsigned int c=p8088->c;
			value1=(p8088->CH<<8)|p8088->CL;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->CH=(result>>8)&0xff;
			p8088->CL=result&0xff;
			cycles+=3;
			name_opcode="dec cx";
			}
			break;

		//dec dx
		case 0x4a:
			{
			unsigned int c=p8088->c;
			value1=(p8088->DH<<8)|p8088->DL;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->DH=(result>>8)&0xff;
			p8088->DL=result&0xff;
			cycles+=3;
			name_opcode="dec dx";
			}
			break;

		//dec bx
		case 0x4b:
			{
			unsigned int c=p8088->c;
			value1=(p8088->BH<<8)|p8088->BL;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->BH=(result>>8)&0xff;
			p8088->BL=result&0xff;
			cycles+=3;
			name_opcode="dec bx";
			}
			break;

		//dec sp
		case 0x4c:
			{
			unsigned int c=p8088->c;
			value1=p8088->SP;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->SP=result&0xffff;
			cycles+=3;
			name_opcode="dec sp";
			}
			break;

		//dec bp
		case 0x4d:
			{
			unsigned int c=p8088->c;
			value1=p8088->BP;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->BP=result&0xffff;
			cycles+=3;
			name_opcode="dec bp";
			}
			break;

		//dec si
		case 0x4e:
			{
			unsigned int c=p8088->c;
			value1=p8088->SI;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->SI=result&0xffff;
			cycles+=3;
			name_opcode="dec si";
			}
			break;

		//dec di
		case 0x4f:
			{
			unsigned int c=p8088->c;
			value1=p8088->DI;
			result=value1-1;
			set_flags_sub_16(p8088,value1,1,result);
			p8088->c=c;
			p8088->DI=result&0xffff;
			cycles+=3;
			name_opcode="dec di";
			}
			break;

		//push ax
		case 0x50:
			push(p8088,(p8088->AH<<8)|p8088->AL);
			name_opcode="push ax";
			cycles+=10;
			break;

		//push cx
		case 0x51:
			push(p8088,(p8088->CH<<8)|p8088->CL);
			name_opcode="push cx";
			cycles+=10;
			break;

		//push dx
		case 0x52:
			push(p8088,(p8088->DH<<8)|p8088->DL);
			name_opcode="push dx";
			cycles+=10;
			break;

		//push bx
		case 0x53:
			push(p8088,(p8088->BH<<8)|p8088->BL);
			name_opcode="push bx";
			cycles+=10;
			break;

		//push sp
		case 0x54:
			push(p8088,p8088->SP);
			name_opcode="push sp";
			cycles+=10;
			break;

		//push bp
		case 0x55:
			push(p8088,p8088->BP);
			name_opcode="push bp";
			cycles+=10;
			break;

		//push si
		case 0x56:
			push(p8088,p8088->SI);
			name_opcode="push si";
			cycles+=10;
			break;

		//push di
		case 0x57:
			push(p8088,p8088->DI);
			name_opcode="push di";
			cycles+=10;
			break;

		//pop ax
		case 0x58:
			result=pop(p8088);
			p8088->AH=result>>8;
			p8088->AL=result&0xff;
			name_opcode="pop ax";
			cycles+=8;
			break;

		//pop cx
		case 0x59:
			result=pop(p8088);
			p8088->CH=result>>8;
			p8088->CL=result&0xff;
			name_opcode="pop cx";
			cycles+=8;
			break;

		//pop dx
		case 0x5a:
			result=pop(p8088);
			p8088->DH=result>>8;
			p8088->DL=result&0xff;
			name_opcode="pop dx";
			cycles+=8;
			break;

		//pop bx
		case 0x5b:
			result=pop(p8088);
			p8088->BH=result>>8;
			p8088->BL=result&0xff;
			name_opcode="pop bx";
			cycles+=8;
			break;

		//pop sp
		case 0x5c:
			p8088->SP=pop(p8088);
			name_opcode="pop sp";
			cycles+=8;
			break;

		//pop bp
		case 0x5d:
			p8088->BP=pop(p8088);
			name_opcode="pop bp";
			cycles+=8;
			break;

		//pop si
		case 0x5e:
			p8088->SI=pop(p8088);
			name_opcode="pop si";
			cycles+=8;
			break;

		//pop di
		case 0x5f:
			p8088->DI=pop(p8088);
			name_opcode="pop di";
			cycles+=8;
			break;

		//jo imm8
		case 0x70:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->o)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jo imm8";
		}
			break;

		//jno imm8
		case 0x71:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->o)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jno imm8";
		}
			break;

		//jb imm8
		case 0x72:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->c)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jb imm8";
		}
			break;

		//jae imm8
		case 0x73:
		{
			unsigned int imm=fetch_x86(p8088);
			value1 = imm;
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->c)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jae imm8";
		}
			break;

		//je imm8
		case 0x74:
		{
			unsigned int imm=fetch_x86(p8088);
			value1 = imm;
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->z)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="je imm8";
		}
			break;

		//jne imm8
		case 0x75:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->z)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jne imm8";
		}
			break;

		//jbe imm8
		case 0x76:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->z || p8088->c)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jbe imm8";
		}
			break;

		//ja imm8
		case 0x77:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->z && !p8088->c)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="ja imm8";
		}
			break;

		//js imm8
		case 0x78:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->s)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="js imm8";
		}
			break;

		//jns imm8
		case 0x79:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->s)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jns imm8";
		}
			break;

		//jpe imm8
		case 0x7a:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->p)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jpe imm8";
		}
			break;

		//jpo imm8
		case 0x7b:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(!p8088->p)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jpo imm8";
		}
			break;

		//jl imm8
		case 0x7c:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->s!=p8088->o)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jl imm8";
		}
			break;

		//jge imm8
		case 0x7d:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->s==p8088->o)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jge imm8";
		}
			break;

		//jle imm8
		case 0x7e:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->s!=p8088->o || p8088->z==1)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jle imm8";
		}
			break;

		//jg imm8
		case 0x7f:
		{
			unsigned int imm=fetch_x86(p8088);
			if((imm&0x80)!=0)
				imm|=0xff00;
			if(p8088->s==p8088->o && p8088->z==0)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+imm)&0xffff;
				prefetch_flush(p8088);
				cycles+=16;
			}
			else
				cycles+=4;
			name_opcode="jg imm8";
		}
			break;

		//add,or,adc,sbb,and,sub,xor,cmp r/m8, imm8
		case 0x80:
		case 0x82:		//really?
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=fetch_x86(p8088);
			switch((modrm>>3)&7)
			{
			case 0:
				result=value1+value2;
				set_flags_add_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="add r/m8 imm8";
				break;
			case 1:
				result=value1|value2;
				set_flags_logic_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="or r/m8 imm8";
				break;
			case 2:
				result=value1+value2+p8088->c;
				set_flags_add_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="adc r/m8 imm8";
				break;
			case 3:
				result=value1-value2-p8088->c;
				set_flags_sub_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sbb r/m8 imm8";
				break;
			case 4:
				result=value1&value2;
				set_flags_logic_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="and r/m8 imm8";
				break;
			case 5:
				result=value1-value2;
				set_flags_sub_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sub r/m8 imm8";
				break;
			case 6:
				result=value1^value2;
				set_flags_logic_8(p8088,value1,value2,result);
				writeback_rm_8(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="xor r/m8 imm8";
				break;
			case 7:
				result=value1-value2;
				set_flags_sub_8(p8088,value1,value2,result);
				cycles+=mod==3? 4:10;
				name_opcode="cmp r/m8 imm8";
				break;
			}
			break;

		//add,or,adc,sbb,and,sub,xor,cmp r/m16, imm16
		case 0x81:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=fetch_x86(p8088);
			value2|=(fetch_x86(p8088)<<8);
			switch((modrm>>3)&7)
			{
			case 0:
				result=value1+value2;
				set_flags_add_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="add r/m16 imm16";
				break;
			case 1:
				result=value1|value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="or r/m16 imm16";
				break;
			case 2:
				result=value1+value2+p8088->c;
				set_flags_add_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="adc r/m16 imm16";
				break;
			case 3:
				result=value1-value2-p8088->c;
				set_flags_sub_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sbb r/m16 imm16";
				break;
			case 4:
				result=value1&value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="and r/m16 imm16";
				break;
			case 5:
				result=value1-value2;
				set_flags_sub_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sub r/m16 imm16";
				break;
			case 6:
				result=value1^value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="xor r/m16 imm16";
				break;
			case 7:
				result=value1-value2;
				set_flags_sub_16(p8088,value1,value2,result);
				cycles+=mod==3? 4:10;
				name_opcode="cmp r/m16 imm16";
				break;
			}
			break;

		//add,or,adc,sbb,and,sub,xor,cmp r/m16, imm8
		case 0x83:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=fetch_x86(p8088);
			if((value2&0x80)!=0)
				value2|=0xff00;
			switch((modrm>>3)&7)
			{
			case 0:
				result=value1+value2;
				set_flags_add_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="add r/m16 imm8";
				break;
			case 1:
				result=value1|value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="or r/m16 imm8";
				break;
			case 2:
				result=value1+value2+p8088->c;
				set_flags_add_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="adc r/m16 imm8";
				break;
			case 3:
				result=value1-value2-p8088->c;
				set_flags_sub_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sbb r/m16 imm8";
				break;
			case 4:
				result=value1&value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="and r/m16 imm8";
				break;
			case 5:
				result=value1-value2;
				set_flags_sub_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="sub r/m16 imm8";
				break;
			case 6:
				result=value1^value2;
				set_flags_logic_16(p8088,value1,value2,result);
				writeback_rm_16(p8088,result);
				cycles+=mod==3? 4:17;
				name_opcode="xor r/m16 imm8";
				break;
			case 7:
				result=value1-value2;
				set_flags_sub_16(p8088,value1,value2,result);
				cycles+=mod==3? 4:10;
				name_opcode="cmp r/m16 imm8";
				break;
			}
			break;

		//test r/m8, r8
		case 0x84:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			result = value1 & value2;
			set_flags_logic_8(p8088,value1,value2,result);
			cycles+=mod==3? 3:9;
			name_opcode="test r/m8 imm8";
			break;

		//test r/m16, r16
		case 0x85:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			result = value1 & value2;
			set_flags_logic_16(p8088,value1,value2,result);
			cycles+=mod==3? 3:9;
			name_opcode="test r/m16 imm16";
			break;

		//xchg r/m8, r8
		case 0x86:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			value2=decode_reg_8(p8088);
			writeback_rm_8(p8088,value2);
			writeback_reg_8(p8088,value1);
			cycles+=mod==3? 4:17;
			name_opcode="xchg r/m8 imm8";
			break;

		//xchg r/m16, r16
		case 0x87:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			value2=decode_reg_16(p8088);
			writeback_rm_16(p8088,value2);
			writeback_reg_16(p8088,value1);
			cycles+=mod==3? 4:17;
			name_opcode="xchg r/m16 imm16";
			break;

		//mov r/m8, r8
		case 0x88:
			decode_ea(p8088);
			result=decode_reg_8(p8088);
			writeback_rm_8(p8088,result);
			cycles+=mod==3? 2:9;
			name_opcode="mov r/m8 r8";
			break;

		//mov r/m16, r16
		case 0x89:
			decode_ea(p8088);
			result=decode_reg_16(p8088);
			writeback_rm_16(p8088,result);
			cycles+=mod==3? 2:9;
			name_opcode="mov r/m16 r16";
			break;

		//mov r8, r/m8
		case 0x8a:
			decode_ea(p8088);
			result=decode_rm_8(p8088);
			writeback_reg_8(p8088,result);
			cycles+=mod==3? 2:8;
			name_opcode="mov r8 r/m8";
			break;

		//mov r16, r/m16
		case 0x8b:
			decode_ea(p8088);
			result=decode_rm_16(p8088);
			writeback_reg_16(p8088,result);
			cycles+=mod==3? 2:8;
			name_opcode="mov r16 r/m16";
			break;

		//mov r/m16, sreg
		case 0x8c:
			decode_ea(p8088);
			switch((modrm>>3)&3)
			{
				case 0:
					result=p8088->ES;
					name_opcode="mov r/m16 es";
					break;
				case 1:
					result=p8088->CS;
					name_opcode="mov r/m16 cs";
					break;
				case 2:
					result=p8088->SS;
					name_opcode="mov r/m16 ss";
					break;
				case 3:
					result=p8088->DS;
					name_opcode="mov r/m16 ds";
					break;
			}
			writeback_rm_16(p8088,result);
			cycles+=mod==3? 2:9;
			break;

		//lea reg16, r/m16
		case 0x8d:
			decode_ea(p8088);
			writeback_reg_16(p8088,ea);
			cycles+=2;
			name_opcode="lea reg16 r/m16";
			if(mod==3)
				undefined_instruction(p8088,opcode);
			break;

		//mov sreg, r/m16
		case 0x8e:
			decode_ea(p8088);
			result=decode_rm_16(p8088);
			switch((modrm>>3)&3)
			{
				case 0:
					p8088->ES=result;
					name_opcode="mov es r/m16";
					break;
				case 1:
					p8088->CSbeforeflush=p8088->CS;
					p8088->IPbeforeflush=p8088->IP;
					p8088->CS=result;
					prefetch_flush(p8088);
					name_opcode="mov cs r/m16";
					break;
				case 2:
					p8088->SS=result;
					p8088->enable_interrupts=0;
					name_opcode="mov ss r/m16";
					break;
				case 3:
					p8088->DS=result;
					name_opcode="mov ds r/m16";
					break;
			}
			cycles+=mod==3? 2:8;
			break;

		//pop r/m16
		case 0x8f:
			decode_ea(p8088);
			result=pop(p8088);
			writeback_rm_16(p8088,result);
			cycles+=mod==3? 8:17;
			name_opcode="pop r/m16";
			break;

		//xchg ax, ax
		case 0x90:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=(p8088->AH<<8)|p8088->AL;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->AH=value1>>8;
			p8088->AL=value1&0xff;
			name_opcode="xchg ax ax";
			cycles+=3;
			break;

		//xchg ax, cx
		case 0x91:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=(p8088->CH<<8)|p8088->CL;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->CH=value1>>8;
			p8088->CL=value1&0xff;
			name_opcode="xchg ax cx";
			cycles+=3;
			break;

		//xchg ax, dx
		case 0x92:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=(p8088->DH<<8)|p8088->DL;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->DH=value1>>8;
			p8088->DL=value1&0xff;
			name_opcode="xchg ax dx";
			cycles+=3;
			break;

		//xchg ax, bx
		case 0x93:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=(p8088->BH<<8)|p8088->BL;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->BH=value1>>8;
			p8088->BL=value1&0xff;
			name_opcode="xchg ax bx";
			cycles+=3;
			break;

		//xchg ax, sp
		case 0x94:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=p8088->SP;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->SP=value1;
			name_opcode="xchg ax sp";
			cycles+=3;
			break;

		//xchg ax, bp
		case 0x95:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=p8088->BP;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->BP=value1;
			name_opcode="xchg ax bp";
			cycles+=3;
			break;

		//xchg ax, si
		case 0x96:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=p8088->SI;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->SI=value1;
			name_opcode="xchg ax si";
			cycles+=3;
			break;

		//xchg ax, di
		case 0x97:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=p8088->DI;
			p8088->AH=value2>>8;
			p8088->AL=value2&0xff;
			p8088->DI=value1;
			name_opcode="xchg ax di";
			cycles+=3;
			break;

		//cbw
		case 0x98:
			if((p8088->AL&0x80)==0)
				p8088->AH=0;
			else
				p8088->AH=0xff;
			name_opcode="cbw";
			cycles+=2;
			break;

		//cwd
		case 0x99:
			if((p8088->AH&0x80)!=0)
			{
				p8088->DH=0xff;
				p8088->DL=0xff;
			}
			else
			{
				p8088->DH=0;
				p8088->DL=0;
			}
			name_opcode="cwd";
			cycles+=5;
			break;

		//call seg offset
		case 0x9a:
			value1=fetch_x86(p8088);
			value1|=fetch_x86(p8088)<<8;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			push(p8088,p8088->CS);
			push(p8088,p8088->IP);
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=value1;
			p8088->CS=value2;
			prefetch_flush(p8088);
			name_opcode="call seg offset";
			cycles+=28;
			break;

		//wait
		case 0x9b:
			cycles+=4;
			name_opcode="wait";
			break;

		//pushf
		case 0x9c:
			result=(p8088->o<<11)|(p8088->d<<10)|(p8088->i<<9)|(p8088->t<<8)|(p8088->s<<7)|(p8088->z<<6)|(p8088->ac<<4)|(p8088->p<<2)|(p8088->c<<0);
			result&=0xfd5;
			result|=0xf002;
			push(p8088,result);
			name_opcode="pushf";
			cycles+=10;
			break;

		//popf
		case 0x9d:
			result=pop(p8088);
			p8088->c=(result>>0)&1;
			p8088->p=(result>>2)&1;
			p8088->ac=(result>>4)&1;
			p8088->z=(result>>6)&1;
			p8088->s=(result>>7)&1;
			p8088->t=(result>>8)&1;
			p8088->i=(result>>9)&1;
			p8088->d=(result>>10)&1;
			p8088->o=(result>>11)&1;
			name_opcode="popf";
			cycles+=8;
			break;

		//sahf
		case 0x9e:
			p8088->c=(p8088->AH>>0)&1;
			p8088->p=(p8088->AH>>2)&1;
			p8088->ac=(p8088->AH>>4)&1;
			p8088->z=(p8088->AH>>6)&1;
			p8088->s=(p8088->AH>>7)&1;
			name_opcode="sahf";
			cycles+=4;
			break;

		//lahf
		case 0x9f:
			p8088->AH=(p8088->s<<7)|(p8088->z<<6)|(p8088->ac<<4)|(p8088->p<<2)|(p8088->c<<0);
			p8088->AH|=0x2;
			name_opcode="lahf";
			cycles+=4;
			break;

		//mov al, addr16
		case 0xa0:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			result=((seg(p8088,p8088->DS)<<4)+result)&0xfffff;
			p8088->AL=memory_read_x86(p8088,result);
			cycles+=10;
			name_opcode="mov al [addr16]";
			break;

		//mov ax, addr16
		case 0xa1:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			result=((seg(p8088,p8088->DS)<<4)+result)&0xfffff;
			p8088->AL=memory_read_x86(p8088,result);
			p8088->AH=memory_read_x86(p8088,(result+1)&0xfffff);
			cycles+=10;
			name_opcode="mov ax [addr16]";
			break;

		//mov addr16, al
		case 0xa2:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			result=((seg(p8088,p8088->DS)<<4)+result)&0xfffff;
			memory_write_x86(p8088,result, p8088->AL);
			cycles+=10;
			name_opcode="mov [addr16] al";
			break;

		//mov addr16, ax
		case 0xa3:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			result=((seg(p8088,p8088->DS)<<4)+result)&0xfffff;
			memory_write_x86(p8088,result, p8088->AL);
			memory_write_x86(p8088,(result+1)&0xfffff, p8088->AH);
			cycles+=10;
			name_opcode="mov [addr16] ax";
			break;

		//movsb
		case 0xa4:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				result=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,result);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=18;
				}
			}
			else
			{
				result=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,result);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				cycles+=18;
			}
			name_opcode="movsb";
			break;

		//movsw
		case 0xa5:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value2=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff);
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,value1);
				memory_write_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff,value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=18;
				}
			}
			else
			{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value2=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff);
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,value1);
				memory_write_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff,value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				cycles+=18;
			}
			name_opcode="movsw";
			break;

		//cmpsb
		case 0xa6:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				set_flags_sub_8(p8088,value1,value2,value1-value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=22;

				if(repprefix==0xf2 && p8088->z!=0) break;
				if(repprefix==0xf3 && p8088->z==0) break;
				}
			}
			else
			{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				set_flags_sub_8(p8088,value1,value2,value1-value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				cycles+=22;
			}
			name_opcode="cmpsb";
			break;

		//cmpsw
		case 0xa7:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value1|=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff)<<8;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				value2|=memory_read_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff)<<8;
				set_flags_sub_16(p8088,value1,value2,value1-value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=22;

				if(repprefix==0xf2 && p8088->z!=0) break;
				if(repprefix==0xf3 && p8088->z==0) break;
				}
			}
			else
			{
				value1=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				value1|=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff)<<8;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				value2|=memory_read_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff)<<8;
				set_flags_sub_16(p8088,value1,value2,value1-value2);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				cycles+=22;
			}
			name_opcode="cmpsw";
			break;

		//test al, imm8
		case 0xa8:
			value1=p8088->AL;
			value2=fetch_x86(p8088);
			result=value1&value2;
			set_flags_logic_8(p8088,value1,value2,result);
			cycles+=4;
			name_opcode="test al imm8";
			break;

		//test ax, imm16
		case 0xa9:
			value1=(p8088->AH<<8)|p8088->AL;
			value2=fetch_x86(p8088);
			value2|=fetch_x86(p8088)<<8;
			result=value1&value2;
			set_flags_logic_16(p8088,value1,value2,result);
			cycles+=4;
			name_opcode="test ax imm16";
			break;

		//stosb
		case 0xaa:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,p8088->AL);
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=11;
				}
			}
			else
			{
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,p8088->AL);
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				cycles+=11;
			}
			name_opcode="stosb";
			break;

		//stosw
		case 0xab:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,p8088->AL);
				memory_write_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff,p8088->AH);
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=11;
				}
			}
			else
			{
				memory_write_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff,p8088->AL);
				memory_write_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff,p8088->AH);
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				cycles+=11;
			}
			name_opcode="stosw";
			break;

		//lodsb
		case 0xac:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				p8088->AL=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=12;
				}
			}
			else
			{
				p8088->AL=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				p8088->SI=(p8088->SI+(p8088->d? 0xffff:1))&0xffff;
				cycles+=12;
			}
			name_opcode="lodsb";
			break;

		//lodsw
		case 0xad:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				p8088->AL=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				p8088->AH=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=12;
				}
			}
			else
			{
				p8088->AL=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+p8088->SI)&0xfffff);
				p8088->AH=memory_read_x86(p8088,((seg(p8088,p8088->DS)<<4)+((p8088->SI+1)&0xffff))&0xfffff);
				p8088->SI=(p8088->SI+(p8088->d? 0xfffe:2))&0xffff;
				cycles+=12;
			}
			name_opcode="lodsw";
			break;

		//scasb
		case 0xae:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				value1=p8088->AL;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				set_flags_sub_8(p8088,value1,value2,value1-value2);
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=15;
				if(repprefix==0xf2 && p8088->z!=0) break;
				if(repprefix==0xf3 && p8088->z==0) break;
				}
			}
			else
			{
				value1=p8088->AL;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				set_flags_sub_8(p8088,value1,value2,value1-value2);
				p8088->DI=(p8088->DI+(p8088->d? 0xffff:1))&0xffff;
				cycles+=15;
			}
			name_opcode="scasb";
			break;

		//scasw
		case 0xaf:
			if(repprefix!=0)
			{
				while(((p8088->CH<<8)|p8088->CL)!=0)
				{
				value1=p8088->AL;
				value1|=p8088->AH<<8;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				value2|=memory_read_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff)<<8;
				set_flags_sub_16(p8088,value1,value2,value1-value2);
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				result=(p8088->CH<<8)|p8088->CL;
				result=(result-1)&0xffff;
				p8088->CH=result>>8;
				p8088->CL=result&0xff;
				cycles+=15;

				if(repprefix==0xf2 && p8088->z!=0) break;
				if(repprefix==0xf3 && p8088->z==0) break;
				}
			}
			else
			{
				value1=p8088->AL;
				value1|=p8088->AH<<8;
				value2=memory_read_x86(p8088,((p8088->ES<<4)+p8088->DI)&0xfffff);
				value2|=memory_read_x86(p8088,((p8088->ES<<4)+((p8088->DI+1)&0xffff))&0xfffff)<<8;
				set_flags_sub_16(p8088,value1,value2,value1-value2);
				p8088->DI=(p8088->DI+(p8088->d? 0xfffe:2))&0xffff;
				cycles+=15;
			}
			name_opcode="scasw";
			break;

		//mov al, imm8
		case 0xb0:
			p8088->AL=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov al imm8";
			break;

		//mov cl, imm8
		case 0xb1:
			p8088->CL=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov cl imm8";
			break;

		//mov dl, imm8
		case 0xb2:
			p8088->DL=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov dl imm8";
			break;

		//mov bl, imm8
		case 0xb3:
			p8088->BL=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov bl imm8";
			break;

		//mov ah, imm8
		case 0xb4:
			p8088->AH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov ah imm8";
			break;

		//mov ch, imm8
		case 0xb5:
			p8088->CH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov ch imm8";
			break;

		//mov dh, imm8
		case 0xb6:
			p8088->DH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov dh imm8";
			break;

		//mov bh, imm8
		case 0xb7:
			p8088->BH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov bh imm8";
			break;

		//mov ax, imm16
		case 0xb8:
			p8088->AL=fetch_x86(p8088);
			p8088->AH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov ax imm16";
			break;

		//mov cx, imm16
		case 0xb9:
			p8088->CL=fetch_x86(p8088);
			p8088->CH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov cx imm16";
			break;

		//mov dx, imm16
		case 0xba:
			p8088->DL=fetch_x86(p8088);
			p8088->DH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov dx imm16";
			break;

		//mov bx, imm16
		case 0xbb:
			p8088->BL=fetch_x86(p8088);
			p8088->BH=fetch_x86(p8088);
			cycles+=4;
			name_opcode="mov bx imm16";
			break;

		//mov sp, imm16
		case 0xbc:
			p8088->SP=fetch_x86(p8088);
			p8088->SP|=fetch_x86(p8088)<<8;
			cycles+=4;
			name_opcode="mov sp imm16";
			break;

		//mov bp, imm16
		case 0xbd:
			p8088->BP=fetch_x86(p8088);
			p8088->BP|=fetch_x86(p8088)<<8;
			cycles+=4;
			name_opcode="mov bp imm16";
			break;

		//mov si, imm16
		case 0xbe:
			p8088->SI=fetch_x86(p8088);
			p8088->SI|=fetch_x86(p8088)<<8;
			cycles+=4;
			name_opcode="mov si imm16";
			break;

		//mov di, imm16
		case 0xbf:
			p8088->DI=fetch_x86(p8088);
			p8088->DI|=fetch_x86(p8088)<<8;
			cycles+=4;
			name_opcode="mov di imm16";
			break;

		//ret imm16
		case 0xc2:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			p8088->IP=pop(p8088);
			p8088->SP = (p8088->SP + result)&0xffff;
			prefetch_flush(p8088);
			cycles+=20;
			name_opcode="ret imm16";
			break;

		//ret
		case 0xc3:
			p8088->IPbeforeflush=p8088->IP;

			p8088->IP=pop(p8088);
			prefetch_flush(p8088);
			cycles+=16;
			name_opcode="ret";
			break;

		//les reg16 r/m16
		case 0xc4:
			decode_ea(p8088);
			if(mod!=3)
			{
				unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
				if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
				switch(segprefix)
				{
				case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
				case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
				case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
				case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
				}
				value1=memory_read_x86(p8088,addr);
				value1|=(memory_read_x86(p8088,(addr+1)&0xfffff)<<8);
				value2=memory_read_x86(p8088,(addr+2)&0xfffff);
				value2|=(memory_read_x86(p8088,(addr+3)&0xfffff)<<8);
				writeback_reg_16(p8088,value1);
				p8088->ES=value2;
			}
			cycles+=16;
			name_opcode="les reg16 r/m16";
			break;

		//lds reg16 r/m16
		case 0xc5:
			decode_ea(p8088);
			if(mod!=3)
			{
				unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
				if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
				switch(segprefix)
				{
				case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
				case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
				case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
				case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
				}
				value1=memory_read_x86(p8088,addr);
				value1|=(memory_read_x86(p8088,(addr+1)&0xfffff)<<8);
				value2=memory_read_x86(p8088,(addr+2)&0xfffff);
				value2|=(memory_read_x86(p8088,(addr+3)&0xfffff)<<8);
				writeback_reg_16(p8088,value1);
				p8088->DS=value2;
			}
			cycles+=16;
			name_opcode="lds reg16 r/m16";
			break;

		//mov r/m8 imm8
		case 0xc6:
			decode_ea(p8088);
			result=fetch_x86(p8088);
			writeback_rm_8(p8088,result);
			name_opcode="mov r/m8 imm8";
			cycles+=(mod==3)?4:10;
			break;

		//mov r/m16 imm16
		case 0xc7:
			decode_ea(p8088);
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			writeback_rm_16(p8088,result);
			name_opcode="mov r/m16 imm16";
			cycles+=(mod==3)?4:10;
			break;

		//retf imm16
		case 0xca:
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=pop(p8088);
			p8088->CS=pop(p8088);
			p8088->SP = (p8088->SP + result)&0xffff;
			prefetch_flush(p8088);
			cycles+=25;
			name_opcode="retf imm16";
			break;

		//retf
		case 0xcb:
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;

			p8088->IP=pop(p8088);
			p8088->CS=pop(p8088);
			prefetch_flush(p8088);
			cycles+=26;
			name_opcode="retf";
			break;

		//int 3
		case 0xcc:
			trap(p8088,3,1);
			prefetch_flush(p8088);
			cycles+=52;
			name_opcode="int 3";
			break;

		//int imm8
		case 0xcd:
			result=fetch_x86(p8088);
			trap(p8088,result,1);
			prefetch_flush(p8088);
			cycles+=51;
			name_opcode="int imm8";
			break;

		//into
		case 0xce:
			if(p8088->o)
			{
				trap(p8088,4,1);
				prefetch_flush(p8088);
				cycles+=53;
			}
			else
			{
				cycles+=4;
			}
			name_opcode="into";
			break;

		//iret
		case 0xcf:
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;

			p8088->IP=pop(p8088);
			p8088->CS=pop(p8088);
			result=pop(p8088);
			p8088->c=(result>>0)&1;
			p8088->p=(result>>2)&1;
			p8088->ac=(result>>4)&1;
			p8088->z=(result>>6)&1;
			p8088->s=(result>>7)&1;
			p8088->t=(result>>8)&1;
			p8088->i=(result>>9)&1;
			p8088->d=(result>>10)&1;
			p8088->o=(result>>11)&1;
			p8088->enable_interrupts=0;
			prefetch_flush(p8088);
			cycles+=32;
			name_opcode="iret";
			break;

		case 0xd0:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			switch((modrm>>3)&7)
			{
			//rol r/m8 1
			case 0:
				result = (value1<<1) | (value1>>7);
				p8088->c=(value1&0x80)? 1:0;
				name_opcode="rol r/m8 1";
				break;

			//ror r/m8 1
			case 1:
				result = (value1>>1) | (value1<<7);
				p8088->c=(value1&1);
				name_opcode="ror r/m8 1";
				break;

			//rcl r/m8 1
			case 2:
				result = (value1<<1) | p8088->c;
				p8088->c=(value1&0x80)? 1:0;
				name_opcode="rcl r/m8 1";
				break;

			//rcr r/m8 1
			case 3:
				result = (value1>>1) | (p8088->c<<7);
				p8088->c=(value1&1);
				name_opcode="rcr r/m8 1";
				break;

			//shl r/m8 1
			case 4:
				result = (value1<<1);
				p8088->c=(value1&0x80)? 1:0;
				set_flags_zsp_8(p8088,result);
				name_opcode="shl r/m8 1";
				break;

			//shr r/m8 1
			case 5:
				result = (value1>>1);
				p8088->c=(value1&1);
				set_flags_zsp_8(p8088,result);
				name_opcode="shr r/m8 1";
				break;

			case 6:
				undefined_instruction(p8088,opcode);
				break;

			//sar r/m8 1
			case 7:
				result = (value1>>1) | (value1&0x80);
				p8088->c=(value1&1);
				set_flags_zsp_8(p8088,result);
				name_opcode="sar r/m8 1";
				break;
			}
			p8088->o = ((result^value1)&0x80)!=0? 1:0;
			writeback_rm_8(p8088,result);
			cycles+=(mod==3)?2:15;
			break;

		case 0xd1:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			switch((modrm>>3)&7)
			{
			//rol r/m16 1
			case 0:
				result = (value1<<1) | (value1>>15);
				p8088->c=(value1&0x8000)? 1:0;
				name_opcode="rol r/m16 1";
				break;

			//ror r/m16 1
			case 1:
				result = (value1>>1) | (value1<<15);
				p8088->c=(value1&1);
				name_opcode="ror r/m16 1";
				break;

			//rcl r/m16 1
			case 2:
				result = (value1<<1) | p8088->c;
				p8088->c=(value1&0x8000)? 1:0;
				name_opcode="rcl r/m16 1";
				break;

			//rcr r/m16 1
			case 3:
				result = (value1>>1) | (p8088->c<<15);
				p8088->c=(value1&1);
				name_opcode="rcr r/m16 1";
				break;

			//shl r/m16 1
			case 4:
				result = (value1<<1);
				p8088->c=(value1&0x8000)? 1:0;
				set_flags_zsp_16(p8088,result);
				name_opcode="shl r/m16 1";
				break;

			//shr r/m16 1
			case 5:
				result = (value1>>1);
				p8088->c=(value1&1);
				set_flags_zsp_16(p8088,result);
				name_opcode="shr r/m16 1";
				break;

			case 6:
				undefined_instruction(p8088,opcode);
				break;

			//sar r/m16 1
			case 7:
				result = (value1>>1) | (value1&0x8000);
				p8088->c=(value1&1);
				set_flags_zsp_16(p8088,result);
				name_opcode="sar r/m16 1";
				break;
			}
			p8088->o = ((result^value1)&0x8000)!=0? 1:0;
			writeback_rm_16(p8088,result);
			cycles+=(mod==3)?2:15;
			break;

		case 0xd2:
			decode_ea(p8088);
			value1=decode_rm_8(p8088);
			switch((modrm>>3)&7)
			{
				case 0: name_opcode="rol r/m8 cl"; break;
				case 1: name_opcode="ror r/m8 cl"; break;
				case 2: name_opcode="rcl r/m8 cl"; break;
				case 3: name_opcode="rcr r/m8 cl"; break;
				case 4: name_opcode="shl r/m8 cl"; break;
				case 5: name_opcode="shr r/m8 cl"; break;
				case 7: name_opcode="sar r/m8 cl"; break;
			}
			cycles+=(mod==3)?8:20;

			if(p8088->CL==0)
				break;

			switch((modrm>>3)&7)
			{
			//rol r/m8 cl
			case 0:
				result = (value1<<(p8088->CL&7)) | (value1>>(8-(p8088->CL&7)));
				p8088->c=(result&1);
				break;

			//ror r/m8 cl
			case 1:
				result = (value1>>(p8088->CL&7)) | (value1<<(8-(p8088->CL&7)));
				p8088->c=(result&0x80)!=0? 1:0;
				break;

			//rcl r/m8 cl
			case 2:
				value1|=p8088->c<<8;
				result = (value1<<(p8088->CL%9)) | (value1>>(9-(p8088->CL%9)));
				p8088->c=(result&0x100)!=0? 1:0;
				break;

			//rcr r/m8 cl
			case 3:
				value1|=p8088->c<<8;
				result = (value1>>(p8088->CL%9)) | (value1<<(9-(p8088->CL%9)));
				p8088->c=(result&0x100)!=0? 1:0;
				break;

			//shl r/m8 cl
			case 4:
				result = (p8088->CL>8)? 0:(value1<<p8088->CL);
				p8088->c=(result&0x100)!=0? 1:0;
				set_flags_zsp_8(p8088,result);
				break;

			//shr r/m8 cl
			case 5:
				result = (p8088->CL>8)? 0:(value1>>(p8088->CL-1));
				p8088->c=(result&1);
				result=result>>1;
				set_flags_zsp_8(p8088,result);
				break;

			case 6:
				undefined_instruction(p8088,opcode);
				break;

			//sar r/m8 cl
			case 7:
				value1 |= (value1&0x80)? 0xff00: 0;
				result = value1 >> ((p8088->CL>=8)? 7:(p8088->CL-1));
				p8088->c=(result&1);
				result=(result>>1)&0xff;
				set_flags_zsp_8(p8088,result);
				break;
			}
			p8088->o = ((result^value1)&0x80)!=0? 1:0;
			writeback_rm_8(p8088,result);
			cycles+=4*p8088->CL;
			break;

		case 0xd3:
			decode_ea(p8088);
			value1=decode_rm_16(p8088);
			switch((modrm>>3)&7)
			{
				case 0: name_opcode="rol r/m16 cl"; break;
				case 1: name_opcode="ror r/m16 cl"; break;
				case 2: name_opcode="rcl r/m16 cl"; break;
				case 3: name_opcode="rcr r/m16 cl"; break;
				case 4: name_opcode="shl r/m16 cl"; break;
				case 5: name_opcode="shr r/m16 cl"; break;
				case 7: name_opcode="sar r/m16 cl"; break;
			}
			cycles+=(mod==3)?8:20;

			if(p8088->CL==0)
				break;

			switch((modrm>>3)&7)
			{
			//rol r/m16 cl
			case 0:
				result = (value1<<(p8088->CL&15)) | (value1>>(16-(p8088->CL&15)));
				p8088->c=(result&1);
				break;

			//ror r/m16 cl
			case 1:
				result = (value1>>(p8088->CL&15)) | (value1<<(16-(p8088->CL&15)));
				p8088->c=(result&0x8000)!=0? 1:0;
				break;

			//rcl r/m16 cl
			case 2:
				{
				unsigned long v=value1;
				unsigned long r;
				v|=(unsigned long)p8088->c<<16;
				r = (v<<(p8088->CL%17)) | (v>>(17-(p8088->CL%9)));
				p8088->c=(r&0x10000)!=0? 1:0;
				value1=(unsigned int)v;
				result=(unsigned int)r;
				}
				break;

			//rcr r/m16 cl
			case 3:
				{
				unsigned long v=value1;
				unsigned long r;
				v|=p8088->c<<16;
				r = (v>>(p8088->CL%17)) | (v<<(17-(p8088->CL%9)));
				p8088->c=(r&0x10000)!=0? 1:0;
				value1=(unsigned int)v;
				result=(unsigned int)r;
				}
				break;

			//shl r/m16 cl
			case 4:
				result = (p8088->CL>16)? 0:(value1<<p8088->CL);
				p8088->c=(result&0x10000)!=0? 1:0;
				set_flags_zsp_16(p8088,result);
				break;

			//shr r/m16 cl
			case 5:
				result = (p8088->CL>16)? 0:(value1>>(p8088->CL-1));
				p8088->c=(result&1);
				result=result>>1;
				set_flags_zsp_16(p8088,result);
				break;

			case 6:
				undefined_instruction(p8088,opcode);
				break;

			//sar r/m16 cl
			case 7:
				value1 |= (value1&0x8000)? 0xffff0000: 0;
				result = value1 >> ((p8088->CL>=16)? 15:(p8088->CL-1));
				p8088->c=(result&1);
				result=(result>>1)&0xffff;
				set_flags_zsp_16(p8088,result);
				break;
			}
			p8088->o = ((result^value1)&0x8000)!=0? 1:0;
			writeback_rm_16(p8088,result);
			cycles+=4*p8088->CL;
			break;

		//aam
		case 0xd4:
			name_opcode="aam imm8";
			value2=fetch_x86(p8088);
			if(value2==0)
			{
				trap(p8088,0,1);
				break;
			}
			value1=p8088->AL;
			result=(((value1/value2)&0xff)<<8) | ((value1%value2)&0xff);
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			set_flags_zsp_16(p8088,result);
			cycles+=83;
			break;

		//aad
		case 0xd5:
			value2=fetch_x86(p8088);
			result=p8088->AH*value2+p8088->AL;
			p8088->AL=result&0xff;
			p8088->AH=(result>>8)&0xff;
			set_flags_zsp_16(p8088,result);
			name_opcode="aad imm8";
			cycles+=60;
			break;

		//salc
		case 0xd6:
			if(p8088->c)
				p8088->AL=0xff;
			else
				p8088->AL=0;
			cycles+=3;
			name_opcode="salc";
			break;

		//xlat
		case 0xd7:
			result=p8088->AL;
			if((p8088->AL&0x80)!=0)
				result|=0xff00;
			result+=(p8088->BH<<8)|p8088->BL;
			result&=0xffff;
			result=(result+(seg(p8088,p8088->DS)<<4))&0xfffff;
			p8088->AL=memory_read_x86(p8088,result);
			name_opcode="xlat";
			cycles+=11;
			break;

		//esc
		case 0xd8: case 0xd9: case 0xda: case 0xdb: case 0xdc: case 0xdd: case 0xde: case 0xdf:
			name_opcode="esc";
			decode_ea(p8088);
			cycles+=2;
//			trap(p8088,7);
//			cycles+=50;
			break;

		//loopnz
		case 0xe0:
			name_opcode="loopnz imm8";
			value1=fetch_x86(p8088);
			if((value1&0x80)!=0)
				value1|=0xff00;
			result=p8088->CL+(p8088->CH<<8);
			result=result-1;
			p8088->CL=result&0xff;
			p8088->CH=(result>>8)&0xff;
			if (result!=0 && p8088->z==0)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+value1)&0xffff;
				cycles+=19;
				prefetch_flush(p8088);
			}
			else
				cycles+=5;
			break;

		//loopz
		case 0xe1:
			name_opcode="loopz imm8";
			value1=fetch_x86(p8088);
			if((value1&0x80)!=0)
				value1|=0xff00;
			result=p8088->CL+(p8088->CH<<8);
			result=result-1;
			p8088->CL=result&0xff;
			p8088->CH=(result>>8)&0xff;
			if (result!=0 && p8088->z!=0)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+value1)&0xffff;
				cycles+=18;
				prefetch_flush(p8088);
			}
			else
				cycles+=5;
			break;

		//loop
		case 0xe2:
			name_opcode="loop imm8";
			value1=fetch_x86(p8088);
			if((value1&0x80)!=0)
				value1|=0xff00;
			result=p8088->CL+(p8088->CH<<8);
			result=result-1;
			p8088->CL=result&0xff;
			p8088->CH=(result>>8)&0xff;
			if (result!=0)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+value1)&0xffff;
				cycles+=18;
				prefetch_flush(p8088);
			}
			else
				cycles+=5;
			break;

		//jcxz imm8
		case 0xe3:
			name_opcode="jcxz imm8";
			value1=fetch_x86(p8088);
			if((value1&0x80)!=0)
				value1|=0xff00;
			if((p8088->CL|(p8088->CH<<8))==0)
			{
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=(p8088->IP+value1)&0xffff;
				cycles+=18;
				prefetch_flush(p8088);
			}
			else
				cycles+=6;
			break;

		//in al, imm8
		case 0xe4:
			name_opcode="in al imm8";
			result=fetch_x86(p8088);
			p8088->AL=port_read_x86(p8088,result)&0xff;
			cycles+=11;
			break;

		//in ax, imm8
		case 0xe5:
			name_opcode="in ax imm8";
			result=fetch_x86(p8088);
			p8088->AL=port_read_x86(p8088,result)&0xff;
			p8088->AH=port_read_x86(p8088,result)&0xff;
			cycles+=15;
			break;

		//out al, imm8
		case 0xe6:
			name_opcode="out al imm8";
			result=fetch_x86(p8088);
			value2 = result;
			port_write_x86(p8088,result,p8088->AL);
			value1 = p8088->AL;
			cycles+=11;
			break;

		//out ax, imm8
		case 0xe7:
			name_opcode="out ax imm8";
			result=fetch_x86(p8088);
			port_write_x86(p8088,result,p8088->AL);
			port_write_x86(p8088,result,p8088->AH);
			cycles+=11;
			break;

		//call imm16
		case 0xe8:
			name_opcode="call imm16";
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			push(p8088,p8088->IP);
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=(p8088->IP+result)&0xffff;
			prefetch_flush(p8088);
			cycles+=19;
			break;

		//jmp imm16
		case 0xe9:
			name_opcode="jmp imm16";
			result=fetch_x86(p8088);
			result|=fetch_x86(p8088)<<8;
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=(p8088->IP+result)&0xffff;
			prefetch_flush(p8088);
			cycles+=15;
			break;

		//jmp imm32
		case 0xea:
		{
			name_opcode="jmp imm32";
			unsigned int ip,cs;
			ip=fetch_x86(p8088);
			ip|=fetch_x86(p8088)<<8;
			cs=fetch_x86(p8088);
			cs|=fetch_x86(p8088)<<8;
			p8088->CSbeforeflush=p8088->CS;
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=ip&0xffff;
			p8088->CS=cs&0xffff;
			prefetch_flush(p8088);
			cycles+=15;
		}
			break;

		//jmp imm8
		case 0xeb:
			name_opcode="jmp imm8";
			result=fetch_x86(p8088);
			if((result&0x80)!=0)
				result|=0xff00;
			p8088->IPbeforeflush=p8088->IP;
			p8088->IP=(p8088->IP+result)&0xffff;
			prefetch_flush(p8088);
			cycles+=15;
			break;

		//in al, dx
		case 0xec:
			name_opcode="in al dx";
			result=(p8088->DH<<8)|p8088->DL;
			p8088->AL=port_read_x86(p8088,result)&0xff;
			cycles+=9;
			break;

		//in ax, dx
		case 0xed:
			name_opcode="in ax dx";
			result=(p8088->DH<<8)|p8088->DL;
			p8088->AL=port_read_x86(p8088,result)&0xff;
			p8088->AH=port_read_x86(p8088,result)&0xff;
			cycles+=13;
			break;

		//out al, dx
		case 0xee:
			name_opcode="out al dx";
			result=(p8088->DH<<8)|p8088->DL;
			port_write_x86(p8088,result,p8088->AL);
			cycles+=9;
			break;

		//out ax, dx
		case 0xef:
			name_opcode="out ax dx";
			result=(p8088->DH<<8)|p8088->DL;
			port_write_x86(p8088,result,p8088->AL);
			port_write_x86(p8088,result,p8088->AH);
			cycles+=9;
			break;

		//halt
		case 0xf4:
			p8088->halt=1;
			name_opcode="hlt";
			cycles+=2;
			break;

		//cmc
		case 0xf5:
			p8088->c=1-p8088->c;
			name_opcode="cmc";
			cycles+=2;
			break;

		case 0xf6:
			decode_ea(p8088);
			switch((modrm>>3)&7)
			{
			//test r/m8 imm8
			case 0:
				value1=decode_rm_8(p8088);
				value2=fetch_x86(p8088);
				set_flags_logic_8(p8088,value1,value2,value1&value2);
				cycles+=(mod==3)?5:11;
				name_opcode="test r/m8 imm8";
				break;

			//not r/m8
			case 2:
				value1=decode_rm_8(p8088);
				result=(~value1)&0xff;
				writeback_rm_8(p8088,result);
				cycles+=(mod==3)?3:16;
				name_opcode="not r/m8";
				break;

			//neg r/m8
			case 3:
				value1=decode_rm_8(p8088);
				result=(~value1+1)&0xff;
				writeback_rm_8(p8088,result);
//				set_flags_sub_8(p8088,0,value1,result);
				set_flags_sub_8(p8088,0,value1,0-value1);
				cycles+=(mod==3)?3:16;
				name_opcode="neg r/m8";
				break;

			//mul r/m8
			case 4:
				value1=decode_rm_8(p8088);
				result=(value1*p8088->AL)&0xffff;
				p8088->AL=result&0xff;
				p8088->AH=result>>8;
				p8088->o=(result&0xff00)!=0? 1:0;
				p8088->c=(result&0xff00)!=0? 1:0;
				p8088->z=(result==0)?1:0;
				cycles+=(mod==3)? 73:79;
				name_opcode="mul r/m8";
				break;

			//imul r/m8
			case 5:
				{
				unsigned long v,r;
				r=decode_rm_8(p8088);
				if((r&0x80)!=0)
					r|=0xff00;
				v=p8088->AL;
				if((v&0x80)!=0)
					v|=0xff00;
				r=(r*v)&0xffff;
				p8088->AL=r&0xff;
				p8088->AH=r>>8;
				r&=0xff00;
				p8088->o=(r!=0xff00 && r!=0)?1:0;
				p8088->c=(r!=0xff00 && r!=0)?1:0;
				cycles+=(mod==3)? 89:95;
				name_opcode="imul r/m8";
				}
				break;

			//div r/m8
			case 6:
				name_opcode="div r/m8";
				value2=decode_rm_8(p8088);
				if(value2==0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				result=(p8088->AH<<8)|p8088->AL;
				result=result / value2;
				if((result&0xff00)!=0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				value1=(p8088->AH<<8)|p8088->AL;
				p8088->AH = value1 % value2;
				p8088->AL = result&0xff;
				cycles+=(mod==3)? 85:91;
				break;

			//idiv r/m8
			case 7:
			{
				unsigned int sign1,sign2;
				unsigned int r1,r2;
				name_opcode="idiv r/m8";
				value1=(p8088->AH<<8)|p8088->AL;
				value2=decode_rm_8(p8088);
				if((value2&0x80)!=0)
					value2|=0xff00;
				if(value2==0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				sign1=(value1&0x8000)!=0? 1:0;
				sign2=(value2&0x8000)!=0? 1:0;
				if(sign1)
					value1=(~value1+1)&0xffff;
				if(sign2)
					value2=(~value2+1)&0xffff;
				r1=value1 / value2;
				r2=value1 % value2;
				if(sign1!=sign2)
				{
					if(r1>0x80)
					{
						cycles+=(mod==3)? 16:20;
						trap(p8088,0,1);
						break;
					}
					r1=(~r1+1)&0xff;
				}
				else
				{
					if(r1>0x7f)
					{
						cycles+=(mod==3)? 16:20;
						trap(p8088,0,1);
						break;
					}
				}
				if(sign1)
					r2=(~r2+1)&0xff;
				p8088->AH=r2;
				p8088->AL=r1;
				cycles+=(mod==3)? 106:112;

			}
				break;

			default:
				undefined_instruction(p8088,opcode);
				break;
			}
			break;

		case 0xf7:
			decode_ea(p8088);
			switch((modrm>>3)&7)
			{
			//test r/m16 imm16
			case 0:
				value1=decode_rm_16(p8088);
				value2=fetch_x86(p8088);
				value2|=fetch_x86(p8088)<<8;
				set_flags_logic_16(p8088,value1,value2,value1&value2);
				cycles+=(mod==3)?5:11;
				name_opcode="test r/m16 imm16";
				break;

			//not r/m16
			case 2:
				value1=decode_rm_16(p8088);
				result=(~value1)&0xffff;
				writeback_rm_16(p8088,result);
				cycles+=(mod==3)?3:16;
				name_opcode="not r/m16";
				break;

			//neg r/m16
			case 3:
				value1=decode_rm_16(p8088);
				result=(~value1+1)&0xffff;
				writeback_rm_16(p8088,result);
				set_flags_sub_16(p8088,0,value1,0-value1);
				cycles+=(mod==3)?3:16;
				name_opcode="neg r/m16";
				break;

			//mul r/m16
			case 4:
			{
				unsigned long r;
				value1=decode_rm_16(p8088);
				r=(value1*((p8088->AH<<8)|p8088->AL))&0xffffffff;
				p8088->AL=r&0xff;
				p8088->AH=(r>>8)&0xff;
				p8088->DL=(r>>16)&0xff;
				p8088->DH=(r>>24)&0xff;
				p8088->o=(r&0xffff0000)!=0? 1:0;
				p8088->c=(r&0xffff0000)!=0? 1:0;
				p8088->z=(r==0)?1:0;
				cycles+=(mod==3)? 73:79;
				name_opcode="mul r/m16";
				break;
			}
			//imul r/m16
			case 5:
				{
				unsigned long v1,v2,r;
				v1=decode_rm_16(p8088);
				if((v1&0x8000)!=0)
					v1|=0xffff0000;
				v2=p8088->AL|(p8088->AH<<8);
				if((v2&0x8000)!=0)
					v2|=0xffff0000;
				r=(v1*v2)&0xffffffff;
				p8088->AL=r&0xff;
				p8088->AH=(r>>8)&0xff;
				p8088->DL=(r>>16)&0xff;
				p8088->DH=(r>>24)&0xff;
				r&=0xffff0000;
				p8088->o=(r!=0xffff0000 && r!=0)?1:0;
				p8088->c=(r!=0xffff0000 && r!=0)?1:0;
				cycles+=(mod==3)? 141:147;
				name_opcode="imul r/m16";
				}
				break;

			//div r/m8
			case 6:
			{
				unsigned long v1,v2,r;
				name_opcode="div r/m16";
				v2=decode_rm_16(p8088);
				if(v2==0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				v1=(p8088->DH<<24)|(p8088->DL<<16)|(p8088->AH<<8)|p8088->AL;
				r=v1 / v2;
				if((r&0xffff0000)!=0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				p8088->DL = (v1 % v2)&0xff;
				p8088->DH = ((v1 % v2)>>8)&0xff;
				p8088->AL = r&0xff;
				p8088->AH = (r>>8)&0xff;
				cycles+=(mod==3)? 153:159;
			}
				break;

			//idiv r/m16
			case 7:
			{
				unsigned int sign1,sign2;
				unsigned long r1,r2,v1,v2;

				name_opcode="idiv r/m16";
				v1=(p8088->DH<<24)|(p8088->DL<<16)|(p8088->AH<<8)|p8088->AL;
				v2=decode_rm_16(p8088);
				if((v2&0x8000)!=0)
					v2|=0xffff0000;
				if(v2==0)
				{
					cycles+=(mod==3)? 16:20;
					trap(p8088,0,1);
					break;
				}
				sign1=(value1&0x80000000)!=0? 1:0;
				sign2=(value2&0x80000000)!=0? 1:0;
				if(sign1)
					v1=(~v1+1)&0xffffffff;
				if(sign2)
					v2=(~v2+1)&0xffffffff;
				r1=v1 / v2;
				r2=v1 % v2;
				if(sign1!=sign2)
				{
					if(r1>0x8000)
					{
						cycles+=(mod==3)? 16:20;
						trap(p8088,0,1);
						break;
					}
					r1=(~r1+1)&0xffff;
				}
				else
				{
					if(r1>0x7fff)
					{
						cycles+=(mod==3)? 16:20;
						trap(p8088,0,1);
						break;
					}
				}
				if(sign1)
					r2=(~r2+1)&0xffff;
				p8088->AH=(r1>>8)&0xff;
				p8088->AL=r1&0xff;
				p8088->DH=(r2>>8)&0xff;
				p8088->DL=r2&0xff;
				cycles+=(mod==3)? 174:180;

			}
				break;

			default:
				undefined_instruction(p8088,opcode);
				break;
			}
			break;

		//clc
		case 0xf8:
			p8088->c=0;
			name_opcode="clc";
			cycles+=2;
			break;

		//stc
		case 0xf9:
			p8088->c=1;
			name_opcode="stc";
			cycles+=2;
			break;

		//cli
		case 0xfa:
			p8088->i=0;
			name_opcode="cli";
			cycles+=2;
			break;

		//sti
		case 0xfb:
			p8088->i=1;
			name_opcode="sti";
			cycles+=2;
			break;

		//cld
		case 0xfc:
			p8088->d=0;
			name_opcode="cld";
			cycles+=2;
			break;

		//std
		case 0xfd:
			p8088->d=1;
			name_opcode="std";
			cycles+=2;
			break;

		case 0xfe:
			decode_ea(p8088);
			switch((modrm>>3)&7)
			{
			//inc r/m8
			case 0:
				value1=decode_rm_8(p8088);
				result=value1+1;
				writeback_rm_8(p8088,result);
				value2=p8088->c;
				set_flags_add_8(p8088,value1,1,result);
				p8088->c=value2;
				name_opcode="inc r/m8";
				cycles+=(mod==3)? 3:15;
				break;

			//dec r/m8
			case 1:
				value1=decode_rm_8(p8088);
				result=value1-1;
				writeback_rm_8(p8088,result);
				value2=p8088->c;
				set_flags_sub_8(p8088,value1,1,result);
				p8088->c=value2;
				name_opcode="dec r/m8";
				cycles+=(mod==3)? 3:15;
				break;
			default:
				undefined_instruction(p8088,opcode);
				break;
			}
			break;

		case 0xff:
		{
			unsigned long v1,v2,r1;

			decode_ea(p8088);

			switch((modrm>>3)&7)
			{
			//inc r/m16
			case 0:
				v1=decode_rm_16(p8088);
				r1=v1+1;
				writeback_rm_16(p8088,r1);
				v2=p8088->c;
				set_flags_add_16(p8088,v1,1,r1);
				p8088->c=v2;
				cycles+=(mod==3)? 3:15;
				name_opcode="inc r/m16";
				break;

			//dec r/m16
			case 1:
				v1=decode_rm_16(p8088);
				r1=v1-1;
				writeback_rm_16(p8088,r1);
				v2=p8088->c;
				set_flags_sub_16(p8088,v1,1,r1);
				p8088->c=v2;
				cycles+=(mod==3)? 3:15;
				name_opcode="dec r/m16";
				break;

			//call r/m16
			case 2:
				v1=decode_rm_16(p8088);
				push(p8088,p8088->IP);
				p8088->IPbeforeflush=p8088->IP;
				p8088->IP=v1;
				prefetch_flush(p8088);
				cycles+=(mod==3)? 16:21;
				name_opcode="call r/m16";
				break;

			//call m32
			case 3:
			{
				name_opcode="call m32";
				if(mod==3)
					break;
				unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
				if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
				switch(segprefix)
				{
				case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
				case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
				case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
				case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
				}
				push(p8088,p8088->CS);
				push(p8088,p8088->IP);
				p8088->CSbeforeflush=p8088->CS;
				p8088->IPbeforeflush=p8088->IP;
				p8088->IP=memory_read_x86(p8088,addr);
				p8088->IP|=(memory_read_x86(p8088,(addr+1)&0xfffff)<<8);
				p8088->CS=memory_read_x86(p8088,(addr+2)&0xfffff);
				p8088->CS|=(memory_read_x86(p8088,(addr+3)&0xfffff)<<8);
				prefetch_flush(p8088);
				cycles+=37;
				break;
			}

			//jmp r/m16
			case 4:
				name_opcode="jmp r/m16";
				p8088->IPbeforeflush=p8088->IP;
				p8088->IP=decode_rm_16(p8088);
				prefetch_flush(p8088);
				cycles+=(mod==3)? 11:18;
				break;

			//jmp m32
			case 5:
			{
				name_opcode="jmp m32";
				if(mod==3)
					break;
				unsigned int addr=((p8088->DS<<4)+ea)&0xfffff;
				if(eaUsesStack) addr=((p8088->SS<<4)+ea)&0xfffff;
				switch(segprefix)
				{
				case 0x26: addr=((p8088->ES<<4)+ea)&0xfffff; break;
				case 0x2e: addr=((p8088->CS<<4)+ea)&0xfffff; break;
				case 0x36: addr=((p8088->SS<<4)+ea)&0xfffff; break;
				case 0x3e: addr=((p8088->DS<<4)+ea)&0xfffff; break;
				}
				p8088->CSbeforeflush=p8088->CS;
				p8088->IPbeforeflush=p8088->IP;

				p8088->IP=memory_read_x86(p8088,addr);
				p8088->IP|=(memory_read_x86(p8088,(addr+1)&0xfffff)<<8);
				p8088->CS=memory_read_x86(p8088,(addr+2)&0xfffff);
				p8088->CS|=(memory_read_x86(p8088,(addr+3)&0xfffff)<<8);
				prefetch_flush(p8088);
				cycles+=37;
				break;
			}

			//push r/m16
			case 6:
				name_opcode="push r/m16";
				cycles+=(mod==3)? 10:16;
				push(p8088,decode_rm_16(p8088));
				break;

			case 7:
				undefined_instruction(p8088,opcode);
				break;
			}
		}
		break;

		default:
			undefined_instruction(p8088,opcode);
			printf("address CS=%x, IP=%x",p8088->CS,p8088->IP);
	}
	p8088->operand1=value1;
	p8088->operand2=value2;
	p8088->op_result=result;
	p8088->cycles=cycles;
	p8088->name_opcode=name_opcode;

	handleOutstandingInterrupts(p8088);
}
