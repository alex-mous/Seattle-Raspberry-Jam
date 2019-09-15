/*
 * pi2ws2812.c: userspace WS2812 programmable RGB LED driver for the RaspberryPi
 * Copyright (c) 2019 mincepi <mincepi@gmail.com>
 *
 * Based on servod.c Copyright (c) 2013 Richard Hirst <richardghirst@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * To compile, make sure pi2ws2812pwm.c, mailbox.h and mailbox.c are in the current directory, then run:
 * gcc -Wall -g -O2 -L/opt/vc/lib -I/opt/vc/include -o pi2ws2812pwm pi2ws2812pwm.c mailbox.c -lbcm_host
 *
 * Connect power and ground to WS2812, connect GPIO 18 to WS2812 data input
 * It may help to put a diode in series with power, especially if your supply voltage is higher than 5v.
 *
 * To load driver for an 8 LED strip run: sudo nice -n -20 pi2ws2812pwm 8
 *
 * Write data to /dev/pi2ws2812pwm, ascii text, 0-255, green red blue space delimited for each LED, with a space and newline at the end.
 *
 * For example: echo 255 0 0 0 128 0 > /dev/pi2ws2812pwm lights first LED green full brightness, second LED red half brightness.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <getopt.h>
#include <math.h>
#include <bcm_host.h>
#include <sched.h>
#include <sys/wait.h>
#include "mailbox.h"

#define DMA0_CHANNEL		7

#define DMA_BASE_OFFSET		0x00007000
#define DMA_LEN			0x400
#define GPIO_BASE_OFFSET	0x00200000
#define GPIO_LEN		0x100
#define PWM_BASE_OFFSET		0x0020C000
#define PWM_LEN			0x28
#define CLK_BASE_OFFSET	        0x00101000
#define CLK_LEN			0xA8

#define DMA_VIRT_BASE		(periph_virt_base + DMA_BASE_OFFSET)
#define CLK_VIRT_BASE		(periph_virt_base + CLK_BASE_OFFSET)
#define GPIO_VIRT_BASE		(periph_virt_base + GPIO_BASE_OFFSET)
#define PWM_VIRT_BASE		(periph_virt_base + PWM_BASE_OFFSET)

#define PWM_PHYS_BASE		(periph_phys_base + PWM_BASE_OFFSET)
#define GPIO_PHYS_BASE		(periph_phys_base + GPIO_BASE_OFFSET)

#define DMA_NO_WIDE_BURSTS	(1<<26)
#define DMA_WAIT_RESP		(1<<3)
#define DMA_D_DREQ		(1<<6)
#define DMA_PER_MAP(x)		((x)<<16)
#define DMA_END			(1<<1)
#define DMA_RESET		(1<<31)
#define DMA_INT			(1<<2)

#define DMA_CS			(0x00/4)
#define DMA_CONBLK_AD		(0x04/4)
#define DMA_TI			(0x08/4)
#define DMA_SOURCE_AD		(0x0c/4)
#define DMA_DEST_AD		(0x10/4)
#define DMA_LENGTH		(0x14/4)
#define DMA_DEBUG		(0x20/4)
#define GPIO_FSEL1		(0x04/4)

#define PWM_CTL			(0x00/4)
#define PWM_STA			(0x04/4)
#define PWM_DMAC		(0x08/4)
#define PWM_RNG1		(0x10/4)
#define PWM_DATA		(0x14/4)
#define PWM_FIFO		(0x18/4)

#define PWMCTL_MODE1		(1<<1)
#define PWMCTL_PWEN1		(1<<0)
#define PWMCTL_CLRF		(1<<6)
#define PWMCTL_USEF1		(1<<5)

#define PWMDMAC_ENAB		(1<<31)
#define PWMDMAC_THRSHLD		((15<<8)|(15<<0))
#define ROUNDUP(val, blksz)	(((val)+((blksz)-1)) & ~(blksz-1))

#define PWMCLK_CNTL		40
#define PWMCLK_DIV		41

typedef struct {
	uint32_t TI, SOURCE, DEST, LENGTH,
		 STRIDE, NEXT, pad[2];
} CB;

#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

static volatile uint32_t *pwm;
static volatile uint32_t *clk;
static volatile uint32_t *dma;
static volatile uint32_t *gpio;

static uint32_t periph_virt_base;
static uint32_t dram_phys_base;
static uint32_t mem_flag;
static CB *CB0;
static uint32_t *buf;
unsigned length, blocks;
char *stack;
char *top;
uint32_t bytes, value;
char * byte;
char * line;
size_t len;
FILE * fp;

static struct {
	int handle;		/* From mbox_open() */
	uint32_t size;		/* Required size */
	unsigned mem_ref;	/* From mem_alloc() */
	unsigned bus_addr;	/* From mem_lock() */
	uint8_t *virt_addr;	/* From mapmem() */
} mbox;
	

static void udelay(int us) {

    struct timespec ts = { 0, us * 1000 };
    nanosleep(&ts, NULL);

}

static uint32_t mem_virt_to_phys(void *virt) {

    uint32_t offset = (uint8_t *)virt - mbox.virt_addr;
    return mbox.bus_addr + offset;

}

//map peripheral, returns MAP_FAILEd on failure
static void * map_peripheral(uint32_t base, uint32_t len) {

    int fd = open("/dev/mem", O_RDWR|O_SYNC);
    void * vaddr;
    if (fd < 0)	printf("Failed to open /dev/mem: %m\n");
    vaddr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, base);
    if (vaddr == MAP_FAILED) printf("Failed to map peripheral at 0x%08x: %m\n", base);
    close(fd);
    return vaddr;

}

//map memory and peripherals, exits on failure
static void map (void) {

    periph_virt_base = bcm_host_get_peripheral_address();
    //pi4 not yet supported in bcm library
    if (periph_virt_base == 0) periph_virt_base = 0xfe000000;
    dram_phys_base = bcm_host_get_sdram_address();


    if (periph_virt_base == 0x20000000) {

	mem_flag = 0x0c; //MEM_FLAG_DIRECT | MEM_FLAG_COHERENT 

    } else {

	mem_flag = 0x04; //MEM_FLAG_DIRECT, Pi 2 and 3

    }

    // Use the mailbox interface to request memory from the VideoCore
    // We specify (-1) for the handle rather than calling mbox_open()
    // so multiple users can share the resource.
    mbox.handle = -1; // mbox_open();
    mbox.size = 4096 * ((((blocks * 12) + 4 + 32) / 4096) + 1);
    mbox.mem_ref = mem_alloc(mbox.handle, mbox.size, 4096, mem_flag);

    if (mbox.mem_ref < 0) {
	printf("Failed to alloc memory from VideoCore\n");
	exit(-1);
    }

    mbox.bus_addr = mem_lock(mbox.handle, mbox.mem_ref);

    if (mbox.bus_addr == ~0) {
	mem_free(mbox.handle, mbox.size);
	printf("Failed to lock memory\n");
	exit(-1);
    }

    mbox.virt_addr = mapmem(BUS_TO_PHYS(mbox.bus_addr), mbox.size);
    dma = map_peripheral(DMA_VIRT_BASE, DMA_LEN); 


    if (dma == MAP_FAILED) {
	unmapmem(mbox.virt_addr, mbox.size);
	mem_unlock(mbox.handle, mbox.mem_ref);
	mem_free(mbox.handle, mbox.size);
	mbox_close(mbox.handle);
	printf("Failed to map DMA controller.\n");
	exit(-1);
    }

    pwm = map_peripheral(PWM_VIRT_BASE, PWM_LEN);

    if (pwm == MAP_FAILED) {
	unmapmem(mbox.virt_addr, mbox.size);
	mem_unlock(mbox.handle, mbox.mem_ref);
	mem_free(mbox.handle, mbox.size);
	mbox_close(mbox.handle);
	munmap((void *)dma, DMA_LEN);
 	printf("Failed to map PWM controller.\n");
	exit(-1);
    }

    clk = map_peripheral(CLK_VIRT_BASE, CLK_LEN);

    if (clk == MAP_FAILED) {
	unmapmem(mbox.virt_addr, mbox.size);
	mem_unlock(mbox.handle, mbox.mem_ref);
	mem_free(mbox.handle, mbox.size);
	mbox_close(mbox.handle);
	munmap((void *)dma, DMA_LEN);
	munmap((void *)pwm, PWM_LEN);
 	printf("Failed to map clock controller.\n");
	exit(-1);
    }

    gpio = map_peripheral(GPIO_VIRT_BASE, GPIO_LEN);

    if (gpio == MAP_FAILED) {
        unmapmem(mbox.virt_addr, mbox.size);
	mem_unlock(mbox.handle, mbox.mem_ref);
	mem_free(mbox.handle, mbox.size);
	mbox_close(mbox.handle);
	munmap((void *)dma, DMA_LEN);
	munmap((void *)pwm, PWM_LEN);
	munmap((void *)clk, CLK_LEN);
 	printf("Failed to map gpio controller.\n");
	exit(-1);
    }

}

//blocks untill line is available
static int send(void *arg) {

    int i;

    for(;;) {

	//fill output array with encoded input values
	len = 0;
	line = NULL;
	bytes = 0;
	getline(&line, &len, fp);

	//blank output array
	for (i = 0; i < (blocks * 3); i += 3) {

	    buf[i + 0] = 0x92492492;
	    buf[i + 1] = 0x49249249;
	    buf[i + 2] = 0x24924924;

	}

	buf[blocks * 3] = 0x0;

	//fill output array with data
	bytes = 0;
	byte = strtok(line, " ");

	while (byte != NULL) {

	    sscanf(byte, "%u", &value);
	    byte = strtok(NULL, " ");


	    bytes++;

	    if (bytes < ((length * 3) + 1)) {

		for (i = 0; i < 8; i++) {

		    if ((value & (0x80 >> i)) != 0) buf[(((((bytes - 1) * 8) + i) * 3) + 1) / 32] |= (0x80000000 >> ((((((bytes - 1) * 8) + i) * 3) + 1) % 32));

		}

	    }

	}

	free(line);

	//wait for dma idle
	while((dma[DMA_CS] & 1) != 0) {};

	//wait for pwm fifo empty
	while((pwm[PWM_STA] & (1 << 1)) == 0) {};

	//disable channel
	pwm[PWM_CTL] &= ~1;
	udelay(50);

	//clear fifo
	pwm[PWM_CTL] |= (1 << 6);
	udelay(500);

	//dma setup and on
	dma[DMA_CS] = DMA_RESET;
	udelay(10);
	dma[DMA_CS] = DMA_END;
	dma[DMA_CONBLK_AD] = mem_virt_to_phys(CB0);
	dma[DMA_DEBUG] = 7; // clear debug error flags
	dma[DMA_CS] = (8 << 20) | (4 << 16) | 1; // go, medium priority

	//enable channel
	pwm[PWM_CTL] |= 1;
	udelay(10000);

    }

    return 0;

}


int main(int argc, char **argv) {

    if ( argc < 2 ) {

	printf("You must specify a length value.\n");

    }

    sscanf(argv[1], "%u", &length); 
    blocks = ((length * 24) / 32) + 1;
    umask(0);
    unlink("/dev/pi2ws2812pwm");

    if (mkfifo("/dev/pi2ws2812pwm", S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH) == -1) {

	printf("Device file creation error. Program must be run as root (or use sudo).\n");
	return(-1);

    }

    fp = fopen("/dev/pi2ws2812pwm", "r+");
    map();
    dma = dma + ((DMA0_CHANNEL * 0x100)/4);
    buf = (uint32_t *)mbox.virt_addr + 8;

    //dma off
    dma[DMA_CS] &= ~1;
    udelay(10);
    dma[DMA_CS] = DMA_RESET;
    udelay(10);

    //set up control blocks
    CB0 = (CB *)mbox.virt_addr;
    CB0->TI = (1 << 26) | (16 << 21) | (5 << 16) | (1 << 8) | (1 << 6) | (1 << 3);
    CB0->SOURCE = mem_virt_to_phys(buf);
    CB0->DEST = 0x7e20c018;
    CB0->LENGTH= (blocks * 12) + 4;
    CB0->NEXT = 0;

    //pwm out on gpio 18
    gpio[GPIO_FSEL1] &= ~(7 << 24);
    gpio[GPIO_FSEL1] |= (2 << 24);

    clk[PWMCLK_CNTL] = 0x5A000000 | (clk[PWMCLK_CNTL] & ~((0xff<<24) | (1<<4)));
    while ((clk[PWMCLK_CNTL] & (1 << 7)) != 0) {};

    //set up clock based on 54 MHz for Pi 4 or 19.2 MHz for or others
    if(periph_virt_base == 0xfe000000) {

	clk[PWMCLK_DIV] = 0x5A000000 | (23<<12);

    } else { 

	clk[PWMCLK_DIV] = 0x5A000000 | (8<<12);
    }

    clk[PWMCLK_CNTL] = 0x5A000000 | 1;
    udelay(5);
    clk[PWMCLK_CNTL] = 0x5A000000 | (clk[PWMCLK_CNTL] & ~(0xff<<24)) | (1<<4);
    udelay(5);

    //Initialise PWM
    pwm[PWM_CTL] = (1 << 6);
    udelay(50);
    pwm[PWM_DMAC] = (1 << 31) | (7 << 8) | 7;
    udelay(50);
    pwm[PWM_RNG1] = 32;
    udelay(50);
    pwm[PWM_CTL] = (1 << 5) | (1 << 1);
    udelay(50);

    //launch background process
    stack = malloc(1024 * 1024);
    top = stack + (1024 * 1024);
    clone(send, top, SIGCHLD, NULL);
    fclose(fp);
    return 0;

}
