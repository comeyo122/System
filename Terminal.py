import RPi.GPIO as gpio
import time
import argparse
import logging
import threading
from rpi_rf import RFDevice

gpio.setmode(gpio.BCM)
gpio.setwarnings(False)

rfpin = 17
hallpin = 18
hallpout = 23
pulselengthIn = 157
protocolIn = 3
codeIn = 12016

pulselengthOut = 608
protocolOut = 1
codeOut = 22016

rfdevice = RFDevice(rfpin)
rfdevice.enable_tx()
gpio.setup(hallpin, gpio.IN)
gpio.setup(hallpout, gpio.IN)

def execute():

		while True:
		if(gpio.input(hallpout) == False) :
			print("Came Out")
			rfdevice.tx_code(codeOut, protocolOut, pulselengthOut)
	
		time.sleep(0.3)

if __name__ == '__main__':
		my_thread = threading.Thread(target=execute)
	my_thread.start()
	while True:
		if(gpio.input(hallpin) == False) :
			print("Came In")
			rfdevice.tx_code(codeIn, protocolIn, pulselengthIn)
		
		time.sleep(0.3)