//Copyright (c) 2019, Alex Mous & Seattle Raspberry Jam
//Licensed under the CC BY-SA 4.0

//Reads the device /dev/input/eventN, checks for EV_KEY events and reports the key on the key-down event
//Note: the keymap for the remote needs to be initialized before running this code (otherwise no events will show up)
//For more information, see https://github.com/alexmous/Seattle-Raspberry-Jam/tree/master/Hardware%20Project%20%235

#include <stdio.h>
#include <iostream>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>
#include <memory.h>
#include <thread>
#include <string>

int fd1 = open("/dev/input/event0", O_RDONLY); //Open the raw input - you may need to change the number "0" to "1" or "2" if this code does not work
int fd0 = open("/dev/input/event1", O_RDONLY); //Open the raw input - you may need to change the number "0" to "1" or "2" if this code does not work

int disp;

fd_set r_set; //Initialize a fd set (used for creating a timeout)

struct timeval time_len; //Time structure for timeout

struct input_event event; //Holds the input event

char buff[sizeof(event)]; //Buffer for raw data

int status; //Status variable

int16_t sum = 0;

int read_data(int16_t *output, int timeout_len, int fd) { //Read raw data from fd with timeout of timeout seconds and return current key (success code will be 00 for error, 11 for timeout, 22 for unaccepted event (not a pressed ev_key event) and 99 for successful)
	FD_ZERO(&r_set); //Clear fd set
	FD_SET(fd, &r_set); //Add our fd to the set

	time_len.tv_sec = timeout_len; //Reset timers
	time_len.tv_usec = 0; //Set timer to 10 seconds

	status = select(fd + 1, &r_set, NULL, NULL, &time_len); //Check for data

	if (status < 0) { //error
		*output = 0x00; //Set default output
		return 00; //Set error code
	}
	else if (status == 0) { //timeout
		*output = 0x00; //Set default output
		return 11; //Set timeout code
	}
	else { //Data to read
		read(fd, buff, sizeof(event)); //Read into event
		memcpy(&event, buff, sizeof(event)); //Copy the buffer into the event structure
		//Check that the event is a key and determine which type of press it is
		if (event.type == EV_KEY && event.value == 1) {
			*output = event.code;
			return 99;
		}
		else if (event.type == EV_REL) {
			*output = event.value;
			return 99;
		}
		else {
			*output = 0x00;
			return 22; //Set 'other event' code
		}
	}
}

int thread_fd0() { //Rotary encoder
	int16_t encoder; //Variable to hold final key
	int8_t success_fd0; //Variable to hold return code of function
	char *sum_c;


	while (1) { //Encoder
		success_fd0 = read_data(&encoder, 1, fd0); //Read the data with timeout of 1 second
		if (success_fd0 == 99) {
			printf("Rotary Encoder: %d\n",encoder);
			sum = sum + encoder;
			sprintf(sum_c, "SUM: %d\n", sum);
			const char *sum_str = sum_c;
			success_fd0 = write(disp, sum_str, 10);
			fflush(stdout); //Flush stdout
		}
	}
}

int thread_fd1() {
	int16_t key; //Variable to hold final key
	int8_t success_fd1; //Variable to hold return code of function

	while (1) { //Button
		success_fd1 = read_data(&key, 1, fd1); //Read the data with timeout of 1 second
		if (success_fd1 == 99) { //Check that data is valid
			printf("KEY DOWN: %hhx\n",key);
			fflush(stdout); //Flush stdout
			success_fd1 = write(disp, "\n\n", 1);
			sum = 0;
		}
	}
}


int main() {
	system("sudo setfont Lat7-Terminus16"); //Change terminal
	system("sudo con2fbmap 8 1"); //Change terminal
	system("sudo chvt 8"); //Change terminal

	disp = open("/dev/tty8", O_WRONLY);

	printf("Setup complete - entering main loop\n");

	std::thread(&thread_fd0).detach();
	std::thread(&thread_fd1).detach();

	while(1); //Loop and do nothing
}
