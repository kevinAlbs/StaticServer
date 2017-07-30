#include <uv.h>
#include <iostream>

// uv_tcp_t "inherits" from uv_stream_t, but really only through c casts
// so instead of nice C++ inheritance we get this poop. (uv_stream_t*)tcp_stream

uv_loop_t loop;
char mem[2]; // 1 byte for reading, 1 for writing :)
uv_write_t writeReq;
uv_tcp_t server, client;
int numClients = 0;
const int kPort = 7000;
const int kMaxBacklog = 128;

// Exit on signal interrupt.

void on_read(uv_stream_t* stream, ssize_t nRead, const uv_buf_t* buf) {
	std::cout << "read " << buf->base << std::endl;
	uv_read_stop(stream);

	// return the character + 1.
	mem[1] = *(buf->base) + 1;
	uv_buf_t bufs[] = { uv_buf_init(mem + 1, 1) };
	uv_write(&writeReq, stream, bufs, 1, NULL);
	
	// close the connection
	std::cout << "todo: close cxn" << std::endl;
}

void allocReadBuffers(uv_handle_t* handle, size_t suggestedSize, uv_buf_t* buf) {
	*buf = uv_buf_init(mem, 1);
}

void onConnect(uv_stream_t* server, int status)
{
	std::cout << "connection made " << status << std::endl;

	if (!status == 0 || numClients >= 1)
	{
		std::cout << "connection unsuccessful " << std::endl;
		return;
	}
	
	// now we allocate our client (only one available)
	uv_tcp_init(&loop, &client);

	if (uv_accept(server,(uv_stream_t*)&client) == 0)
	{
		std::cout << "accepted client connection " << std::endl;
		uv_read_start((uv_stream_t*)&client, allocReadBuffers, on_read);
	}
}

int main(int argc, char** argv) {
	// TODO: First read all of the files in a directory, and mmap them.

	uv_loop_init(&loop);

	uv_tcp_init(&loop, &server);
	
	struct sockaddr_in addr;
	uv_ip4_addr("0.0.0.0", kPort, &addr);

	std::cout << "Listening for connection on port " << kPort << std::endl;

	// Bind the handle to this address.
	uv_tcp_bind(&server, (sockaddr*)&addr, 0);

	// Listen for new connections.
	// max backlog is the maximum number of queued connections before they start getting dropped.
	int r = uv_listen(reinterpret_cast<uv_stream_t*>(&server), kMaxBacklog, onConnect);
	uv_run(&loop, UV_RUN_DEFAULT);
}