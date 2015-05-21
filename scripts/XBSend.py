#!/usr/bin/env python

from xbee import ZigBee
import serial
import struct
import math
import ast
import socket

PORT = '/dev/ttyUSB0'
BAUD_RATE = 9600

# Abre porta serial
ser = serial.Serial(PORT, BAUD_RATE)

# Cria um objeto Xbee-API
xbee = ZigBee(ser,escaped=True)

while 1:
	# Send data (a string)
	ser.write("teste")
