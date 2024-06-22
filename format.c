#include <stdio.h>
#include <stdlib.h>

int main()
{
	FILE* disk=fopen("cpm-disk.cpm","r+b");
	fseek(disk,0x1a00,SEEK_SET);
	for(int i=0; i<64*10; i++)
	{
		for(int j=0; j<32; j++)
		{
			fprintf(disk,"%c",0xe5);
		}
	}
	fclose(disk);
	return 0;

}
