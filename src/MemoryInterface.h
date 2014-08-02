#pragma once

template <typename AddressType>
class MemoryInterface {
	public:
		virtual ~MemoryInterface() {}

		struct AccessViolation {};
		struct ReadOnlyViolation {};

		virtual void load(void* destination, AddressType address, AddressType size) const = 0;
		virtual void store(AddressType address, const void* data, AddressType size) = 0;
};