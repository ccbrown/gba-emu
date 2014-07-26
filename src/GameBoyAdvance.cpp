#include "GameBoyAdvance.h"

GameBoyAdvance::GameBoyAdvance() {
	_cpu.mmu().attach(0x0, &_systemROM, 0, _systemROM.size());
	_cpu.mmu().attach(0x02000000, &_onBoardRAM, 0, _onBoardRAM.size());
	_cpu.mmu().attach(0x03000000, &_onChipRAM, 0, _onChipRAM.size());
	_cpu.mmu().attach(0x03fffe00, &_unknownRAM1, 0, _unknownRAM1.size());
	_cpu.mmu().attach(0x04000000, &_ioRegisters, 0, _ioRegisters.size());
	_cpu.mmu().attach(0x04000400, &_unknownRAM2, 0, _unknownRAM2.size());

	_cpu.mmu().attach(0x05000000, &_paletteRAM, 0, _paletteRAM.size());
	_cpu.mmu().attach(0x06000000, &_videoRAM, 0, _videoRAM.size());
	_cpu.mmu().attach(0x07000000, &_objectAttributeRAM, 0, _objectAttributeRAM.size());

	_cpu.mmu().attach(0x08000000, &_gamePakROM1, 0, _gamePakROM1.size());
	_cpu.mmu().attach(0x0a000000, &_gamePakROM2, 0, _gamePakROM2.size());
	_cpu.mmu().attach(0x0c000000, &_gamePakROM3, 0, _gamePakROM3.size());
	_cpu.mmu().attach(0x0e000000, &_gamePakSRAM, 0, _gamePakSRAM.size());
}

void GameBoyAdvance::loadBIOS(const void* data, size_t size) {
	memcpy(_systemROM.storage(), data, size);
}

void GameBoyAdvance::loadGamePak(const void* data, size_t size) {
	memcpy(_gamePakROM1.storage(), data, size);
}

void GameBoyAdvance::hardReset() {
	_cpu.setRegister(ARM7TDMI::kVirtualRegisterPC, 0);
}

void GameBoyAdvance::run() {
	while (true) {
		_cpu.step();
	}
}