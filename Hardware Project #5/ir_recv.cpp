#include <stdio.h>
#include <iostream>
#include <map>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

fd_set r_set;

int fd = open("/dev/input/event0", O_RDONLY);

char buff[32]; //Buffer for raw data

int8_t scancode; //Scancode (1 byte)

struct timeval time_len; //Time structure for timeout

int status; //Status variable

std::map<int8_t, std::string> keymap { //Keymap dictionary
{0x01 , "1"},
{0x02 , "2"},
{0x03 , "3"},
{0x04 , "4"},
{0x05 , "5"},
{0x06 , "6"},
{0x07 , "7"},
{0x08 , "8"},
{0x09 , "9"},
{0x00 , "0"}
};

int read_data(std::string *output, int timeout) { //Read raw data from fd with timeout of timeout seconds and check if in keymap table: if so return value, if not return nothing
	FD_ZERO(&r_set); //Clear fd set
	FD_SET(fd, &r_set); //Add our fd to the set

	time_len.tv_sec = timeout; //Reset timers
	time_len.tv_usec = 0; //Set timer to 10 seconds

	status = select(fd + 1, &r_set, NULL, NULL, &time_len); //Check for data

	if (status <= 0) { //error or timeout
		*output = ""; //Set default output
	}
	else { //Data to read
		read(fd, buff, 32); //Read 32 bytes into buff
		scancode = buff[12]; //Use the 13th byte as the scancode
		*output = keymap[scancode]; //Find which key this is
	}
}

int main() {
	std::string key; //Variable to hold final key

	while (1) {
		read_data(&key, 1); //Read the data with timeout of 1 second
		if (key != "") {
			std::cout << key << "\n";
			if (keymap[scancode] == "0") { //Shutdown button
				system("sudo wall ~~~~~~~~~~~~~~~Shutting down in 30 seconds~~~~~~~~~~~~~~~Press any button to cancel~~~~~~~~~~~~~~~"); //Send warning message
				read_data(&key, 30); //See if another button is pressed within 30 seconds
				if (key == "") { //No button was pressed
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


//	fclose(fd);
