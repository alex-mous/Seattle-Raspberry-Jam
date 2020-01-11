#!/bin/bash
#Copyright (c) 2019, PolarPiBerry
#Licensed under the CC-BY-SA 2.0

#Connect a galvanometer to your Raspberry Pi and use it to display the processor temperature

#Enter your max and min values here
min=100;
max=600;

#setup GPIO pin 18 for PWM
sudo gpio -g mode 18 pwm

while :
    do
        raw=`vcgencmd measure_temp`;
        temp=${raw:5:2}; #should never exceed 99!
        let temp="(($temp - 30)*100)/69"; #remove base temperature, multiply by 100 (for percent) and set range (since 99-30=69)
        let pwmv="(($max-$min)*$temp)/100 + $min";
        sudo gpio -g pwm 18 $pwmv #output to the meter
        sleep 5;
    done