#include "GBAVideoController.h"

#include "GameBoyAdvance.h"

#include "FixedEndian.h"

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

void GBAVideoController::_drawPixel(int x, int y, const Pixel& pixel) {
	if (x >= 240 || x < 0 || y >= 160 || y < 0) { return; }
	_drawPixelBuffer[x + y * 240] = pixel;
}

void GBAVideoController::_updateDisplay() {
	switch (_controlRegister & kControlMaskBGMode) {
		case 0:
		case 1:
		case 2:
			// TODO
			break;
		case 3: { // single frame bitmap
			for (int x = 0; x < 240; ++x) {
				for (int y = 0; y < 160; ++y) {
					auto pixel = *reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage()) + x + y * 240;
					_drawPixel(x, y, pixel);
				}
			}
			break;
		}
		case 4: // two frame bitmap
		case 5: {
			for (int x = 0; x < 240; ++x) {
				for (int y = 0; y < 160; ++y) {
					auto pixel = *reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage()) + x + y * 240 + ((_controlRegister & kControlFlagDisplayFrame) ? 0x5000 : 0);
					_drawPixel(x, y, pixel);
				}
			}
			break;
		}
		default:
			for (int x = 0; x < 240; ++x) {
				for (int y = 0; y < 160; ++y) {
					_drawPixel(x, y, Pixel(0, 0, 0));
				}
			}
	}
	
	if (_controlRegister & kControlFlagOBJEnable) {
		_drawObjects();
	}

	std::unique_lock<std::mutex> lock(_renderMutex);
	std::swap(_drawPixelBuffer, _readyPixelBuffer);
}

void GBAVideoController::_drawObjects() {
	for (int i = 0; i < 128; ++i) {
		auto attributes = *reinterpret_cast<OBJAttributes*>(reinterpret_cast<uint8_t*>(_videoRAM.storage()) + i * 8);

		if (!attributes.rotationAndScaling && attributes.displayMode) {
			// hidden
			continue;
		}

		int width = 0;
		int height = 0;

		if (attributes.shape == 0) {
			// square
			width = height = (8 << attributes.size);
		} else {
			// horizontal or vertical
			switch (attributes.size) {
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
			if (attributes.shape == 2) {
				std::swap(width, height);
			}
		}
		
		if (attributes.rotationAndScaling && attributes.displayMode) {
			width <<= 1;
			height <<= 1;
		}

		for (int row = 0; row < (height >> 3); ++row) {
			int rowTile = attributes.tileBase + (_controlRegister & kControlFlagOBJTileMapping) ? (row * (width >> 3)) : (row * 0x20);
			for (int col = 0; col < (width >> 3); ++col) {
				int tile = rowTile + col;
				_drawTile(attributes.x + (row << 3), attributes.y + (col << 3), tile, false, attributes.fullPalette ? -1 : attributes.palette);
			}
		}
	}
}

void GBAVideoController::_drawTile(int x, int y, int tile, bool isBackground, int palette) {
	auto tilesAddress = (_controlRegister & kControlMaskBGMode) < 3 ? 0x00010000 : 0x00014000;
	auto tileData = reinterpret_cast<uint8_t*>(_videoRAM.storage()) + tilesAddress + tile * (palette < 0 ? 64 : 32);
	auto paletteData = reinterpret_cast<uint16_t*>(_paletteRAM.storage()) + (isBackground ? 0 : 0x100) + (palette < 0 ? 0 : (palette << 4));
	for (int ty = 0; ty < 8; ++ty) {
		for (int tx = 0; tx < 8; ++tx) {
			int pixel = (ty << 3) + tx;
			int color = palette < 0 ? *(tileData + pixel) : ((pixel & 1) ? ((*(tileData + (pixel >> 1)) & 0xf0) >> 4) : (*(tileData + (pixel >> 1)) & 0x0f));
			if (color || isBackground) {
				_drawPixel(x + tx, y + ty, Pixel(paletteData[color]));
			}
		}
	}
}
