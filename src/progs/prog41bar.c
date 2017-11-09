/* prog41bar.c -- Produce HP41 program barcode */
/* 2001 A. R. Duell, and placed under the GPL */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include "config.h"

/* This program reads in an HP41 binary program on standard input and 
   writes the corresponding barcode file to standard output. HP41
   program barcode contains the functions coded using the same binary
   codes as in the binary program file (and, indeed, in HP41 memory), so 
   all (?) that has to be done is to calculate the header bytes for each 
   row, and correct the END instruction

   The format of HP41 program barcode is given in 'Creating your own
   HP-41 Bar Code' 
   
   Basically, each row of barcode consists of : 
   Running checksum (1 byte)
   Type code (=1 for program barcode) (1 nybble)
   Sequence number (incremented after every row) (1 nybble)
   Number of bytes at the start of this row that continue an instruction
   at the end of the previous row (1 nybble)
   Number of bytes at the end of this row that start an instruction continued
   in the next row (1 nybble)
   HP41 instruction bytes, using the 'standard' code for the 
   instructions (1 to 13 bytes) */

/* MEMORY_SIZE is larger than any real HP41's memory, so any valid
   program will fit */
#define MEMORY_SIZE 4096
/* HP41 memory (allow 3 more bytes to patch end if necessary) */
unsigned char memory[MEMORY_SIZE+3];

/* Maximum number of instruction bytes in a row of barcode */
#define LINE_LENGTH 13

#ifndef min
#define min(A,B) ((A)<(B) ? (A) : (B))
#endif

int read_prog(void)
/* Read in HP41 program from standard input and return length */
  {
    int byte_counter; /* current address */
    int byte; /* byte read */

    byte_counter=0; /* start loading at start of memory */
    while (((byte=getchar())!=EOF) && (byte_counter<MEMORY_SIZE))
      {
        memory[byte_counter++]=byte;
      }
    return(byte_counter); /* number of bytes read */
  }

void patch_end(int *program_length)
/* The END instruction of the program must be changed to indicate the 
   GTOs are not compiled. If there is no END, then one must be added */
  {
    if(((memory[(*program_length)-3]&0xF0)==0xC0) && 
        (!(memory[(*program_length)-1]&0x80)))
      {
        /* There is an END, correct it */
        memory[(*program_length)-3]=0xC0;
        memory[(*program_length)-2]=0;
        memory[(*program_length)-1]=0x2F;
      }
    else
      {
        /* There is no END -- add one */
        memory[(*program_length)++]=0xC0;
        memory[(*program_length)++]=0;
        memory[(*program_length)++]=0x2F;
        /* Program is now 3 bytes longer */
      }
  }

void add_to_checksum(int *checksum, unsigned char data)
/* Add data to checksum using end-around carry */
  {
    (*checksum) += data; /* Add the new data */
    if((*checksum)>255)
      {
        /* There is a carry out of the MSB */
        (*checksum)++; /* add it end-around */
      }
    (*checksum) &= 255; /* Keep only low byte */
  }


void output_row(int row, unsigned char n_start, unsigned char n_end, int pc,
                unsigned char length, int  *checksum)
/* Output one row of barcode given all the paramters */
  {
    unsigned char type_byte; /* type and sequence code */
    unsigned char overflow; /* left over bytes at start and end */
    int i; /* loop counter */

    type_byte=0x10 + (row&0xF); /* type = 1 is program & sequence number */
    overflow = ((n_start&0xF)<<4) + (n_end&0xF); /* Number of extra bytes at
                                                    start and end */
    /* Calculate checksum for this row */
    add_to_checksum(checksum,type_byte);
    add_to_checksum(checksum,overflow);
    for(i=0;i<length;i++)
      {
        add_to_checksum(checksum,memory[pc+i]);
      }
    /* Now output the barcode data */
    putchar(length+2); /* Total # bytes = length + 3, but length
                          character in barcode file = # bytes -1 */
    putchar(*checksum); /* output running checksum */
    putchar(type_byte); /* header bytes */
    putchar(overflow);
    for(i=0;i<length;i++)
      {
        putchar(memory[pc+i]); /* and data bytes */
      } 
  }

unsigned char inst_length(int pc)
/* On entry, PC points to the start of an HP41 instruction. Return the length
   of this instruction in bytes */
  {
    unsigned char length; /* length of this instruction */

    /* HP41 instructions are grouped by high nybble */
    length= (unsigned char) 0;
    switch(memory[pc]>>4)
      {
        case 0 : /* Short labels */
        case 2 : /* Short RCLs */
        case 3 : /* Short STOs */
        case 4 : /* Single byte instructions */
        case 5 :
        case 6 : 
        case 7 :
        case 8 : length=1;
                 break;

        case 1 : /* alpha GTO/XEQ and digit entry */
                 if(memory[pc]>0x1c)
                   {
                     /* Alpha GTO/XEQ, length=2 + text length */
                     length=(memory[pc+1]&0xf)+2;
                   }
                 else
                   {
                     /* Digit entry, terminated by non-digit instruction */
                     length=0;
                     while((memory[pc+length]>0xf) && (memory[pc+length]<0x1d))
                       {
                         length++;
                       }
                   }
                 break;
        case 9 : /* 2 byte instructions */
        case 0xa :
        case 0xb : /* Short GTOs */
                   length=2;
                   break;

       case 0xc : if((memory[pc]&0xe)==0xe)
                    {
                      /* It's 0xce or 0xcf, exchanges and local lables */
                      length=2;
                    }
                  else
                    {
                      /* It's a global */
                      if(memory[pc+2]&0x80)
                        {
                          /* It's a label, length = 3 + length of text */
                          length = (memory[pc+2] & 0xf)+3;
                        }
                      else
                        {
                          /* It's an end, 3 bytes long */
                          length=3;
                        }
                    }  
                  break;

       case 0xd : /* Numeric GTOs */
       case 0xe : /* Numeric XEQs */
                  length=3;
                  break;

       case 0xf : /* Text */
                  length=(memory[pc]&0xf)+1;
                  break;
      }
    return(length); /* and give back the length */ 
  }


void output_barcode(int length)
/* Output the barcode for the program */
  {
    unsigned char n_start; /* Number of bytes at the start of this row
                              that continue an instruction in the last row */
    unsigned char inst_left; /* Number of bytes remaining in this instruction */
    unsigned char space_left; /* Space left for program bytes in this row */
    int row; /* Current row number */
    int pc; /* Program counter in HP41 memory */
    int start_pc; /* Program counter at the start of this row */
    unsigned char bytes_fit; /* Number of bytes of this instruction fitted
                                in this row */
    int checksum; /* barcode running checksum */


    checksum=0; 
    pc=0; /* Start at the beginning of memory */
    row=0; /* And row 0 */
    space_left=LINE_LENGTH; /* with an empty row */
    inst_left=0; /* And no instruction in progress */
    n_start= (unsigned char) 0;
    start_pc= (unsigned char) 0;
    
    while(pc<length)
      {
        /* Step through memory, generating the barcode */
        if(space_left==LINE_LENGTH)
          {
            /* This is the start of a new row of barcode. */
            n_start=min(LINE_LENGTH,inst_left); /* How many leftover
                                                    bytes will fit? */
            start_pc=pc; /* PC at the start of this row */
          }
        if(!inst_left)
          {
            /* If no instruction in progress, start a new one */
            inst_left=inst_length(pc);
          }
        bytes_fit = min(space_left,inst_left); /* How much will fit in this
                                                   row */
        pc+=bytes_fit; /* PC now points to first byte that didn't fit */
        space_left-=bytes_fit; /* Decrement space available */
        inst_left-=bytes_fit; /* Decrement number of bytes left in this 
                                 instruction */
        if(!space_left)
          {
            /* The row is full, output it */
            output_row(row++,n_start,(inst_left?bytes_fit:0),start_pc,
                        LINE_LENGTH,&checksum);
            space_left=LINE_LENGTH; /* And start a new row */
          }
      }
    /* All complete rows of barcode have been output. Is there a partial row */
    if(space_left!=LINE_LENGTH)
      {
        /* Yes, there is, output it */
        output_row(row++,n_start,0,start_pc,LINE_LENGTH-space_left,&checksum);
      }
  }

int main(int argc, char **argv)
  {
    int program_length;

    SETMODE_STDIN_BINARY;
    SETMODE_STDOUT_BINARY;

    program_length=read_prog(); /* Read in program */
    patch_end(&program_length); /* set END to indicate uncompiled GTOs */
    output_barcode(program_length); /* And output the barcode */
    exit(0);
  }

