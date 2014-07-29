#include "GameBoyAdvance.h"

#include "FixedEndian.h"

GameBoyAdvance::GameBoyAdvance() : _videoController(this), _io(this) {
	_cpu.mmu().attach(0x0, &_systemROM, 0, _systemROM.size());
	_cpu.mmu().attach(0x02000000, &_onBoardRAM, 0, _onBoardRAM.size());
	_cpu.mmu().attach(0x03000000, &_onChipRAM, 0, _onChipRAM.size());
	_cpu.mmu().attach(0x03ffff00, &_onChipRAM, 0x7f00, 0x100);

	_cpu.mmu().attach(0x04000000, &_io, 0x00, 0x01000000);

	_cpu.mmu().attach(0x08000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0a000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0c000000, &_gamePakROM, 0, _gamePakROM.size());
	_cpu.mmu().attach(0x0e000000, &_gamePakSRAM, 0, _gamePakSRAM.size());
}

void GameBoyAdvance::loadBIOS(const void* data, size_t size) {
	memcpy(_systemROM.storage(), data, size);
}

void GameBoyAdvance::loadGamePak(const void* data, size_t size) {
	memcpy(_gamePakROM.storage(), data, size);
}

void GameBoyAdvance::run() {
	_cpu.reset();

	while (true) {
		_cpu.step();
		_videoController.cycle();
		_videoController.cycle();
		_videoController.cycle();
	}
}

void GameBoyAdvance::interruptRequest(uint16_t interrupts) {
	_io.interruptRequest(interrupts);
}

GameBoyAdvance::IO::IO(GameBoyAdvance* gba) : _gba(gba) {
	_storage = calloc(_storageSize, 1);
}

GameBoyAdvance::IO::~IO() {
	free(_storage);
}

void GameBoyAdvance::IO::interruptRequest(uint16_t interrupts) {
	auto& enabled  = *reinterpret_cast<LittleEndian<uint16_t>*>(reinterpret_cast<uint8_t*>(_storage) + 0x200);
	auto& requests = *reinterpret_cast<LittleEndian<uint16_t>*>(reinterpret_cast<uint8_t*>(_storage) + 0x202);
	
	if (interrupts & enabled) {
		requests = (requests | (enabled & interrupts));
		printf("INTERRUPT REQUEST: %hu\n", static_cast<uint16_t>(requests));
		_gba->cpu().interrupt();
	}
}

void GameBoyAdvance::IO::load(void* destination, uint32_t address, uint32_t size) const {
	#define GBA_IO_LOAD_ADVANCE(x) address += x; size -= x; destination = reinterpret_cast<uint8_t*>(destination) + x
	
	while (size) {
		auto& destinationUInt8  = *reinterpret_cast<uint8_t*>(destination);
		auto& destinationUInt16 = *reinterpret_cast<LittleEndian<uint16_t>*>(destination);
	
		if (address > 0x0800) {
			address = 0x0800 + (address & 0xffff);
		}

		switch (address) {
			case 0x0004: // DISPSTAT
				if (size < 2) { throw IOError(); }
				destinationUInt16 = _gba->videoController().statusRegister();
				GBA_IO_LOAD_ADVANCE(2);
				break;
			case 0x0006: // VCOUNT (low byte)
				destinationUInt8 = static_cast<uint8_t>(_gba->_videoController.currentScanline());
				GBA_IO_LOAD_ADVANCE(1);
				break;
			case 0x0007: // VCOUNT (high byte)
				destinationUInt8 = static_cast<uint8_t>(_gba->_videoController.currentScanline() >> 8);
				GBA_IO_LOAD_ADVANCE(1);
				break;
			default:
				if (address >= _storageSize) { throw IOError(); }
				destinationUInt8 = reinterpret_cast<uint8_t*>(_storage)[address];
				GBA_IO_LOAD_ADVANCE(1);
		}
	}
}

void GameBoyAdvance::IO::store(uint32_t address, const void* data, uint32_t size) {
	#define GBA_IO_STORE_ADVANCE(x) address += x; size -= x; data = reinterpret_cast<const uint8_t*>(data) + x

	while (size) {
		auto& dataUInt8 = *reinterpret_cast<const uint8_t*>(data);
		auto& dataUInt16 = *reinterpret_cast<const LittleEndian<uint16_t>*>(data);
	
		auto destination = reinterpret_cast<uint8_t*>(_storage) + address;
		auto& destinationUInt8 = *destination;

		if (address > 0x0800) {
			address = 0x0800 + (address & 0xffff);
		}

		switch (address) {
			case 0x0004: // DISPSTAT
				if (size < 2) { throw IOError(); }
				_gba->videoController().updateStatusRegister(dataUInt16);
				GBA_IO_STORE_ADVANCE(2);
				break;
			case 0x0202: // IF - clears bits to acknowledge interrupts
			case 0x0203:
				destinationUInt8 = (destinationUInt8 & ~dataUInt8);
				GBA_IO_STORE_ADVANCE(1);
				break;
			default:
				if (address >= _storageSize) { throw IOError(); }
				destinationUInt8 = *reinterpret_cast<const uint8_t*>(data);
				GBA_IO_STORE_ADVANCE(1);
		}
	}
}
