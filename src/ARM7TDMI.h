#pragma once

#include "MMU.h"

class ARM7TDMI {
	public:
		enum VirtualRegister {
			kVirtualRegisterR0,
			kVirtualRegisterR1,
			kVirtualRegisterR2,
			kVirtualRegisterR3,
			kVirtualRegisterR4,
			kVirtualRegisterR5,
			kVirtualRegisterR6,
			kVirtualRegisterR7,
			kVirtualRegisterR8,
			kVirtualRegisterR9,
			kVirtualRegisterR10,
			kVirtualRegisterR11,
			kVirtualRegisterR12,
			kVirtualRegisterR13,
			kVirtualRegisterLR,
			kVirtualRegisterPC,
			kVirtualRegisterCPSR,
			kVirtualRegisterSPSR,
			kVirtualRegisterCount
		};

		enum PhysicalRegister {
			kPhysicalRegisterR0,
			kPhysicalRegisterR1,
			kPhysicalRegisterR2,
			kPhysicalRegisterR3,
			kPhysicalRegisterR4,
			kPhysicalRegisterR5,
			kPhysicalRegisterR6,
			kPhysicalRegisterR7,
			kPhysicalRegisterR8,
			kPhysicalRegisterR9,
			kPhysicalRegisterR10,
			kPhysicalRegisterR11,
			kPhysicalRegisterR12,
			kPhysicalRegisterR13,
			kPhysicalRegisterLR,
			kPhysicalRegisterPC,
			
			kPhysicalRegisterR8FIQ,
			kPhysicalRegisterR9FIQ,
			kPhysicalRegisterR10FIQ,
			kPhysicalRegisterR11FIQ,
			kPhysicalRegisterR12FIQ,
			kPhysicalRegisterR13FIQ,
			kPhysicalRegisterLRFIQ,
			kPhysicalRegisterSPSRFIQ,

			kPhysicalRegisterR13SVC,
			kPhysicalRegisterLRSVC,
			kPhysicalRegisterSPSRSVC,

			kPhysicalRegisterR13ABT,
			kPhysicalRegisterLRABT,
			kPhysicalRegisterSPSRABT,

			kPhysicalRegisterR13IRQ,
			kPhysicalRegisterLRIRQ,
			kPhysicalRegisterSPSRIRQ,

			kPhysicalRegisterR13UND,
			kPhysicalRegisterLRUND,
			kPhysicalRegisterSPSRUND,

			kPhysicalRegisterCPSR,

			kPhysicalRegisterCount
		};

		enum Mode : uint8_t {
			kModeUser = 0x10,
			kModeFIQ = 0x11,
			kModeIRQ = 0x12,
			kModeSupervisor = 0x13,
			kModeAbort = 0x17,
			kModeUndefined = 0x1b,
			kModeSystem = 0x1f,
		};

		enum PSRFlag : uint32_t {
			kPSRFlagNegative  = (1u << 31),
			kPSRFlagZero      = (1u << 30),
			kPSRFlagCarry     = (1u << 29),
			kPSRFlagOverflow  = (1u << 28),
		};
		
		enum Condition : uint8_t {
			kConditionEqual,
			kConditionNotEqual,
			kConditionUnsignedHigherOrSame,
			kConditionUnsignedLower,
			kConditionNegative,
			kConditionPositiveOrZero,
			kConditionOverflow,
			kConditionNoOverflow,
			kConditionUnsignedHigher,
			kConditionUnsignedLowerOrSame,
			kConditionGreaterOrEqual,
			kConditionLess,
			kConditionGreater,
			kConditionLessOrEqual,
			kConditionAlways,
			kConditionNever
		};
		
		struct UnknownInstruction {};
		
		ARM7TDMI();
		
		void step();

		void branch(uint32_t address);
		void branchWithLink(uint32_t address);

		void setMode(Mode mode);

		uint32_t getRegister(VirtualRegister r) const { return *_virtualRegisters[r]; }
		void setRegister(VirtualRegister r, uint32_t value) { *_virtualRegisters[r] = value; }

		MMU<uint32_t>& mmu() { return _mmu; }
		
		bool checkCondition(Condition condition) const;

	private:	
		MMU<uint32_t> _mmu;
		uint32_t* _virtualRegisters[kVirtualRegisterCount]{0};
		uint32_t _physicalRegisters[kPhysicalRegisterCount]{0};
		Mode _mode = kModeUndefined;
		bool _isInThumbState = false;

		void _stepARM();
		void _stepThumb();
};