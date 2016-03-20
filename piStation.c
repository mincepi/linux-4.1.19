/*   piStation.c -- Raspberry Pi gpio buttons keyboard emulation device driver.
 *
 *   Copyright 2014 the pi hacker  https://sites.google.com/site/thepihacker
 *
 *   This file is subject to the terms and conditions of the GNU General Public
 *   License. See the file COPYING in the Linux kernel source for more details.
 *
 *   Based on my ps2pi.c, pi2meter.c, and pi kernel modules
 *
 *   The following resources helped me greatly:
 *
 *   Analog Devices wiki: linux-kernel:timers
 *   1995-2014
 *
 *   Timers and lists in the 2.6 kernel
 *   M. Tim Jones 30 mar 2010
 *
 *
 *   The Linux kernel module programming howto
 *   Copyright (C) 2001 Jay Salzman
 *
 *   The Linux USB input subsystem Part 1
 *   Copyright (C) 2007 Brad Hards
 *   switch debouncing
 *
 *
 *   kbd FAQ
 *   Copyright (C) 2009 Andries Brouwer
 *
 *   Switches should be connected from a gpio to groud.
 *
 */


//You don't need to do anything special to compile this module.

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/io.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>


#define IO_BASE		0x20000000

//gpio
#define GPFSEL1		0x200004/4
#define mask (1<<2 | 1<<3 | 1<<4 | 1<<9 | 1<<10 | 1<<11 | 1<<17 | 1<<22)

static unsigned history[] = {mask, mask, mask, mask, mask, mask, mask, mask};
static unsigned previous = mask;
static unsigned pointer = 0;
volatile unsigned *(io_base);
static struct input_dev *dev;
static struct hrtimer timer;

//key polling and debounce routine
enum hrtimer_restart keycheck(struct hrtimer *timer)
{
    static unsigned pressed, released, i;
    pressed = 0;
    released = mask;
    history[pointer] = ioread32(io_base);

    for (i = 0; i < 8; i++)
    {
	released = released & history[i];
	pressed = pressed | history[i];
    }

    //find which keys have changed
    released = released & ~previous & mask;
    pressed = ~pressed & previous & mask;



//printk(KERN_ALERT "postloop history %x pressed %x released %x\n", history[pointer], pressed, released);


    //handle new key presses
    if (pressed != 0)
    {
	if (pressed & 1<<2) input_report_key(dev,KEY_UP,1);
	if (pressed & 1<<3) input_report_key(dev,KEY_LEFT,1);
	if (pressed & 1<<4) input_report_key(dev,KEY_DOWN,1);
	if (pressed & 1<<9) input_report_key(dev,KEY_ESC,1);
	if (pressed & 1<<10) input_report_key(dev,KEY_SPACE,1);
	if (pressed & 1<<11) input_report_key(dev,KEY_ENTER,1);
	if (pressed & 1<<17) input_report_key(dev,KEY_RIGHT,1);
	if (pressed & 1<<22) input_report_key(dev,KEY_LEFTCTRL,1);
	input_sync(dev);
    }

    //handle new key releases
    if (released != 0)
    {
	if (released & 1<<2) input_report_key(dev,KEY_UP,0);
	if (released & 1<<3) input_report_key(dev,KEY_LEFT,0);
	if (released & 1<<4) input_report_key(dev,KEY_DOWN,0);
	if (released & 1<<9) input_report_key(dev,KEY_ESC,0);
	if (released & 1<<10) input_report_key(dev,KEY_SPACE,0);
	if (released & 1<<11) input_report_key(dev,KEY_ENTER,0);
	if (released & 1<<17) input_report_key(dev,KEY_RIGHT,0);
	if (released & 1<<22) input_report_key(dev,KEY_LEFTCTRL,0);
	input_sync(dev);
    }


    //update variables
    previous = (previous | released) & ~pressed;
    pointer = ++pointer & 0x7;

    //do again in 4 milliseconds
    hrtimer_forward(timer, hrtimer_cb_get_time(timer), ktime_set(0,4000000));
    return HRTIMER_RESTART;
}


int init_module(void)
{
    int i, retval;

    //set pin functions
    io_base = ioremap(0x20200000, 168);
    iowrite32(ioread32(io_base) & ~(7<<6 | 7<<9 | 7<<12 | 7<<27), io_base);
    iowrite32(ioread32(io_base + 1) & ~(7<<0 | 7<<3 | 7<<21), io_base + 1);
    iowrite32(ioread32(io_base + 2) & ~(7<<6), io_base + 2);

    //set pin pullups
    iowrite32(0x02, io_base + 37);
    udelay(10);
    iowrite32(mask, io_base + 38);
    udelay(10);
    iowrite32(0x0, io_base + 37);
    iowrite32(0x0, io_base + 38);
    iounmap(io_base);

    //set up input device
    dev=input_allocate_device();
    dev->name = "piStation";
    dev->phys = "piStation/input0";
    dev->id.bustype = BUS_HOST;
    dev->id.vendor = 0x0001;
    dev->id.product = 0x0001;
    dev->id.version = 0x0100;
    dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_REP);
//	dev->keycode = translate;
    dev->keycodesize = sizeof(unsigned char);
    dev->keycodemax = 0xff;
    for (i = 1; i < 0xff; i++) set_bit(i,dev->keybit);
    retval = input_register_device(dev);

    //set up timer
    io_base = ioremap(0x20200034, 4);
    hrtimer_init(&timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    timer.function = &keycheck;
    hrtimer_start(&timer, ktime_set(0,4000000), HRTIMER_MODE_REL);
    printk(KERN_INFO "piStation device installed\n");

    //numlock on, should query state first
    input_report_key(dev,KEY_NUMLOCK,1);
    input_sync(dev);
    return 0;
}

void cleanup_module(void)
{
    //stop timer
    hrtimer_cancel(&timer);
    iounmap(io_base);
    //tear down input device
    input_unregister_device(dev);
    printk(KERN_INFO "piStation device removed\n");
    return;
}

MODULE_LICENSE("GPL");

