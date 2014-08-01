#include "GBAVideoController.h"

#include "GameBoyAdvance.h"

#include "FixedEndian.h"
#include "BIT_MACROS.h"

GBAVideoController::GBAVideoController(GameBoyAdvance* gba) : _gba(gba) {
	_gba->cpu().mmu().attach(0x05000000, &_paletteRAM, 0, _paletteRAM.size());
	_gba->cpu().mmu().attach(0x06000000, &_videoRAM, 0, _videoRAM.size());
	_gba->cpu().mmu().attach(0x07000000, &_objectAttributeRAM, 0, _objectAttributeRAM.size());
	
	glGenTextures(1, &_texture);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 240, 160, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	_pixelBufferA = reinterpret_cast<Pixel*>(calloc(240 * 160, sizeof(Pixel)));
	_pixelBufferB = reinterpret_cast<Pixel*>(calloc(240 * 160, sizeof(Pixel)));
	_pixelBufferC = reinterpret_cast<Pixel*>(calloc(240 * 160, sizeof(Pixel)));
	
	_renderPixelBuffer  = _pixelBufferA;
	_drawPixelBuffer  = _pixelBufferB;
	_readyPixelBuffer = _pixelBufferC;
}

GBAVideoController::~GBAVideoController() {
	glDeleteTextures(1, &_texture);
	free(_pixelBufferA);
	free(_pixelBufferB);
	free(_pixelBufferC);
}

void GBAVideoController::cycle() {
	if (++_cycleCounter < 4) {
		return;
	} else {
		_cycleCounter = 0;
	}
	
	// refresh
	
	if (_refreshCoordinate.x >= 240 || _refreshCoordinate.y >= 160 || (_controlRegister & kControlFlagForcedBlank)) {
		// blank
	} else if (_refreshCoordinate.x == 239 && _refreshCoordinate.y == 159) {
		// cheat and just refresh the entire display at once
		_updateDisplay();
	}
	
	// increment the coordinate
	
	if (++_refreshCoordinate.x >= 308) {
		_refreshCoordinate.x = 0;
		if (++_refreshCoordinate.y >= 228) {
			_refreshCoordinate.y = 0;
		}
	}

	// update flags

	if (_refreshCoordinate.x >= 240) {
		_statusRegister |= kStatusFlagHBlank;
	}

	if (_refreshCoordinate.y >= 160) {
		_statusRegister |= kStatusFlagVBlank;
	}
	
	if (((_statusRegister & 0xf0) >> 8) == _refreshCoordinate.y) {
		_statusRegister |= kStatusFlagVCounter;
	}

	// fire interrupts
	
	uint16_t interrupts = 0;

	if (_refreshCoordinate.x == 240 && (_statusRegister & kStatusFlagHBlankIRQEnable)) {
		interrupts |= GameBoyAdvance::kInterruptHBlank;
	}

	if (_refreshCoordinate.x == 0) {
		if (_refreshCoordinate.y == 160 && (_statusRegister & kStatusFlagVBlankIRQEnable)) {
			interrupts |= GameBoyAdvance::kInterruptVBlank;
		}
	
		if (((_statusRegister & 0xf0) >> 8) == _refreshCoordinate.y && (_statusRegister & kStatusFlagVCounterMatchIRQEnable)) {
			interrupts |= GameBoyAdvance::kInterruptVCounterMatch;
		}
	}
	
	if (interrupts) {
		_gba->interruptRequest(interrupts);
	}
}

void GBAVideoController::render() {
	glEnable(GL_TEXTURE_2D);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);

	{
		std::unique_lock<std::mutex> lock(_renderMutex);
		std::swap(_readyPixelBuffer, _renderPixelBuffer);
	}
	
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 240, 160, GL_RGB, GL_UNSIGNED_BYTE, _renderPixelBuffer);

	glBegin(GL_TRIANGLE_STRIP);

	glTexCoord2f(0.0, 1.0);
	glVertex2f(-1.0, -1.0);

	glTexCoord2f(0.0, 0.0);
	glVertex2f(-1.0,  1.0);

	glTexCoord2f(1.0, 1.0);
	glVertex2f( 1.0, -1.0);

	glTexCoord2f(1.0, 0.0);
	glVertex2f( 1.0,  1.0);

	glEnd();
}

void GBAVideoController::updateStatusRegister(uint16_t value) {
	_statusRegister = (_statusRegister & 0x0007) | (value & 0xfff8);
	printf("video status register update: %08x\n", _statusRegister & 0xfff8);
}

void GBAVideoController::setControlRegister(uint16_t value) {
	if (_controlRegister != value) {
		_controlRegister = value;
		printf("video control register update: %08x\n", value);
	}
}

void GBAVideoController::setBackground(int n, const Background& background) {
	printf("background %d update: %04x\n", n, static_cast<uint16_t>(background));
	_backgrounds[n] = background;
}

void GBAVideoController::_drawPixel(const Window& window, int x, int y, const Pixel& pixel) {
	if (x >= window.width || x < 0 || y >= window.height || y < 0) { return; }
	_drawPixelBuffer[window.x + x + (window.y + y) * 240] = pixel;
}

void GBAVideoController::_drawBitmap(const Window& window, int x, int y, int w, int h, uint32_t frameAddress, int frameWidth) {
	for (int bx = 0; bx < w; ++bx) {
		for (int by = 0; by < h; ++by) {
			auto pixel = *reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage() + frameAddress) + bx + by * frameWidth;
			_drawPixel(window, x + bx, y + by, pixel);
		}
	}
}

void GBAVideoController::_drawTileBackground(const Window& window, int bg, bool textMode) {
	auto& background = _backgrounds[bg];
	
	if (textMode) {
		_drawTextModeBackgroundMap(window, _backgroundXOffsets[bg], _backgroundYOffsets[bg], background.mapBase, background.tiles * 0x4000, background.isFullPalette);
	} else {
		// TODO
		for (int bgx = 0; bgx < window.width; ++bgx) {
			for (int bgy = 0; bgy < window.height; ++bgy) {
				_drawPixel(window, window.x + bgx, window.y + bgy, Pixel(100, 150, 100));
			}
		}
	}
}

void GBAVideoController::_drawTextModeBackgroundMap(const Window& window, int x, int y, uint32_t address, uint32_t tiles, bool isFullPalette) {
	auto entries = reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage() + address);

	for (int tx = 0; tx < 32; ++tx) {
		for (int ty = 0; ty < 32; ++ty) {
			auto entry = entries[tx + ty * 32];
			auto tile = BITFIELD_UINT16(entry, 9, 0);
			auto flipHorizontally = BIT10(entry);
			auto flipVertically = BIT11(entry);
			auto palette = isFullPalette ? -1 : BITFIELD_UINT16(entry, 15, 12);
			_drawTile(window, x + tx * 8, y + ty * 8, tiles + tile * (isFullPalette ? 64 : 32), true, palette, flipHorizontally, flipVertically);
		}
	}
}

void GBAVideoController::_updateDisplay() {
	Window window(0, 0, 240, 160);
	
	for (int x = 0; x < 240; ++x) {
		for (int y = 0; y < 160; ++y) {
			_drawPixel(window, x, y, Pixel(*reinterpret_cast<LittleEndian<uint16_t>*>(_paletteRAM.storage())));
		}
	}
	
	switch (_controlRegister & kControlMaskBGMode) {
		case 0:
			// TODO: respect priority
			if (_controlRegister & kControlFlagBG0Enable) {
				_drawTileBackground(window, 0, true);
			}
			if (_controlRegister & kControlFlagBG1Enable) {
				_drawTileBackground(window, 1, true);
			}
			if (_controlRegister & kControlFlagBG2Enable) {
				_drawTileBackground(window, 2, true);
			}
			if (_controlRegister & kControlFlagBG3Enable) {
				_drawTileBackground(window, 3, true);
			}
		case 1:
		case 2:
			// TODO
			break;
		case 3:
			_drawBitmap(window, 0, 0, 240, 160, 0, 240);
			break;
		case 4:
			_drawBitmap(window, 0, 0, 240, 160, (_controlRegister & kControlFlagDisplayFrame) ? 0x5000 : 0, 240);
			break;
		case 5:
			_drawBitmap(window, 0, 0, 160, 128, (_controlRegister & kControlFlagDisplayFrame) ? 0x5000 : 0, 160);
			break;
		default:
			for (int x = 0; x < 240; ++x) {
				for (int y = 0; y < 160; ++y) {
					_drawPixel(window, x, y, Pixel(0, 0, 0));
				}
			}
	}
	
	if (_controlRegister & kControlFlagOBJEnable) {
		_drawObjects(window);
	}

	std::unique_lock<std::mutex> lock(_renderMutex);
	std::swap(_drawPixelBuffer, _readyPixelBuffer);
}

void GBAVideoController::_drawObjects(const Window& window) {	
	for (int i = 0; i < 128; ++i) {
		uint16_t attributes0 = *reinterpret_cast<LittleEndian<uint16_t>*>(reinterpret_cast<uint8_t*>(_objectAttributeRAM.storage()) + i * 8);
		uint16_t attributes1 = *reinterpret_cast<LittleEndian<uint16_t>*>(reinterpret_cast<uint8_t*>(_objectAttributeRAM.storage()) + i * 8 + 2);
		uint16_t attributes2 = *reinterpret_cast<LittleEndian<uint16_t>*>(reinterpret_cast<uint8_t*>(_objectAttributeRAM.storage()) + i * 8 + 4);

		if (!BIT8(attributes0) && BIT9(attributes0)) {
			// hidden
			continue;
		}

		auto x = BITFIELD_UINT32(attributes1, 8, 0);
		auto y = BITFIELD_UINT32(attributes0, 7, 0);

		int width = 0;
		int height = 0;

		auto shape = BITFIELD_UINT32(attributes0, 15, 14);
		auto size  = BITFIELD_UINT32(attributes1, 15, 14);

		if (!shape) {
			// square
			width = height = (8 << size);
		} else {
			// horizontal or vertical
			switch (size) {
				case 0:
					width  = 16;
					height =  8;
					break;
				case 1:
					width  = 32;
					height =  8;
					break;
				case 2:
					width  = 32;
					height = 16;
					break;
				case 3:
					width  = 64;
					height = 32;
					break;
			}
			if (shape == 2) {
				std::swap(width, height);
			}
		}
		
		if (BIT8(attributes0) && BIT9(attributes0)) {
			width <<= 1;
			height <<= 1;
		}

		auto tilesAddress = (_controlRegister & kControlMaskBGMode) < 3 ? 0x00010000 : 0x00014000;
		auto tileBase = BITFIELD_UINT32(attributes2, 9, 0);

		for (int row = 0; row < (height >> 3); ++row) {
			int rowTile = tileBase + (
				(_controlRegister & kControlFlagOBJTileMapping)
					// one dimensional mapping
					? (row * (width >> (BIT13(attributes0) ? 2 : 3)))
					// two dimensional mapping
					: (row * 0x20)
			);
			for (int col = 0; col < (width >> 3); ++col) {
				int tile = rowTile + (col << (BIT13(attributes0) ? 1 : 0));
				_drawTile(window, x + (col << 3), y + (row << 3), tilesAddress + tile * 32, false, BIT13(attributes0) ? -1 : BITFIELD_UINT32(attributes2, 15, 12));
			}
		}
	}
}

void GBAVideoController::_drawTile(const Window& window, int x, int y, uint32_t address, bool isBackground, int palette, bool flipHorizontally, bool flipVertically) {
	auto tileData = _videoRAM.storage() + address;
	auto paletteData = reinterpret_cast<LittleEndian<uint16_t>*>(_paletteRAM.storage()) + (isBackground ? 0 : 0x100) + (palette < 0 ? 0 : (palette << 4));
	for (int ty = 0; ty < 8; ++ty) {
		for (int tx = 0; tx < 8; ++tx) {
			int pixel = ty * 8 + tx;
			int color = palette < 0 ? *(tileData + pixel) : ((pixel & 1) ? ((*(tileData + (pixel >> 1)) & 0xf0) >> 4) : (*(tileData + (pixel >> 1)) & 0x0f));
			if (color || isBackground) {
				_drawPixel(window, x + (flipHorizontally ? (8 - tx) : tx), y + (flipVertically ? (8 - ty) : ty), Pixel(paletteData[color]));
			}
		}
	}
}

GBAVideoController::Background::Background(uint16_t data)
	: priority(BITFIELD_UINT16(data, 1, 0))
	, tiles(BITFIELD_UINT16(data, 3, 2))
	, isMosaic(BIT6(data))
	, isFullPalette(BIT7(data))
	, mapBase(BITFIELD_UINT16(data, 12, 8))
	, wrapAround(BIT13(data))
	, screenSize(BITFIELD_UINT16(data, 15, 14))
{}
			
GBAVideoController::Background::operator uint16_t() const {
	return (screenSize << 14) | ((wrapAround ? 1 : 0) << 13) | (mapBase << 8) | ((isFullPalette ? 1 : 0) << 7) | ((isMosaic ? 1 : 0) << 6) | (tiles << 2) | priority;
}
