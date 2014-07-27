#pragma once

#include "ARM7TDMI.h"
#include "Memory.h"

class GameBoyAdvance {
	public:
		GameBoyAdvance();
		
		void loadBIOS(const void* data, size_t size);
		void loadGamePak(const void* data, size_t size);
		
		void run();
		
	private:
		ARM7TDMI _cpu;

		// general memory
		Memory<uint32_t> _systemROM{0x4000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _onBoardRAM{0x1000000};
		Memory<uint32_t> _onChipRAM{0x1000000};

		// display memory
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};

		// gamepak memory
		Memory<uint32_t> _gamePakROM{0x2000000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _gamePakSRAM{0x10000};

		struct IO : MemoryInterface<uint32_t> {			
			IO();
			virtual ~IO();
			
			struct IOError {};
				
			enum IORegister : uint32_t {
				kIORegisterBootFlag = 0x300,
			};
			
			virtual void load(void* destination, uint32_t address, uint32_t size) const override;
			virtual void store(uint32_t address, const void* data, uint32_t size) override;
			
			void* _storage = nullptr;
			const size_t _storageSize = 0x800;
		} _io;
};