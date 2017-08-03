#pragma once

#include <functional>
#include <iostream>
#include <sstream>
#include <uv.h>

#include "filemap.h"

namespace staticserver {

class ClientSession {
public:
	void init(uv_loop_t* loop, FileMap* fileMap);

	void start(std::function<void(ClientSession*)> onClose);

	uv_tcp_t& clientSocket();

private:

	static void _allocClientBuffer(
		uv_handle_t* clientHandle, unsigned long suggestedSize, uv_buf_t* buf);

	static void _onReadThunk(uv_stream_t* clientStream, long nRead, const uv_buf_t* buf);

	static void _onCloseThunk(uv_handle_t* clientHandle);

	static void _onWriteFinish(uv_write_t* writeReq, int status);

	static void _onTimeout(uv_timer_t* timerReq);

	void _close();

	void _onRead(long nRead, const uv_buf_t* buf);

	void _sendResponse();

	void _sendNotFound(const std::string& path);

	void _sendBadRequest(const std::string& msg);

	char _lastFour[4] = {0, 0, 0, 0}; // circular queue
	int _lastFourIter = 0; // points to one past the last read character.
	std::stringstream _request;
	int _requestLen = 0;
	uv_tcp_t _clientSocket;
	FileMap* _fileMap;
	std::function<void(ClientSession*)> _onClose;
	std::string _responseHeader;
	uv_write_t _writeReq;
	uv_buf_t* _writeBuffers;
	uv_timer_t _timerReq;
};

} // namespace staticserver