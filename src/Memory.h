#pragma once

#include <cstring>
#include <cstdlib>
#include <stdint.h>

#include "MemoryInterface.h"

template <typename AddressType>
class Memory : public MemoryInterface<AddressType> {
	public:
		enum {
			kFlagReadOnly = (1 << 0),
		};

		Memory(AddressType size, int flags = 0) : _size(size), _flags(flags) {
			_storage = reinterpret_cast<uint8_t*>(calloc(size, 1));
		}
		
		~Memory() {
			free(_storage);
		}
		
		using typename MemoryInterface<AddressType>::AccessViolation;
		using typename MemoryInterface<AddressType>::ReadOnlyViolation;

		AddressType size() const { return _size; }
		
		void load(void* destination, AddressType address, AddressType size) const override {
			if (address + size > _size) { throw AccessViolation(); }
			memcpy(destination, _storage + address, size);
		}

		void store(AddressType address, const void* data, AddressType size) override {
			if (_flags & kFlagReadOnly) { throw ReadOnlyViolation(); }
			if (address + size > _size) { throw AccessViolation(); }
			memcpy(_storage + address, data, size);
		}
		
		uint8_t* storage() { return _storage; }

	private:
		uint8_t* _storage = nullptr;
		AddressType _size = 0;
		int _flags = 0;
};