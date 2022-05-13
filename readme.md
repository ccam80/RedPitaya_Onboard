# Getting started
## Create SD Card image
The shard memory consisting of two files (mem and cma) both located in the dev folder.
To get it working like this the SD card image of Parvel Demin is necessary, it can be installed to the RedPitaya according to the description in the LED blinker tutorial:
http://pavel-demin.github.io/red-pitaya-notes/led-blinker/

or follow this steps

-	Download SD Card image from https://www.dropbox.com/sh/5fy49wae6xwxa8a/AAB1xH748EwOday_5ZN24nsva/red-pitaya-debian-9.13-armhf-20210423.zip?dl=1
-	If SD Card have been used before, it’s may be necessary to remove all partitions and create a new one. Can be done by windows tool “disk management”
-	Write the image to the SD Card, e.g. with Win32 Disk Imager: https://sourceforge.net/projects/win32diskimager/
- Fix PC IP to 192.168.1.x and connect Ethernet wire
- Connect via SSH (Putty) to 192.168.1.100:22
  ````
  Account: root
  PW: changeme
  ````
- Type following commands (for resize partition):
  ````
  echo -e "d\n2\nw" | fdisk /dev/mmcblk0
  parted -s /dev/mmcblk0 mkpart primary ext4 16MiB 100%
  resize2fs /dev/mmcblk0p2
  ````
- To add another IP address, open ``/etc/network/interfaces.d/eth0`` and add the following lines:
  ````
  iface eth0 inet static
  address 192.168.1.100
  ````
- Update environment and install useful tools with following commands:
  ````
  apt-get update
  apt install build-essential
  apt-get install cron
  ````

## Run the Server
### Single run
- Coppy ``feedback_server.c`` with a FTP client (e.g. WinSCP) to the FPGA into ``/usr/src``
- Connect via SSH (Putty) to 192.168.1.100:22
  ````
  Account: root
  PW: changeme
  ````
- Type ``gcc -o feedback_server -g -O3 -march=armv7-a -mtune=cortex-a9 -D_GNU_SOURCE /usr/src/feedback_server.c -lm`` to compile the *c file
- Type ``./feedback_server`` to start the server
- Press ``strg+c`` to kill the server

### Auto run @ reboot
- Compile like for single run
- Open SSH console
- type ``crontab -e``to open the cronejob file
- add the line ``@reboot ./feedback_server``
- close and save file
Remark: To disable the auto run, simply delete the added line
