#pragma once

#include <uv.h>
#include <functional>
#include "filemap.h"

namespace staticserver {

class ClientSession {
public:
	void init(uv_loop_t* loop, const FileMap* fileMap) {
		this->_fileMap = fileMap;
		uv_tcp_init(loop, &clientSocket);
	}

	void start(std::function<void(ClientSession*)> onClose) {
		_onClose = onClose;
	}

	uv_tcp_t clientSocket;
private:
	const FileMap* _fileMap;
	std::function<void(ClientSession*)> _onClose;
};

} // namespace staticserver