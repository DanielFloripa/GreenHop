#!/usr/bin/env python
''' ' ! /usr/bin/python'''

from xbee import ZigBee
import serial
import struct
import datetime
import time
import math
import ast
import socket
import json
from zbxsend import Metric, send_to_zabbix

PORT = '/dev/ttyUSB0'
BAUD_RATE = 9600
ZBX_PORT = 10051
ZBX_SERVER = '10.10.10.162'
ZBX_KEY = [
 'temp', 'humi', 'dewpoint', 'temp2', 'mq2Gas', 'presence',
 'fire','pressure','pressureSea', 'altitude', 'altitudeSea',
  'energyCons', 'noise', 'light', 'counter', 'dewpointPython']

# Configuracao estatica para cada nodo da rede
# '1' para ativar funcionalidade e '0' para desativar
# @TODO: Funcao ainda a ser imprementada como xbee.send()

#NODE_CONFIG[node][1,1,1,1,1,1,1,1,1,1,1,1,1,1,1]
  
'''  Metric: (host, key, value, clock)
send_to_zabbix([Metric('teste', 'node2.temp', 32)], ZBX_SERVER, ZBX_PORT)'''

# Transforma binario em hex
def hex(bindata):
    return ''.join('%02x' % ord(byte) for byte in bindata)

# Calcula ponto de orvalho, Formula do arduino
def dewPoint(celsius, humidity):
	RATIO = 373.15 / (273.15 + celsius)
	SUM = -7.90298 * (RATIO - 1)
	SUM += 5.02808 * math.log10(RATIO)
	SUM += (-1.3816e-7) * (math.pow(10, (11.344 * (1 - 1/RATIO ))) - 1)
	SUM += (8.1328e-3) * (math.pow(10, (-3.49149 * (RATIO - 1))) - 1)
	SUM += math.log10(1013.246)
	VP = math.pow(10, (SUM-3)) * humidity;
	T = math.log(VP/0.61078)
	return ((241.88*T)/(17.558-T))

# Idem ponto Orvalho
def dewPointFast(celsius, humidity):
	a = 17.271
	b = 237.7
	temp = (a * celsius) / (b + celsius) + math.log(humidity/100)
	Td = (b * temp) / (a - temp)
	return Td

# Envia para o Zabbix em json
def send_zbx(node, dataVector, clock):
	# print "Tamanho do vetor: ", len(dataVector) #deve ser 16
	for index in range(len(dataVector)):
		send_to_zabbix([Metric(node, ZBX_KEY[index], dataVector[index], clock)], ZBX_SERVER, ZBX_PORT)

# Cria um pacote estruturado
def append_packet(s, sizeToRead):
	x=0
	y=4
	while x < sizeToRead:
		s.append("{0:.2f}".format(struct.unpack('f',response['rf_data'][x:y])[0]))
		x+=4
		y+=4
	temp = ast.literal_eval(s[0])
	humi = ast.literal_eval(s[1])
	print "t:", temp, "h:",humi
	if (temp + humi) > 0:
		s.append("{0:.2f}".format(dewPointFast(temp,humi)))
	else:
		s.append(-100)
	return s
 
# Imprime e envia para zabbix
def print_and_send(node, dataVector, clock):
	print node, dataVector, "\n"
	send_zbx(node, dataVector, clock)


# Abre porta serial
ser = serial.Serial(PORT, BAUD_RATE)
 
# Cria um objeto Xbee-API
xbee = ZigBee(ser,escaped=True)
 
# loop principal
while True:
	try:
		response = xbee.wait_read_frame()#wait_read_frame calls XBee._wait_for_frame() and waits until a valid frame appears on the serial port.
						#Once it receives a frame, wait_read_frame attempts to parse the data contained within it and returns the
						#resulting dictionary
		clock = time.time() # formato unix
		## clock = datetime.datetime.now().isoformat()
		sa = hex(response['source_addr_long'][6:]) # apenas os dois ultimos bytes do endereco
		rf = hex(response['rf_data']) # para extrair o tamanho do pacote
		src_addr = hex(response['source_addr'][0:])
		options = hex(response['options'][0:]) # Opcoes define a intencao do pacote
		datalength = len(rf) # qtdd de bits
		qtdd_packets = datalength/8 # retorna a qtdd de pacotes de 1 byte
		sizeToRead = qtdd_packets*4 # tamanho para ler do pacote de resposta
		if options != '41':
			print "Nova opcao (!=41): ", options, "\n"
		if datalength != 120:
			print "Tamanho anormal (!=120) de pacote: ", datalength, "\n"
		if sa == "9acb":#2
			node2 = 'node2'
			iniVec2 = []
			dataVector2 = append_packet(iniVec2, sizeToRead)
			print_and_send(node2, dataVector2, clock)
		elif sa=="a04e" :#3
			node3 = 'node3'
			iniVec3 = []
			dataVector3 = append_packet(iniVec3, sizeToRead)
			print_and_send(node3, dataVector3, clock)
		elif sa=="9772" :#4
			node4 = 'node4'
			iniVec4 = []
			dataVector4 = append_packet(iniVec4, sizeToRead)
			print_and_send(node4, dataVector4, clock)
		elif sa=="b059" :#5
			node5 = 'node5'
			iniVec5 = []
			dataVector5 = append_packet(iniVec5, sizeToRead)
			print_and_send(node5, dataVector5, clock)
		elif sa=="9f78" :#6
			node6 = 'node6'
			iniVec6 = []
			dataVector6 = append_packet(iniVec6, sizeToRead)
                        print_and_send(node6, dataVector6, clock)
		elif sa=="9cd1" :#7
			node7 = 'node7'
			iniVec7 = []
			dataVector7 = append_packet(iniVec7, sizeToRead)
                        print_and_send(node7, dataVector7, clock)
		else:
			print "ATENCAO novo nodo: ", sa, " tam:",datalength,"\n"
	except KeyboardInterrupt:
		break
ser.close()
