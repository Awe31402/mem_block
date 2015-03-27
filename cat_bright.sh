#!/bin/bash
adb devices
adb root
while [ 1 ]
do
	sleep 1
	adb shell cat /sys/class/backlight/intel_backlight/brightness 
done
