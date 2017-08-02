#include "staticserver.h"

#include <fstream>
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
char* header;

int fileSize = 0;
char* rawFileData;
uv_buf_t* fileBuffers;
int fileBufferCount = 0;
int numFileBuffers = 0;

class Client
{
public:
    uv_tcp_t stream;
    char buffer[512]; // acts as both read and write (since static server)
    std::stringstream request;
    int requestLen = 0;
    char lastFour[4] = {0, 0, 0, 0}; // circular queue
    int lastFourIter = 0; // points to one past the last read character.
    uv_write_t writeReq;

    void sendNotFound() {
        std::cout << "sending 404" << std::endl;
        char* notFoundMsg = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        strcpy(buffer, notFoundMsg);
        uv_buf_t writeBufs[] = { uv_buf_init(this->buffer, strlen(notFoundMsg)) };
        uv_write(&writeReq, (uv_stream_t*)&stream, writeBufs, 1, [](uv_write_t* writeReq, int status){
            std::cout << "finished" << std::endl;
            // TODO: get client from writeReq->handle pointer.
            // You get the idea.
            uv_close((uv_handle_t))
        });
    }
};

std::unordered_map<ClientId, Client> clientMap; // todo: maybe not the best form of allocation

void _badRequest(uv_stream_t* clientHandle) {
    std::cout << "bad request" << std::endl;
    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    static char* badRequestMsg = "HTTP blah blah bad request";
}

void _onClose(uv_handle_t* clientHandle) {
    ClientId clientId = (ClientId)clientHandle->data;
    clientMap.erase(clientId);
}

void _onWriteFinish(uv_write_t* clientHandle, int status) {
    std::cout << "write finished" << std::endl;
    if (status != 0) return _badRequest(clientHandle->handle);
    ClientId clientId = (ClientId)clientHandle->handle->data;
    std::cout << " client data " << clientId << std::endl;
    Client& client = clientMap.at(clientId);
    uv_close((uv_handle_t*)&client.stream, _onClose);
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

    // copy the write buffers before sending?
    uv_buf_t* fileBufsCopy = new uv_buf_t[numFileBuffers + 1];
    for (int i = 0; i < numFileBuffers + 1; i++) {
        fileBufsCopy[i] = fileBuffers[i];
    }

    // Write the entire file (in buffer chunks).
    uv_write(&client.writeReq, (uv_stream_t*)&client.stream, fileBufsCopy, numFileBuffers + 1, _onWriteFinish);

}

void _onRead(uv_stream_t* clientHandle, long nRead, const uv_buf_t* buf) {
    if (nRead == UV_EOF) {
        // Client ended early, just close.
        std::cout << "eof detected, closing" << std::endl;
        uv_close((uv_handle_t*)clientHandle, _onClose);
        return;
    }

    std::cout << "read data" << (char*)buf->base << std::endl;

    ClientId clientId = (ClientId)clientHandle->data;
    Client& client = clientMap.at(clientId);
    client.request.write(buf->base, nRead);
    client.requestLen += nRead;

    std::string str(fileBuffers[0].base, fileBuffers[0].len);
    std::cout << "fileBuffers[0]=" << str << std::endl;

    std::string str2(fileBuffers[1].base, fileBuffers[1].len);
    std::cout << "fileBuffers[1]=" << str2 << std::endl;

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
    // buf->base = client.buffer;
    // buf->len = sizeof(Client::buffer);
    buf->base = new char[suggestedSize];
    buf->len = suggestedSize;

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

    std::ifstream fileIn("test.jpg", std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
    fileSize = fileIn.tellg();
    std::cout << "file is " << fileSize << " bytes " << std::endl;

    // seek to the beginning.
    fileIn.seekg(0);

    rawFileData = new char[fileSize];
    // now read the entire file.
    fileIn.read(rawFileData, fileSize);

    //fileSize = 100;
    //rawFileData = "this is a test";
    //fileSize = strlen(rawFileData);

    // now create buffers by dividing it up.
    // TODO: please fix this crap.
    numFileBuffers = ((fileSize + 511) / 512);
    std::cout << "numFileBuffers=" << numFileBuffers << std::endl;
    fileBuffers = new uv_buf_t[numFileBuffers + 1]; // 1 for header.
    for (int i = 0; i < numFileBuffers; i++) {
        int bufferSize = 512;
        if (i == numFileBuffers - 1) {
            // size may not be 512.
            bufferSize = fileSize % 512;
            std::cout << "buffer size of last is " << bufferSize << std::endl; // should be 385.
        }
        fileBuffers[i + 1] = uv_buf_init(rawFileData + (i * 512), bufferSize);
    }

    
    // numFileBuffers = 1;
    // char* str = "this is a test";
    // fileBuffers[1] = uv_buf_init(str, strlen(str));
    // fileSize = strlen(str);

    std::stringstream headerStream; 
    headerStream << "HTTP/1.1 200 OK\r\nContent-Type: image/jpg \r\nContent-Length:" << fileSize << "\r\n\r\n";
    std::string headerStr = headerStream.str();
    char* header = (char*)headerStr.c_str();
    // Why was this messing up?
    // char* header = (char*)headerStream.str().c_str();
    std::cout << "header is " << headerStr << std::endl;
    fileBuffers[0] = uv_buf_init(header, strlen(header)); // DON'T INCLUDE THE NULL CHARACTER!

    std::string str3(fileBuffers[0].base, fileBuffers[0].len);
    std::cout << "fileBuffers[0]=" << str3 << std::endl;

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