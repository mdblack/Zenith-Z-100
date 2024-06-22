#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
	unsigned char* disk;
	int disksize=0;

	if(argc<1)
	{
		printf("need input file\n");
		return 0;
	}
	FILE* cpmdisk=fopen(argv[1],"rb");
	if(cpmdisk==NULL)
	{
		printf("can't find disk.img\n");
		return 0;
	}
	fseek(cpmdisk,0,SEEK_END);
	disksize=ftell(cpmdisk);
	rewind(cpmdisk);
	disk=(unsigned char*)malloc(sizeof(unsigned char)*disksize);
	fread(disk,1,disksize,cpmdisk);
	fclose(cpmdisk);

	int i=0x1a00;
	int j=0x2000;
	while(j<disksize)
		disk[i++]=disk[j++];

	cpmdisk=fopen(argv[1],"wb");
	for(j=0; j<i; j++)
		fprintf(cpmdisk,"%c",disk[j]);
	fclose(cpmdisk);

	return 0;
}
