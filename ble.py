# Connect to an "eval()" service over BLE UART.
import os
import sys
from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement
from adafruit_ble.services.nordic import UARTService

ble = BLERadio()
ble.stop_scan()
uart_connection = None

while True:
    if not uart_connection:
        print("Trying to connect...")
        for adv in ble.start_scan(ProvideServicesAdvertisement):
            if UARTService in adv.services:
                uart_connection = ble.connect(adv)
                print("Connected")
                break
        ble.stop_scan()

    if uart_connection and uart_connection.connected:
        uart_service = uart_connection[UARTService]
        while uart_connection.connected:
                 # while 1:
                    # byte = os.read(0,1)
                    # os.write(2,byte)
                    # uart_service.write(byte)
                    #if (byte == b'\n'):
                    #   break
                 # print(" ->")
                 line = sys.stdin.readline()
                 uart_service.write(line.encode("utf-8"))
                 #print(line);
                 print(uart_service.readline().decode("utf-8"))

