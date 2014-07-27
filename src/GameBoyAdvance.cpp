#include "GameBoyAdvance.h"

GameBoyAdvance::GameBoyAdvance() : _videoController(&_cpu) {
	_cpu.mmu().attach(0x0, &_systemROM, 0, _systemROM.size());
	_cpu.mmu().attach(0x02000000, &_onBoardRAM, 0, _onBoardRAM.size());
	_cpu.mmu().attach(0x03000000, &_onChipRAM, 0, _onChipRAM.size());

	_cpu.mmu().attach(0x04000060, &_io, 0x60, 0x01000000 - 0x60);

	_cpu.mmu().attach(0x08000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0a000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0c000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0e000000, &_gamePakSRAM, 0, _gamePakSRAM.size());

	_cpu.reset();
}

void GameBoyAdvance::loadBIOS(const void* data, size_t size) {
	memcpy(_systemROM.storage(), data, size);
}

void GameBoyAdvance::loadGamePak(const void* data, size_t size) {
	memcpy(_gamePakROM.storage(), data, size);
}

void GameBoyAdvance::run() {
	while (true) {
		_cpu.step();
	}
}

GameBoyAdvance::IO::IO() {
	_storage = calloc(_storageSize, 1);
}

GameBoyAdvance::IO::~IO() {
	free(_storage);
}

void GameBoyAdvance::IO::load(void* destination, uint32_t address, uint32_t size) const {
	switch (address) {
		default:
			if (address + size > _storageSize) { throw IOError(); }
			memcpy(destination, reinterpret_cast<uint8_t*>(_storage) + address, size);
	}
}

void GameBoyAdvance::IO::store(uint32_t address, const void* data, uint32_t size) {
	switch (address) {
		default:
			if (address + size > _storageSize) { throw IOError(); }
			memcpy(reinterpret_cast<uint8_t*>(_storage) + address, data, size);
	}
}
