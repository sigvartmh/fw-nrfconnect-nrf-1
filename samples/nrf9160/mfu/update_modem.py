import sys
import argparse
import logging
import time
import os
from pynrfjprog import HighLevel

def run(serial_device_name, nrf9160_modem_firmware_zip_location, baudrate):
    if not os.path.exists(nrf9160_modem_firmware_zip_location):
        raise Exception("Invalid path to modem firmware zip file provided. Skipping nrf9160 modem device firmware upgrade example.")
    print('# nrf9160 modem firmware upgrade over serial port example started...')

    # Configure logging to see HighLevel API output
    logging.basicConfig(level=logging.INFO, format="%(message)s")

    # Find the hex file to flash. It should be located in the same directory as this example file.
    with HighLevel.API() as api:
        start_time = time.time()
        with HighLevel.ModemUARTDFUProbe(api, serial_device_name, baudrate) as modem_dfu_probe:
            modem_dfu_probe.program(nrf9160_modem_firmware_zip_location)
            modem_dfu_probe.verify(nrf9160_modem_firmware_zip_location)

        # Print upload time
        print("------------------------------------------------------------")
        print("The operation took {0:.2f} seconds ".format(time.time() - start_time))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("uart", default=None, help="Serial device name for UART")
    parser.add_argument("firmware", default=None, help="Path to nrf9160 modem firmware zip folder")
    parser.add_argument("baudrate", default=115200, help="Baud rate of UART communication", type=int)
    args = parser.parse_args()
    run(args.uart, args.firmware, args.baudrate)

