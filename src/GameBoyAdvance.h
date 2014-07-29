#pragma once

#include "ARM7TDMI.h"
#include "Memory.h"

#include "GBAVideoController.h"

class GameBoyAdvance {
	public:
		GameBoyAdvance();
		
		void loadBIOS(const void* data, size_t size);
		void loadGamePak(const void* data, size_t size);
		
		void run();
		
		ARM7TDMI& cpu() { return _cpu; }
		GBAVideoController& videoController() { return _videoController; }
			
		enum Interrupt : uint16_t {
			kInterruptVBlank               = (1 <<  0),
			kInterruptHBlank               = (1 <<  1),
			kInterruptVCounterMatch        = (1 <<  2),
			kInterruptTimer0Overflow       = (1 <<  3),
			kInterruptTimer1Overflow       = (1 <<  4),
			kInterruptTimer2Overflow       = (1 <<  5),
			kInterruptTimer3Overflow       = (1 <<  6),
			kInterruptSerialCommunication  = (1 <<  7),
			kInterruptDMA0                 = (1 <<  8),
			kInterruptDMA1                 = (1 <<  9),
			kInterruptDMA2                 = (1 << 10),
			kInterruptDMA3                 = (1 << 11),
			kInterruptKeypad               = (1 << 12),
			kInterruptGamePak              = (1 << 13),
		};
		
		void interruptRequest(uint16_t interrupts);
		
	private:
		// the order here is important. the cpu MUST come first
		ARM7TDMI _cpu;
		GBAVideoController _videoController;

		// general memory
		Memory<uint32_t> _systemROM{0x4000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _onBoardRAM{0x1000000};
		Memory<uint32_t> _onChipRAM{0x00ffff00};

		// gamepak memory
		Memory<uint32_t> _gamePakROM{0x2000000, Memory<uint32_t>::kFlagReadOnly};
		Memory<uint32_t> _gamePakSRAM{0x10000};

		struct IO : MemoryInterface<uint32_t> {			
			IO(GameBoyAdvance* gba);
			virtual ~IO();
			
			struct IOError {};
				
			void interruptRequest(uint16_t interrupts);
			
			virtual void load(void* destination, uint32_t address, uint32_t size) const override;
			virtual void store(uint32_t address, const void* data, uint32_t size) override;

			GameBoyAdvance* const _gba = nullptr;
			void* _storage = nullptr;
			const size_t _storageSize = 0x800;
		} _io;
};