#include <iostream>

class Test {
public:
	void freeMe() {
		delete this;
	}
};

int main(int argc, char** argv) {
	Test* obj = new Test();
	obj->freeMe();
	std::cout << "done" << std::endl;
}