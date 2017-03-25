#!/bin/sh

cd /home/user/contiki/tools/
sudo /home/user/contiki/tools/tunslip6 -B 115200 -s /dev/ttyUSB0 aaaa::1/64
