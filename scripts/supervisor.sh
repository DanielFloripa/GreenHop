#!/bin/sh
## Script que supervisiona o monitor em python


sudo service zabbix_server start ## verifica se servidor esta rodando
#atualiza hora e data
sudo service ntp stop
sudo ntpdate 10.20.107.1
sudo service ntp start

$cont
while true; do
	cont=$((cont + 1))
	echo 'Executando script em Python: ' $cont
	python /root/xbee/pyMonitorLABP2D_v4.py;
done;
