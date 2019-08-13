#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <gpiod.h>
#include <fcntl.h>
#include <sys/stat.h>


struct gpiod_chip* chip;
struct gpiod_line* clk;
struct gpiod_line* cs;
struct gpiod_line* din;
int bytes, value;
char * line;
char * byte;
int len;

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

int main(void) {

    int i;
    FILE * file;

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
    command(0xa0f);
    command(0xb07);
    command(0xc01);
    command(0xf00);

    //set up fake device
    mkfifo("/dev/pi2max7219", 666);
    file = fopen("/dev/pi2max7219", "r+");

    if (file <= 0) {
        printf("\nError: insufficient user permissions!\n\n");
	return 1;
    }

    //receive data and display it
    for(;;) {

	len = 0;
	line = NULL;
	bytes = 0;

	getline(&line, &len, file);
	byte = strtok(line, " ");

	while (byte != NULL) {

	    sscanf(byte, "%u", &value);
    	    byte = strtok(NULL, " ");
    	    bytes++;
    	    if (bytes < 9) command((bytes << 8) | value);

	}

	free(line);

    }

    fclose(file);
    gpiod_chip_close(chip);
    return 0;
}
