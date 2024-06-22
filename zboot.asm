;http://www.gaby.de/cpm/manuals/archive/cpm22htm/axe.asm

;this is a sample cold start loader, which, when
;modified
;resides on track 00, sector 01 (the first sector on the
;diskette), we assume that the controller has loaded
;this sector into memory upon system start-up (this
;program can be keyed-in, or can exist in read-only
;memory
;beyond the address space of the cp/m version you are
;running). the cold start loader brings the cp/m system 
;into memory at"loadp" (3400h +"bias"). in a 20k 
;memory system, the value of"bias" is 000h, with 
;large
;values for increased memory sizes (see section 2). 
;after
;loading the cp/m system, the cold start loader 
;branches
;to the "boot" entry point of the bios, which begins at
; "bios" +"bias". the cold start loader is not used un-
;til the system is powered up again, as long as the bios 
;is not overwritten. the origin is assumed at 0000h, and 
;must be changed if the controller brings the cold start 
;loader into another area, or if a read-only memory 
;area
;is used.
	org	0		;base of ram in
				;cp/m
;msize	equ	48
		;min mem size in kbytes

;bias	equ	(msize-20)*1024
bias	equ	07000h		;offset from 20k
				;system

;base of ccp
;ccp	equ	3400h+bias
ccp	equ	0a400h

;bdos	equ	ccp+806h
bdos	equ	0ac06h
;base of bdos

;bios	equ	ccp+1600h
;base of the bios
bios	equ	0ba00h

biosl	equ	0300h		;length of the bios

;boot	equ	bios
boot	equ	0ba00h

;size	equ	bios+biosl-ccp
;size of cp/m
size	equ	1900h		;size of cp/m
				;system
;sects	equ	size/128
	;# of sectors to load
sects	equ	72		;# of sectors to load

;	insert 8088 commands to relocate this code to address 0
;	then switch CPUs to 8085

;cli
db 0fah
;mov al,1dh
db 0b0h,1dh
;out 0fch
db 0e6h,0fch
;mov ax,0
db 0b8h,0,0
;mov es,ax
db 8eh,0c0h
;mov ds,ax
db 8eh,0d8h
;mov ss,ax
db 8eh,0d0h
;mov cl,255
db 0b1h,0ffh
;mov di,0
db 0bfh,0,0
;mov si,400h
db 0beh,00h,04h
;loop:
;mov al,[ds:si]
db 8ah,04h
;mov [es:di],al
db 26h,88h,05h
;inc si
db 46h
;inc di
db 47h
;dec cl
db 0feh,0c9h
;jnz loop
db 75h,0f5h

;mov di,0h
db 0bfh,0h,00h
;mov al,0c3h
db 0b0h,0c3h
;mov [es:di],al
db 26h,88h,05h
;inc di
db 47h
;mov al,03ch
db 0b0h,03ch
;mov [es:di],al
db 26h,88h,05h
;inc di
db 47h
;mov al,00h
db 0b0h,0h
;mov [es:di],al
db 26h,88h,05h
;inc di
db 47h

;mov al,0
db 0b0h,0
;out 0feh
db 0e6h,0feh


;loop2:
;jmp loop2
db 0ebh,0feh

;
;	begin the load operation 

cold:
	lxi	b,2		;b=0, c=sector 2
	mvi	d,sects		;d=# sectors to
				;load
	lxi	h,ccp		;base transfer
				;address
lsect:	;load the next sector

;	insert inline code at this point to
;	read one 128 byte sector from the
;	track given in register b, sector
;	given in register c,
;	into the address given by <hl>

;one sector
	mvi	a,1
	out	0bh
;track
	mov	a,b
	out	0ch
;sector
	mov	a,c
	out	0dh
;drive
	mvi	a,0
	out	0eh
;address
	mov	a,h
	out	0ah
	mov	a,l
	out	09h
;do read
	mvi	a,20h
	out	0fh

;branch	to location "cold" if a read error occurs
;
;
;
;
;	user supplied read operation goes
;	here...
;
;
;
;
;go to next sector if load is incomplete
	dcr	d		;sects=sects-1
	jz	boot		;head. for the bios

;	more sectors to load
;

;we aren't using a stack, so use <sp> as scratch
;register
;	to hold the load address increment
	lxi	sp,128		;128 bytes per
				;sector
	dad	sp		;<hl> = <hl> + 128
	inr	c		;sector=sector + 1
	mov	a,c
	cpi	27		;last sector of
				;track?
	jc	lsect		;no, go read
				;another

;end of track, increment to next track

	mvi	c,1		;sector = 1
	inr	b		;track = track + 1
	jmp	lsect		;for another group
	end			;of boot loader
