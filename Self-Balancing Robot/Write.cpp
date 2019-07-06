//Copyright (c) 2019, Alex Mous
//Licensed under the Creative Commons Attribution-ShareAlike 4.0 International (CC-BY-4.0)

#include "GPIO.h"
#include <iostream>
#include <string.h>

//Define the motor pins for the left and right motor
#define MOTOR_L_PIN_1 "6" //left motor, pin 1
#define MOTOR_L_PIN_2 "13" //left motor, pin 2
#define MOTOR_R_PIN_1 "19" //right motor, pin 2
#define MOTOR_R_PIN_2 "26" //right motor, pin 2

//Create GPIO objects
GPIO motors[4] {MOTOR_L_PIN_1,MOTOR_L_PIN_2,MOTOR_R_PIN_1,MOTOR_R_PIN_2};

int setupMotors() {
	int status; //Create a status variable

	for (int i = 0; i < 4; i++) { //Cycle through pins and do setup
		status = motors[i].setupPin(1); //Create pin
		if (status != 0) return 1; //Return error code

		status = motors[i].setDirection(1); //Set pin direction to output
		if (status != 0) return 1; //Return error code

		status = motors[i].writeValue(0); //Set all pins to low
		if (status != 0) return 1; //Return error code
	}
}

int cleanup() {
	int status;

	for (int i = 0; i <= 4; i++) { //Cycle through pins
		status = motors[i].setupPin(0); //Destroy (cleanup) pin
		if (status != 0) return 1; //Return error code
	}
}

int main() {
	setupMotors();




	cleanup();
	return 0; //Return non-error code
}

