#pragma once

#include "MemoryInterface.h"

#include <map>

template <typename AddressType>
class MMU {
	public:
		struct AccessViolation {};
	
		void attach(AddressType address, MemoryInterface<AddressType>* memory, AddressType offset, AddressType size) {
			_attachedMemory[address] = AttachedMemory(memory, offset, size);
		}

		template <typename T>
		T load(AddressType address) {
			T ret;
			load(&ret, address, static_cast<AddressType>(sizeof(ret)));
			return ret;
		}

		void load(void* destination, AddressType address, AddressType size) {
			if (_attachedMemory.empty()) { throw AccessViolation(); }

			auto it = _attachedMemory.lower_bound(address);

			if (it->first != address) {
				if (it == _attachedMemory.begin()) {
					throw AccessViolation();
				}
				--it;
			}
			
			if (address - it->first + size > it->second.size) {
				throw AccessViolation();
			}
			
			return it->second.memory->load(destination, address - it->first + it->second.offset, size);
		}

		template <typename T>
		void store(AddressType address, T data) {
			store(address, &data, static_cast<AddressType>(sizeof(T)));
		}
		
		void store(AddressType address, const void* data, AddressType size) {
			if (_attachedMemory.empty()) { throw AccessViolation(); }

			auto it = _attachedMemory.lower_bound(address);

			if (it->first != address) {
				if (it == _attachedMemory.begin()) {
					throw AccessViolation();
				}
				--it;
			}
			
			if (address - it->first + size > it->second.size) {
				throw AccessViolation();
			}
			
			return it->second.memory->store(address - it->first + it->second.offset, data, size);
		}
		
	private:
		struct AttachedMemory {
			AttachedMemory() {}
			AttachedMemory(MemoryInterface<AddressType>* memory, AddressType offset, AddressType size)
				: memory(memory), offset(offset), size(size) {}
			
			MemoryInterface<AddressType>* memory = nullptr;
			AddressType offset = 0;
			AddressType size = 0;
		};
	
		std::map<AddressType, AttachedMemory> _attachedMemory;
};