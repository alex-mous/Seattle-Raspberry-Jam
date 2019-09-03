/*
 * clock.c: userspace clock program using mincepi's pi2max2719 character device for the Raspberry Pi
 * Copyright (c) 2019 mincepi <mincepi@gmail.com>
 *
 * Clock font is based on the Lat7-Terminus12x6 Linux console font.
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
 * To compile run: gcc -o clock clock.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

int i, j;
uint64_t buf[8];
time_t rawtime;
struct tm *info;

uint64_t font[80] = {	0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70,
			0x40, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
			0x70, 0x88, 0x88, 0x10, 0x20, 0x40, 0x80, 0xf8,
			0x70, 0x88, 0x08, 0x30, 0x08, 0x08, 0x88, 0x70,
			0x08, 0x18, 0x28, 0x48, 0x88, 0xf8, 0x08, 0x08,
			0xf8, 0x80, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70,
			0x70, 0x80, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x70,
			0xf8, 0x08, 0x08, 0x10, 0x10, 0x20, 0x20, 0x20,
			0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x88, 0x70,
			0x70, 0x88, 0x88, 0x88, 0x78, 0x08, 0x08, 0x70 };

//fill buf with current time in bitmap form
void get() {

    int i;
    int position = 48;
    for(i = 0; i < 8; i++) buf[i] = 0;

    time(&rawtime);
    info = localtime(&rawtime);

    //hours
    if (info->tm_hour > 12) info->tm_hour -= 12;
    if ((info->tm_hour / 10) != 0) for(i = 0; i < 8; i++) buf[i] |= ((font[((info->tm_hour / 10) * 8) + i]) << position);
    if ((info->tm_hour / 10) == 1) {position -= 4;} else {position -= 7;}
    for(i = 0; i < 8; i++) buf[i] |= ((font[((info->tm_hour % 10) * 8) + i]) << position);
    if ((info->tm_hour % 10) == 1) {position -= 4;} else {position -= 7;}

    //colon
    buf[2] |= ((uint64_t)0x80 << position);
    buf[5] |= ((uint64_t)0x80 << position);
    position -= 3;

    //minutes
    for(i = 0; i < 8; i++) buf[i] |= ((font[((info->tm_min / 10) * 8) + i]) << position);
    if ((info->tm_min / 10) == 1) {position -= 4;} else {position -= 7;}
    for(i = 0; i < 8; i++) buf[i] |= ((font[((info->tm_min % 10) * 8) + i]) << position);

    return;

}

//display clock on led
void send() {

    FILE * file;
    int i;
    file = fopen("/dev/pi2max7219", "w");
    for (i = 0; i < 8; i++) fprintf(file, "%lld ", (buf[i] >> 56));
    fprintf(file, "\n");
    fclose(file);
    return;

}

int main() {

    for(;;) {

        get();

	//scroll and display every ten seconds
	for (i = 0; i < 40; i++) {

	    send();
	    for (j = 0; j < 8; j++) buf[j] <<= 1;
	    usleep(100000);

	}

	sleep(3);

    }

    return 0;

}