#pragma once

#include <cstring>
#include <cstdlib>
#include <stdint.h>

template <typename AddressType>
class Memory {
	public:
		Memory(AddressType size) {
			_storage = malloc(size);
		}
		
		~Memory() {
			free(_storage);
		}

		template <typename T>
		T load(AddressType address) const {
			return *reinterpret_cast<T*>(reinterpret_cast<char*>(_storage) + address);
		}

		void load(void* destination, AddressType address, AddressType size) const {
			memcpy(destination, reinterpret_cast<char*>(_storage) + address, size);
		}

		void store(AddressType address, const void* data, AddressType size) {
			memcpy(reinterpret_cast<char*>(_storage) + address, data, size);
		}
		
		void* storage() { return _storage; }

	private:
		void* _storage = nullptr;
};