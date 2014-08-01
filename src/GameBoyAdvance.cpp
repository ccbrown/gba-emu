#include "GameBoyAdvance.h"

#include "FixedEndian.h"
#include "BIT_MACROS.h"

#include <thread>

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
		// TODO: timing / actual power saving
		if (!_isInHaltMode) {
			_cpu.step();
		}
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
		_gba->_isInHaltMode = false;
		requests = (requests | (enabled & interrupts));
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
			case 0x0000: // DISPCNT
				if (size < 2) { throw IOError(); }
				destinationUInt16 = _gba->videoController().controlRegister();
				GBA_IO_LOAD_ADVANCE(2);
				break;
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
			case 0x0008: // BGXCNT
			case 0x000a:
			case 0x000c:
			case 0x000e:
				if (size < 2) { throw IOError(); }
				destinationUInt16 = static_cast<uint16_t>(_gba->videoController().background((address - 0x0008) >> 1));
				GBA_IO_LOAD_ADVANCE(2);
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
		auto& destinationUInt16 = *reinterpret_cast<LittleEndian<uint16_t>*>(destination);

		if (address > 0x0800) {
			address = 0x0800 + (address & 0xffff);
		}

		switch (address) {
			case 0x0000: // DISPCNT
				if (size < 2) { throw IOError(); }
				_gba->videoController().setControlRegister(dataUInt16);
				GBA_IO_STORE_ADVANCE(2);
				break;
			case 0x0004: // DISPSTAT
				if (size < 2) { throw IOError(); }
				_gba->videoController().updateStatusRegister(dataUInt16);
				GBA_IO_STORE_ADVANCE(2);
				break;
			case 0x0008: // BGXCNT
			case 0x000a:
			case 0x000c:
			case 0x000e:
				if (size < 2) { throw IOError(); }
				_gba->videoController().setBackground((address - 0x0008) >> 1, static_cast<uint16_t>(dataUInt16));
				GBA_IO_STORE_ADVANCE(2);
				break;
			case 0x00ba: // dma 0 control
			case 0x00c6: // dma 1 control
			case 0x00d2: // dma 2 control
			case 0x00de: // dma 3 control
				if (size < 2) { throw IOError(); }
				destinationUInt16 = dataUInt16;
				if (BIT15(dataUInt16)) {
					uint32_t dma = (address - 0x00ba) / 12;
					auto& registers = _dmaRegisters[dma];
					auto dmaBase = reinterpret_cast<uint8_t*>(_storage) + 0x00b0 + dma * 12;
					registers.source = *reinterpret_cast<LittleEndian<uint32_t>*>(dmaBase + 0) & 0x0fffffff;
					registers.destination = *reinterpret_cast<LittleEndian<uint32_t>*>(dmaBase + 4) & 0x0fffffff;
					registers.count = *reinterpret_cast<LittleEndian<uint16_t>*>(dmaBase + 8);
				}
				GBA_IO_STORE_ADVANCE(2);
				checkDMATransfers();
				break;
			case 0x0202: // IF - clears bits to acknowledge interrupts
			case 0x0203:
				destinationUInt8 = (destinationUInt8 & ~dataUInt8);
				GBA_IO_STORE_ADVANCE(1);
				break;
			case 0x0301:
				if (!BIT7(dataUInt8) && !_gba->_isInHaltMode) {
					_gba->_isInHaltMode = true;
				}
				GBA_IO_STORE_ADVANCE(1);
				break;
			default:
				if (address >= _storageSize) { throw IOError(); }
				destinationUInt8 = dataUInt8;
				GBA_IO_STORE_ADVANCE(1);
		}
	}
}

void GameBoyAdvance::IO::checkDMATransfers() {
	for (uint32_t i = 0; i < 4; ++i) {
		auto dmaBase = reinterpret_cast<uint8_t*>(_storage) + 0x00b0 + i * 12;
		auto& control = *reinterpret_cast<LittleEndian<uint16_t>*>(dmaBase + 10);

		if (!BIT15(control)) { continue; }

		uint16_t start = BITFIELD_UINT16(control, 13, 12);

		auto displayStatus = _gba->videoController().statusRegister();

		if (start == 1 && !(displayStatus & GBAVideoController::kStatusFlagVBlank)) {
			continue;
		}

		if (start == 2 && ((displayStatus & GBAVideoController::kStatusFlagVBlank) || !(displayStatus & GBAVideoController::kStatusFlagHBlank))) {
			continue;
		}
		
		if (start == 3) {
			// TODO: sound fifo dmas
			control = control & 0x7fff;
			continue;
		}
		
		if (BIT9(control) || BIT11(control)) {
			throw UnimplementedFeature();
		}
		
		if (BIT7(control) && BIT8(control)) {
			throw IOError();
		}

		auto& registers = _dmaRegisters[i];

		uint32_t count = registers.count ? registers.count : (i == 3 ? 0x10000 : 0x4000);

		printf("dma transfer: %08u %s words from %08x to %08x\n", count, BIT10(control) ? "32-bit" : "16-bit", registers.source, registers.destination);

		while (count) {
			if (BIT10(control)) {
				_gba->cpu().mmu().store<uint32_t>(registers.destination, _gba->cpu().mmu().load<uint32_t>(registers.source));
			} else {
				_gba->cpu().mmu().store<uint16_t>(registers.destination, _gba->cpu().mmu().load<uint16_t>(registers.source));
			}
			auto size = BIT10(control) ? 4 : 2;
			registers.destination += (BIT5(control) == BIT6(control) ? size : (!BIT6(control) ? -size : 0));
			registers.source += (!BIT7(control) && !BIT8(control) ? size : (!BIT8(control) ? -size : 0));
			--count;
		}
		
		if (BIT5(control) && BIT6(control)) {
			registers.destination = *reinterpret_cast<LittleEndian<uint32_t>*>(dmaBase + 4) & 0x0fffffff;
		}		
		
		control = control & 0x7fff;

		if (BIT14(control)) {
			_gba->interruptRequest(kInterruptDMA0 << i);
		}
	}
}
