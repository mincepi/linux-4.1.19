/*
 *   analog2au.c -- Raspberry Pi audio capture program.
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
 *   To compile this program: gcc -lm -o analog2au analog2au.c
 *
 *   Tested with gcc 4.6.3
 *
 *   analog2pi module must be loaded first
 *
 *   Use it like this: analog2au 40 > out.au
 *
 *   The sbove example captures 1 second of data and writes it to an au format stereo file named out.au
 *
 *   The 40 above is an example: it can be changed to whatever you need.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/fcntl.h>

int i, fh, blocks;
int8_t *buffer;

/* generic au file header */
static unsigned char header[32] =
{

/* magic number */  0x2e, 0x73, 0x6e, 0x64,
/* data offset  */  0x00, 0x00, 0x00, 0x20,
/* data size    */  0xff, 0xff, 0xff, 0xff,
/* encoding     */  0x00, 0x00, 0x00, 0x02,
/* sample rate  */  0x00, 0x00, 0xd3, 0xee,
/* channels     */  0x00, 0x00, 0x00, 0x02,
/* metadata     */  0x00, 0x00, 0x00, 0x00,
/* metadata     */  0x00, 0x00, 0x00, 0x00

};

//linearize a PCM sample
int8_t pcmlin (char sample)
{

//    static char point;
    static double linear;

    linear = sample;
    linear = 845.447 - (15.4178 * linear) + (0.105858 * pow(linear, 2)) - (0.000283491 * pow(linear, 3));

    //range check
    if (linear > 300) linear = 300;
    if (linear < 1) linear = 1;

    return (150 - linear) * 256 / 300;

//pcm equation 917.687 - 21.0253 x + 0.217488 x^2 - 0.00118923 x^3 +  2.61684*10^-6 x^4
//Pi 2 pcm equation 845.447 - 15.4178 x + 0.105858 x^2  - 0.000283491 x^3

}

//linearize a SPI sample
int8_t spilin (char sample)
{

//    static char point;
    static double linear;

    linear = sample;
    linear = 440.751 - (15.6334 * linear) + (0.208849 * pow(linear, 2)) - (0.0011687 * pow(linear, 3));

    //range check
    if (linear > 300) linear = 300;
    if (linear < 1) linear = 1;

    return (150 - linear) * 256 / 300;

//spi equation 683.587 - 19.4566 x + 0.264273 x^2 - 0.00209011 x^3 +  6.96715*10^-6 x^4
//Pi 2 spi equation  440.751 - 15.6334 x + 0.208849 x^2  - 0.0011687 x^3

}

int main (int argc, char *argv[])
{

    sscanf(argv[1], " %i", &blocks);

    //map buffer
    buffer = malloc((blocks * 2700) + 32);

    //open device
    fh = open("/dev/analog2pi", O_RDWR);

    //read data blocks
    for (i = 32; i < (blocks * 2700); i += 2700)
    {

	read(fh, buffer + i, 2700);

    }

    //write header
    memcpy(buffer, header, 32);

    //linearize SPI data in buffer
    for (i = 32; i < ((blocks * 2700) + 32); i += 2)
    {

	*(buffer + i) = spilin(*(buffer + i));

    }

    //linearize PCM data in buffer
    for (i = 33; i < ((blocks * 2700) + 32); i += 2)
    {

	*(buffer + i) = pcmlin(*(buffer + i));

    }

    //write buffer data to stdout
    write(1, buffer, (blocks * 2700) + 32);
    free(buffer);

    //close device
    close(fh);

}
