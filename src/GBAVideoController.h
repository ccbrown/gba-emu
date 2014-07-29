#pragma once

#include "ARM7TDMI.h"
#include "Memory.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>

#include <mutex>
#include <unordered_map>

class GBAVideoController : public MemoryInterface<uint32_t> {
	public:
		GBAVideoController(ARM7TDMI* cpu);
		virtual ~GBAVideoController();
		
		struct IOError {};
	
		virtual void load(void* destination, uint32_t address, uint32_t size) const override;
		virtual void store(uint32_t address, const void* data, uint32_t size) override;

		void render();
		void setPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);

	private:
		ARM7TDMI* const _cpu = nullptr;
	
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};

		void* _ioRegisters = nullptr;
		const size_t _ioRegistersSize = 0x60;
		
		GLuint _texture = GL_INVALID_VALUE;
		
		std::mutex _renderMutex;
		
		struct PixelCoordinate {
			PixelCoordinate(int x, int y) : x(x), y(y) {}
			int x, y;
			
			bool operator==(const PixelCoordinate& other) const {
				return x == other.x && y == other.y;
			}

			struct Hasher {
				std::size_t operator()(const PixelCoordinate& coord) const {
					return (coord.x << 16) | coord.y;
				}
			};
		};
		
		struct Pixel {
			Pixel() : red(0), green(0), blue(0) {}
			Pixel(uint8_t red, uint8_t green, uint8_t blue) : red(red), green(green), blue(blue) {}
			uint8_t red, green, blue;
		};
		
		// protected by _renderMutex
		std::unordered_map<PixelCoordinate, Pixel, PixelCoordinate::Hasher> _pixelUpdates;
};
