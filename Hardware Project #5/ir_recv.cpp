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

int fd = open("/dev/input/event0", O_RDONLY); //Open the raw input - you may need to change the number "0" to "1" or "2" if this code does not work

fd_set r_set; //Initialize a fd set (used for creating a timeout)

struct timeval time_len; //Time structure for timeout

struct input_event event; //Holds the input event

char buff[sizeof(event)]; //Buffer for raw data

int status; //Status variable

int read_data(int8_t *output, int timeout_len) { //Read raw data from fd with timeout of timeout seconds and return current key (success code will be 00 for error, 11 for timeout, 22 for unaccepted event (not a pressed ev_key event) and 99 for successful)
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
		else {
			*output = 0x00;
			return 22; //Set 'other event' code
		}
	}
}

int main() {
	int8_t key; //Variable to hold final key
	int8_t success; //Variable to hold return code of function

	printf("Setup complete - entering main loop\n");

	while (1) { //Loop forever
		success = read_data(&key, 1); //Read the data with timeout of 1 second
		if (success == 99) { //Check that data is valid
			printf("KEY DOWN: %hhx\n",key);
			fflush(stdout); //Flush stdout
			if (key == KEY_POWER) { //Shutdown button pressed
				system("sudo wall -n ~~~~~~~~~~~~~~~Shutting down in 30 seconds~~~~~~~~~~~~~~~Press any button to cancel~~~~~~~~~~~~~~~"); //Send warning message
				while (1) { //Loop to remove bad events
					success = read_data(&key, 30); //See if another button is pressed within 30 seconds
					if (success == 11 | success == 99) break; //Look for timeout or key press
				}
				if (key == 0x00) { //No button was pressed
					system("sudo wall -n ~~~~~~~~~~~~~~~Shutting down now~~~~~~~~~~~~~~~"); //Send final warning message
					system("sudo shutdown -h now"); //Shutdown now
					exit(0); //Exit the program
				}
				else { //Some button was pressed
					system("sudo wall -n ~~~~~~~~~~~~~~~Shutdown cancelled~~~~~~~~~~~~~~~"); //Send cancelled message
				}
			}
		}
	}
}
