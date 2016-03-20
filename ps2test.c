/*
 *   ps2test.c -- Raspberry Pi PS/2 keyboard test program.
 *   Used to determine pi2ps2 module load parameters.
 *
 *   Copyright 2014 the pi hacker 
 *
 *   https://sites.google.com/site/thepihacker/ps2pi
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *   Some of the code is copied from the GPIO register access example program 
 *   by Dom and Gert.  It had no license notice.
 */

//To compile this program: gcc ps2test.c -o ps2test.
//Make sure the pi2ps2 module is not loaded, then run ps2test as root.
//The keyboard should already be connected.
//Follow the onscreen instructions.
//Tested with gcc 4.6.3

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>

#define IO_BASE		0x20000000 //Pi A,B,Zero etc.
//#define IO_BASE		0x3F000000 //Pi 2

//gpio
#define GPFSEL1		0x200004/4

//uart
//default uart CLOCK IS 3MHz
#define DR		0x201000/4
#define FR		0x201018/4
#define IBRD		0x201024/4
#define FBRD		0x201028/4
#define LCRH		0x20102c/4
#define IMSC		0x20138c/4
#define CR		0x201030/4


int  mem_fd;
void *io_base_map;
volatile unsigned *io_base;


void setup_io()
{
   // open /dev/mem
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem n");
      exit(-1);
   }

   //mmap IO_BASE
   io_base_map = mmap(
      NULL,             				//Any adddress in our space will do
      0x400000,       					//Map length
      PROT_READ|PROT_WRITE,				//Enable reading & writting to mapped memory
      MAP_SHARED,       				//Shared with other processes
      mem_fd,           				//File to map
      IO_BASE         					//Offset to IO peripherals
   );

   close(mem_fd); 					//No need to keep mem_fd open after mmap

   if (io_base_map == MAP_FAILED) {
      printf("mmap error'a0%dn", (int)io_base_map);	//errno also set!
      exit(-1);
   }

   // Always use volatile pointer!
   io_base = (volatile unsigned *)io_base_map;
}


int main(int argc, char **argv) 
{
    unsigned rcv,div,frac,lower,upper;
    setup_io();

    //set pin 10 as uart rxd
    *(io_base+GPFSEL1) &= ~(7<<15);
    *(io_base+GPFSEL1) |= (4<<15);

    //set uart to receive 8 bit, 1 stop, parity odd
    *(io_base+CR) = 0;
    //    while ((*(io_base+FR) & (1<<6)) == 0) { }
    *(io_base+LCRH) = ((3<<5)|(1<<1));
    *(io_base+IMSC) = 0;
    *(io_base+CR) = ((1<<9)|1);

    //baud rate divisor 30 (baud rate = 1/16th uart clock)
    //remember to disable serial console on this port
    //to set uart clock, use option in /boot/config.txt file.  Example for 3MHz: init_uart_clock=3000000
    *(io_base+FBRD) = 35;
    *(io_base+IBRD) = 15;
    usleep(1);
    *(io_base+CR) = (1<<9);
    *(io_base+CR) |= 1;

    //empty buffer
    do rcv = *(io_base+DR);
    while ((*(io_base+FR) & (1<<6)) != 0);

    printf("Press and hold down the semicolon key on the ps/2 keyboard until this program exits.\n");

    //upper integer bound
    div = 31;
    do {
	div--;
        if (div == 1) return;
	*(io_base+CR) = 0;
        *(io_base+LCRH) = ((3<<5)|(1<<1));
        *(io_base+IBRD) = div;
        *(io_base+FBRD) = 0;
        usleep(1);
        *(io_base+CR) = (1<<9);
        *(io_base+CR) |= 1;
        while ((*(io_base+FR) & (1<<6)) == 0) { }   //wait for character
    }
    while (*(io_base+DR) != 0x4c);

    //upper fractional bound
    frac = 10;

    do {
	if (frac == 1) return;
        frac--;
        *(io_base+CR) = 0;
        *(io_base+LCRH) = ((3<<5)|(1<<1));
        *(io_base+FBRD) = (6*frac) + (frac/3);
        usleep(1);
        *(io_base+CR) = (1<<9);
        *(io_base+CR) |= 1;
        while ((*(io_base+FR) & (1<<6)) == 0) { }   //wait for character
    }
    while (*(io_base+DR) != 0x4c);

    upper = (10*div) + frac;
    //printf("Upper bound %u.%u\n",div,frac);

    //lower integer bound
    do {
	div--;
        if (div == 1) return;
        *(io_base+CR) = 0;
        *(io_base+LCRH) = ((3<<5)|(1<<1));
        *(io_base+IBRD) = div;
        *(io_base+FBRD) = 0;
        usleep(1);
        *(io_base+CR) = (1<<9);
        *(io_base+CR) |= 1;
        while ((*(io_base+FR) & (1<<6)) == 0) { }   //wait for character
    }
    while (*(io_base+DR) == 0x4c);

    //lower fractional bound
    frac = 10;

    do {
	if (frac == 1) return;
	frac--;
        *(io_base+CR) = 0;
        *(io_base+LCRH) = ((3<<5)|(1<<1));
        *(io_base+FBRD) = (6*frac) + (frac/3);
        usleep(1);
        *(io_base+CR) = (1<<9);
        *(io_base+CR) |= 1;
        while ((*(io_base+FR) & (1<<6)) == 0) { }   //wait for character
    }
    while (*(io_base+DR) == 0x4c);

    //	printf("Lower bound %u.%u\n",div,frac+1);
    lower = (10*div) + frac + 1;
    div=((upper-lower)/2)+(lower);
    frac=div-((div/10)*10);	
    div=div/10;
    printf("Edit module and change integer=%u and fractional=%u\n",div,(6*frac) + (frac/3));

}
