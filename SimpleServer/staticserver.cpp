#include "staticserver.h"

#include <iostream>
#include <unordered_map>
#include <sstream>

namespace staticserver
{

namespace {

typedef long ClientId;
uv_loop_t _loop;
uv_tcp_t _server; // stream which accepts TCP connections.
Config _config;
ClientId currentId = 0;

struct Client
{
    uv_tcp_t stream;
    char buffer[512]; // acts as both read and write (since static server)
    std::stringstream request;
};

std::unordered_map<ClientId, Client> clientMap; // todo: maybe not the best form of allocation

void _onRead(uv_stream_t* clientHandle, long nRead, const uv_buf_t* buf) {
    std::cout << "read data" << (char*)buf->base << std::endl;
    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    client.request.write(buf->base, buf->len);

    std::cout << "total data is " << client.request.str();

    // if request is finished/valid, write back the resource requested.
}

// buffer may need to be larger?
void _allocClientBuffer(uv_handle_t* clientHandle, unsigned long suggestedSize, uv_buf_t* buf) {
    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    // todo: maybe don't ignore suggested size?
    buf->base = client.buffer;
    buf->len = sizeof(Client::buffer);
}

void _onConnect(uv_stream_t* server, int status) {
    if (_config.printInfo)
        std::cout << "TCP connection starting " << status << std::endl;

    if (status != 0) return;

    // allocate a connection (read/write buffer) + stream
    ClientId clientId = currentId++;
    if (_config.printInfo) std::cout << "client assigned " << clientId << std::endl;
    clientMap.emplace(clientId, Client());
    Client& client = clientMap.at(clientId);
    uv_tcp_init(&_loop, &client.stream);
    client.stream.data = (void*)clientId; // TODO: use raw pointer to client?

    // accept this connection
    int acceptStatus = uv_accept(server, (uv_stream_t*)&client.stream);
    if (acceptStatus != 0)
    {
        if (_config.printInfo) std::cout << "failed to connect" << std::endl;
        return;
    }

    // set up read listener
    // based on the docs, it says that the user is responsible for freeing the buffer and that
    // libuv will not reuse it.
    uv_read_start((uv_stream_t*)&client.stream, _allocClientBuffer, _onRead);
}

} // namespace

void start(const Config& inConfig) {
    _config = inConfig;
    uv_loop_init(&_loop);
    if (_config.printInfo)
        std::cout << " starting server at port " << 1025 << std::endl;

    uv_tcp_init(&_loop, &_server);
    // bind this TCP connection socket to any interface on this host (0.0.0.0).
    // sockaddr_in is a specialization of sockaddr.
    sockaddr_in serverAddr;
    uv_ip4_addr("0.0.0.0", 1025, &serverAddr);
    uv_tcp_bind(&_server, (sockaddr*)&serverAddr, 0);

    const int kBacklog = 128;

    uv_listen((uv_stream_t*)&_server, kBacklog, _onConnect);

    uv_run(&_loop, UV_RUN_DEFAULT);
}

} // namespace staticserver

int main(int argc, char** argv) {
    staticserver::Config config;
    config.printInfo = true;
    staticserver::start(config);
}