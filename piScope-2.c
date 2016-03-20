/*
 *   piScope.c -- Raspberry Pi scope display program.
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
 *   To compile this program you will need to install the libx11-dev package.
 *
 *   Then type: gcc -I/usr/include/X11 -lX11 -lm -o piScope piScope.c
 *
 *   Make sure the analog2pi module is loaded, then run piScope
 *
 *   Tested with gcc 4.6.3
 */


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int i, fh, screen, white, black, trace, grid, background, offset, scale, pan, time;
static unsigned pause;
char buffer[2704];
char text[100];
Display *display;
Window  w;
GC gc;
XEvent event;
XGCValues values;
XColor screen_def_return, exact_def_return;

//linearize PCM sample
int pcmlin (sample)
{

    static int point;
    static double linear;

    linear = sample;
    linear = 845.447 - (15.4178 * linear) + (0.105858 * pow(linear, 2)) - (0.000283491 * pow(linear, 3));
    point = linear;
//    if (point < 0) return 0;
//    if (point > 300) return 300;
    return (point);

//fit from mathematica 876.046 - 19.8876 x + 0.202483 x^2 - 0.00108566 x^3 + 2.33633*10^-6 x^4
//Pi 2 fit from mathematica 845.447 - 15.4178 x + 0.105858 x^2  - 0.000283491 x^3
}

//linearize SPI sample
int spilin (sample)
{

    static int point;
    static double linear;

    linear = sample;
    linear = 440.751 - (15.6334 * linear) + (0.208849 * pow(linear, 2)) - (0.0011687 * pow(linear, 3));
    point = linear;
//    if (point < 0) return 0;
//    if (point > 300) return 300;
    return (point);
//fit from mathematica 654.534 - 18.5095 x + 0.248049 x^2 - 0.00193036 x^3 + 6.33645*10^-6 x^4
//Pi 2 fit from mathematica 440.751 - 15.6334 x + 0.208849 x^2  - 0.0011687 x^3
}

//draw labels
void labels()
{

    sprintf(text, ".125V/Div   100uS/Div           ");
    values.foreground = trace;
    XChangeGC(display, gc, GCForeground, &values);
    XDrawString(display, w, gc, 5, 600, text, 30);

}

//draw reticle
void reticle()
{
    static int i;

    values.foreground = grid;
    values.line_width = 1;
    XChangeGC(display, gc, GCForeground | GCLineWidth, &values);

    //horizontal lines
    for (i = 3;i < 582;i += 36)
    {

        XDrawLine(display, w, gc, 0, i, 980, i);

    }

    //vertical lines
    for (i = 3;i < 980;i += 36){

        XDrawLine(display, w, gc, i, 0, i, 584);

    }

    // horizontal axes tick marks
    for (i = 3;i < 980;i += 9){

        XDrawLine(display, w, gc, i, 144, i, 151);
        XDrawLine(display, w, gc, i, offset + 144, i, offset + 151);

    }

}

//draw waveforms
void draw()
{

    static int i, start;
    float scale;

    start = (float)pan / 18.432;
    scale = 663.552 / 2 / time;

    values.foreground = trace;
    values.line_width = 3;
    XChangeGC(display, gc, GCForeground | GCLineWidth, &values);

    //draw spi
    for(i = start; i < (start + 490); i += 2)
    {

	XDrawLine(display, w, gc, scale * (float)(i - start), spilin(buffer[i + start]) + 5, scale * (float)(i + 2 - start), spilin(buffer[i + 2 + start]) + 5);

    }

    //draw pcm
    for(i = start + 1; i < (start + 490); i += 2)
    {

	XDrawLine(display, w, gc, scale * (float)(i - 1 - start), pcmlin(buffer[i + start]) + offset + 8, scale * (float)(i + 1 - start), pcmlin(buffer[i + 2 + start]) + offset + 8);

    }

}

//set up window etc.
void setup()
{

    //set up display
    display = XOpenDisplay(NULL);

//pass as param instead of exit
    if (display == NULL)
    {

        fprintf(stderr, "Unable to open X display\n");
        exit(1);

    }

    //set up custom colors
    screen = DefaultScreen(display);
    black = BlackPixel(display, DefaultScreen(display));
    white = WhitePixel(display, DefaultScreen(display));
    grid = black;

    XAllocNamedColor(display, DefaultColormap(display, screen), "Green", &screen_def_return, &exact_def_return);
    trace = screen_def_return.pixel;

    XAllocNamedColor(display, DefaultColormap(display, screen), "DarkCyan", &screen_def_return, &exact_def_return);
    background = screen_def_return.pixel;

    //set up the window
    w = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 980, 607, 5, background, background);
    XStoreName(display, w, "piScope");

    XSelectInput(display, w, StructureNotifyMask | KeyPressMask) ;
    XMapWindow(display, w);

    //wait for map to complete
    for (;;)
    {

	XNextEvent(display, &event);
	if (event.type == MapNotify) break;

    }

    gc=XCreateGC(display, w, 0, &values);

}

int keyboard ()
{

    if (XCheckWindowEvent(display, w, KeyPressMask, &event) == 0) return;

//fprintf(stderr, "keycode %i\n", event.xkey.keycode);
    //handle quit key
    if (event.xkey.keycode == 24) return -1;

    //handle pause key
    if (event.xkey.keycode == 65) pause ^= 0x1;

    //handle down arrow
    if (event.xkey.keycode == 116) offset += 36;

    //handle up arrow
    if (event.xkey.keycode == 111) offset -= 36;

    //handle right arrow
    if (event.xkey.keycode == 114) pan -= time;

    //handle left arrow
    if (event.xkey.keycode == 113) pan += time;

//add handle time

    //range check
    if (offset > 288) offset = 288;
    if (offset < 0) offset = 0;

//redo limits based on time
    if (pan > 2210) pan = 2210;
    if (pan < 0) pan = 0;

    return 0;

}

int main (int argc, char *argv[])
{

    offset = 288;
    pan = 0;
    scale = 2;
    time = 100;
    pause = 0;

    //set up
    setup();

    //open device
    fh = open("/dev/analog2pi", O_RDWR);

    for(;;)
    {

	//read keyboard
	if (keyboard() == -1) break;

	//read device
	if (pause != 1) read(fh,buffer,2700);

	//draw labels
	XClearWindow(display, w);
	labels();

	//draw waveforms
	draw();

	//draw reticle
	reticle();

//	XFlush(display);

	//wait a bit
	if (pause == 0) usleep(100000); else usleep(100000);

    }

//should be function
    //tear down
    close(fh);
    XClearWindow(display,w);
    XFlush(display);
    XFreeGC(display,gc);
    XUnmapWindow(display,w);
    XDestroyWindow(display,w);
    XCloseDisplay(display);

}

