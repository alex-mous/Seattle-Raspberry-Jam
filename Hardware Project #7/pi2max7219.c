/*
 * pi2max.c: userspace driver for the pi2max2719 character device for the Raspberry Pi
 * Copyright (c) 2019 mincepi <mincepi@gmail.com>
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
 * To compile install gpiod headers (apt-get install libgpiod-dev)
 * then run: gcc -l gpiod -o pi2max7219 pi2max7219.c
 *
 * To load driver run: sudo nice -n -20 pi2max7219
 *
 * Write data to /dev/pi2max7219, ascii text hex formatted, 0-ff, space delimited for each column, with a space and newline at the end.
 *
 * For example: echo 10 22 > /dev/pi2max7219 lights fifth LED in the first column, second and sixth LEDs in the second column.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gpiod.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sched.h>
#include <sys/wait.h>

struct gpiod_chip* chip;
struct gpiod_line* clk;
struct gpiod_line* cs;
struct gpiod_line* din;
int bytes, value;
char * line;
char * byte;
FILE * file;
size_t len;
char *stack;
char *top;

//send 16 bit command to max7219
void command(int command) {

    int i;

    gpiod_line_set_value(cs, 0);

    for(i = 0; i < 16; i++) {

	if((command & (0x8000 >> i)) == 0) {

	    gpiod_line_set_value(din, 0);

	} else {

	    gpiod_line_set_value(din, 1);

	}

	gpiod_line_set_value(clk, 0);
	usleep(1);
	gpiod_line_set_value(clk, 1);
	usleep(1);

    }
 
    gpiod_line_set_value(cs, 1);
    usleep(1);
    return;

}

//get data from device file and send to display
static int send(void *arg) {

    int i;

    for(;;) {

	len = 0;
	line = NULL;
	bytes = 0;
	getline(&line, &len, file);
	byte = strtok(line, " ");

	while (byte != NULL) {

	    sscanf(byte, "%x", &value);
    	    byte = strtok(NULL, " ");
    	    bytes++;

	    if (bytes < 9) {

		value |= (bytes << 8);
		gpiod_line_set_value(cs, 0);

		for(i = 0; i < 16; i++) {

		    if((value & (0x8000 >> i)) == 0) {

			gpiod_line_set_value(din, 0);

		    } else {

			gpiod_line_set_value(din, 1);

		    }

		    gpiod_line_set_value(clk, 0);
		    usleep(1);
		    gpiod_line_set_value(clk, 1);
		    usleep(1);

		}
 
		gpiod_line_set_value(cs, 1);
		usleep(1);

	    }

	}

	free(line);

    }

    fclose(file);
    gpiod_chip_close(chip);
    return(0);

}

int main(void) {

    int i;

    //set up fake device file
    umask(0);
    unlink("/dev/pi2max7219");

    if (mkfifo("/dev/pi2max7219", S_IRUSR | S_IWUSR | S_IWGRP | S_IWOTH) == -1) {

	printf("Device file creation error. Program must be run as root (or use sudo).\n");
	return(-1);

    }

    file = fopen("/dev/pi2max7219", "r+");

    //set up gpios
    chip = gpiod_chip_open_by_name("gpiochip0");
    clk = gpiod_chip_get_line(chip, 14);
    cs = gpiod_chip_get_line(chip, 15);
    din = gpiod_chip_get_line(chip, 18);
    gpiod_line_request_output(clk, "pi2max7219", 0);
    gpiod_line_request_output(cs, "pi2max7219", 0);
    gpiod_line_request_output(din, "pi2max7219", 0);

    //blank display and initialize  max7219
    for(i = 0x100; i < 0x900; i += 0x100) command(i);
    command(0xa01); //intensity, valid values are from 0xa01 to 0xa0f
    command(0xb07);
    command(0xc01);
    command(0xf00);

    //launch background process
    stack = malloc(1024 * 1024);
    top = stack + (1024 * 1024);
    clone(send, top, SIGCHLD, NULL);
    fclose(file);
    gpiod_chip_close(chip);
    return 0;

}