python3 asm80.py zboot.asm
python3 asm80.py ZCPM22.ASM
python3 asm80.py zbios.asm
dd if=/dev/zero of=cpm-disk.cpm bs=128 count=2880
dd if=zboot.com of=cpm-disk.cpm bs=128 conv=notrunc
dd if=ZCPM22.com of=cpm-disk.cpm bs=128 seek=1 conv=notrunc
dd if=zbios.com of=cpm-disk.cpm bs=128 seek=45 conv=notrunc
gcc -o format format.c
./format
