//Margaret Black, 2024
//breakpoint handling contributed by porkypiggy64, 2024

// *This file implements a command-line debugger
/* debug.c */

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

#include "mainboard.h"
#include "debug.h"

unsigned int lastaddress=0;
extern FILE* printer_out;
extern FILE* printer_in;
extern char* image_name_a;
extern char* image_name_b;

//given a character sequence, parse a hex word from it
//returns 1000000 if the sequence is invalid or terminates early
unsigned int parseDataValue(char* line)
{
	unsigned int val=0;

	for(int i=0; i<4; i++)
	{
		if(line[i]>='A' && line[i]<='F')
			val=(val<<4)|(line[i]-'A'+0xa);
		else if(line[i]>='a' && line[i]<='f')
			val=(val<<4)|(line[i]-'a'+0xa);
		else if(line[i]>='0' && line[i]<='9')
			val=(val<<4)|(line[i]-'0');
		else
		{
			if(i==0) return 1000000;
			break;
		}
	}
	return (val&0xffff);
}

//given a character sequence, parse a hex address from it
// handles both 5 digit addresses and segment:offset addresses
unsigned int parseAddress(char* line)
{
	unsigned int addr=0;
	unsigned int seg=0;
	for(int i=0; line[i]!='\0' && line[i]!=' '; i++)
	{
		char c=line[i];
		if(c==':')
		{
			seg=addr;
			addr=0;
			continue;
		}
		addr=addr*16;
		if(c>='A' && c<='Z')
			addr+=(c-'A'+0xa);
		if(c>='a' && c<='z')
			addr+=(c-'a'+0xa);
		if(c>='0' && c<='9')
			addr+=(c-'0');
	}
	return (seg*0x10+addr)&0xfffff;
}

//used in printing ascii equivelent
// if c is a printable character return it, otherwise return "."
char asciiOf(char c)
{
	if(c>=20 && c<=0x7a)
		return c;
	return '.';
}

void regedit(Z100* z, char* line)
{
	if(line[1]=='\n')
		line[1]='\0';
	else if (line[2]=='\n')
		line[2]='\0';
	printf("New value for %s? ",line);
	char d[5];
	d[1]=d[2]=d[3]=d[4]='\0';
	for(int i=0; i<5; i++)
	{
		d[i]=fgetc(stdin);
		if(d[i]=='\n')
			break;
	}
	printf("\n");
	unsigned int v=parseDataValue(d);
	if(v==1000000)
	{
		return;
	}
	if(strcmp(line,"ax")==0)
	{
		z->p8088->AH=(v>>8);
		z->p8088->AL=v&0xff;
	}
	if(strcmp(line,"bx")==0)
	{
		z->p8088->BH=(v>>8);
		z->p8088->BL=v&0xff;
	}
	if(strcmp(line,"cx")==0)
	{
		z->p8088->CH=(v>>8);
		z->p8088->CL=v&0xff;
	}
	if(strcmp(line,"dx")==0)
	{
		z->p8088->DH=(v>>8);
		z->p8088->DL=v&0xff;
	}
	if(strcmp(line,"ah")==0)
		z->p8088->AH=v&0xff;
	if(strcmp(line,"al")==0)
		z->p8088->AL=v&0xff;
	if(strcmp(line,"bh")==0)
		z->p8088->BH=v&0xff;
	if(strcmp(line,"bl")==0)
		z->p8088->BL=v&0xff;
	if(strcmp(line,"ch")==0)
		z->p8088->CH=v&0xff;
	if(strcmp(line,"cl")==0)
		z->p8088->CL=v&0xff;
	if(strcmp(line,"dh")==0)
		z->p8088->DH=v&0xff;
	if(strcmp(line,"dl")==0)
		z->p8088->DL=v&0xff;
	if(strcmp(line,"sp")==0)
		z->p8088->SP=v;
	if(strcmp(line,"bp")==0)
		z->p8088->BP=v;
	if(strcmp(line,"si")==0)
		z->p8088->SI=v;
	if(strcmp(line,"di")==0)
		z->p8088->DI=v;
	if(strcmp(line,"cs")==0)
		z->p8088->CS=v;
	if(strcmp(line,"ss")==0)
		z->p8088->SS=v;
	if(strcmp(line,"ds")==0)
		z->p8088->DS=v;
	if(strcmp(line,"es")==0)
		z->p8088->ES=v;
	if(strcmp(line,"ip")==0)
		z->p8088->IP=v;
	if(strcmp(line,"fl")==0)
	{
		z->p8088->o=(v>>8)&1;
		z->p8088->d=(v>>7)&1;
		z->p8088->i=(v>>6)&1;
		z->p8088->t=(v>>5)&1;
		z->p8088->s=(v>>4)&1;
		z->p8088->z=(v>>3)&1;
		z->p8088->ac=(v>>2)&1;
		z->p8088->p=(v>>1)&1;
		z->p8088->c=(v>>0)&1;
	}
	if(strcmp(line,"pc")==0)
		z->p8085.PC=v;
	if(strcmp(line,"a")==0)
		z->p8085.A=v&0xff;
	if(strcmp(line,"b")==0)
		z->p8085.B=v&0xff;
	if(strcmp(line,"c")==0)
		z->p8085.C=v&0xff;
	if(strcmp(line,"d")==0)
		z->p8085.D=v&0xff;
	if(strcmp(line,"e")==0)
		z->p8085.E=v&0xff;
	if(strcmp(line,"h")==0)
		z->p8085.H=v&0xff;
	if(strcmp(line,"l")==0)
		z->p8085.L=v&0xff;
	if(strcmp(line,"sp")==0)
		z->p8085.SP=v;
	if(strcmp(line,"hl")==0)
	{
		z->p8085.H=(v>>8);
		z->p8085.L=v&0xff;
	}
	if(strcmp(line,"f")==0)
	{
		z->p8085.i=(v>>5)&1;
		z->p8085.s=(v>>4)&1;
		z->p8085.z=(v>>3)&1;
		z->p8085.ac=(v>>2)&1;
		z->p8085.p=(v>>1)&1;
		z->p8085.c=(v>>0)&1;
	}
	prefetch_flush(z->p8088);
}

extern Z100* z100object;

unsigned int unassemble_memory_read(unsigned int addr, Z100* c)
{
	return z100_memory_read(addr,z100object);
}
void unassemble_memory_write(unsigned int addr, unsigned char data, Z100* c){}
unsigned int unassemble_port_read(unsigned int address, Z100* c){return 0;}
void unassemble_port_write(unsigned int address, unsigned char data, Z100* c){}

void unassemble(Z100* z, unsigned int address)
{
	address&=0xfffff;
	lastaddress=address;
	//make a dummy Z100 with a dummy 8088 and dummy callbacks
	Z100* dummyz=(Z100*)malloc(sizeof(Z100));
	dummyz->p8088=new8088(dummyz);
	dummyz->p8088->wait_state_enabled=0;
	assignCallbacks8088(dummyz->p8088,unassemble_memory_read,unassemble_memory_write,unassemble_port_read,unassemble_port_write);

	for(int i=0; i<16; i++)
	{
		reset8088(dummyz->p8088);
		dummyz->p8088->CS=(address&0xf0000)>>4;
		dummyz->p8088->IP=(address&0xffff);
		prefetch_flush(dummyz->p8088);
		unsigned int startAddr=(dummyz->p8088->CS<<4)+dummyz->p8088->IP;
		doInstruction8088(dummyz->p8088);
		unsigned int endAddr=(dummyz->p8088->CS<<4)+dummyz->p8088->IP;
		unsigned int flushAddr=(dummyz->p8088->CSbeforeflush<<4)+dummyz->p8088->IPbeforeflush;
		if(flushAddr!=startAddr)
		{
			endAddr=flushAddr;
		}
		printf("%x: ",startAddr);
		if(endAddr>startAddr && endAddr-startAddr<8)
			for(address=startAddr; address<endAddr; address++)
				printf("%x ",z100_memory_read(address,z));
		printf("\t%s\t\top=%x r1=%x r2=%x imm=%x cyc=%x",dummyz->p8088->name_opcode,
			dummyz->p8088->opcode,
			dummyz->p8088->operand1,
			dummyz->p8088->operand2,
			dummyz->p8088->immediate,
			dummyz->p8088->cycles
);
		printf("\n");
		lastaddress=address&0xfffff;
	}
}

void hexedit(Z100* z, unsigned int address)
{
	address&=0xfffff;

	while(1)
	{
		unsigned char c=z100_memory_read(address,z);
		printf("\n%05X %02X ( %c ) : ",address,c,asciiOf(c));
		char d[3];
		d[1]=d[2]='\0';
		d[0]=fgetc(stdin);
		if(d[0]!='\n' && d[0]!='\r')
			d[1]=fgetc(stdin);

		unsigned int v=parseDataValue(d);
		if(v==1000000)
		{
			break;
		}
		v=v&0xff;
		z100_memory_must_write(address,(unsigned char)v,z);
		lastaddress=address=(address+1)&0xfffff;
		fgetc(stdin);
	}
	printf("\n");
}

void hexdump(Z100* c, unsigned int address)
{
	printf("\nOffset 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F    ASCII text\n");
	for(unsigned int i=0; i<128; i+=16)
	{
		printf("%05X  ",(address+i)&0xfffff);

		for(int j=i; j<i+16; j++)
		{
			printf("%02X ",z100_memory_read((address+j)&0xfffff,c));
		}
		printf("   ");
		for(int j=i; j<i+16; j++)
		{
			printf("%c",asciiOf(z100_memory_read((address+j)&0xfffff,c)));
		}
		printf("\n");

	}
	lastaddress=(address+128)&0xfffff;
}

//read a line from console
char* inputLine(void)
{
	char* retline=NULL;
	int i=0;
	int c;
	while(1)
	{
		c=fgetc(stdin);
		if(c==EOF || c=='\n' || c=='\r')
			break;
		retline=(char*)realloc(retline, i+1);
		if(c>='A' && c<='Z')
			c+=('a'-'A');
		retline[i]=(char)c;
		i++;
	}
	if(c=='\n' || c=='\r' || i>0)
	{
		retline=(char*)realloc(retline,i+1);
		retline[i]='\0';
	}
	return retline;
}

void print8085Regs(Z100* c)
{
	printf("\n");
	P8085 p8085=c->p8085;
		printf("A=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X SP=%04X\n",
			p8085.A, p8085.B, p8085.C, p8085.D, p8085.E, p8085.H, p8085.L, p8085.SP);
		printf("carry=%X parity=%X auxcarry=%X zero=%X sign=%X i=%X m75=%X m65=%X m55=%X\n",
			p8085.c, p8085.p, p8085.ac, p8085.z, p8085.s,
			p8085.i, p8085.m75, p8085.m65, p8085.m55);
		printf("PC=%04X last instruction = %s ( %X )\n",p8085.PC,p8085.name,p8085.opcode);
}
void print8088Regs(Z100* c)
{
	printf("\n");
	P8088* p8088=c->p8088;
		printf("AX=%02X %02X BX=%02X %02X CX=%02X %02X DX=%02X %02X SP=%04X BP=%04X SI=%04X DI=%04X SS=%04X DS=%04X ES=%04X\n",
			p8088->AH, p8088->AL,p8088->BH,p8088->BL,p8088->CH,p8088->CL,p8088->DH,p8088->DL,
			p8088->SP,p8088->BP,p8088->SI,p8088->DI,p8088->SS,p8088->DS,p8088->ES);
		printf("carry=%X parity=%X auxcarry=%X zero=%X sign=%X trap=%X intr=%X dir=%X overflow=%X\n",
			p8088->c,p8088->p,p8088->ac,p8088->z,p8088->s,p8088->t,p8088->i,p8088->d,p8088->o);
		printf("CS:IP=%04X:%04X last instruction = %s ( %x )\n",
			p8088->CS,p8088->IP, p8088->name_opcode, p8088->opcode);
}

void printRegs(Z100* c)
{
	if(c->active_processor == PR8085)
	{
		print8085Regs(c);
	}
	else
	{
		print8088Regs(c);
	}
	printf("\n");
}

typedef enum
{
	NONE,
	EXECUTE,
	READ,
	WRITE,
	PORTIN,
	PORTOUT
	// REGEQU,
	// MEMEQU
} bpop_t;

typedef struct
{
	bpop_t operation;
	unsigned int value; // address, reg val, mem val, vector, port
} bpcond_t;

import_list(bpcond_t, bpcond_list);

typedef struct
{
	bpcond_list conditions;
	int active;
	int hits;
} breakpoint_t;

import_list(breakpoint_t, bp_list);

bp_list new_bplist(void)
{
	return new(bp_list);
}

breakpoint_t new_bp(void)
{
	return (breakpoint_t){ new(bpcond_list), 1, 0 };
}

void bp_new_cond(breakpoint_t *bp, bpop_t op, unsigned int value)
{
	list_add(bp->conditions, ((bpcond_t){ op, value }));
}

bp_list bps;

void bp_exec_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == EXECUTE && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					pauseS();
					printf("EXEC HIT\n");
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_read_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == READ && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					pauseS();
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_write_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == WRITE && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					pauseS();
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_in_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == PORTIN && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					pauseS();
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void bp_out_check(unsigned int address)
{
	for (size_t i = 0; i < list_size(bps); i++)
	{
		breakpoint_t bp = list_get(bps, i);
		if (bp.active)
		{
			for (size_t j = 0; j < list_size(bp.conditions); j++)
			{
				bpcond_t cond = list_get(bp.conditions, j);
				if (cond.operation == PORTOUT && cond.value == address)
				{
					bp.hits++;
					list_set(bps, i, bp);
					pauseS();
					break;	// Don't return yet, let other BPs hit.
				}
			}
		}
	}
}

void debug_init()
{
	bps = new(bp_list);
}
void debug_start(Z100* c)
{
	lastaddress=(c->p8088->CS<<4)+(c->p8088->IP);
}
void handleCommand(char* line, Z100* c)
{
	if(line[0]=='t')
	{
		int times=1;
		if(line[1]!='\0')
		{
			if(line[1]==' ') line++;
			sscanf(line+1,"%d",&times);
		}
		for(int i=0; i<times; i++)
		{
			z100singleinstruction(c);
			printRegs(c);
		}
	}
	else if(line[0]=='!')
	{
		unsigned int address=0;
		if(line[1]==' ') line++;
		address=parseAddress(line+1);
		trap(c->p8088,address,1);
	}
	else if (line[0]=='q')
	{
		exit(0);
	}
	else if (line[0]=='d')
	{
		unsigned int address=lastaddress;
		if(line[1]==' ') line++;
		if(line[1]!='\0')
			address=parseAddress(line+1);
		hexdump(c,address);
	}
	else if (line[0]=='e')
	{
		unsigned int address=lastaddress;
		if(line[1]==' ') line++;
		if(line[1]!='\0')
			address=parseAddress(line+1);
		hexedit(c,address);
	}
	else if (line[0]=='r')
	{
		if(c->active_processor == PR8085)
			printf("8085 is active processor\n");
		else
			printf("8088 is active processor\n");
		printf("ROM option: %x\n",c->romOption);
		print8085Regs(c);
		print8088Regs(c);
		if(line[1]==' ') line++;
		if(line[1]!='\0')
			regedit(c,line+1);
	}
	else if(line[0]=='i')
	{
		if(line[1]==' ') line++;
		int a=parseAddress(line+1);
		unsigned int val = z100_port_read(a,c);
		printf("\nPort %x reads %x\n",a,val);
	}
	else if (line[0]=='o')
	{
		if(line[1]==' ') line++;
		int a=parseAddress(line+1);
		line++;
		for(; line[0]!=' '; line++);
		line++;
		int d=parseAddress(line);
		printf("\nWriting %x to port %x\n",a,d);
		z100_port_write(a,d,c);
	}
	else if (line[0]=='b')
	{
		char* command=line;
			if (command[1]=='c')
			{
				bps = new(bp_list);
				return;
			}
			if (command[1] == 'l' || command[1] == 'L')
			{
				for (size_t i = 0; i < list_size(bps); i++)
				{
					printf("BP %zu:", i);
					for (size_t j = 0; j < list_size(list_get(bps, i).conditions); j++)
					{
						bpcond_t cond = list_get(list_get(bps, i).conditions, j);
						const char *op;
						switch (cond.operation)
						{
							case EXECUTE:
								op = "EXEC";
								break;
							case READ:
								op = "READ";
								break;
							case WRITE:
								op = "WRIT";
								break;
							case PORTIN:
								op = "PTIN";
								break;
							case PORTOUT:
								op = "POUT";
								break;
							default:
								op = "UNKN";
								break;
						}
						printf(" %s(%X)", op, cond.value);
					}
					printf(" [%s] [%d hit%s]\n", list_get(bps, i).active ? "ACTIVE" : "INACTIVE", list_get(bps, i).hits, (list_get(bps, i).hits == 1) ? "" : "s");
				}
				return;
			}
			if(line[2]==' ')
				line++;
			unsigned int value=parseAddress(line+2);
			if (command[1] == 'X' || command[1] == 'x')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, EXECUTE, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'r' || command[1] == 'R')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, READ, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'i')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, PORTIN, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'o')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, PORTOUT, value);
				list_add(bps, bp);
			}
			else if (command[1] == 'w' || command[1] == 'W')
			{
				breakpoint_t bp = new_bp();
				bp_new_cond(&bp, WRITE, value);
				list_add(bps, bp);
			}
	}
	else if(line[0]=='u')
	{
		if(line[1]==' ')line++;
		unsigned int address=lastaddress;
		if(line[1]!='\0')
			address=parseAddress(line+1);
		unassemble(c,address);
	}
	else if(line[0]=='f')
	{
		printf("\nImage file name: ");
		char name[100];
		scanf("%s",name);
		if(line[1]=='a')
		{
			image_name_a=(char*)malloc(100);
			strcpy(image_name_a,name);
		}
		else
		{
			image_name_b=(char*)malloc(100);
			strcpy(image_name_b,name);
		}
		reloadDisk(c->jwd1797);
		printf("\n");
	}
	else if(line[0]=='p')
	{
		printf("\nPrinter file name: ");
		char fname[100];
		scanf("%s",fname);
		if(line[1]=='i')
		{
			printer_in=fopen(fname,"rb");
		}
		else
		{
			printer_out=fopen(fname,"wb");
		}
	}
	if(line[0]=='s')
	{
		if(line[1]==' ')
		{
			switch(line[2])
			{
				case 'w':
					if(c->p8088->wait_state_enabled==1)
					{
						printf("disabling 8088 wait states\n");
						c->p8088->wait_state_enabled=0;
					}
					else
					{
						printf("enabling 8088 wait states\n");
						c->p8088->wait_state_enabled=1;
					}
			}
		}
	}

	else if (line[0]=='h' || line[0]=='?')
	{
		printf("\nCommands:\n");
		printf("B[R|W|X|I|O] xxxx:xxxx = set breakpoint at xxxx:xxxx\n");
		printf("BC = clear breakpoints\n");
		printf("BL = list breakpoints\n");
		printf("D [xxxxx|xxxx:xxxx] = dump from xxxxx\n");
		printf("E [xxxxx|xxxx:xxxx] = edit xxxxx\n");
		printf("F[A|B] = set floppy A/B image file\n");
		printf("G = resume running\n");
		printf("I xx = in from port xx\n");
		printf("O xx yy = out yy to port xx\n");
		printf("P[I|O] = set printer input/output file\n");
		printf("R = print registers\n");
		printf("R [AX|BX|...] = edit register AX, BX ...\n");
		printf("Q = exit\n");
		printf("T [d] = single step d times\n");
		printf("U [xxxxx|xxxx:xxxx] = unassemble from xxxxx\n");
		printf("! d = trap xx\n");
		printf("S parameter = set\n");
	}
}

void handleDebug(Z100* c)
{
	printf("instructions completed: %ld\n",c->instructions_done);
	printf("simulated time (us): %f\n",c->total_time_elapsed);

	printRegs(c);

	printf(">");
	char* line = inputLine();
	while(line[0] != 'g')
	{
		if(line[0]!='\0')
		{
			handleCommand(line,c);
		}
		free(line);
		printf(">");
		line=inputLine();
	}
	free(line);
	unpause();
}
