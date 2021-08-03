import argparse
import serial
import time

npages = 512
page_size = 64 # bytes
eeprom_size = npages * page_size

def echo_poll():
	arduino.write(bytes([0xee]))
	arduino.flush()
	echo = arduino.read()
	print(echo)
	if (echo[0] != 0xee):
		raise RuntimeError("did not recieve correct echo byte")
	print("read")

def erase(args):
	arduino.write(b'e')
	arduino.flush()
	echo_poll()

def sdp(args):
	if (args.state == "disable"):
		arduino.write(b's')
	elif (args.state == "enable"):
		arduino.write(b'S')
	else: # something else?
		raise ValueError("unknown SDP state argument")
	arduino.flush()
	echo_poll()

def read(args):
	if (args.N > npages) or (args.N <= 0):
		raise ValueError(f"number of pages to write must be between 1 and {npages} inclusive")
	n = args.N - 1
	arduino.write(b'r')
	arduino.write(bytes(n & 0xff))
	arduino.write(bytes(n >> 8))
	arduino.flush()
	while n >= 0:
		written = args.file.write(arduino.read(page_size))
		if(written != page_size):
			raise RuntimeError("amount of bytes written to file not equal to page size")
		n -= 1
	echo_poll()

def write(args):
	if (args.N > npages) or (args.N <= 0):
		raise ValueError(f"number of pages to read must be between 1 and {npages} inclusive")
	n = args.N - 1
	arduino.write(b'w')
	arduino.write(n & 0xff)
	arduino.write(n >> 8)
	while n >= 0:
		read = arduino.write(args.file.read(page_size))
		if(read != page_size):
			raise RuntimeError("amount of bytes read from file not equal to page size")
		n -= 1
	echo_poll()

def dump(args):
	n = npages - 1
	arduino.write(b'd')
	arduino.flush()
	while n >= 0:
		data = arduino.read(page_size)
		written = args.file.write(data)
		if(written != page_size):
			raise RuntimeError("amount of bytes written to file not equal to page size")
		n -= 1
	echo_poll()

def flash(args):
	n = npages - 1
	arduino.write(b'f')
	arduino.flush()
	while n >= 0:
		read = arduino.write(args.file.read(page_size))
		if(read != page_size):
			raise RuntimeError("amount of bytes read from file not equal to page size")
		n -= 1
	echo_poll()

parser = argparse.ArgumentParser(description="Flash, erase, or modify the software data protection state of an EEPROM")
parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="serial port name (default %(default)s)")
parser.add_argument("-b", "--baud-rate", type=int, default=115200, help="baud rate (default %(default)s)")

subparsers = parser.add_subparsers(title="commands", description="valid commands:")

parser_erase = subparsers.add_parser("erase", help="fill EEPROM with 0xFF bytes")
parser_erase.set_defaults(func=erase)

parser_sdp = subparsers.add_parser("sdp", help="enable/disable software data protection")
parser_sdp.add_argument("state", choices=["disable", "enable"])
parser_sdp.set_defaults(func=sdp)

parser_read = subparsers.add_parser("read", help="read N pages from EEPROM into file")
parser_read.add_argument("file", type=argparse.FileType("wb", 0), help="file to store the data in")
parser_read.add_argument("N", type=int, help="number of pages")
parser_read.set_defaults(func=read)

parser_write = subparsers.add_parser("write", help="write N pages from file to EEPROM")
parser_write.add_argument("file", type=argparse.FileType("rb", 0), help="file to read the data from")
parser_write.add_argument("N", type=int, help="number of pages")
parser_write.set_defaults(func=write)

parser_dump = subparsers.add_parser("dump", help="dump contents of EEPROM to a file")
parser_dump.add_argument("file", type=argparse.FileType("wb", 0), help="file to store the data in")
parser_dump.set_defaults(func=dump)

parser_flash = subparsers.add_parser("flash", help="flash entire EEPROM with a file")
parser_flash.add_argument("file", type=argparse.FileType("rb", 0), help="file to read the data from")
parser_flash.set_defaults(func=flash)

args = parser.parse_args()

arduino = serial.Serial(port=args.port, baudrate=args.baud_rate, timeout=None)

# give time for the arduino to reboot?
time.sleep(10)

args.func(args)
