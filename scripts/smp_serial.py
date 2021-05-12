from crc import CrcCalculator, Configuration
import cbor2
import base64
import struct
import serial

PACKET_START = [0x06, 0x09]
PACKET_CONT = [0x04, 0x14]
PACKET_END = 0x0a
GROUP_ID_OS = 0
COMMAND_ECHO = 0
COMMAND_ERASE_SETTINGS = 6

# Operations
READ = 0
READ_RSP = 1
WRITE = 2
WRITE_RSP = 3

FRAME_SIZE = 127


class echo_packet:
    def __init__(self, data):
        return data

class mcumgr_packet:
    def __init__(self, start, data):
        self.start = start[0].to_bytes(1, "little") + start[1].to_bytes(1, "little")
        self.data = data
        self.end = PACKET_END.to_bytes(1, "little") 

    def send(self):
        return self.start + self.data + self.end

class data:
    def __init__(self, body):
        # Encode to cbor
        self.length = (len(body) + 2).to_bytes(2, "big")#16-bit
        self.body = body

    def encode(self):
        crc16 = self.crc16()
        pack = self.length + self.body + crc16
        return base64.b64encode(pack)

    def crc16(self):
        initial_value = 0
        polynomial = 0x1021
        configuration = Configuration(16, polynomial, initial_value, 0x00, False, False)
        crc_calc = CrcCalculator(configuration, True)
        data = self.body
        return crc_calc.calculate_checksum(data).to_bytes(2, "big")
        

class body:
    def __init__(self, operation, group, command, data):
        self.data = bytearray(cbor2.dumps(data))
        print(len(self.data))
        print("command:", command)
        self.header = operation.to_bytes(1, "big")+ bytes(1)+ len(self.data).to_bytes(2, "big") + group.to_bytes(2, "big") + bytes(1) + command.to_bytes(1, "big")
        print(self.header)

    def encode(self):
        return self.header + self.data

test = body(WRITE, GROUP_ID_OS, COMMAND_ECHO, {'d' : "new test"})
print(test.encode().hex())
pkt_data = data(test.encode())
echo_cmd = mcumgr_packet(PACKET_START, pkt_data.encode()).send()

with serial.Serial('/dev/ttyACM0', 115200, timeout=1) as ser:
    ser.write(echo_cmd);
    d = ser.read(len(echo_cmd))
    #print(d)

test = body(WRITE, GROUP_ID_OS, COMMAND_ERASE_SETTINGS, {'rc' : "test"})
print(test.encode().hex())
pkt_data = data(test.encode())
echo_cmd = mcumgr_packet(PACKET_START, pkt_data.encode()).send()
with serial.Serial('/dev/ttyACM0', 115200, timeout=1) as ser:
    ser.write(echo_cmd);
    d = ser.read(len(echo_cmd))
    #print(d)
