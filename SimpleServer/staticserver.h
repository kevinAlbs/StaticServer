#pragma once

#include "clientsession.h"
#include "filemap.h"
#include <uv.h>
#include <unordered_map>
#include <utility> // std::pair
#include <string>

namespace staticserver {

// Responsible for reading the static files and listening for TCP client connections.
class Server {
public:
	typedef std::pair<std::string, std::string> FileAndMimeType;
	Server();
	~Server();

	bool addFile(const std::string& filename, const std::string& mimeType);
	void start();

private:
	ClientSession* _allocClient();
	void _freeClient(ClientSession*);
	void _onClientConnect(int status);

	FileMap _fileMap;
	uv_loop_t _loop;
	uv_tcp_t _serverSocket;
};

} // namespace staticserver