#pragma once
#include <string>

struct IntPoint {
	int x;
	int y;
	IntPoint(int x, int y) : x(x), y(y) {};
};

struct IntPointDir {
	int x;
	int y;
	std::string dir;
	IntPointDir(int x, int y, std::string dir) : x(x), y(y), dir(dir) {};
};