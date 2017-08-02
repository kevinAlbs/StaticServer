#pragma once

#include <iostream>
#include <uv.h>
#include <functional>
#include "filemap.h"

namespace staticserver {

class ClientSession {
public:
	void init(uv_loop_t* loop, const FileMap* fileMap) {
		this->_fileMap = fileMap;
		uv_tcp_init(loop, &_clientSocket);
		_clientSocket.data = (void*)this;
	}

	void start(std::function<void(ClientSession*)> onClose) {
		_onClose = onClose;
		uv_read_start((uv_stream_t*)&_clientSocket,
			ClientSession::_allocClientBuffer,
			ClientSession::_onReadThunk);
	}

	uv_tcp_t& clientSocket() {
		return _clientSocket;
	}

private:
	static void _allocClientBuffer(
		uv_handle_t* clientHandle, unsigned long suggestedSize, uv_buf_t* buf) {
		ClientSession* clientSession = (ClientSession*)((uv_tcp_t*)clientHandle)->data;
		buf->base = new char[suggestedSize];
    	buf->len = suggestedSize;
	}

	static void _onReadThunk(uv_stream_t* clientStream, long nRead, const uv_buf_t* buf) {
		ClientSession* clientSession = (ClientSession*)((uv_tcp_t*)clientStream)->data;
		clientSession->_onRead(nRead, buf);
	}

	void _onRead(long nRead, const uv_buf_t* buf) {
		std::cout << "onread called" << std::endl;
	}

	uv_tcp_t _clientSocket;
	const FileMap* _fileMap;
	std::function<void(ClientSession*)> _onClose;
};

} // namespace staticserver