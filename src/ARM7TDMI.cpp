#include "ARM7TDMI.h"

#include "FixedEndian.h"

ARM7TDMI::ARM7TDMI() {
	_mode = kModeUser;
	for (int i = 0; i <= 15; ++i) {
		_virtualRegisters[kVirtualRegisterR0 + i] = &_physicalRegisters[kPhysicalRegisterR0 + i];
	}	
	_virtualRegisters[kVirtualRegisterCPSR] = &_physicalRegisters[kPhysicalRegisterCPSR];
	_virtualRegisters[kVirtualRegisterSPSR] = nullptr;
}

void ARM7TDMI::step() {
	if (_isInThumbState) {
		_stepThumb();
	} else {
		_stepARM();
	}
}

void ARM7TDMI::branch(uint32_t address) {
	setRegister(kVirtualRegisterPC, address);
}

void ARM7TDMI::branchWithLink(uint32_t address) {
	setRegister(kVirtualRegisterLR, getRegister(kVirtualRegisterPC));
	branch(address);
}

void ARM7TDMI::setMode(Mode mode) {
	if (mode == _mode) { return; }

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

void ARM7TDMI::_stepARM() {
	// fetch the instruction (and advance pc)
	auto pc = getRegister(kVirtualRegisterPC);
	uint32_t instruction = mmu().load<BigEndian<uint32_t>>(pc);
	setRegister(kVirtualRegisterPC, pc + 4);

	// check the condition
	if (!checkCondition(static_cast<Condition>(instruction >> 28))) {
		return;
	}
	
	// execute the instruction

	if ((instruction & 0x0f000000) == 0x0f000000) {
		// SWI
		setMode(kModeSupervisor);
		setRegister(kVirtualRegisterSPSR, getRegister(kVirtualRegisterCPSR));
		branchWithLink(0x00000008);
	} else if ((instruction & 0x0e000000) == 0x0a000000) {
		uint32_t offset = ((instruction & 0xff0000) >> 16) | (instruction & 0xff00) | ((instruction & 0xff) << 16);
		if ((instruction & 0xf0000000) == 0xf0000000) {
			// BLX
			branchWithLink(getRegister(kVirtualRegisterPC) + 4 + (offset << 2) + ((instruction & 0x100000) ? 2 : 0));
			_isInThumbState = true;
		} else if (instruction & 0x1000000) {
			// BL
			branchWithLink(getRegister(kVirtualRegisterPC) + 4 + (offset << 2));
		} else {
			// B
			branch(getRegister(kVirtualRegisterPC) + 4 + (offset << 2));
		}
	} else {
		printf("unknown arm instruction %02x\n", instruction);
		throw UnknownInstruction();
	}
}

void ARM7TDMI::_stepThumb() {
	// fetch the instruction (and advance pc)
	auto pc = getRegister(kVirtualRegisterPC);
	uint16_t instruction = mmu().load<BigEndian<uint16_t>>(pc);
	setRegister(kVirtualRegisterPC, pc + 2);
	
	// execute the instruction

	printf("unknown thumb instruction %02x\n", instruction);
	throw UnknownInstruction();
}