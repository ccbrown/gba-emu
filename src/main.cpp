#include "ARM7TDMI.h"

#include <stdint.h>
#include <cstdio>

int main(int argc, const char* argv[]) {
	ARM7TDMI cpu;

	Memory<uint32_t> bios(0x4000);

	{
		auto f = fopen(argv[1], "rb");
	
		fseek(f, 0, SEEK_END);
		auto size = ftell(f);
		rewind(f);

		fread(bios.storage(), 1, size, f);
		fclose(f);
	}

	cpu.mmu().attach(0x0, &bios, 0, 0x4000);

	Memory<uint32_t> rom(0x2000000);

	{
		auto f = fopen(argv[2], "rb");
	
		fseek(f, 0, SEEK_END);
		auto size = ftell(f);
		rewind(f);

		fread(rom.storage(), 1, size, f);
		fclose(f);
	}

	cpu.mmu().attach(0x08000000, &rom, 0, 0x2000000);

	cpu.setRegister(ARM7TDMI::kVirtualRegisterPC, 0x0);

	Memory<uint32_t> ramOnBoard(0x40000);
	cpu.mmu().attach(0x02000000, &ramOnBoard, 0, 0x40000);

	Memory<uint32_t> ramOnChip(0x8000);
	cpu.mmu().attach(0x03000000, &ramOnBoard, 0, 0x8000);

	Memory<uint32_t> ioRegisters(0x3ff);
	cpu.mmu().attach(0x04000000, &ioRegisters, 0, 0x3ff);

	while (true) {
		cpu.step();
	}
	
	return 0;
}