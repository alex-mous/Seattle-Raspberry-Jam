#include <stdio.h>
#include <iostream>
#include <map>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/input.h>
#include <memory.h>


fd_set r_set;

struct input_event event;

int fd = open("/dev/input/event0", O_RDONLY);

char buff[sizeof(event)]; //Buffer for raw data

char scancode; //Scancode (1 byte)

struct timeval time_len; //Time structure for timeout

int status; //Status variable

int read_data(int8_t *output, int timeout_len) { //Read raw data from fd with timeout of timeout seconds and return current key (success code will be 0x00 for error, 0x11 for timeout, 0x22 for unaccepted event (not a pressed ev_key event) and 0xff for successful)
	FD_ZERO(&r_set); //Clear fd set
	FD_SET(fd, &r_set); //Add our fd to the set

	time_len.tv_sec = timeout_len; //Reset timers
	time_len.tv_usec = 0; //Set timer to 10 seconds

	status = select(fd + 1, &r_set, NULL, NULL, &time_len); //Check for data

	if (status < 0) { //error or timeout
		*output = 0x00; //Set default output
		return 00; //Set error code
	}
	else if (status == 0) { //error or timeout
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

	while (1) { //Loop forever
		success = read_data(&key, 1); //Read the data with timeout of 1 second
		if (success == 99) { //Check that data is valid
			printf("KEY: %hhx\n",key);
			fflush(stdout); //Flush stdout
			if (key == KEY_POWER) { //Shutdown button pressed
				printf("POWER KEY\n");
				system("sudo wall ~~~~~~~~~~~~~~~Shutting down in 30 seconds~~~~~~~~~~~~~~~Press any button to cancel~~~~~~~~~~~~~~~"); //Send warning message
				while (1) { //Loop to remove bad events
					success = read_data(&key, 30); //See if another button is pressed within 30 seconds
					if (success == 11 | success == 99) break; //Look for timeout or key press
				}
				if (key == 0x00) { //No button was pressed
					system("sudo wall ~~~~~~~~~~~~~~~Shutting down now~~~~~~~~~~~~~~~"); //Send final warning message
					system("sudo shutdown -h now"); //Shutdown now
					exit(0); //Exit the program
				}
				else { //Some button was pressed
					system("sudo wall ~~~~~~~~~~~~~~~Shutdown cancelled~~~~~~~~~~~~~~~"); //Send cancelled message
				}
			}
		}
	}


}
