#include "clientsession.h"
#include "staticserver.h"

#include <iostream>
#include <fstream>
#include <memory>

namespace staticserver {

const int kBacklog = 128;
const int kPort = 1025;

Server::Server() {}

Server::~Server() {
	// free any data allocated for files.
	// go through map and free.
	for (auto& iter : _fileMap) {
		delete iter.second.rawFileData;
	}
}

bool Server::addFile(const std::string& path, const std::string& mimeType) {
	// read file.
	InMemoryFile fileEntry;

	std::ifstream fileIn(path, std::ios_base::in | std::ios_base::binary | std::ios_base::ate);
	// seek to the end to get the file size.
	fileEntry.fileSize = fileIn.tellg();

	// seek back to the beginning and read.
	fileIn.seekg(0);
	fileEntry.rawFileData = new char[fileEntry.fileSize];
	fileEntry.mimeType = mimeType;
	fileIn.read(fileEntry.rawFileData, fileEntry.fileSize);

	// add to file map.
	_fileMap.insert({path, fileEntry});

	return true;
}

void Server::start() {
	uv_loop_init(&_loop);
	uv_tcp_init(&_loop, &_serverSocket);
	_serverSocket.data = (void*)this;
    // bind this TCP connection socket to any interface on this host (0.0.0.0).
    // sockaddr_in is a specialization of sockaddr.
    sockaddr_in serverAddr;
    uv_ip4_addr("0.0.0.0", kPort, &serverAddr);
    uv_tcp_bind(&_serverSocket, (sockaddr*)&serverAddr, 0);
    uv_listen((uv_stream_t*)&_serverSocket, kBacklog, [](uv_stream_t* serverSocket, int status) {
    	Server* server = (Server*)((uv_tcp_t*)serverSocket)->data;
    	server->_onClientConnect(status);
    });

    std::cout << "listening on " << kPort << std::endl;
	uv_run(&_loop, UV_RUN_DEFAULT);
}

void Server::_onClientConnect(int status) {
	std::cout << "client connected" << std::endl;

	if (status != 0) return;

 	// I need to allocate a new client session to handle reading the http request and sending the response.
 	// the client session will need to access the filemap and loop (for tcp_init)
 	// client will also need to let the server know when it is closed...
	ClientSession* clientSession = _allocClient();
	int acceptStatus = uv_accept((uv_stream_t*)&_serverSocket, (uv_stream_t*)&clientSession->clientSocket);
    if (acceptStatus != 0)
    {
        std::cout << "failed to connect" << std::endl;
        _freeClient(clientSession);
        return;
    }

    clientSession->start([this](ClientSession* clientSession){
    	this->_freeClient(clientSession);
    });
}

ClientSession* Server::_allocClient() {
	ClientSession* clientSession = new ClientSession();
	clientSession->init(&_loop, &_fileMap);
	return clientSession;
}

void Server::_freeClient(ClientSession* clientSession) {
	delete clientSession;
}

} // namespace staticserver

int main(int argc, char** argv) {
	using namespace staticserver;
	Server server;
	server.addFile("test.jpg", "image/jpeg");
	server.start();
}