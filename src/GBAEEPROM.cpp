#include "GBAEEPROM.h"

#include <cstring>
#include <cstdlib>

GBAEEPROM::GBAEEPROM(AddressType size) : _size(size), _addressBits(size <= 512 ? 6 : 14) {
	_storage = reinterpret_cast<uint8_t*>(calloc(size, 1));
}

GBAEEPROM::~GBAEEPROM() {
	free(_storage);
}
		
void GBAEEPROM::load(void* destination, AddressType address, AddressType size) const {
	if (!_isReading) {
		memset(destination, 1, size);
		return;
	}

	auto ptr = reinterpret_cast<uint16_t*>(destination);
	
	for (AddressType i = 0; i < size / 2; ++i, ++ptr) {
		*ptr = _storage[_currentAddress++];
	}
}

void GBAEEPROM::store(AddressType address, const void* data, AddressType size) {
	auto ptr = reinterpret_cast<const uint16_t*>(data);
	
	for (AddressType i = 0; i < size / 2; ++i, ++ptr) {
		auto bit = *ptr;
		
		if (!_isProcessingRequest) {
			// was waiting on first 1 bit of request
			if (bit) {
				_isProcessingRequest = true;
				_isReading = false;
				_isWriting = false;
			}
			continue;
		}

		if (!_isWriting && !_isReading) {
			// was waiting on second 1 or 0 bit
			(bit ? _isReading : _isWriting) = true;
			_currentAddress = 0;
			_addressBitsReceived = 0;
			_currentByte = 0;
			_dataBitsReceived = 0;
			continue;
		}

		if (_addressBitsReceived < _addressBits) {
			// was waiting on more address bits
			_currentAddress = (_currentAddress << 1) | (bit ? 1 : 0);
			if (++_addressBitsReceived == _addressBits) {
				// done with the address. go ahead and translate it into bytes
				_currentAddress <<= 3;
			}
			continue;
		}
		
		if (_isWriting && _dataBitsReceived < 64) {
			// was waiting on more data bits
			_currentByte = (_currentByte << 1) | (bit ? 1 : 0);
			if ((++_dataBitsReceived & 0x7) == 0) {
				// done with this byte
				_storage[_currentAddress++] = _currentByte;
			}
			continue;
		}
		
		// done with the request. this bit terminates it (not sure if the value is supposed to indicate something)
		_isProcessingRequest = false;
	}
}
