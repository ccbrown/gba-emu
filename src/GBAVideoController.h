#pragma once

#include "ARM7TDMI.h"
#include "Memory.h"

class GBAVideoController : public MemoryInterface<uint32_t> {
	public:
		GBAVideoController(ARM7TDMI* cpu);
		virtual ~GBAVideoController();
		
		struct IOError {};
	
		virtual void load(void* destination, uint32_t address, uint32_t size) const override;
		virtual void store(uint32_t address, const void* data, uint32_t size) override;

	private:
		ARM7TDMI* const _cpu = nullptr;
	
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};

		void* _ioRegisters = nullptr;
		const size_t _ioRegistersSize = 0x60;
};