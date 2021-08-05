import argparse
import serial
from time import sleep
import os

eeprom_pages = 512
page_size = 64 # bytes
eeprom_size = eeprom_pages * page_size

def echo_poll():
	written = arduino.write(bytes([0xee]))
	if (written != 1):
		raise RuntimeError("unable to write echo byte")
	echo = arduino.read()
	if (echo[0] != 0xee):
		raise RuntimeError(f"did not recieve correct echo byte ({echo[0]:2x} instead of ee)")

def erase(args):
	arduino.write(b'e')
	echo_poll()

def sdp(args):
	if (args.state == "disable"):
		arduino.write(b's')
	elif (args.state == "enable"):
		arduino.write(b'S')
	else: # something else?
		raise ValueError("unknown SDP state argument")
	echo_poll()

def dump(args):
	arduino.write(b'd')
	for _ in range(0, eeprom_pages):
		data = arduino.read(page_size)
		if (len(data) != page_size):
			raise RuntimeError(f"received {len(data)} bytes instead of {page_size}")
		written = args.file.write(data)
		if(written != page_size):
			raise RuntimeError(f"amount of bytes written to file ({written}) not equal to page size ({page_size})")
	echo_poll()

def flash(args):
	arduino.write(b'f')
	size = os.stat(args.file.name).st_size
	if (size != eeprom_size):
		raise RuntimeError(f"file size ({size}) does not match EEPROM size ({eeprom_size})")
	for n in range(0, eeprom_pages):
		page = args.file.read(page_size)
		if (len(page) != page_size):
			raise RuntimeError(f"read {len(data)} bytes from file instead of {page_size}")
		ready = arduino.read()
		if(len(ready) != 1):
			raise RuntimeError("did not receive ready byte")
		if(ready[0] != 0xaa):
			raise RuntimeError(f"incorrect ready byte: expected aa, received {ready[0]:2x}")
		sent = arduino.write(page)
		if(sent != page_size):
			raise RuntimeError(f"amount of bytes sent ({read}) not equal to page size ({page_size})")
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

parser_dump = subparsers.add_parser("dump", help="dump contents of EEPROM to a file")
parser_dump.add_argument("file", type=argparse.FileType("wb", 0), help="file to store the data in")
parser_dump.set_defaults(func=dump)

parser_flash = subparsers.add_parser("flash", help="flash entire EEPROM with a file")
parser_flash.add_argument("file", type=argparse.FileType("rb", 0), help="file to read the data from")
parser_flash.set_defaults(func=flash)

args = parser.parse_args()

arduino = serial.Serial(port=args.port, baudrate=args.baud_rate, timeout=None)

ready = arduino.read()
if(len(ready) != 1):
	raise RuntimeError("did not receive ready byte")
if(ready[0] != 0xab):
	raise RuntimeError(f"incorrect ready byte {ready[0]:2x}, expected ab")

args.func(args)
