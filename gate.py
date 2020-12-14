#!/usr/bin/env python3

import argparse
import signal
import sys
import time
import logging

from rpi_rf import RFDevice
import RPi.GPIO as GPIO

import threading
from socket import *

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)

GPIO.setup(16,GPIO.OUT)
GPIO.setup(20,GPIO.OUT)
GPIO.setup(21,GPIO.OUT)

GPIO.setup(25,GPIO.OUT)
GPIO.setup(8,GPIO.OUT)
GPIO.setup(7,GPIO.OUT)

GPIO.setup(5,GPIO.OUT)
GPIO.setup(6,GPIO.OUT)
GPIO.setup(13,GPIO.OUT)

#IN
GPIO.output(16, False)
GPIO.output(20, False)
#GPIO.output(21, False)
GPIO.output(21, True)

#IN
GPIO.output(25, False)
GPIO.output(8, False)
GPIO.output(7, True)
#GPIO.output(7, False)

#OUT
#GPIO.output(5, False)
GPIO.output(5, True)
GPIO.output(6, False)
GPIO.output(13, False)

IN_NUM = 3
OUT_NUM = 2

def server_thread():
    server_sock = socket(AF_INET, SOCK_DGRAM)
    server_sock.bind(('192.168.0.10', 5089))

    while True:
        msg, sender = server_sock.recvfrom(1024)
        #print(msg[0], msg[1], msg[2], msg[3], msg[4], msg[5])
        global IN_NUM
        global OUT_NUM
        temp = IN_NUM
        IN_NUM = msg[0] - 48
        OUT_NUM = msg[2] - 48
       
        if temp == IN_NUM:
            continue

        print("*************************************")
        print("******** Server send message ********")
        print("*************************************")
        print("*********** Modify Status ***********")
        print("*************************************")

        if IN_NUM == 4:
            GPIO.output(5, False)
            GPIO.output(6, True)
            time.sleep(1)
            GPIO.output(6, False)
            GPIO.output(13, True)
        elif IN_NUM == 3:
            if temp == 4:
                GPIO.output(13, False)
                GPIO.output(6, True)
                time.sleep(1)
                GPIO.output(6, False)
                GPIO.output(5, True)
            else:
                GPIO.output(25, False)
                GPIO.output(8, True)
                time.sleep(1)
                GPIO.output(8, False)
                GPIO.output(7, True)
        elif IN_NUM == 2:
            if temp == 1:
                GPIO.output(16, False)
                GPIO.output(20, True)
                time.sleep(1)
                GPIO.output(20, False)
                GPIO.output(21, True)
            else:
                GPIO.output(7, False)
                GPIO.output(8, True)
                time.sleep(1)
                GPIO.output(8, False)
                GPIO.output(25, True)
        elif IN_NUM == 1:
            GPIO.output(21, False)
            GPIO.output(20 ,True)
            time.sleep(1)
            GPIO.output(20, False)
            GPIO.output(16, True)

server_t = threading.Thread(target=server_thread)
server_t.start()

print("********* Program Start *********")

client_socket = socket(AF_INET, SOCK_STREAM)
client_socket.connect(('192.168.0.7', 5090))

logging.info("Connect Success")
print("====== Connect Success =====")


rfdevice = None

# pylint: disable=unused-argument
def exithandler(signal, frame):
    rfdevice.cleanup()
    sys.exit(0)

logging.basicConfig(level=logging.INFO, datefmt='%Y-%m-%d %H:%M:%S',
                    format='%(asctime)-15s - [%(levelname)s] %(module)s: %(message)s', )

parser = argparse.ArgumentParser(description='Receives a decimal code via a 433/315MHz GPIO device')
parser.add_argument('-g', dest='gpio', type=int, default=27,
                    help="GPIO pin (Default: 27)")
args = parser.parse_args()

signal.signal(signal.SIGINT, exithandler)
rfdevice = RFDevice(args.gpio)
rfdevice.enable_rx()
timestamp = None
logging.info("Listening for codes on GPIO " + str(args.gpio))

while True:
    if rfdevice.rx_code_timestamp != timestamp:
        timestamp = rfdevice.rx_code_timestamp
        logging.info(str(rfdevice.rx_code) +
                     " [pulselength " + str(rfdevice.rx_pulselength) +
                     ", protocol " + str(rfdevice.rx_proto) + "]")

        if rfdevice.rx_code > 10000 and rfdevice.rx_code < 14000:
            print("===== Come In =====")
            status = 1
            userId = 12016 % 10000
            send_msg = '%i,%s'%(status, userId)
            client_socket.send(send_msg.encode())
            time.sleep(1.5)

        if rfdevice.rx_code > 20000 and rfdevice.rx_code <24000:
            print("===== Come Out =====")
            status = 2
            userId = 22016 % 10000
            send_msg = '%i,%s'%(status, userId)
            client_socket.send(send_msg.encode())
            time.sleep(1.5)

    time.sleep(0.01)

GPIO.cleanup()
rfdevice.cleanup()