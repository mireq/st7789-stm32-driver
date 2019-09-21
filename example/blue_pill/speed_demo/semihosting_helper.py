# -*- coding: utf-8 -*-
import gdb
import struct


def on_trap():
	frame = gdb.selected_frame()
	arch = frame.architecture()
	pc = frame.read_register("pc")
	instruction = arch.disassemble(int(pc))[0]['asm'].split()
	if instruction[0] != 'bkpt':
		return
	svc_command = int(instruction[1], 16)
	if svc_command != 0xAB:
		return
	command = int(frame.read_register("r0"))
	message = frame.read_register("r1")
	try:
		command_handler = SVC_COMMANDS[command]
	except KeyError:
		print("Unknown command 0x%04x" % command)
	else:
		command_handler(message)


def write0(message):
	char_pointer_type = gdb.lookup_type('char').pointer()
	message_pointer = message.cast(char_pointer_type)
	gdb.write(message_pointer.string())
	gdb.flush()
	gdb.execute('continue')


def writePixels(message):
	inferior = gdb.inferiors()[0]
	batch, size = struct.unpack('<HH', inferior.read_memory(message, 4).tobytes())
	with open('image.png.dat', 'rb') as fp:
		fp.seek(batch * size * 2)
		inferior.write_memory(message, fp.read(size * 2));
	gdb.execute('continue')


SVC_COMMANDS = {
	0x04: write0,
	0xff: writePixels,
}
