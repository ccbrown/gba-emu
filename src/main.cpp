#include "ARM7TDMI.h"

#include <stdint.h>
#include <cstdio>

int main(int argc, const char* argv[]) {
	Memory<uint32_t> bios(0x4000);

	{
		auto f = fopen(argv[1], "rb");
	
		fseek(f, 0, SEEK_END);
		auto size = ftell(f);
		rewind(f);

		fread(bios.storage(), 1, size, f);
		fclose(f);
	}
	
	Memory<uint32_t> rom(0x2000000);

	{
		auto f = fopen(argv[2], "rb");
	
		fseek(f, 0, SEEK_END);
		auto size = ftell(f);
		rewind(f);

		fread(rom.storage(), 1, size, f);
		fclose(f);
	}
	
	ARM7TDMI cpu;
	
	cpu.mmu().attach(0x0, &bios, 0, 0x4000);
	cpu.mmu().attach(0x8000000, &rom, 0, 0x2000000);
	cpu.setRegister(ARM7TDMI::kVirtualRegisterPC, 0x8000000);
	
	for (int i = 0; true; ++i) {
		cpu.step();
		printf("%u\n", i);
	}
	
	return 0;
}