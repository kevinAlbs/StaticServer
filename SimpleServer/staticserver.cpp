#include "staticserver.h"

#include <iostream>
#include <unordered_map>
#include <sstream>

namespace staticserver {
namespace {

typedef long ClientId;
uv_loop_t _loop;
uv_tcp_t _server; // stream which accepts TCP connections.
Config _config;
ClientId currentId = 0;
const int kMaxRequestLen = 1024;

struct Client
{
    uv_tcp_t stream;
    char buffer[512]; // acts as both read and write (since static server)
    std::stringstream request;
    int requestLen = 0;
    char lastFour[4] = {0, 0, 0, 0}; // circular queue
    int lastFourIter = 0; // points to one past the last read character.
    uv_write_t writeReq;
};

std::unordered_map<ClientId, Client> clientMap; // todo: maybe not the best form of allocation

void _badRequest(uv_stream_t* clientHandle) {
    std::cout << "bad request" << std::endl;
    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    static char* badRequestMsg = "HTTP blah blah bad request";
}

void _onWriteFinish(uv_write_t* clientHandle, int status) {
    std::cout << "write finished";
    if (status != 0) return _badRequest(clientHandle->handle);
}

void _sendResponse(uv_stream_t* clientHandle) {
    std::cout << "printing response";

    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);

    // TODO: the string stream isn't really necessary if I'm just tokenizing.
    // Really it's more useful for conversion to bool,int,double, etc.
    std::string method;
    client.request >> method;
    std::cout << " got method " << method << "|" << std::endl;
    if (method != "GET") return _badRequest(clientHandle);
    // good is on most recent stream operation
    if (!client.request.good()) return _badRequest(clientHandle);
    // TODO: check that we can read.
    std::cout << "about to get path" << std::endl;
    std::string path;
    client.request >> path;
    std::cout << "got path" << path << std::endl;

    // Write the entire file (in buffer chunks).
    static char* msg = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";

    uv_buf_t writeBuf = uv_buf_init(client.buffer, 45-6);
    strcpy(client.buffer, msg);
    uv_buf_t writeBufs[1] = { writeBuf };
    uv_write(&client.writeReq, (uv_stream_t*)&client.stream, writeBufs, 1, _onWriteFinish);

}

void _onClose(uv_handle_t* clientHandle) {
    ClientId clientId = (ClientId)clientHandle->data;
    clientMap.erase(clientId);
}

void _onRead(uv_stream_t* clientHandle, long nRead, const uv_buf_t* buf) {
    if (nRead == UV_EOF) {
        // Client ended early, just close.
        uv_close((uv_handle_t*)clientHandle, _onClose);
        return;
    }

    std::cout << "read data" << (char*)buf->base << std::endl;

    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    client.request.write(buf->base, nRead);
    client.requestLen += nRead;

    if (client.requestLen > kMaxRequestLen)
    {
        // close client and respond with 400 bad request.
        uv_read_stop(clientHandle);
        _badRequest(clientHandle);
        return;
    }

    // Update the last four character's read.
    int nLastCharsInBuf = std::min(nRead, 4l);
    for (int i = 0; i < nLastCharsInBuf; i++) {
        client.lastFour[client.lastFourIter] = buf->base[nRead - nLastCharsInBuf + i];
        client.lastFourIter = (client.lastFourIter + 1) % 4;
    }

    // TODO: can be microoptimized.
    static char endPattern[4] = {'\r', '\n', '\r', '\n'};
    bool bIsEnd = true;
    for (int i = 0; i < 4; i++) {
        int j = (client.lastFourIter + i) % 4;
        if (endPattern[i] != client.lastFour[j]) {
            bIsEnd = false;
            break;
        }
    }

    // if request is finished/valid, write back the resource requested.
    if (bIsEnd) {
        std::cout << "end detected" << std::endl;
        std::cout << client.request.str() << std::endl;
        _sendResponse(clientHandle);
    }
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