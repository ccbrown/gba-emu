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
		
		enum StatusFlag : uint16_t {
			kStatusFlagVBlank                 = (1 << 0),
			kStatusFlagHBlank                 = (1 << 1),
			kStatusFlagVCounter               = (1 << 2),
			kStatusFlagVBlankIRQEnable        = (1 << 3),
			kStatusFlagHBlankIRQEnable        = (1 << 4),
			kStatusFlagVCounterMatchIRQEnable = (1 << 5),
		};
		
		uint16_t statusRegister() const { return _statusRegister; }
		void updateStatusRegister(uint16_t value);

		enum ControlFlag : uint16_t {
			kControlFlagCGBMode                = (1 <<  3),
			kControlFlagDisplayFrame           = (1 <<  4),
			kControlFlagHBlankIntervalFree     = (1 <<  5),
			kControlFlagOBJTileMapping         = (1 <<  6),
			kControlFlagForcedBlank            = (1 <<  7),
			kControlFlagBG0Enable              = (1 <<  8),
			kControlFlagBG1Enable              = (1 <<  9),
			kControlFlagBG2Enable              = (1 << 10),
			kControlFlagBG3Enable              = (1 << 11),
			kControlFlagOBJEnable              = (1 << 12),
			kControlFlagWindow0Enable          = (1 << 13),
			kControlFlagWindow1Enable          = (1 << 14),
			kControlFlagOBJWindowEnable        = (1 << 15),
		};
		
		static const uint16_t kControlMaskBGMode = 0x0007;

		uint16_t controlRegister() const { return _controlRegister; }
		void setControlRegister(uint16_t value);

	private:
		GameBoyAdvance* const _gba = nullptr;
	
		Memory<uint32_t> _paletteRAM{0x400};
		Memory<uint32_t> _videoRAM{0x18000};
		Memory<uint32_t> _objectAttributeRAM{0x400};
		
		GLuint _texture = GL_INVALID_VALUE;
		
		uint16_t _statusRegister = 0;
		uint16_t _controlRegister = 0;
		
		std::mutex _renderMutex;
		
		struct PixelCoordinate {
			PixelCoordinate(uint16_t x, uint16_t y) : x(x), y(y) {}
			uint16_t x, y;
		};

		struct Pixel {
			Pixel() : red(0), green(0), blue(0) {}
			Pixel(uint16_t packed) : red(((packed >> 10) & 0x1f) << 3), green(((packed >> 5) & 0x1f) << 3), blue((packed & 0x1f) << 3) {}
			Pixel(uint8_t red, uint8_t green, uint8_t blue) : red(red), green(green), blue(blue) {}
			uint8_t red, green, blue;
		};

		PixelCoordinate _refreshCoordinate{0, 0};

		void _updateDisplay();
		
		void _drawObjects();
		void _drawTile(int x, int y, int tile, bool isBackground, int palette = -1);
		void _drawPixel(int x, int y, const Pixel& pixel);

		int _cycleCounter = 0;
		
		// protected by _renderMutex
		Pixel* _readyPixelBuffer = nullptr;

		Pixel* _renderPixelBuffer = nullptr;
		Pixel* _drawPixelBuffer = nullptr;
		
		Pixel* _pixelBufferA = nullptr;
		Pixel* _pixelBufferB = nullptr;
		Pixel* _pixelBufferC = nullptr;
};
