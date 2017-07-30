#include "StaticServer.h"

#include <iostream>
#include <unordered_map>

namespace staticserver
{

namespace {

uv_loop_t _loop;
uv_tcp_t _server; // stream which accepts TCP connections.
Config _config;
uint32_t currentId = 0;

struct Client
{
    uv_tcp_t stream;
    char buffer[512]; // acts as both read and write (since static server)
};

std::unordered_map<uint32_t, Client> clientMap; // todo: maybe not the best form of allocation

// void _onRead(uv_stream_t* client, ssize_t nRead, ) {}

void _onConnect(uv_stream_t* server, int status) {
    if (_config.printInfo)
        std::cout << "TCP connection starting " << status << std::endl;

    if (status != 0) return;

    // allocate a connection (read/write buffer) + stream
    uint32_t clientId = currentId++;
    if (_config.printInfo) std::cout << "client assigned " << clientId << std::endl;
    clientMap.emplace(clientId, Client());
    Client& client = clientMap.at(clientId);
    uv_tcp_init(&_loop, &client.stream);

    // accept this connection
    int acceptStatus = uv_accept(server, (uv_stream_t*)&client.stream);
    if (acceptStatus != 0)
    {
        if (_config.printInfo) std::cout << "failed to connect" << std::endl;
        return;
    }

    // set up read listener
    // uv_read_start((uv_stream_t*)&client.stream, _onRead);
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
    staticserver::start(config);
}