// WD1797 Implementation
// By: Joe Matta
// email: jmatta1980@hotmail.com
// November 2020
// Copyright (C) 2020

// expanded by Margaret Black, 2024

// jwd1797.h

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

typedef struct {

unsigned char dataShiftRegister;
/* during the SEEK command, the dataRegister holds the address of the desired
  track position - otherwise it holds the assembled byte read from or writen
  to the dataShiftRegister */
unsigned char dataRegister;
/* holds track number of current read/write head position, increment every
  step-in toward track 76, and decrement every step-out toward track 00 */
unsigned char trackRegister;  // do not load when device is busy
/* holds desired sector position */
unsigned char sectorRegister; // do not load when device is busy
/* holds command currently being executed */
unsigned char commandRegister; // do not load when device is busy - except force int
/* holds device status information relevant to the previously executed command */
unsigned char statusRegister;
unsigned char CRCRegister;
unsigned char controlLatch;
unsigned char controlStatus;

// keep track of current byte being pointed to by the READ/WRITE head
unsigned long disk_img_index_pointer;
unsigned long rotational_byte_pointer;
unsigned long rw_start_byte;

// ready input from disk drive interface (0 = not ready, 1 = ready)
// int ready;
// step direction output to disk drive interface (0 = out->track00, 1 = in->track39)
// int stepDirection;
// step pulse output to disk drive interface (MFM - 2 microseconds, FM - 4)
// int stepPulse;

char* currentCommandName;
int currentCommandType;

// TYPE I command flags
// stepping motor rate - determined by TYPE I command bits 0 and 1
int stepRate;
int verifyFlag;
int headLoadFlag;
int trackUpdateFlag;
// TYPE II/III command flags
int dataAddressMark;
int updateSSO;
int delay15ms;
int swapSectorLength;
int multipleRecords;
// TYPE IV - interrupt condition flags
int interruptNRtoR;
int interruptRtoNR;
int interruptIndexPulse;
int interruptImmediate;

int command_action_done;  // flag indicates if the command action is done
int command_done; // flag indicating that entire command is done -
int head_settling_done;
int verify_operation_active;
int verify_operation_done;
int e_delay_done;
int start_byte_set;

int terminate_command;

// ALL timers in microseconds
double master_timer;  // for TESTING
double index_pulse_timer;
double index_encounter_timer;
double step_timer;
double verify_head_settling_timer;
double e_delay_timer;
double assemble_data_byte_timer;
unsigned int rotational_byte_read_limit;
unsigned int rotational_byte_read_timer;
unsigned int rotational_byte_read_timer_OVR;
double HLD_idle_reset_timer;
double HLT_timer;
// *
unsigned int read_track_bytes_read;

// DRIVE pins
int index_pulse_pin;
int ready_pin;
int tg43_pin;
int HLD_pin;
int HLT_pin;
int not_track00_pin;
int direction_pin;  // (0 = out->track00, 1 = in->track39)
int sso_pin;
// int not_test_pin;

int delayed_HLD;
int HLT_timer_active;
int HLD_idle_index_count;

// computer interface pins
int drq;  /* also appears as status bit 1 during read/write operations. It is set
  high when a byte is assembled and transferred to the data register to be sent
  to the processor data bus on read operations. It is cleared when the data
  register is read by the processor. On writes, this is set high when a byte is
  transferred to the data shift register and another byte is requested from the
  processor to be written to disk. It is reset (cleared) when a new byte is loaded
  into the data register to be written. */
int intrq;  // attached to I0 of the slave 8259 interrupt controller in the Z-100.
  /* It is set at the completion of every command and is reset by reading the status
    register or by loading the command register with a new command. It is also
    set when a forced interrupt condition is met. */
int not_master_reset; /* if this pin goes low for at least 50us, 0b00000011
  into the command register (RESTORE 30ms step (for 1MHz)) and the NOT READY
  status bit 7 is reset. When the not_master_reset pin goes back high, the
  RESTORE command is executed and the sector register is set to 0b00000001 */

int current_track;

unsigned int cylinders; // (tracks per side)
unsigned int num_heads; // WD1797 has two read heads, one for each side of the disk
unsigned int sectors_per_track;
unsigned int sector_length;

long disk_img_file_size;

unsigned char* formattedDiskArray;
unsigned char* sectorToWrite;
int bytesToWrite;
int actual_num_track_bytes;

// emulator internal
int new_byte_read_signal_;
int track_start_signal_;

// verification operation
int zero_byte_counter;
int a1_byte_counter;
int verify_index_count;
int address_mark_search_count;
int id_field_found;
int id_field_data_array_pt;
int id_field_data_collected;
int data_a1_byte_counter;
int data_mark_search_count;
int data_mark_found;
/* collects ID Field data
  (0: cylinders, 1: head, 2: sector, 3: sector len, 4: CRC1, 5: CRC2) */
unsigned char id_field_data[6];
/* flag to indicate that a TYPE II command (ie. READ SECTOR) has verified
  all the ID address mark data and can continue */
int ID_data_verified;
/* used for the extracted value from the sector length ID field. Also used for
  the iteration of READING/WRITING bytes of a sector*/
int intSectorLength;
int all_bytes_inputted; // indictes when an entire data field has been read
int IDAM_byte_count;  // count for collecting the 6 IDAM bytes for READ ADDRESS
int start_track_read_;

// control latch
int wait_enabled;

e8259_t* pic;

} JWD1797;

JWD1797* newJWD1797();
void resetJWD1797(JWD1797*);
void writeJWD1797(JWD1797*, unsigned int, unsigned int);
unsigned int readJWD1797(JWD1797*, unsigned int);
void doJWD1797Cycle(JWD1797*, double);
void doJWD1797Command(JWD1797*);

void commandStep(JWD1797*, double);
void printAllRegisters(JWD1797*);
void printCommandFlags(JWD1797*);
void typeIStatusReset(JWD1797*);
void setupForcedIntCommand(JWD1797*);
void setupTypeICommand(JWD1797*);
void setTypeICommand(JWD1797*);
void setupTypeIICommand(JWD1797*);
void setTypeIICommand(JWD1797*);
void setupTypeIIICommand(JWD1797*);
void setTypeIIICommand(JWD1797*);
void printBusyMsg();
void updateTG43Signal(JWD1797*);
void handleIndexPulse(JWD1797*, double);
void handleHLDIdle(JWD1797*);
void handleHLTTimer(JWD1797*, double);
unsigned char* diskImageToCharArray(char*, JWD1797*);
void assembleFormattedDiskArray(JWD1797*, char*);
unsigned char getFDiskByte(JWD1797*);
void handleVerifyHeadSettleDelay(JWD1797*, double);
int verifyIndexTimeout(JWD1797*, int);
int IDAddressMarkSearch(JWD1797*);
int verifyTrackID(JWD1797*);
int collectIDFieldData(JWD1797*);
void typeIVerifySequence(JWD1797*, double);
int typeIICmdIDVerify(JWD1797*);
int getSectorLengthFromID(JWD1797*);
int handleEDelay(JWD1797*, double);
int dataAddressMarkSearch(JWD1797*);
int verifyCRC(JWD1797*);
void updateControlStatus(JWD1797*);
void writeSector(JWD1797*);
int pr8088_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num);
int pr8085_FD1797WaitStateCondition(unsigned char opCode, unsigned char port_num);
void reloadDisk(JWD1797*);
void patchOS(char*,int);
