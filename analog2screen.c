/*
 *   analog2screen.c -- Raspberry Pi scope raw data display program.
 *   Works with the analog2pi kernel module.
 *
 *   Copyright 2015 Stephen Burke     thepihacker@gmail.com
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
 *   To compile this program: gcc -o analog2screen analog2screen.c
 *   Tested with gcc 4.6.3
 *   Make sure the analog2pi module is loaded, then run analog2screen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/fcntl.h>

int i, fh;
unsigned char buffer[1004];
float average;

int main (int argc, char *argv[])
{


    //open device
    fh = open("/dev/analog2pi", O_RDWR);

	//read device
	read(fh,buffer,1000);

	//display data
	printf("-----------------------------SPI--------------------------------\n");
	average = 0;

	for(i=0; i<672; i+= 2)
	{

	    printf("%3i ", buffer[i]);
	    average += buffer[i];
	    if ((i & 0x1f) == 0x1e) printf("\n");

	}
		
	printf(" SPI average %f\n", average / 336);

	printf("-----------------------------PCM--------------------------------\n");
	average = 0;

	for(i=1; i<672; i+= 2)
	{

	    printf("%3i ", buffer[i]);
	    average += buffer[i];
	    if ((i & 0x1f) == 0x1f) printf("\n");

	}
	printf(" PCM average %f\n", average / 336);

}
