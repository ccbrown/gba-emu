#include "GBAVideoController.h"

GBAVideoController::GBAVideoController(ARM7TDMI* cpu) : _cpu(cpu) {
	_ioRegisters = calloc(_ioRegistersSize, 1);
	
	_cpu->mmu().attach(0x04000000, this, 0, _ioRegistersSize);
	_cpu->mmu().attach(0x05000000, &_paletteRAM, 0, _paletteRAM.size());
	_cpu->mmu().attach(0x06000000, &_videoRAM, 0, _videoRAM.size());
	_cpu->mmu().attach(0x07000000, &_objectAttributeRAM, 0, _objectAttributeRAM.size());
}

GBAVideoController::~GBAVideoController() {
	free(_ioRegisters);
}

void GBAVideoController::load(void* destination, uint32_t address, uint32_t size) const {
	switch (address) {
		default:
			if (address + size > _ioRegistersSize) { throw IOError(); }
			memcpy(destination, reinterpret_cast<uint8_t*>(_ioRegisters) + address, size);
	}
}

void GBAVideoController::store(uint32_t address, const void* data, uint32_t size) {
	switch (address) {
		default:
			if (address + size > _ioRegistersSize) { throw IOError(); }
			memcpy(reinterpret_cast<uint8_t*>(_ioRegisters) + address, data, size);
	}
}
