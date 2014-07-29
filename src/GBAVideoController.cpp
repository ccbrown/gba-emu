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
}

GBAVideoController::~GBAVideoController() {
	glDeleteTextures(1, &_texture);
}

void GBAVideoController::cycle() {
	if (++_cycleCounter < 4) {
		return;
	} else {
		_cycleCounter = 0;
	}
	
	// refresh the pixel

	if (_refreshCoordinate.x >= 240 || _refreshCoordinate.y >= 160 || (_controlRegister & kControlFlagForcedBlank)) {
		// blank
		_refreshPixel(_refreshCoordinate, Pixel(255, 255, 255));
	} else {
		switch (_controlRegister & kControlMaskBGMode) {
			case 0:
				_refreshPixel(_refreshCoordinate, Pixel(70, 0, 0));
				break;
			case 1:
				_refreshPixel(_refreshCoordinate, Pixel(0, 70, 0));
				break;
			case 2:
				_refreshPixel(_refreshCoordinate, Pixel(0, 0, 70));
				break;
			case 3: { // single frame bitmap
				auto pixel = *reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage()) + _refreshCoordinate.x + _refreshCoordinate.y * 240;
				_refreshPixel(_refreshCoordinate, pixel);
				break;
			}
			case 4: // two frame bitmap
			case 5: {
				auto pixel = *reinterpret_cast<LittleEndian<uint16_t>*>(_videoRAM.storage()) + _refreshCoordinate.x + _refreshCoordinate.y * 240 + ((_controlRegister & kControlFlagDisplayFrame) ? 0x5000 : 0);
				_refreshPixel(_refreshCoordinate, pixel);
				break;
			}
			default:
				_refreshPixel(_refreshCoordinate, Pixel(0, 0, 0));
		}
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
	
	if ((_statusRegister & 0xf0) == _refreshCoordinate.y) {
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
	
		if ((_statusRegister & 0xf0) == _refreshCoordinate.y && (_statusRegister & kStatusFlagVCounterMatchIRQEnable)) {
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

	decltype(_pixelUpdates) pixelUpdates;
	
	{
		std::unique_lock<std::mutex> lock(_renderMutex);
		pixelUpdates.swap(_pixelUpdates);
	}
	
	for (auto& kv : pixelUpdates) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, kv.first.x, kv.first.y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, &kv.second);
	}

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

void GBAVideoController::_refreshPixel(const PixelCoordinate& coordinate, const Pixel& pixel) {
	std::unique_lock<std::mutex> lock(_renderMutex);
	_pixelUpdates[coordinate] = pixel;
}
