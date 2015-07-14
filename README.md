# seismo
Seismograph for the Raspberry Pi

Installation:
I’m using a patched kernel from EMLID and installing a library called wiringPi.
Steps:

-Download a raspian OS image (either from raspberrypi.org, or get a modified one from http://www.emlid.com/raspberry-pi-real-time-kernel/)

-Copy it to the sd card using win32DiskImager (or something else if your on Mac or Linux)
-Boot up the pi

-run the configurator

		sudo raspi-config
		
-Set a new password (especially if you plan on this pi being connected to the net)

-Expand the file system to fill the card

-Go to International options

  -Add locale en_AU UTF-8
  
  -Change timezone
  
  -Change keyboard layout (the one that works for me is generic 101/other/US
  
-Enable ssh

-reboot

-Install wiringPi

		git clone git://git.drogon.net/wiringPi
		
		cd wiringPi
		
		./build

