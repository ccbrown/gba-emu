#include "ARM7TDMI.h"

#include "BIT_MACROS.h"
#include "FixedEndian.h"

#include <cassert>

#define BITFIELD_REGISTER(opcode, msb, lsb) static_cast<VirtualRegister>(kVirtualRegisterR0 + BITFIELD_UINT32(opcode, msb, lsb))

ARM7TDMI::ARM7TDMI() {
	for (int i = 0; i <= 15; ++i) {
		_virtualRegisters[kVirtualRegisterR0 + i] = static_cast<PhysicalRegister>(kPhysicalRegisterR0 + i);
	}	
	_virtualRegisters[kVirtualRegisterCPSR] = kPhysicalRegisterCPSR;
	setRegister(kVirtualRegisterCPSR, 0 | kModeSupervisor);
	_updateVirtualRegisters();
}

void ARM7TDMI::step() {
	if (_toExecute.isThumb) {
		printf("%08x ", getRegister(kVirtualRegisterPC) - 4);
		_executeThumb(_toExecute.opcode);
	} else if (_toExecute.opcode != kARMNOPcode) {
		printf("%08x ", getRegister(kVirtualRegisterPC) - 8);
		_executeARM(_toExecute.opcode);
	}
	
	_toExecute = _toDecode;

	if (getCPSRFlag(kPSRFlagThumb)) {
		auto pc = getRegister(kVirtualRegisterPC);
		assert(!(pc & 0x1));
		_toDecode.opcode = mmu().load<LittleEndian<uint16_t>>(pc);
		setRegister(kVirtualRegisterPC, pc + 2);
		_toDecode.isThumb = true;
	} else {
		auto pc = getRegister(kVirtualRegisterPC);
		assert(!(pc & 0x3));
		_toDecode.opcode = mmu().load<LittleEndian<uint32_t>>(pc);
		setRegister(kVirtualRegisterPC, pc + 4);
		_toDecode.isThumb = false;
	}
}

void ARM7TDMI::branch(uint32_t address) {
	setRegister(kVirtualRegisterPC, address);
	_flushPipeline();
}

void ARM7TDMI::setMode(Mode mode) {
	auto cpsr = getRegister(kVirtualRegisterCPSR);
	setRegister(kVirtualRegisterCPSR, (cpsr & ~0x1f) | mode);
	_updateVirtualRegisters();
	if (mode != kModeUser && mode != kModeSystem) {
		setRegister(kVirtualRegisterSPSR, cpsr);
	}
}

void ARM7TDMI::setCPSRFlags(uint32_t flags, bool set) {
	if (set) {
		setRegister(kVirtualRegisterCPSR, getRegister(kVirtualRegisterCPSR) | flags);
	} else {
		clearCPSRFlags(flags);
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
	printf("%08x: ", opcode);

	if (!checkCondition(static_cast<Condition>(opcode >> 28))) {
		printf("[SKIPPED]\n");
		return;
	}	

	if ((opcode & 0x0f000000) == 0x0f000000) {
		// SWI
		printf("SWI %u\n", BITFIELD_UINT32(opcode, 23, 0));
		setMode(kModeSupervisor);
		setCPSRFlags(kPSRFlagIRQ);
		_branchWithLink(0x00000008);
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
			_branchWithLink(address);
			setCPSRFlags(kPSRFlagThumb);
		} else if (BIT24(opcode)) {
			// BL
			printf("BL %08x\n", address);
			_branchWithLink(address);
		} else {
			// B
			printf("B %08x\n", address);
			branch(address);
		}
		return;
	}
	
	if ((opcode & 0x0fffff00) == 0x12fff00) {
		// BX or BLX
		uint32_t address = getRegister(ARMRm(opcode));
		if ((opcode & 0xf0) == 0x10) {
			// BX
			printf("BX %08x\n", address);
			branch(address & 0xfffffffe);
			if (address & 1) {
				setCPSRFlags(kPSRFlagThumb);
			}
			return;
		} else if ((opcode & 0xf0) == 0x30) {
			// BLX
			printf("BLX %08x\n", address);
			_branchWithLink(address & 0xfffffffe);
			if (address & 1) {
				setCPSRFlags(kPSRFlagThumb);
			}
			return;
		}
	}
	
	if (_executeARMDataProcessing(opcode)) { return; }
	
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
	
	if (_executeARMDataTransfer(opcode)) { return; }
	if (_executeARMBlockTransfer(opcode)) { return; }
	
	printf("unknown arm opcode %02x\n", opcode);
	throw UnknownInstruction();
}

void ARM7TDMI::_executeThumb(uint16_t opcode) {
	printf("    %04x: ", opcode);

	if ((opcode & 0xf600) == 0xb400) {
		// PUSH or POP
		printf(BIT11(opcode) ? "POP " : "PUSH ");
		auto stack = getRegister(kVirtualRegisterSP);
		if (BIT11(opcode)) {
			// POP
			for (int i = 0; i <= 7; ++i) {
				if (!(opcode & (1 << i))) { continue; }
				printf("r%d ", i);
				setRegister(static_cast<VirtualRegister>(kVirtualRegisterR0 + i), mmu().load<LittleEndian<uint32_t>>(stack));
				stack += 4;
			}
			if (BIT8(opcode)) {
				printf("pc ");
				setRegister(kVirtualRegisterPC, mmu().load<LittleEndian<uint32_t>>(stack));
				_flushPipeline();
				stack += 4;
			}
		} else {
			// PUSH
			if (BIT8(opcode)) {
				printf("lr ");
				stack -= 4;
				mmu().store<LittleEndian<uint32_t>>(stack, getRegister(kVirtualRegisterLR));
			}
			for (int i = 7; i >= 0; --i) {
				if (!(opcode & (1 << i))) { continue; }
				printf("r%d ", i);
				stack -= 4;
				mmu().store<LittleEndian<uint32_t>>(stack, getRegister(static_cast<VirtualRegister>(kVirtualRegisterR0 + i)));
			}
		}
		setRegister(kVirtualRegisterSP, stack);
		printf("\n");
		return;
	}
	
	if ((opcode & 0xf000) == 0xa000) {
		// get pc or sp + offset
		auto rd = BITFIELD_REGISTER(opcode, 10, 8);
		auto offset = BITFIELD_UINT32(opcode, 7, 0) << 2;
		if (BIT11(opcode)) {
			setRegister(rd, getRegister(kVirtualRegisterSP) + offset);
		} else {
			setRegister(rd, (getRegister(kVirtualRegisterPC) & ~2) + offset);
		}
		return;
	}
	
	if ((opcode & 0xf800) == 0x4800) {
		// LDR pc-relative
		auto r = BITFIELD_UINT32(opcode, 10, 8);
		auto offset = BITFIELD_UINT32(opcode, 7, 0) << 2;
		auto address = (getRegister(kVirtualRegisterPC) & ~2) + offset;
		printf("LDR r%u with pc + %08x (%08x)\n", r, offset, address);
		setRegister(static_cast<VirtualRegister>(kVirtualRegisterR0 + r), mmu().load<uint32_t>(address));
		return;
	}
	
	if (_executeThumbHighRegisterOp(opcode)) { return; }

	if (_executeThumbALUOp(opcode)) { return; }
	
	if ((opcode & 0xe000) == 0x2000) {
		// immediate operation
		auto rd = BITFIELD_REGISTER(opcode, 10, 8);
		uint32_t n  = BITFIELD_UINT32(opcode, 7, 0);
		if (BIT12(opcode)) {
			if (BIT11(opcode)) {
				// SUB
				auto result = _aluOperation(kALUOperationSUB, getRegister(rd), n);
				printf("SUB r%u = r%u - %08x = %08x\n", rd, rd, n, result);
				setRegister(rd, result);
			} else {
				// ADD
				auto result = _aluOperation(kALUOperationADD, getRegister(rd), n);
				printf("ADD r%u = r%u + %08x = %08x\n", rd, rd, n, result);
				setRegister(rd, result);
			}
		} else if (BIT11(opcode)) {
			// CMP
			printf("CMP r%u - %08x\n", rd, n);
			_aluOperation(kALUOperationSUB, getRegister(rd), n);
		} else {
			// MOV
			printf("MOV %08x to r%u\n", n, rd);
			setRegister(rd, n);
			_updateZNFlags(n);
		}
		return;
	}
	
	if ((opcode & 0xf800) == 0xf000) {
		// long BL or BLX, first half
		printf("BL or BLX first half\n");
		uint32_t offset = BITFIELD_UINT32(opcode, 10, 0) << 12;
		if (offset & 0x400000) {
			offset |= 0xff800000;
		}
		setRegister(kVirtualRegisterLR, getRegister(kVirtualRegisterPC) + offset);
		return;
	}
	
	if ((opcode & 0xf800) == 0xf800) {
		// long BL, second half
		uint32_t offset = BITFIELD_UINT32(opcode, 10, 0) << 1;
		uint32_t address = getRegister(kVirtualRegisterLR) + offset;
		printf("BL %08x\n", address);
		_branchWithLink(address);
		return;
	}

	if ((opcode & 0xf801) == 0xe800) {
		// long BLX, second half
		uint32_t address = getRegister(kVirtualRegisterLR) + (BITFIELD_UINT32(opcode, 10, 0) << 1);
		printf("BLX %08x\n", address);
		_branchWithLink(address);
		clearCPSRFlags(kPSRFlagThumb);
		return;
	}
	
	if ((opcode & 0xff00) == 0xdf00) {
		// SWI
		printf("SWI %u\n", BITFIELD_UINT32(opcode, 7, 0));
		clearCPSRFlags(kPSRFlagThumb);
		setMode(kModeSupervisor);
		setCPSRFlags(kPSRFlagIRQ);
		_branchWithLink(0x00000008);
		return;
	}
	
	if ((opcode & 0xe000) == 0 && (opcode & 0xf800) != 0x1800) {
		// move shifted register
		ShiftType shiftType = kShiftTypeLSL;
		if (BIT12(opcode)) {
			shiftType = kShiftTypeASR;
		} else if (BIT11(opcode)) {
			shiftType = kShiftTypeLSR;
		}
		auto rs = BITFIELD_REGISTER(opcode, 5, 3);
		auto rd = BITFIELD_REGISTER(opcode, 2, 0);
		auto n = BITFIELD_UINT32(opcode, 10, 6);
		printf("%s r%u by %08x into r%u\n", shiftType == kShiftTypeLSL ? "LSL" : shiftType == kShiftTypeLSR ? "LSR" : "ASR", rs, n, rd);
		bool carry = getCPSRFlag(kPSRFlagCarry);
		auto result = ShiftSpecial(getRegister(rs), shiftType, n, &carry);
		setRegister(rd, result);
		_updateZNFlags(result);
		return;
	}
	
	if ((opcode & 0xf800) == 0x1800) {
		// add or subtract
		auto rs = BITFIELD_REGISTER(opcode, 5, 3);
		auto rd = BITFIELD_REGISTER(opcode, 2, 0);
		if (BIT10(opcode)) {
			// immediate
			auto n = BITFIELD_UINT32(opcode, 8, 6);
			if (BIT9(opcode)) {
				auto result = _aluOperation(kALUOperationSUB, getRegister(rs), n);
				printf("SUB r%u = r%u - %08x = %08x\n", rd, rs, n, result);
				setRegister(rd, result);
			} else {
				auto result = _aluOperation(kALUOperationADD, getRegister(rs), n);
				printf("ADD r%u = r%u + %08x = %08x\n", rd, rs, n, result);
				setRegister(rd, result);
			}
		} else {
			// register
			auto r = BITFIELD_REGISTER(opcode, 8, 6);
			if (BIT9(opcode)) {
				auto result = _aluOperation(kALUOperationSUB, getRegister(rs), getRegister(r));
				printf("SUB r%u = r%u - r%u = %08x\n", rd, rs, r, result);
				setRegister(rd, result);
			} else {
				auto result = _aluOperation(kALUOperationADD, getRegister(rs), getRegister(r));
				printf("ADD r%u = r%u + r%u = %08x\n", rd, rs, r, result);
				setRegister(rd, result);
			}
		}
		return;
	}
	
	if ((opcode & 0xf000) == 0x5000) {
		// load or store
		auto ro = BITFIELD_REGISTER(opcode, 8, 6);
		auto rb = BITFIELD_REGISTER(opcode, 5, 3);
		auto address = getRegister(rb) + getRegister(ro);
		auto rd = BITFIELD_REGISTER(opcode, 2, 0);
		auto operation = BITFIELD_UINT32(opcode, 11, 9);
		switch (operation) {
			case 0:
				printf("STR r%u to [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				mmu().store<LittleEndian<uint32_t>>(address, getRegister(rd));
				return;
			case 1:
				printf("STRH r%u to [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				mmu().store<LittleEndian<uint16_t>>(address, static_cast<uint16_t>(getRegister(rd)));
				return;
			case 2:
				printf("STRB r%u to [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				mmu().store<LittleEndian<uint8_t>>(address, static_cast<uint8_t>(getRegister(rd)));
				return;
			case 3:
				printf("LDSB r%u from [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<int8_t>>(address)));
				return;
			case 4:
				printf("LDR r%u from [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				setRegister(rd, mmu().load<LittleEndian<uint32_t>>(address));
				return;
			case 5:
				printf("LDRH r%u from [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<uint16_t>>(address)));
				return;
			case 6:
				printf("LDRB r%u from [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<uint8_t>>(address)));
				return;
			case 7:
				printf("LDSH r%u from [r%u + r%u] (%08x)\n", rd, rb, ro, address);
				setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<int16_t>>(address)));
				return;
		}
	}
	
	if ((opcode & 0xf000) == 0x8000) {
		// load or store halfword
		auto rb = BITFIELD_REGISTER(opcode, 5, 3);
		auto rd = BITFIELD_REGISTER(opcode, 2, 0);
		auto offset = BITFIELD_UINT32(opcode, 10, 6) << 1;
		auto address = getRegister(rb) + offset;
		if (BIT11(opcode)) {
			printf("LDRH r%u from [r%u + %08x] (%08x)\n", rd, rb, offset, address);
			setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<uint16_t>>(address)));
		} else {
			printf("STRH r%u to [r%u + %08x] (%08x)\n", rd, rb, offset, address);
			mmu().store<LittleEndian<uint16_t>>(address, static_cast<uint16_t>(getRegister(rd)));
		}
		return;
	}
	
	if ((opcode & 0xe000) == 0x6000) {
		// load or store with immediate offset
		auto rb = BITFIELD_REGISTER(opcode, 5, 3);
		auto rd = BITFIELD_REGISTER(opcode, 2, 0);
		auto offset = BITFIELD_UINT32(opcode, 10, 6) << 1;
		if (!BIT12(opcode)) {
			// word
			offset <<= 2;
			uint32_t address = getRegister(rb) + offset;
			if (BIT11(opcode)) {
				printf("LDR r%u from [r%u + %08x] (%08x)\n", rd, rb, offset, address);
				setRegister(rd, mmu().load<LittleEndian<uint32_t>>(address));
			} else {
				printf("STR r%u to [r%u + %08x] (%08x)\n", rd, rb, offset, address);
				mmu().store<LittleEndian<uint32_t>>(address, getRegister(rd));
			}
		} else {
			// byte
			uint32_t address = getRegister(rb) + offset;
			if (BIT11(opcode)) {
				printf("LDRB r%u from [r%u + %08x] (%08x)\n", rd, rb, offset, address);
				setRegister(rd, static_cast<uint32_t>(mmu().load<LittleEndian<uint8_t>>(address)));
			} else {
				printf("STRB r%u to [r%u + %08x] (%08x)\n", rd, rb, offset, address);
				mmu().store<LittleEndian<uint8_t>>(address, static_cast<uint8_t>(getRegister(rd)));
			}
		}
		return;
	}
	
	if ((opcode & 0xf000) == 0xd000) {
		// conditional branch
		auto condition = static_cast<Condition>((opcode >> 8) & 0xf);
		if (condition != kConditionAlways && condition != kConditionNever) {
			uint32_t offset = static_cast<uint32_t>(static_cast<int8_t>(BITFIELD_UINT32(opcode, 7, 0))) << 1;
			uint32_t address = getRegister(kVirtualRegisterPC) + offset;
			printf("B %08x (condition = %u)\n", address, condition);
			if (checkCondition(condition)) {
				branch(address);
			}
			return;
		}
	}
	
	if ((opcode & 0xf800) == 0xe000) {
		// unconditional branch
		uint32_t offset = BITFIELD_UINT32(opcode, 10, 0) << 1;
		if (offset & 0x800) {
			offset |= 0xfffff000;
		}
		auto address = getRegister(kVirtualRegisterPC) + offset;
		printf("B %08x\n", address);
		branch(address);
		return;
	}
	
	if ((opcode & 0xff00) == 0xb000) {
		// add offset to SP
		uint32_t offset = BITFIELD_UINT32(opcode, 6, 0) << 2;
		if (BIT7(opcode)) {
			auto result = getRegister(kVirtualRegisterSP) - offset;
			printf("SUB sp = sp - %08x = %08x\n", offset, result);
			setRegister(kVirtualRegisterSP, result);
		} else {
			auto result = getRegister(kVirtualRegisterSP) + offset;
			printf("ADD sp = sp + %08x = %08x\n", offset, result);
			setRegister(kVirtualRegisterSP, result);
		}
		return;
	}

	if ((opcode & 0xf000) == 0x9000) {
		// load / store SP-relative
		auto rd = BITFIELD_REGISTER(opcode, 10, 8);
		auto offset = BITFIELD_UINT32(opcode, 7, 0);
		if (BIT11(opcode)) {
			printf("LDR r%u from sp + %08x\n", rd, offset);
			setRegister(rd, mmu().load<LittleEndian<uint32_t>>(getRegister(kVirtualRegisterSP) + offset));
		} else {
			printf("STR r%u to sp + %08x\n", rd, offset);
			mmu().store<LittleEndian<uint32_t>>(getRegister(kVirtualRegisterSP) + offset, getRegister(rd));
		}
		return;
	}
	
	if ((opcode & 0xf000) == 0xc000) {
		// STM or LDM
		auto rb = BITFIELD_REGISTER(opcode, 10, 8);
		auto address = getRegister(rb);
		uint32_t rlist = BITFIELD_UINT32(opcode, 7, 0);
		printf("%s r%u: ", BIT11(opcode) ? "LDMIA" : "STMIA", rb);
		for (int i = 0; i <= 7; ++i) {
			if (!(rlist & (1 << i))) { continue; }
			auto r = static_cast<VirtualRegister>(kVirtualRegisterR0 + i);
			printf("r%u ", r);
			if (BIT11(opcode)) {
				setRegister(r, mmu().load<LittleEndian<uint32_t>>(address));
			} else {
				mmu().store<LittleEndian<uint32_t>>(address, getRegister(r));
			}
			address += 4;
		}
		if (!BIT11(opcode) || !(rlist & (1 << rb))) {
			setRegister(rb, address);
		}
		printf("\n");
		return;
	}

	printf("unknown thumb opcode %04x\n", opcode);
	throw UnknownInstruction();
}

void ARM7TDMI::_updateZNFlags(uint32_t n) {
	setCPSRFlags(kPSRFlagZero, n == 0);
	setCPSRFlags(kPSRFlagNegative, BIT31(n));
}

void ARM7TDMI::_branchWithLink(uint32_t address) {
	setRegister(kVirtualRegisterLR, (getRegister(kVirtualRegisterPC) - (_toExecute.isThumb ? 2 : 4)) | (_toExecute.isThumb ? 1 : 0));
	branch(address);
}

void ARM7TDMI::_flushPipeline() {
	_toDecode = Instruction();
}

void ARM7TDMI::_updateVirtualRegisters() {
	auto mode = static_cast<Mode>(getRegister(kVirtualRegisterCPSR) & 0x1f);
	
	if (mode == kModeFIQ) {
		for (int i = 0; i <= 4; ++i) {
			_virtualRegisters[kVirtualRegisterR8 + i] = static_cast<PhysicalRegister>(kPhysicalRegisterR8FIQ + i);
		}
	} else {
		for (int i = 0; i <= 4; ++i) {
			_virtualRegisters[kVirtualRegisterR8 + i] = static_cast<PhysicalRegister>(kPhysicalRegisterR8 + i);
		}
	}
	
	switch (mode) {
		case kModeFIQ:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSPFIQ;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLRFIQ;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterSPSRFIQ;
			break;
		case kModeSupervisor:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSPSVC;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLRSVC;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterSPSRSVC;
			break;
		case kModeAbort:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSPABT;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLRABT;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterSPSRABT;
			break;
		case kModeIRQ:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSPIRQ;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLRIRQ;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterSPSRIRQ;
			break;
		case kModeUndefined:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSPUND;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLRUND;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterSPSRUND;
			break;
		default:
			_virtualRegisters[kVirtualRegisterSP]   = kPhysicalRegisterSP;
			_virtualRegisters[kVirtualRegisterLR]   = kPhysicalRegisterLR;
			_virtualRegisters[kVirtualRegisterSPSR] = kPhysicalRegisterInvalid;
	}
}

bool ARM7TDMI::_getARMDataProcessingOp2(uint32_t opcode, uint32_t* op2) {
	bool updateFlags = BIT20(opcode);
	bool carryFlag = getCPSRFlag(kPSRFlagCarry);
	
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
			*op2 = Shift(n, shiftType, getRegister(ARMRs(opcode)), &carryFlag);
		} else {
			// immediate shift
			uint32_t shift = BITFIELD_UINT32(opcode, 11, 7);
			*op2 = ShiftSpecial(n, shiftType, shift, &carryFlag);
		}
		if (updateFlags) {
			setCPSRFlags(kPSRFlagCarry, carryFlag);
		}
	}

	return true;
}

bool ARM7TDMI::_executeARMDataProcessing(uint32_t opcode) {
	if (opcode & 0xc000000) { return false; }
	
	uint32_t operation = BITFIELD_UINT32(opcode, 24, 21);
	
	uint32_t op2 = 0;
	if (!_getARMDataProcessingOp2(opcode, &op2)) {
		return false;
	}
	
	auto rd = ARMRd(opcode);
	
	bool updateFlags = BIT20(opcode);
	
	switch (operation) {
		case 0x0: {
			uint32_t result = _aluOperation(kALUOperationAND, getRegister(ARMRn(opcode)), op2, updateFlags);
			printf("AND r%u = r%u & %08x = %08x\n", rd, ARMRn(opcode), op2, result);
			setRegister(rd, result);
			break;
		}
		case 0x1: {
			uint32_t result = _aluOperation(kALUOperationEOR, getRegister(ARMRn(opcode)), op2, updateFlags);
			printf("EOR r%u = r%u ^ %08x = %08x\n", rd, ARMRn(opcode), op2, result);
			setRegister(rd, result);
			break;
		}
		case 0x2: {
			uint32_t result = _aluOperation(kALUOperationSUB, getRegister(ARMRn(opcode)), op2, updateFlags);
			printf("SUB r%u = r%u - %08x = %08x\n", rd, ARMRn(opcode), op2, result);
			setRegister(rd, result);
			break;
		}
		case 0x3: {
			uint32_t result = _aluOperation(kALUOperationSUB, op2, getRegister(ARMRn(opcode)), updateFlags);
			printf("SUB r%u = %08x - r%u = %08x\n", rd, op2, ARMRn(opcode), result);
			setRegister(rd, result);
			break;
		}
		case 0x4: {
			uint32_t result = _aluOperation(kALUOperationADD, getRegister(ARMRn(opcode)), op2, updateFlags);
			printf("ADD r%u = r%u + %08x = %08x\n", rd, ARMRn(opcode), op2, result);
			setRegister(rd, result);
			break;
		}
		case 0x8:
			if (!updateFlags || rd != kVirtualRegisterR0) { return false; }
			printf("TST r%u & %08x\n", ARMRn(opcode), op2);
			_aluOperation(kALUOperationAND, getRegister(ARMRn(opcode)), op2, updateFlags);
			break;
		case 0x9:
			if (!updateFlags || rd != kVirtualRegisterR0) { return false; }
			printf("TEQ r%u ^ %08x\n", ARMRn(opcode), op2);
			_aluOperation(kALUOperationEOR, getRegister(ARMRn(opcode)), op2, updateFlags);
			break;
		case 0xa:
			if (!updateFlags || rd != kVirtualRegisterR0) { return false; }
			printf("CMP r%u - %08x\n", ARMRn(opcode), op2);
			_aluOperation(kALUOperationSUB, getRegister(ARMRn(opcode)), op2, updateFlags);
			break;
		case 0xb:
			if (!updateFlags || rd != kVirtualRegisterR0) { return false; }
			printf("CMN r%u + %08x\n", ARMRn(opcode), op2);
			_aluOperation(kALUOperationADD, getRegister(ARMRn(opcode)), op2, updateFlags);
			break;
		case 0xc:
			printf("ORR r%u = r%u | %08x\n", rd, ARMRn(opcode), op2);
			setRegister(rd, _aluOperation(kALUOperationORR, getRegister(ARMRn(opcode)), op2, updateFlags));
			break;
		case 0xd: {
			// MOV
			if (ARMRn(opcode) != kVirtualRegisterR0) { return false; }
			auto rd = ARMRd(opcode);
			printf("MOV %08x to r%u\n", op2, rd);
			setRegister(rd, op2);
			if (updateFlags) {
				_updateZNFlags(op2);
			}
			break;
		}
		case 0xe:
			printf("BIC r%u = r%u & ~%08x\n", rd, ARMRn(opcode), op2);
			setRegister(rd, _aluOperation(kALUOperationBIC, getRegister(ARMRn(opcode)), op2, updateFlags));
			break;
		default:
			return false;
	}

	if (rd == kVirtualRegisterPC) {
		_flushPipeline();
		if (updateFlags) {
			setRegister(kVirtualRegisterCPSR, getRegister(kVirtualRegisterSPSR));
			_updateVirtualRegisters();
		}
	}
	
	return true;
}

bool ARM7TDMI::_executeARMDataTransfer(uint32_t opcode) {
	if ((opcode & 0xc000000) != 0x4000000) { return false; }
	
	auto rd = ARMRd(opcode);
	auto rdp = _physicalRegister(rd, !BIT24(opcode) && BIT21(opcode));
	auto rn = ARMRn(opcode);

	uint32_t base = getRegister(rn);
	uint32_t offset = 0;
		
	if (BIT25(opcode)) {
		// register offset shifted by immediate
		if (BIT4(opcode)) { return false; }
		offset = getRegister(ARMRm(opcode));
		uint32_t shift = BITFIELD_UINT32(opcode, 11, 7);
		auto shiftType = static_cast<ShiftType>(BITFIELD_UINT32(opcode, 6, 5));
		bool carry = getCPSRFlag(kPSRFlagCarry);
		offset = ShiftSpecial(offset, shiftType, shift, &carry);
	} else {
		// immediate offset
		offset = BITFIELD_UINT32(opcode, 11, 0);
	}

	uint32_t indexed = BIT23(opcode) ? (base + offset) : (base - offset);
	if (!(BIT24(opcode) && !BIT21(opcode))) {
		// writeback
		setRegister(rn, indexed);
	}

	uint32_t address = BIT24(opcode) ? indexed : base;

	if (BIT20(opcode)) {
		printf("LDR r%u from %08x\n", rd, address);
		if (BIT22(opcode)) {
			setRegister(rdp, mmu().load<uint8_t>(address));
		} else {
			setRegister(rdp, mmu().load<LittleEndian<uint32_t>>(address));
		}
	} else {
		printf("STR r%u to %08x\n", rd, address);
		if (BIT22(opcode)) {
			mmu().store<uint8_t>(address, static_cast<uint8_t>(getRegister(rdp)));
		} else {
			mmu().store<LittleEndian<uint32_t>>(address, getRegister(rdp));
		}
	}
	
	return true;
}

bool ARM7TDMI::_executeARMBlockTransfer(uint32_t opcode) {
	if ((opcode & 0x0e000000) != 0x08000000) { return false; }
	
	auto rn = ARMRn(opcode);
	uint32_t address = getRegister(rn);
	
	printf("%s r%u (%08x): ", BIT20(opcode) ? "LDM" : "STM", rn, address);
	
	for (int i = BIT23(opcode) ? 0 : 15; i >= 0 && i <= 15; i += BIT23(opcode) ? 1 : -1) {
		if (!(opcode & (1 << i))) { continue; }

		printf("r%u ", i);

		if (BIT24(opcode)) {
			if (BIT23(opcode)) {
				address += 4;
			} else {
				address -= 4;
			}
		}
		
		auto r = _physicalRegister(static_cast<VirtualRegister>(kVirtualRegisterR0 + i), BIT22(opcode) && !(BIT20(opcode) && BIT15(opcode)));

		if (BIT20(opcode)) {
			uint32_t value = mmu().load<LittleEndian<uint32_t>>(address);
			printf("(%08x) ", value);
			setRegister(r, value);
		} else {
			auto value = getRegister(r);
			printf("(%08x) ", value);
			mmu().store<LittleEndian<uint32_t>>(address, value);
		}

		if (!BIT24(opcode)) {
			if (BIT23(opcode)) {
				address += 4;
			} else {
				address -= 4;
			}
		}
	}
	
	if (BIT21(opcode)) {
		setRegister(rn, address);
	}
	
	if (BIT22(opcode) && BIT20(opcode) && BIT15(opcode)) {
		// LDR with PC, bit 22 means copy spsr to cpsr
		setRegister(kVirtualRegisterCPSR, getRegister(kVirtualRegisterSPSR));
		_updateVirtualRegisters();
	}
	
	printf("\n");
	
	return true;
}

bool ARM7TDMI::_executeThumbALUOp(uint16_t opcode) {
	if ((opcode & 0xfc00) != 0x4000) { return false; }

	uint32_t op = BITFIELD_UINT32(opcode, 9, 6);
	auto rs = BITFIELD_REGISTER(opcode, 5, 3);
	auto rd = BITFIELD_REGISTER(opcode, 2, 0);

	switch (op) {
		case 0x0: {
			auto result = _aluOperation(kALUOperationAND, getRegister(rd), getRegister(rs));
			printf("AND r%u = r%u & r%u = %08x\n", rd, rd, rs, result);
			setRegister(rd, result);
			return true;
		}
		case 0x1: {
			auto result = _aluOperation(kALUOperationEOR, getRegister(rd), getRegister(rs));
			printf("EOR r%u = r%u ^ r%u = %08x\n", rd, rd, rs, result);
			setRegister(rd, result);
			return true;
		}
		case 0x8:
			printf("TST r%u & r%u\n", rd, rs);
			_aluOperation(kALUOperationAND, getRegister(rd), getRegister(rs));
			return true;
		case 0xa:
			printf("CMP r%u - r%u\n", rd, rs);
			_aluOperation(kALUOperationSUB, getRegister(rd), getRegister(rs));
			return true;
		case 0xb:
			printf("CMN r%u + r%u\n", rd, rs);
			_aluOperation(kALUOperationADD, getRegister(rd), getRegister(rs));
			return true;
		case 0xc: {
			auto result = _aluOperation(kALUOperationORR, getRegister(rd), getRegister(rs));
			printf("ORR r%u = r%u | r%u = %08x\n", rd, rd, rs, result);
			setRegister(rd, result);
			return true;
		}
		case 0xf:
			printf("MVN r%u = ~r%u\n", rd, rs);
			setRegister(rd, _aluOperation(kALUOperationMVN, getRegister(rs), 0));
			return true;
	}

	return false;
}

bool ARM7TDMI::_executeThumbHighRegisterOp(uint16_t opcode) {
	if ((opcode & 0xfc00) != 0x4400) { return false; }
		
	auto rs = static_cast<VirtualRegister>(BITFIELD_UINT32(opcode, 5, 3) | (BIT6(opcode) ? 0x8 : 0));
	auto rd = static_cast<VirtualRegister>(BITFIELD_UINT32(opcode, 2, 0) | (BIT7(opcode) ? 0x8 : 0));

	if (BIT9(opcode)) {
		if (BIT8(opcode)) {
			// BX or BLX
			if (opcode & 0x7) { return false; }
			uint32_t address = getRegister(rs);
			if (!(address & 1)) {
				clearCPSRFlags(kPSRFlagThumb);
				address &= ~3;
			} else {
				address &= ~1;
			}
			if (BIT7(opcode)) {
				printf("BLX r%u (%08x)\n", rs, address);
				_branchWithLink(address);
			} else {
				printf("BX r%u (%08x)\n", rs, address);
				branch(address);
			}
		} else {
			if (!BIT6(opcode) && !BIT7(opcode)) { return false; }

			// MOV
			auto value = getRegister(rs);
			printf("MOV r%u (%08x) to r%u\n", rs, value, rd);
			setRegister(rd, value);
			if (rd == kVirtualRegisterPC) {
				_flushPipeline();
			}
		}
	} else if (BIT8(opcode)) {
		// CMP
		if (!BIT6(opcode) && !BIT7(opcode)) { return false; }
		printf("CMP r%u - r%u\n", rd, rs);
		_aluOperation(kALUOperationSUB, getRegister(rd), getRegister(rs));
	} else {
		// ADD
		if (!BIT6(opcode) && !BIT7(opcode)) { return false; }
		auto result = _aluOperation(kALUOperationADD, getRegister(rd), getRegister(rs), false);
		printf("ADD r%u = r%u + r%u = %08x\n", rd, rd, rs, result);
		setRegister(rd, result);
	}
	
	return true;
}

uint32_t ARM7TDMI::_aluOperation(ALUOperation op, uint32_t a, uint32_t b, bool updateFlags) {
	switch (op) {
		case kALUOperationEOR: {
			uint32_t result = a ^ b;
			if (updateFlags) {
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationAND: {
			uint32_t result = a & b;
			if (updateFlags) {
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationSUB: {
			uint32_t result = a - b;
			if (updateFlags) {
				setCPSRFlags(kPSRFlagCarry, result > a);
				setCPSRFlags(kPSRFlagOverflow, (a & 0x80000000) != (b & 0x80000000) && (a & 0x80000000) != (result & 0x80000000));
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationADD: {
			uint32_t result = a + b;
			if (updateFlags) {
				setCPSRFlags(kPSRFlagCarry, result < a);
				setCPSRFlags(kPSRFlagOverflow, (a & 0x80000000) == (b & 0x80000000) && (a & 0x80000000) != (result & 0x80000000));
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationMVN: {
			assert(b == 0);
			uint32_t result = ~a;
			if (updateFlags) {
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationORR: {
			uint32_t result = a | b;
			if (updateFlags) {
				_updateZNFlags(result);
			}
			return result;
		}
		case kALUOperationBIC: {
			uint32_t result = a & ~b;
			if (updateFlags) {
				_updateZNFlags(result);
			}
			return result;
		}
		default:
			assert(false);
	}
	
	return 0;
}

ARM7TDMI::PhysicalRegister ARM7TDMI::_physicalRegister(ARM7TDMI::VirtualRegister r, bool forceUserMode) const {
	if (forceUserMode) {
		if (r == kVirtualRegisterCPSR) {
			return kPhysicalRegisterCPSR;
		} else if (r == kVirtualRegisterSPSR) {
			return kPhysicalRegisterInvalid;
		}
		return static_cast<PhysicalRegister>(kPhysicalRegisterR0 + r - kVirtualRegisterR0);
	}
	
	return _virtualRegisters[r];
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRn(uint32_t opcode) {
	return BITFIELD_REGISTER(opcode, 19, 16);
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRd(uint32_t opcode) {
	return BITFIELD_REGISTER(opcode, 15, 12);
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRs(uint32_t opcode) {
	return BITFIELD_REGISTER(opcode, 11, 8);
}

ARM7TDMI::VirtualRegister ARM7TDMI::ARMRm(uint32_t opcode) {
	return BITFIELD_REGISTER(opcode, 3, 0);
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

uint32_t ARM7TDMI::ShiftSpecial(uint32_t n, ARM7TDMI::ShiftType type, uint32_t amount, bool* carry) {
	if (amount == 0) {
		switch (type) {
			case kShiftTypeLSL:
				// no change to carry
				return n;
			case kShiftTypeLSR:
				if (carry) {
					*carry = BIT31(n);
				}
				return 0;
			case kShiftTypeASR:
				if (carry) {
					*carry = BIT31(n);
				}
				return static_cast<uint32_t>(static_cast<int32_t>(n) >> 31);
			case kShiftTypeROR:
				return (Shift(n, kShiftTypeROR, 1, carry) & 0x7fffffff) | (*carry ? 0x80000000 : 0);
		}
		
		assert(false);
		return 0;
	}
	
	return Shift(n, type, amount, carry);
}
