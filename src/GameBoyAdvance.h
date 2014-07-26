#pragma once

#include "ARM7TDMI.h"

class GameBoyAdvance {
	public:
		GameBoyAdvance();
		
		void loadBIOS(const void* data, size_t size);
		void loadGamePak(const void* data, size_t size);
		
		void hardReset();
		
		void run();
		
	private:
		ARM7TDMI _cpu;

		// general memory
		Memory<uint32_t> _systemROM{0x4000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _onBoardRAM{0x40000};
		Memory<uint32_t> _onChipRAM{0x8000};
		Memory<uint32_t> _unknownRAM1{0x200};
		Memory<uint32_t> _ioRegisters{0x400};
		Memory<uint32_t> _unknownRAM2{0x204};

		// display memory
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};

		// gamepak memory
		Memory<uint32_t> _gamePakROM1{0x2000000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _gamePakROM2{0x2000000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _gamePakROM3{0x2000000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _gamePakSRAM{0x10000};
};