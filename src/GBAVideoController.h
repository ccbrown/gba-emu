#pragma once

#include "Memory.h"

#include <OpenGL/OpenGL.h>
#include <OpenGL/glu.h>

#include <mutex>
#include <unordered_map>

class GameBoyAdvance;

class GBAVideoController {
	public:
		GBAVideoController(GameBoyAdvance* gba);
		virtual ~GBAVideoController();
	
		void cycle();

		/**
		* Can be called from any thread.
		*/
		void render();

		uint16_t currentScanline() const { return _refreshCoordinate.y; }
		void setPixel(uint16_t x, uint16_t y, uint8_t red, uint8_t green, uint8_t blue);
		
		enum StatusRegisterFlag : uint16_t {
			kStatusRegisterFlagVBlank                 = (1 << 0),
			kStatusRegisterFlagHBlank                 = (1 << 1),
			kStatusRegisterFlagVCounter               = (1 << 2),
			kStatusRegisterFlagVBlankIRQEnable        = (1 << 3),
			kStatusRegisterFlagHBlankIRQEnable        = (1 << 4),
			kStatusRegisterFlagVCounterMatchIRQEnable = (1 << 5),
		};
		
		uint16_t statusRegister() const { return _statusRegister; }
		void updateStatusRegister(uint16_t value) { _statusRegister = (_statusRegister & 0x0007) | (value & 0xfff8); }

	private:
		GameBoyAdvance* const _gba = nullptr;
	
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};
		
		GLuint _texture = GL_INVALID_VALUE;
		
		uint16_t _statusRegister = 0;
		
		std::mutex _renderMutex;
		
		struct PixelCoordinate {
			PixelCoordinate(uint16_t x, uint16_t y) : x(x), y(y) {}
			uint16_t x, y;
			
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
		
		PixelCoordinate _refreshCoordinate{0, 0};
			
		int _cycleCounter = 0;
		
		// protected by _renderMutex
		std::unordered_map<PixelCoordinate, Pixel, PixelCoordinate::Hasher> _pixelUpdates;
};
