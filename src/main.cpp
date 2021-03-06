#include "GameBoyAdvance.h"

#include <stdint.h>
#include <string>
#include <fstream>
#include <streambuf>
#include <thread>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#include <GLUT/glut.h>

std::unique_ptr<GameBoyAdvance> gGBA;

static void RenderScreen() {
	glClear(GL_COLOR_BUFFER_BIT);
	
	gGBA->videoController().render();
	
	glutSwapBuffers();
}

int main(int argc, char* argv[]) {
	glutInit(&argc, argv);

	if (argc < 3) {
		printf("usage: %s bios rom\n", argv[0]);
		return 1;
	}

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(200, 150);
	glutInitWindowSize(240, 160);
	glutCreateWindow("GBA");
	glutDisplayFunc(RenderScreen);
	glutIdleFunc(RenderScreen);

	gGBA.reset(new GameBoyAdvance());

	{
		std::ifstream ifs(argv[1]);
		std::string fileContents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		gGBA->loadBIOS(fileContents.data(), fileContents.size());
	}

	{
		std::ifstream ifs(argv[2]);
		std::string fileContents((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
		gGBA->loadGamePak(fileContents.data(), fileContents.size(), 8192);
	}

	std::thread gbaThread([] {
		gGBA->run();
	});

	glutMainLoop();
	
	return 0;
}

#pragma clang diagnostic pop