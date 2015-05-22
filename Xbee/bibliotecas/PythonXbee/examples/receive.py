#! /usr/bin/python

import config
import serial
import time
from xbee import ZigBee

def toHex(s):
    lst = []
    for ch in s:
        hv = hex(ord(ch)).replace('0x', '')
        if len(hv) == 1:
            hv = '0'+hv
        hv = '0x' + hv
        lst.append(hv)

def decodeReceivedFrame(data):
	source_addr_long = (data['source_addr_long'])
	source_addr = (data['source_addr'])
	options = (data['options'])
	rfData = int(data['rf_data'],0)
	return [source_addr_long, source_addr, options, rfData]

PORT = '/dev/ttyUSB0'
BAUD_RATE = 9600

# Open serial port
ser = serial.Serial(PORT, BAUD_RATE)

zb = ZigBee(ser, escaped=True)

while True:
	try:
		data = zb.wait_read_frame()
		decodedData = decodeReceivedFrame(data)
		print (decodedData)
	except KeyboardInterrupt:
		break
