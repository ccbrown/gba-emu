#pragma once

#include "MemoryInterface.h"

#include <map>

template <typename AddressType>
class MMU : public MemoryInterface<AddressType> {
	public:
		void attach(AddressType address, MemoryInterface<AddressType>* memory, AddressType offset, AddressType size) {
			_attachedMemory[address] = AttachedMemory(memory, offset, size);
		}
		
		using typename MemoryInterface<AddressType>::AccessViolation;
		using typename MemoryInterface<AddressType>::ReadOnlyViolation;
		
		template <typename T>
		T load(AddressType address) {
			T ret;
			load(&ret, address, static_cast<AddressType>(sizeof(ret)));
			return ret;
		}

		virtual void load(void* destination, AddressType address, AddressType size) const override {
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

		virtual void store(AddressType address, const void* data, AddressType size) override {
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
			
			try {
				return it->second.memory->store(address - it->first + it->second.offset, data, size);
			} catch (ReadOnlyViolation e) {
				printf("warning: attempt to write %08x bytes to read-only memory at %08x\n", size, address);
			}
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