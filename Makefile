# export BOARD_TAG=mega ; export MONITOR_PORT=/dev/ttyUSB0 ; export BOARD_SUB=atmega1280
# export BOARD_TAG=uno ; export MONITOR_PORT=/dev/ttyACM0
#
# Options:
# make
# make upload
# make monitor

BOARD_TAG    = mega
BOARD_SUB    = atmega1280
MONITOR_PORT = /dev/ttyUSB0
include ../Makefiles/Arduino.mk
