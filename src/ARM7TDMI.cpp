#include "ARM7TDMI.h"

#include "BIT_MACROS.h"
#include "FixedEndian.h"

#include <cassert>

ARM7TDMI::ARM7TDMI() {
	for (int i = 0; i <= 15; ++i) {
		_virtualRegisters[kVirtualRegisterR0 + i] = &_physicalRegisters[kPhysicalRegisterR0 + i];
	}	
	_virtualRegisters[kVirtualRegisterCPSR] = &_physicalRegisters[kPhysicalRegisterCPSR];
	_virtualRegisters[kVirtualRegisterSPSR] = nullptr;
	setRegister(kVirtualRegisterCPSR, 0 | kModeUndefined);
}

void ARM7TDMI::step() {
	if (_toExecute.isThumb) {
		_executeThumb(static_cast<uint16_t>(_toExecute.opcode));
	} else if (_toExecute.opcode != kARMNOPcode) {
		_executeARM(_toExecute.opcode);
	}
	
	_toExecute = _toDecode;

	if (getCPSRFlag(kPSRFlagThumb)) {
		auto pc = getRegister(kVirtualRegisterPC);
		_toDecode.opcode = mmu().load<LittleEndian<uint16_t>>(pc);
		setRegister(kVirtualRegisterPC, pc + 2);
		_toDecode.isThumb = true;
	} else {
		auto pc = getRegister(kVirtualRegisterPC);
		_toDecode.opcode = mmu().load<LittleEndian<uint32_t>>(pc);
		setRegister(kVirtualRegisterPC, pc + 4);
		_toDecode.isThumb = false;
	}
}

void ARM7TDMI::branch(uint32_t address) {
	setRegister(kVirtualRegisterPC, address);
	_toExecute = Instruction();
	_toDecode = Instruction();
}

void ARM7TDMI::branchWithLink(uint32_t address) {
	setRegister(kVirtualRegisterLR, getRegister(kVirtualRegisterPC));
	branch(address);
}

void ARM7TDMI::setMode(Mode mode) {
	setRegister(kVirtualRegisterCPSR, (getRegister(kVirtualRegisterCPSR) & 0x1f) | mode);
	_updateVirtualRegisters();
}

void ARM7TDMI::setCPSRFlag(PSRFlag flag, bool set) {
	if (set) {
		setRegister(kVirtualRegisterCPSR, getRegister(kVirtualRegisterCPSR) | flag);
	} else {
		clearCPSRFlag(flag);
	}
}

bool ARM7TDMI::checkCondition(Condition condition) const {
	auto cspr = getRegister(kVirtualRegisterCPSR);
	
	switch (condition) {
		case kConditionEqual: return (cspr & kPSRFlagZero);
		case kConditionNotEqual: return !(cspr & kPSRFlagZero);
		case kConditionUnsignedHigherOrSame: return (cspr & kPSRFlagCarry);
		case kConditionUnsignedLower: return !(cspr & kPSRFlagCarry);
		case kConditionNegative: return (cspr & kPSRFlagNegative);
		case kConditionPositiveOrZero: return !(cspr & kPSRFlagNegative);
		case kConditionOverflow: return (cspr & kPSRFlagOverflow);
		case kConditionNoOverflow: return !(cspr & kPSRFlagOverflow);
		case kConditionUnsignedHigher: return (cspr & kPSRFlagCarry) && !(cspr & kPSRFlagZero);
		case kConditionUnsignedLowerOrSame: return !(cspr & kPSRFlagCarry) || (cspr & kPSRFlagZero);
		case kConditionGreaterOrEqual: return (cspr & kPSRFlagNegative) ? (cspr & kPSRFlagOverflow) : !(cspr & kPSRFlagOverflow);
		case kConditionLess: return (cspr & kPSRFlagNegative) ? !(cspr & kPSRFlagOverflow) : (cspr & kPSRFlagOverflow);
		case kConditionGreater: return !(cspr & kPSRFlagZero) && ((cspr & kPSRFlagNegative) ? (cspr & kPSRFlagOverflow) : !(cspr & kPSRFlagOverflow));
		case kConditionLessOrEqual: return (cspr & kPSRFlagZero) || ((cspr & kPSRFlagNegative) ? !(cspr & kPSRFlagOverflow) : (cspr & kPSRFlagOverflow));
		case kConditionAlways: return true;
		case kConditionNever: return false;
	}
	
	return false;
}

void ARM7TDMI::_executeARM(uint32_t opcode) {
	if (!checkCondition(static_cast<Condition>(opcode >> 28))) {
		return;
	}

	if ((opcode & 0x0f000000) == 0x0f000000) {
		// SWI
		printf("SWI\n");
		setMode(kModeSupervisor);
		setRegister(kVirtualRegisterSPSR, getRegister(kVirtualRegisterCPSR));
		branchWithLink(0x00000008);
		return;
	}
	
	if ((opcode & 0x0e000000) == 0x0a000000) {
		// B, BL, or BLX
		uint32_t offset = opcode & 0xffffff;
		if (offset & 0x800000) {
			offset |= 0xff000000;
		}
		offset <<= 2;
		uint32_t address = getRegister(kVirtualRegisterPC) + offset;
		if ((opcode & 0xf0000000) == 0xf0000000) {
			// BLX
			if (BIT24(opcode)) {
				address += 2;
			}
			printf("BLX %08x\n", address);
			branchWithLink(address);
			setCPSRFlag(kPSRFlagThumb);
		} else if (BIT24(opcode)) {
			// BL
			printf("BL %08x\n", address);
			branchWithLink(address);
		} else {
			// B
			printf("B %08x\n", address);
			branch(address);
		}
		return;
	}
	
	if ((opcode & 0xdef0000) == 0x1a00000) {
		// MOV
		uint32_t op2 = 0;
		if (_getARMDataProcessingOp2(opcode, &op2)) {
			printf("MOV %08u to r%u\n", op2, ARMRd(opcode));
			setRegister(ARMRd(opcode), op2);
			_updateZNFlags(op2);
			return;
		}
	}
	
	if ((opcode & 0xd900000) == 0x1000000) {
		// PSR
		if (BIT21(opcode)) {
			// MSR
			if ((opcode & 0xf000) == 0xf000) {
				uint32_t mask = 0
					| (BIT19(opcode) ? kPSRMaskFlags : 0) 
					| (BIT18(opcode) ? kPSRMaskStatus : 0) 
					| (BIT17(opcode) ? kPSRMaskExtension : 0) 
					| (BIT16(opcode) ? kPSRMaskControl : 0)
				;
				if (BIT25(opcode)) {
					// immediate op
					auto shift = BITFIELD_UINT32(opcode, 11, 8) << 1;
					auto val = Shift(BITFIELD_UINT32(opcode, 7, 0), kShiftTypeROR, shift);
					auto psr = BIT22(opcode) ? kVirtualRegisterSPSR : kVirtualRegisterCPSR;
					printf("MSR %08x to %s\n", val, BIT22(opcode) ? "spsr" : "cpsr");
					setRegister(psr, (getRegister(psr) & ~mask) | (val & mask));
					if (psr == kVirtualRegisterCPSR && (mask & kPSRMaskControl)) {
						_updateVirtualRegisters();
					}
					return;
				} else if ((opcode & 0xff0) == 0) {
					// register op
					auto val = getRegister(ARMRm(opcode));
					auto psr = BIT22(opcode) ? kVirtualRegisterSPSR : kVirtualRegisterCPSR;
					printf("MSR r%u (%08x) to %s\n", ARMRm(opcode), val, BIT22(opcode) ? "spsr" : "cpsr");
					setRegister(psr, (getRegister(psr) & ~mask) | (val & mask));
					if (psr == kVirtualRegisterCPSR && (mask & kPSRMaskControl)) {
						_updateVirtualRegisters();
					}
					return;
				}
			}
		} else if ((opcode & 0xf0fff) == 0xf0000) {
			// MRS
			auto psr = BIT22(opcode) ? kVirtualRegisterSPSR : kVirtualRegisterCPSR;
			printf("MRS %s to r%u\n", BIT22(opcode) ? "spsr" : "cpsr", ARMRd(opcode));
			setRegister(ARMRd(opcode), getRegister(psr));
			return;
		}
	}
	
	printf("unknown arm opcode %02x\n", opcode);
	throw UnknownInstruction();
}

void ARM7TDMI::_executeThumb(uint16_t opcode) {
	printf("unknown thumb opcode %02x\n", opcode);
	throw UnknownInstruction();
}

void ARM7TDMI::_updateZNFlags(uint32_t n) {
	setCPSRFlag(kPSRFlagZero, n == 0);
	setCPSRFlag(kPSRFlagNegative, BIT31(n));
}

void ARM7TDMI::_updateVirtualRegisters() {
	auto mode = static_cast<Mode>(getRegister(kVirtualRegisterCPSR) & 0x1f);
	
	if (mode == kModeFIQ) {
		for (int i = 0; i <= 4; ++i) {
			_virtualRegisters[kVirtualRegisterR8 + i] = &_physicalRegisters[kPhysicalRegisterR8FIQ + i];
		}
	} else {
		for (int i = 0; i <= 4; ++i) {
			_virtualRegisters[kVirtualRegisterR8 + i] = &_physicalRegisters[kPhysicalRegisterR8 + i];
		}
	}
	
	switch (mode) {
		case kModeFIQ:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13FIQ];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLRFIQ];
			_virtualRegisters[kVirtualRegisterSPSR] = &_physicalRegisters[kPhysicalRegisterSPSRFIQ];
			break;
		case kModeSupervisor:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13SVC];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLRSVC];
			_virtualRegisters[kVirtualRegisterSPSR] = &_physicalRegisters[kPhysicalRegisterSPSRSVC];
			break;
		case kModeAbort:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13ABT];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLRABT];
			_virtualRegisters[kVirtualRegisterSPSR] = &_physicalRegisters[kPhysicalRegisterSPSRABT];
			break;
		case kModeIRQ:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13IRQ];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLRIRQ];
			_virtualRegisters[kVirtualRegisterSPSR] = &_physicalRegisters[kPhysicalRegisterSPSRIRQ];
			break;
		case kModeUndefined:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13UND];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLRUND];
			_virtualRegisters[kVirtualRegisterSPSR] = &_physicalRegisters[kPhysicalRegisterSPSRUND];
			break;
		default:
			_virtualRegisters[kVirtualRegisterR13] = &_physicalRegisters[kPhysicalRegisterR13];
			_virtualRegisters[kVirtualRegisterLR] = &_physicalRegisters[kPhysicalRegisterLR];
			_virtualRegisters[kVirtualRegisterSPSR] = nullptr;
	}
}

bool ARM7TDMI::_getARMDataProcessingOp2(uint32_t opcode, uint32_t* op2) {
	bool updateFlags = BIT20(opcode);
	bool shiftCarry = false;
	
	if (BIT25(opcode)) {
		// immediate op 2
		uint32_t n = opcode & 0xff;
		uint32_t shift = BITFIELD_UINT32(opcode, 11, 8) << 1;
		*op2 = Shift(n, kShiftTypeROR, shift);
	} else {
		// register op 2
		auto n = getRegister(ARMRm(opcode));
		auto shiftType = static_cast<ShiftType>(BITFIELD_UINT32(opcode, 6, 5));
		if (BIT4(opcode)) {
			// register shift
			if (BIT7(opcode)) {
				return false;
			}
			*op2 = Shift(n, shiftType, getRegister(ARMRs(opcode)), &shiftCarry);
		} else {
			// immediate shift
			uint32_t shift = BITFIELD_UINT32(opcode, 11, 7);
			if (shift == 0) {
				switch (shiftType) {
					case kShiftTypeLSL:
						*op2 = n;
						updateFlags = false;
						break;
					case kShiftTypeLSR:
						*op2 = static_cast<uint32_t>(static_cast<int32_t>(n) >> 31);
						shiftCarry = BIT31(n);
						break;
					case kShiftTypeASR:
						*op2 = static_cast<uint32_t>(static_cast<int32_t>(n) >> 31);
						shiftCarry = BIT31(n);
						break;
					case kShiftTypeROR:
						*op2 = (Shift(n, kShiftTypeROR, 1, &shiftCarry) & 0x7fffffff) | (getCPSRFlag(kPSRFlagCarry) ? 0x80000000 : 0);
						break;
				}
			} else {
				*op2 = Shift(n, shiftType, shift, &shiftCarry);
			}
		}
		if (updateFlags) {
			setCPSRFlag(kPSRFlagCarry, shiftCarry);
		}
	}

	return true;
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRn(uint32_t opcode) {
	return static_cast<VirtualRegister>(kVirtualRegisterR0 + BITFIELD_UINT32(opcode, 19, 16));
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRd(uint32_t opcode) {
	return static_cast<VirtualRegister>(kVirtualRegisterR0 + BITFIELD_UINT32(opcode, 15, 12));
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRs(uint32_t opcode) {
	return static_cast<VirtualRegister>(kVirtualRegisterR0 + BITFIELD_UINT32(opcode, 11, 8));
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRm(uint32_t opcode) {
	return static_cast<VirtualRegister>(kVirtualRegisterR0 + BITFIELD_UINT32(opcode, 3, 0));
}

uint32_t ARM7TDMI::Shift(uint32_t n, ARM7TDMI::ShiftType type, uint32_t amount, bool* carry) {
	switch (type) {
		case kShiftTypeLSL:
			if (carry) { *carry = n & (1 << (32 - amount)); }
			return n << amount;
		case kShiftTypeLSR:
			if (carry) { *carry = n & (1 << (amount - 1)); }
			return n >> amount;
		case kShiftTypeASR:
			if (carry) { *carry = n & (1 << (amount - 1)); }
			return static_cast<uint32_t>(static_cast<int32_t>(n) >> amount);
		case kShiftTypeROR:
			if (carry) { *carry = n & (1 << (amount & 0x1f - 1)); }
			return (n << (sizeof(n) * 8 - amount)) | (n >> amount);
	}

	assert(false);
	return 0;
}
