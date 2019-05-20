#!/bin/bash

#Shell script to move a servo according to the temperature of the CPU

#Function definitions
#	setup_pin	Setup pin 18 for PWM
#	get_temp	Read the CPU temperature and format it correctly, then return the value
#	main		Main program logic: run the setup code then enter infinite loop: run get_temp and check if changed, if so then adjust the servo, delay 15 seconds, repeat

#Created by Alex Mous, 2019
#Licensed under the CC BY-SA 4.0

declare -A convert=( [0]=200 [1]=190 [2]=180 [3]=170 [4]=160 [5]=150 [6]=140 [7]=130 [8]=120 [9]=110 [10]=100 ) #Create an array to map the temperature values to servo PWM values
PREV_TEMP=-1 #Variable to store the previous temperature

setup_pin () { #Setup function
	gpio -g mode 18 pwm #Setup pin 18 for PWM
	gpio -g pwm 18 150 #Set pulse width to 1.5ms
	gpio pwm-ms #Set mark/space mode, so frequency (50Hz)=19200000/clock_divider/range
	gpio pwmc 192 #Set clock divider to 192
	gpio pwmr 2000 #Set the range to 2000
}

get_temp () { #Retrieve the temperature and store it in the return variable
	RAW=`cat /sys/class/thermal/thermal_zone0/temp` #Open and read temperature file
	RAW=$[(($RAW/1000)+5)/10] #Convert the temperature to a range between 1 and 10 (rounding to the nearest ten)
	return $RAW #Return the temperature
}

main () { #Main function
	setup_pin #Run the setup function
	while : #Forever loop
	do
		get_temp #Get the temperature
		TEMP=$? #Read the return variable
		ANGLE=${convert[$TEMP]} #Input the temperature into the array to find the servo PMW value
		if [ $TEMP -ne $PREV_TEMP ]; then #Check that the new temp is different from the previous temp
			gpio -g pwm 18 200 #Start at 0
			sleep 1 //Wait for the servo to move
			gpio -g pwm 18 $ANGLE #Set the servo to the new PWM value
		fi
		sleep 5 #Limit on loop frequency
		PREV_TEMP=$TEMP #Set the previous temperature to equal the new temperature
	done
}

main #Run the main logic

