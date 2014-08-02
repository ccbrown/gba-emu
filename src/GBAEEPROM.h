#pragma once

#include <stdint.h>

#include "MemoryInterface.h"

class GBAEEPROM : public MemoryInterface<uint32_t> {
	public:
		typedef uint32_t AddressType;
	
		GBAEEPROM(AddressType size);
		virtual ~GBAEEPROM();
		
		struct InvalidRequest {};

		AddressType size() const { return _size; }
		
		virtual void load(void* destination, AddressType address, AddressType size) const override;
		virtual void store(AddressType address, const void* data, AddressType size) override;
		
		uint8_t* storage() { return _storage; }

	private:
		uint8_t* _storage = nullptr;
		AddressType _size = 0;
		int _addressBits = 0;

		bool _isReading = false;
		bool _isWriting = false;
		bool _isProcessingRequest = false;
		int _addressBitsReceived = 0;
		mutable uint32_t _currentAddress = 0;
		int _dataBitsReceived = 0;
		uint8_t _currentByte = 0;
};