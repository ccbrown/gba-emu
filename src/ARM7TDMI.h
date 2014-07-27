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
			kVirtualRegisterSP,
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
			kPhysicalRegisterSP,
			kPhysicalRegisterLR,
			kPhysicalRegisterPC,
			
			kPhysicalRegisterR8FIQ,
			kPhysicalRegisterR9FIQ,
			kPhysicalRegisterR10FIQ,
			kPhysicalRegisterR11FIQ,
			kPhysicalRegisterR12FIQ,
			kPhysicalRegisterSPFIQ,
			kPhysicalRegisterLRFIQ,
			kPhysicalRegisterSPSRFIQ,

			kPhysicalRegisterSPSVC,
			kPhysicalRegisterLRSVC,
			kPhysicalRegisterSPSRSVC,

			kPhysicalRegisterSPABT,
			kPhysicalRegisterLRABT,
			kPhysicalRegisterSPSRABT,

			kPhysicalRegisterSPIRQ,
			kPhysicalRegisterLRIRQ,
			kPhysicalRegisterSPSRIRQ,

			kPhysicalRegisterSPUND,
			kPhysicalRegisterLRUND,
			kPhysicalRegisterSPSRUND,

			kPhysicalRegisterCPSR,

			kPhysicalRegisterCount,
			kPhysicalRegisterInvalid
		};

		enum Mode : uint8_t {
			kModeUser       = 0x10,
			kModeFIQ        = 0x11,
			kModeIRQ        = 0x12,
			kModeSupervisor = 0x13,
			kModeAbort      = 0x17,
			kModeUndefined  = 0x1b,
			kModeSystem     = 0x1f,
		};

		enum PSRFlag : uint32_t {
			kPSRFlagNegative  = (1u << 31),
			kPSRFlagZero      = (1u << 30),
			kPSRFlagCarry     = (1u << 29),
			kPSRFlagOverflow  = (1u << 28),
			kPSRFlagIRQ       = (1u <<  7),
			kPSRFlagFIQ       = (1u <<  6),
			kPSRFlagThumb     = (1u <<  5),
		};
		
		static const uint32_t kPSRMaskControl   = 0x000000ff;
		static const uint32_t kPSRMaskExtension = 0x0000ff00;
		static const uint32_t kPSRMaskStatus    = 0x00ff0000;
		static const uint32_t kPSRMaskFlags     = 0xff000000;
		
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
		
		enum ShiftType : uint8_t {
			kShiftTypeLSL,
			kShiftTypeLSR,
			kShiftTypeASR,
			kShiftTypeROR
		};
		
		enum ALUOperation : uint8_t {
			kALUOperationAND,
			kALUOperationEOR,
			kALUOperationSUB,
			kALUOperationADD,
			kALUOperationMVN,
			kALUOperationORR,
			kALUOperationBIC,
			kALUOperationROR,
			kALUOperationLSL,
			kALUOperationLSR,
			kALUOperationASR,
			kALUOperationMUL,
			kALUOperationADC,
			kALUOperationSBC,
		};
		
		struct UnknownInstruction {};
		
		ARM7TDMI();
		
		void step();
		
		void reset();

		void branch(uint32_t address);

		void setMode(Mode mode);

		uint32_t getRegister(VirtualRegister r) const { return _physicalRegisters[_virtualRegisters[r]]; }
		void setRegister(VirtualRegister r, uint32_t value) { _physicalRegisters[_virtualRegisters[r]] = value; }

		uint32_t getRegister(PhysicalRegister r) const { return _physicalRegisters[r]; }
		void setRegister(PhysicalRegister r, uint32_t value) { _physicalRegisters[r] = value; }

		bool getCPSRFlag(PSRFlag flag) const { return getRegister(kVirtualRegisterCPSR) & flag; }
		void setCPSRFlags(uint32_t flags, bool set = true);
		void clearCPSRFlags(uint32_t flags) { setRegister(kVirtualRegisterCPSR, getRegister(kVirtualRegisterCPSR) & ~flags); }

		MMU<uint32_t>& mmu() { return _mmu; }
		
		bool checkCondition(Condition condition) const;

	private:	
		MMU<uint32_t> _mmu;
		PhysicalRegister _virtualRegisters[kVirtualRegisterCount];
		uint32_t _physicalRegisters[kPhysicalRegisterCount]{0};

		static const uint32_t kARMNOPcode = 0xe1a00000;

		struct Instruction {
			uint32_t opcode = kARMNOPcode;
			bool isThumb = 0;
		};
		
		Instruction _toDecode;
		Instruction _toExecute;

		void _executeARM(uint32_t opcode);
		void _executeThumb(uint16_t opcode);

		void _updateZNFlags(uint32_t n);

		void _branchWithLink(uint32_t address);
		void _flushPipeline();

		void _updateVirtualRegisters();

		bool _getARMDataProcessingOp2(uint32_t opcode, uint32_t* op2);

		bool _executeARMDataProcessing(uint32_t opcode);
		bool _executeARMDataTransfer(uint32_t opcode);
		bool _executeARMBlockTransfer(uint32_t opcode);
		
		bool _executeThumbALUOp(uint16_t opcode);
		bool _executeThumbHighRegisterOp(uint16_t opcode);
		
		uint32_t _aluOperation(ALUOperation op, uint32_t a, uint32_t b, bool updateFlags = true);

		PhysicalRegister _physicalRegister(VirtualRegister r, bool forceUserMode = false) const;

		static VirtualRegister ARMRn(uint32_t opcode);
		static VirtualRegister ARMRd(uint32_t opcode);
		static VirtualRegister ARMRs(uint32_t opcode);
		static VirtualRegister ARMRm(uint32_t opcode);

		static uint32_t Shift(uint32_t n, ShiftType type, uint32_t amount, bool* carry = nullptr);
		static uint32_t ShiftSpecial(uint32_t n, ShiftType type, uint32_t amount, bool* carry);
};