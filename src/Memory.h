#pragma once

#include <cstring>
#include <cstdlib>
#include <stdint.h>

template <typename AddressType>
class Memory {
	public:
		enum {
			kFlagReadOnly = (1 << 0),
		};
		
		struct AccessViolation {};
		struct ReadOnlyViolation {};

		Memory(AddressType size, int flags = 0) : _size(size), _flags(flags) {
			_storage = malloc(size);
		}
		
		~Memory() {
			free(_storage);
		}
		
		AddressType size() const { return _size; }

		void load(void* destination, AddressType address, AddressType size) const {
			if (address + size > _size) { throw AccessViolation(); }
			memcpy(destination, reinterpret_cast<char*>(_storage) + address, size);
		}

		void store(AddressType address, const void* data, AddressType size) {
			if (_flags & kFlagReadOnly) { throw ReadOnlyViolation(); }
			if (address + size > _size) { throw AccessViolation(); }
			memcpy(reinterpret_cast<char*>(_storage) + address, data, size);
		}
		
		void* storage() { return _storage; }

	private:
		void* _storage = nullptr;
		AddressType _size = 0;
		int _flags = 0;
};