#include "svc.h"

void *svcCall(int command, const void *message) {
	void *output;
	__asm volatile
	(
		"mov r0, %[com] \n"
		"mov r1, %[msg] \n"
		"bkpt #0xAB \n"
		"mov %[out], r0"
		: [out] "=r" (output)
		: [com] "r" (command), [msg] "r" (message)
		: "r0", "r1"
	);
	return output;
}


void svcWrite0(const char *message) {
	svcCall(0x04, message);
}
