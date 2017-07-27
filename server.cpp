#include <iostream>
#include <uv.h>
#include <memory>
#include <iomanip>
#include <string>

// It's annoying, but you need to maintain state across function calls since the uv_loop will
// call them.
uv_loop_t loop;
int64_t counter = 0;
uv_fs_t readReq;
char mem[10];
uv_buf_t readBuf;


void wait_for_a_while(uv_idle_t* handle) {
	std::cout << std::setw(4) << std::setfill('0') << counter << " ";
	// Since libuv is not threaded, an atomic is not necessary here.
	counter++;
	// Stop watching this event.
	if (counter > 10) {
		std::cout << std::endl;
		uv_idle_stop(handle);
	}
}

// This is called when the read request is fulfilled.
// Each read request is a "one-shot" request. To read more than one buffer worth
// of data, call it again.
void on_read(uv_fs_t* readReq) {
	std::cout << "File read " << std::endl;
	if (readReq->result <= 0) return;

	int len = readReq->result;
	std::string out(readBuf.base, len);

	// Read the rest of the file.
	uv_fs_read(&loop, readReq, readReq->result, &readBuf, 1, -1, on_read);
	std::cout << " " << out << std::endl;
}


// the passed request will be the same one initialized for the uv_fs_open call.
void on_open(uv_fs_t* openReq) {
	std::cout << "File opened" << std::endl;
	if (openReq->result <= 0) return;

	// Set up a read buffer.
	readBuf = uv_buf_init(mem, sizeof(mem));

	// Add a read request.
	// Use -1 for the offset to use the file pointers position.
	uv_fs_read(&loop, &readReq, openReq->result, &readBuf, 1, -1, on_read);
}

int main(int argc, char** argv) {
	std::cout << "Serving" << std::endl;

	uv_idle_t idler; // runs at the end of every event loop	

	uv_loop_init(&loop);

	// In general, to watch for an event:
	// uv_TYPE_init(&loop, &handle)
	// uv_TYPE_start(&handle, &callback)
	uv_idle_init(&loop, &idler);
	uv_idle_start(&idler, wait_for_a_while);

	uv_fs_t openReq;
	// These requests (unlike idler) are one shot deals so they don't have a separate
	// start/stop methods.
	uv_fs_open(&loop, &openReq, "test.txt", O_RDONLY, 0, on_open);

	// uv_run will block until all handlers are stopped.
	uv_run(&loop, UV_RUN_DEFAULT);

	uv_loop_close(&loop);
	return 0;
}