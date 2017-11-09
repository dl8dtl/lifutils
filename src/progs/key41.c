/* key41.c -- A filter to display an HP41 key definition file */
/* 2000 A. R. Duell, and placed under the GPL */

/* An HP41 key definition file consists of a number of 8 byte records 
   which represent the contents of the 7 byte Key Assignment Registers. 
   Each KAR can contain the functions for 2 keys.

   After descrambling, the 7 byte registers are decoded as followed : 
   Byte 0 : Always 0xf0
   Byte 1-2 : Second function code (roughly the same as the bytes used
              for that function in program memory)
   Byte 3 : Second keycode
   Byte 4-5 : First function code
   Byte 6 : First keycode

   The keycode is 0 if no key is defined by that part of the KAR.

   Further details on the format of a KAR can be found in 'Extend your 
   HP41' or 'The HP41 Synthetic Quick Reference Guide' */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include "config.h"
#include "xrom.h"
#include "descramble_41.h"
#include "byte_tables41.h"
#include "byte_key_tables41.h"

void display_hex(unsigned char *key)
/* Display the function bytes from a key definition in hex */
  {
    printf("%02x %02x\n",key[0],key[1]);
  }

void xrom(unsigned char *key)
/* Display XROM assignments */
  {
    int rom,fn; /* ROM and function numbers */
    char * name; /* The ROM function name */

    rom=(key[0]&7)*4 + (key[1]>>6);
    fn=key[1]&0x3f;
    name=get_xrom_by_id(rom,fn);
    if (name == (char *) NULL )
       printf("XROM %02d,%02d\n",rom,fn);
    else
       printf("%s\n",name);
  }

void display_suffix(unsigned char byte)
/* Display suffix for 2-byte synthetic assignments */
  {
    if(byte&0x80)
      {
        /* If the high bit is set, it's indirect */
        printf(" IND");
      }
    if((byte&0x7f)<102)
      {
        /* It's a simple numeric address */
        printf(" %02d\n",byte&0x7f);
      }
    else
      {
        /* It's an alpha address */
        printf(" %c\n",suffix[(byte&0x7f)-102]);
      }
  }

void qloader(unsigned char byte)
/* Display Q-loader assignments from row 1 of the byte table */
  {
    if(byte<=0x1c)
      {
        /* It's a digit-entry Q-loader */
        printf("%c Q-loader\n",digit[byte-0x10]);
      }
    else
      {
        /* It's a GTO/XEQ Q-loader */
        printf("%s Q-loader\n",alpha_gto[byte-0x1d]);
      }
  }

void single_row_b(unsigned char byte)
/* Display the (mostly GTO) single-byte assingments in row B */
  {
    switch(byte)
      {
        /* 0xb0 and 0xb1 are odd */
        case 0xb0 : printf("RCL 00\n");
                    break;
        case 0xb1 : printf("STO 00\n");
                    break;
        case 0xb2 :
        case 0xb7 :
        case 0xba :
        case 0xbb :
        case 0xbc :
        case 0xbf : /* These are also byte jumpers */
                    printf("GTO %02d / Bytejumper\n",byte-0xb1);
                    break; 
        default :   printf("GTO %02d\n",byte-0xb1);
                    break;
      }
  }

void single_row_c(unsigned char byte)
/* Display single-byte assignments from row C of the bytetable */
  {
    switch(byte)
      {
        case 0xc4 : printf("END / LBL\"\"\n");
                    break;
        case 0xcb : printf("END / GTO\n");
                    break;
        case 0xcd : printf("LBL Q-loader\n");
                    break;
        case 0xce :
        case 0xcf : printf("%s\n",row_c[byte-0xce]);
                    break;
        default :   printf("END\n");
                    break;
      }
  }

void single_row_d(unsigned char byte)
/* Display single byte assignments from row D of the bytetable */
  {
    switch(byte)
      {
        case 0xd0 : printf("GTO\n"); /* This is the normal GTO assignment */
                    break;
        case 0xd1 :
        case 0xd5 :
        case 0xde : printf("GTO (IND ST)\n");
                    break;
        case 0xd2 : printf("GTO / XEQ\"\",ST\n");
                    break;
        case 0xdb : printf("GTO (ST)\n");
                    break;
        default :   printf("GTO 00\n");
                    break;
      }
  }

void single_row_e(unsigned char byte)
/* Display single-byte assignments from row E of the bytetable */
  {
    switch(byte)
      {
        case 0xe0 : printf("XEQ\n"); /* This is the normal XEQ assignment */
                    break;
        case 0xe2 :
        case 0xe4 :
        case 0xef : printf("XEQ (IND ST)\n");
                    break;
        case 0xe5 :
        case 0xe7 :
        case 0xe9 :
        case 0xeb : printf("XEQ ___\n");
                    break;
        default :   printf("XEQ 00\n");
                    break;
      }
  }

void single_row_f(unsigned char byte)
/* Display single-byte assignments from row F of the bytetable */
  {
    switch(byte)
      {
        case 0xf8 :
        case 0xf9 :
        case 0xfc :
        case 0xfe :
        case 0xff : /* Byte grabbers */
                    printf("Bytegrabber(%1d) / Bytejumper\n",byte-0xf6);
                    break;
        default :   /* Bytejumpers */
                    printf("GTO 00 / Bytejumper\n");
                    break;
      }
  }

void single_byte_asn(unsigned char *key, int row)
/* Display single-byte assignements -- ones where the high nybble of the 
   first byte is 0 */
  {
    /* Decode based on the high nybble of the second byte */
    switch(key[1]>>4)
      {
        case 0 : /* Non-programamble functions */
                 if(key[1]==0x0c) /* Synthetic toggle key assignment */
                   {
                     switch(row) /* What it does depends on the row */
                       {
                         case 0 :
                         case 4 : printf("ALPHA\n");
                                  break;
                         case 1 : 
                         case 5 : printf("PRGM\n");
                                  break;
                         default : printf("USER\n");
                                   break;
                       }
                   }
                 else
                   {
                     printf("%s\n",non_prog[key[1]]);
                   }
                 break;
        case 1 : /* Q-loaders */
                 qloader(key[1]);
                 break;
        case 2 : /* Mostly synthetic short RCLs */
                 if(key[1]==0x20) 
                   {
                     display_hex(key); /* Byte 0x20 is strange */
                   }
                 else
                   {
                     /* It's a normal RCL */
                     printf("RCL %02d\n",key[1]&0xf);
                   }
                 break;
        case 3 : /* Mostly synthetic short STOs */
                if((key[1]==0x33) || (key[1]==0x37) || (key[1]==0x3c))
                  {
                    display_hex(key); /* These are odd */
                  }
                else
                  {
                     /* It's a normal STO */
                     printf("STO %02d\n",key[1]&0xf);
                  }
                break;
        case 4 :
        case 5 :
        case 6 :
        case 7 :
        case 8 : /* Normal single-byte assignments */
                 printf("%s\n",single_byte[key[1]-0x40]);
                 break;
        case 9 : /* Normal prefix bytes */
                 printf("%s\n",double_byte[key[1]-0x90]);
                 break;
        case 0xa : /* Synthetic XROM prefixes and normal prefixes */
                 printf("%s\n",single_byte_a[key[1]-0xa0]);
                 break;
        case 0xb : /* Mostly GTO, some are also bytejumpers */ 
                 single_row_b(key[1]);
                 break;
        case 0xc : /* Mostly ENDs */
                 single_row_c(key[1]);
                 break;
        case 0xd : /* Mostly GTOs */
                 single_row_d(key[1]);
                 break;
        case 0xe : /* XEQs */
                 single_row_e(key[1]);
                 break;
        case 0xf : /* Byte grabbers and jumpers */
                 single_row_f(key[1]);
                 break;
      }
  }

void double_row_b(unsigned char *key)
/* Display double-byte compiled GTOs from row B of the bytetable */
  {
    int comp_dist; /* Compiled GTO Distance */

    comp_dist=(((key[1] & 0xf) * 7) + ((key[1]>>4)&0x7))*((key[1]&0x80)?-1:1);
    switch(key[0])
      {
        case 0xb0 : printf("NOP GTO 15\n");
                    break;
        case 0xb1 : if(key[1]<=16)
                      {
                        printf("STO %02d",key[1]);
                        if(key[1])
                          {
                            printf(" / GTO compiled %d bytes\n",comp_dist);
                          }
                        else
                          {
                            printf("\n");
                          }
                      }
                    else
                      {
                        printf("GTO 00");
                        if(key[1])
                          {
                            printf(" compiled %d bytes\n",comp_dist);
                          }
                        else
                          {
                            printf("\n");
                          }
                      }
                    break;
        default :   printf("GTO %02d",key[0]-0xb1);
                    if(key[1])
                      {
                        printf(" compiled %d bytes\n",comp_dist);
                      }
                    else
                      {
                        printf("\n");
                      }
                    break;
      }
  }

void double_row_c(unsigned char *key)
/* Display double-byte assignments from row C of the bytetable */
  {
    switch(key[0])
      {
        case 0xce : /* X<> instructions */
                    printf("%s",row_c[0]);
                    display_suffix(key[1]);
                    break;
        case 0xcf : /* Local labels */
                    if(key[1]==0xff)
                      {
                        printf("Null Register / SST\n");
                      }
                    else
                      {
                        printf("%s",row_c[1]);
                        display_suffix(key[1]);
                      }
                    break;
        case 0xcd : /* LBL Q-loader */
                    printf("LBL Q-loader\n");
                    break;
        default :   /* ENDs */
                    printf("END\n");
                    break;
      }
  }

void double_row_f(unsigned char *key)
/* Display byte grabbers from row F of the byte table */
  {
    if(key[1]==0xff)
      {
        /* If suffix = 0xff, it's actually a NOP */
        printf("NOP GTO 15");
       }
    else
      {
        if(key[1]<=0xe)
          {
            /* If suffix <=14, then it's a 2-byte GTO */
            printf("GTO %02d",key[1]);
          }
        else
          {
            switch(key[0])
              {
                case 0xf0 :
                case 0xf1 : /* Byte inserters */
                            printf("Insert byte %02x",key[1]);
                            break;
                case 0xf2 : /* Insert a 2 character string, first character 0 */
                            printf("Insert string \\000 ");
                            if(isprint(key[1]))
                              {
                                /* if the character is printable, print it */
                                putchar(key[1]);
                              }
                            else
                              {
                                /* unprintable, so print as octal escape 
                                   sequence */
                                printf("\\%03o",key[1]);
                              }
                            break;
                case 0xf3 :
                case 0xf4 :
                case 0xf5 :
                case 0xf6 : /* These are byte grabbers if used where
                               there are 3 nulls */
                            printf("Bytegrabber(%d) without moving",
                                    key[0]-0xf2);
                            break;
                default :   /* These are standard byte grabbers */
                            printf("Bytegrabber(%d)",key[0]-0xf6);
                            break;
              } 
          }
      }
    /* all these are byte jumpers in RUN mode */
    printf(" / bytejumper -> Alpha\n");
  }

void display_gto_xeq_suffix(unsigned char byte)
/* Display suffix for 2-byte synthetic GTO and XEQ assignments */
  {
    if(byte&0x80)
      {
        /* If the high bit is set, it's indirect in PRGM mode,
            Looks for a label with similar suffix in RUN mode
            Display this by putting brackets round the 'IND'  */
        printf(" (IND)");
      }
    /* Display the rest of the suffix, masking out the high bit so that
       display_suffix will not print 'IND' */
    display_suffix(byte&0x7f);
  }

void display_key(unsigned char *key, int hex_flag)
/* Display a single key definition */
  {
    int keycode; /* Keycode from the KAR */
    int row, col, shift, asn_keycode; /* Keycode parts for ASN function */

    keycode= key[2];
    if(keycode!=0) 
      {
        /* The keycode byte in the definition can be decoded as follows:
           Decrement it (now in the range 0-0x4c)
           Low 3 bits -> Row (0-7)
           Bit 3 -> Set = shifted
           High bits -> Col (0-4) */

        keycode--; 
        row = keycode&7;
        shift=keycode&8;
        col=keycode>>4;
        /* There's a missing key in row 3 */
        if((row==3) && (col>0)) 
          {
            col--;
          }
        /* calculate ASN keycode and print it */
        asn_keycode=((row+1)*10+(col+1))*(shift?-1:1);
        printf("% 2d : ",asn_keycode);
        if(hex_flag)
          {
            /* display function in hex */
            display_hex(key);
           }
        else
          {
            /* Decode the key assignment based on the high nybble
               of the 1st byte */
            switch(key[0]>>4)
              {
                case 0 : /* Normal Single-byte function assignments */
                         single_byte_asn(key,row);
                         break;
                case 1 : /* Q-loaders */
                         qloader(key[0]);
                         break;
                case 2 : /* Synthetic short RCLs */
                         printf("RCL %02d\n",key[0]&0xf);
                         break;
                case 3 : /* Synthetic short STOs */
                         printf("STO %02d\n",key[0]&0xf);
                         break;
                case 4 :
                case 5 : 
                case 6 :
                case 7 :
                case 8 : /* Synthetically-assigned one byte instructions
                            -- the second byte is ignored */
                         printf("%s\n",single_byte[key[0]-0x40]);
                         break;
                case 9 : /* Synthetically-assigned complete 2-byte 
                            instructions */
                         printf("%s",double_byte[key[0]-0x90]);
                         display_suffix(key[1]);
                         break;
                case 0xa : /* XROMs and synthetically-assigned 2-byte 
                              instructions */
                         if(key[0]<=0xa7)
                           {
                             /* It's an XROM */
                             xrom(key);
                           }
                         else
                           {
                             if(key[0]<=0xad)
                               {
                                 /* It's a 2-byte instruction */
                                 printf("%s",double_byte[key[0]-0x90]);
                                 display_suffix(key[1]);
                               }
                             else
                               {
                                 /* It's a GTO/XEQ IND, possibly NOPped */
                                 if(key[0]==0xaf)
                                   {
                                     printf("NOP ");
                                   }
                                 /* Fall thru */
                                 printf("%s",(key[1]&0x80)?"XEQ":"GTO");
                                 display_suffix((key[1]&0x7f)+0x80);
                               }
                           }
                         break;
                case 0xb : /* Compiled GTOs */
                         double_row_b(key);
                         break;   
                case 0xc : /* Mostly ENDs */
                        double_row_c(key);
                        break;
                case 0xd : /* GTOs */
                        if(key[1]==0xff)
                          {
                            /* It's actually a NOP */
                            printf("NOP GTO 15\n");
                          }
                        else
                          {
                            printf("GTO");
                            display_gto_xeq_suffix(key[1]);
                          }
                       break;
                case 0xe : /* XEQs */
                        printf("XEQ");
                        display_gto_xeq_suffix(key[1]);
                        break;
                case 0xf : /* byte grabbers */
                        double_row_f(key);
                        break;
              }  
          }
      }
  }

void display_kar(unsigned char *kar, int hex_flag)
/* Dispaly the 2 key definitions in a KAR */
  {
    /* Is this a valid KAR ? */
    if(kar[0]==0xf0)
      {
        /* Yes, display the keys */
        display_key(kar+1,hex_flag);
        display_key(kar+4,hex_flag);
      }
  }

void usage(void)
  {
    fprintf(stderr,"Usage: key41 [-h] [-x xrom_name_file] [-x...]\n");
    fprintf(stderr,"       -h flag prints functions in hex rather\n"); 
    fprintf(stderr,"       than as names\n");
    fprintf(stderr,"       -x xrom_name_file uses those names for XROM\n");
    fprintf(stderr,"       functions\n");
    exit(1);
  }

int main(int argc, char **argv)
  {
    int option; /* current option character */
    int hex_flag=0; /* print functions in hex, not as names */
    unsigned char rec[8]; /* One file record */
    unsigned char kar[7]; /* Key assignment register */

    SETMODE_STDIN_BINARY;

    init_xrom(); /* Load initial xrom names */
    optind=1;
    while((option=getopt(argc,argv,"hx:?"))!=-1)
      {
        switch(option)
          {
            case 'h' : hex_flag=1;
                       break;
            case 'x' : read_xrom(optarg);
                       break;
            case '?' : usage();
          }
      }
    if(optind!=argc)
      {
        usage();
      }

    /* read key file and display it */
    while(fread(rec,sizeof(char),8,stdin)==8)
      {
        /* Turn record into KAR */
        descramble(rec,kar);
        /* Display KAR */
        display_kar(kar,hex_flag);
       }
    exit(0);
  }
