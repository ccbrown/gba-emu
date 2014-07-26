#include "GameBoyAdvance.h"

#include <stdint.h>
#include <string>
#include <fstream>
#include <streambuf>

int main(int argc, const char* argv[]) {
	if (argc < 3) {
		printf("usage: %s bios rom\n", argv[0]);
		return 1;
	}
	
	GameBoyAdvance gba;

	{
		std::ifstream ifs(argv[1]);
		std::string fileContents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		gba.loadBIOS(fileContents.data(), fileContents.size());
	}

	{
		std::ifstream ifs(argv[2]);
		std::string fileContents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		gba.loadGamePak(fileContents.data(), fileContents.size());
	}

	gba.hardReset();
	gba.run();
	
	return 0;
}