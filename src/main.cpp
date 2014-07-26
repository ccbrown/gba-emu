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

	Memory<uint32_t> onBoardRAM(0x40000);
	cpu.mmu().attach(0x02000000, &onBoardRAM, 0, 0x40000);

	Memory<uint32_t> onChipRAM(0x8000);
	cpu.mmu().attach(0x03000000, &onChipRAM, 0, 0x8000);

	Memory<uint32_t> unknown1(0x200);
	cpu.mmu().attach(0x03fffe00, &unknown1, 0, 0x200);

	Memory<uint32_t> ioRegisters(0x3ff);
	cpu.mmu().attach(0x04000000, &ioRegisters, 0, 0x3ff);

	Memory<uint32_t> unknown2(0x204);
	cpu.mmu().attach(0x04000400, &unknown2, 0, 0x204);

	Memory<uint32_t> colorPalettes(0x400);
	cpu.mmu().attach(0x05000000, &colorPalettes, 0, 0x400);

	Memory<uint32_t> videoRAM(0x18000);
	cpu.mmu().attach(0x06000000, &videoRAM, 0, 0x18000);

	Memory<uint32_t> objectAttributes(0x400);
	cpu.mmu().attach(0x07000000, &objectAttributes, 0, 0x400);

	while (true) {
		cpu.step();
	}
	
	return 0;
}