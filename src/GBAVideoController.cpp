#include "GBAVideoController.h"

GBAVideoController::GBAVideoController(ARM7TDMI* cpu) : _cpu(cpu) {
	_ioRegisters = calloc(_ioRegistersSize, 1);
	
	_cpu->mmu().attach(0x04000000, this, 0, _ioRegistersSize);
	_cpu->mmu().attach(0x05000000, &_paletteRAM, 0, _paletteRAM.size());
	_cpu->mmu().attach(0x06000000, &_videoRAM, 0, _videoRAM.size());
	_cpu->mmu().attach(0x07000000, &_objectAttributeRAM, 0, _objectAttributeRAM.size());
	
	glGenTextures(1, &_texture);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, _texture);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 240, 160, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	
	setPixel( 20,  20, 255,   0,   0);
	setPixel(220,  20,   0, 255,   0);
	setPixel(220, 140,   0,   0, 255);
	setPixel( 20, 140, 255, 255, 255);
}

GBAVideoController::~GBAVideoController() {
	free(_ioRegisters);
	glDeleteTextures(1, &_texture);
}

void GBAVideoController::load(void* destination, uint32_t address, uint32_t size) const {
	switch (address) {
		default:
			if (address + size > _ioRegistersSize) { throw IOError(); }
			memcpy(destination, reinterpret_cast<uint8_t*>(_ioRegisters) + address, size);
	}
}

void GBAVideoController::store(uint32_t address, const void* data, uint32_t size) {
	switch (address) {
		default:
			if (address + size > _ioRegistersSize) { throw IOError(); }
			memcpy(reinterpret_cast<uint8_t*>(_ioRegisters) + address, data, size);
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

void GBAVideoController::setPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
	std::unique_lock<std::mutex> lock(_renderMutex);
	_pixelUpdates[PixelCoordinate(x, y)] = Pixel(red, green, blue);
}
