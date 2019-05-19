#!/bin/bash

#Shell script to move a servo according to the temperature of the CPU

#Function definitions
#	setup_pin	Setup pin 18 for PWM
#	get_temp	Read the CPU temperature and format it correctly, then return the value
#	main		Main program logic: run the setup code then enter infinite loop: run get_temp and check if changed, if so then adjust the servo, delay 15 seconds, repeat

#Created by Alex Mous, 2019
#Licensed under the CC BY-SA 4.0

declare -A convert=( [0]=200 [1]=190 [2]=180 [3]=170 [4]=160 [5]=150 [6]=140 [7]=130 [8]=120 [9]=110 [10]=100 )
PREV_TEMP=-1

setup_pin () {
	gpio -g mode 18 pwm
	gpio -g pwm 18 150
	gpio pwm-ms
	gpio pwmc 192
	gpio pwmr 2000
}

get_temp () {
	RAW=`cat /sys/class/thermal/thermal_zone0/temp`
	RAW=$[(($RAW/1000)+5)/10]
	return $RAW
}

main () {
	setup_pin
	while :
	do
		get_temp
		TEMP=$?
		ANGLE=${convert[$TEMP]}
		if [ $TEMP -ne $PREV_TEMP ]; then
			gpio -g pwm 18 200
			sleep 1
			gpio -g pwm 18 $ANGLE
		fi
		sleep 5
		PREV_TEMP=$TEMP
	done
}

main

