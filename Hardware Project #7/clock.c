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
			0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70,
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

    int i, hour;

    time(&rawtime);
    info = localtime(&rawtime);
    for(i = 0; i < 8; i++) buf[i] = ((font[((info->tm_min % 10) * 8) + i]) << 26);
    for(i = 0; i < 8; i++) buf[i] |= ((font[((info->tm_min / 10) * 8) + i]) << 32);
    hour = info->tm_hour;
    if (hour > 12) hour -= 12;
    for(i = 0; i < 8; i++) buf[i] |= ((font[((hour % 10) * 8) + i]) << 42);

    if ((hour / 10) != 0) for(i = 0; i < 8; i++) buf[i] |= ((font[((hour / 10) * 8) + i]) << 48);

    //colon
    buf[2] |= ((uint64_t)1 << 42);
    buf[5] |= ((uint64_t)1 << 42);
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

	//scroll and display
	for (i = 0; i < 40; i++) {

	    send();
	    for (j = 0; j < 8; j++) buf[j] <<= 1;
	    usleep(70000);

	}

	sleep(3);

    }

    return 0;

}