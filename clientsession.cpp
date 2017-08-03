#include "clientsession.h"

#include "config.h"

namespace staticserver {

namespace {

ClientSession* handleToClientSession(void* handle) {
	return (ClientSession*)((uv_tcp_t*)handle)->data;
}

int getNumBuffers(int dataSize) {
	const int kBufferSize = STATICSERVER_WRITE_BUFFER_SIZE;
	return (dataSize + (kBufferSize - 1)) / kBufferSize;
}

// Assumes startingBuffer is an array of buffers of sufficient size.
void putDataInBuffers(char* data, int dataSize, uv_buf_t* startingBuffer) {
	const int kBufferSize = STATICSERVER_WRITE_BUFFER_SIZE;
	int dataOffset = 0;
	int numBuffers = getNumBuffers(dataSize);
	int lastBufferSize = dataSize % kBufferSize;
	if (lastBufferSize == 0) lastBufferSize = kBufferSize;

	for (int i = 0; i < numBuffers - 1; i++) {
		startingBuffer[i].base = data + dataOffset;
		startingBuffer[i].len = kBufferSize;
		dataOffset += kBufferSize;
	}

	startingBuffer[numBuffers - 1].base = data + dataOffset;
	startingBuffer[numBuffers - 1].len = lastBufferSize;
}

} // namespace

void ClientSession::init(uv_loop_t* loop, FileMap* fileMap) {
	this->_fileMap = fileMap;
	uv_tcp_init(loop, &_clientSocket);
	uv_timer_init(loop, &_timerReq);
	_clientSocket.data = (void*)this;
	_timerReq.data = (void*)this;
}

void ClientSession::start(std::function<void(ClientSession*)> onClose) {
	_onClose = onClose;
	uv_read_start((uv_stream_t*)&_clientSocket, _allocClientBuffer, _onReadThunk);
	uv_timer_start(&_timerReq, _onTimeout, STATICSERVER_TIMEOUT, 0);
}

uv_tcp_t& ClientSession::clientSocket() {
	return _clientSocket;
}

// These static thunk methods are necessary to retrieve the ClientSession object to call
// member functions.
/* static */ void ClientSession::_allocClientBuffer(
	uv_handle_t* clientHandle, unsigned long suggestedSize, uv_buf_t* buf) {
	// TODO: we could avoid an allocation if we statically allocate here, but I don't
	// yet want to make the sizeof(ClientSession) huge.
	ClientSession* clientSession = handleToClientSession(clientHandle);
	buf->base = new char[suggestedSize];
	buf->len = suggestedSize;
}

/* static */ void ClientSession::_onReadThunk(uv_stream_t* clientStream, long nRead, const uv_buf_t* buf) {
	ClientSession* clientSession = handleToClientSession(clientStream);
	clientSession->_onRead(nRead, buf);
}

/* static */ void ClientSession::_onCloseThunk(uv_handle_t* clientHandle) {
	std::cout << "closing" << std::endl;
	ClientSession* clientSession = handleToClientSession(clientHandle);
	clientSession->_onClose(clientSession);
}

/* static */ void ClientSession::_onWriteFinish(uv_write_t* writeReq, int status) {
	ClientSession* clientSession = handleToClientSession(writeReq->handle);
	std::cout << "Write finished" << std::endl;
	delete[] clientSession->_writeBuffers;
	clientSession->_close();
}

/* static */ void ClientSession::_onTimeout(uv_timer_t* timerReq) {
	ClientSession* clientSession = handleToClientSession(timerReq);
	std::cout << "timed out" << std::endl;
	clientSession->_close();
}

void ClientSession::_close() {
	// uv_close will cancel any pending write requests, passing UV_ECANCELED. So no need to worry
	// about leaked write buffers.
	uv_timer_stop(&_timerReq);
	// _close can be called multiple times if the timer is triggered midst write. In this case
	// we should not close again.
	if (uv_is_closing((uv_handle_t*)&_clientSocket)) return;
	uv_close((uv_handle_t*)&_clientSocket, _onCloseThunk);
}

void ClientSession::_onRead(long nRead, const uv_buf_t* buf) {
	std::cout << "onread called" << std::endl;

	// If client ended early, close.
	if (nRead == UV_EOF) return _close();

	_request.write(buf->base, nRead);

	// As soon as we read the first 3 characters, make sure this is a GET request.
	// TODO: this is a bit hacky. Probably unnecessary if we implement a timer.
	if (_requestLen < 3 && nRead > 0) {
		if (_request.get() != 'G' || _request.get() != 'E' || _request.get() != 'T')
			return _sendBadRequest("Method not supported");

		for(int i = 0; i < 3; i++) _request.unget();
	}

	_requestLen += nRead;

	std::cout << "read data" << (char*)buf->base << std::endl;
	
	if (_requestLen > STATICSERVER_MAX_REQUEST_LEN) return _sendBadRequest("Request too long");

	// Update the last four character's read.
	int nLastCharsInBuf = std::min(nRead, 4l);
	for (int i = 0; i < nLastCharsInBuf; i++) {
	    _lastFour[_lastFourIter] = buf->base[nRead - nLastCharsInBuf + i];
	    _lastFourIter = (_lastFourIter + 1) % 4;
	}

	// TODO: could be microoptimized.
	static char endPattern[4] = {'\r', '\n', '\r', '\n'};
	bool bIsEnd = true;
	for (int i = 0; i < 4; i++) {
	    int j = (_lastFourIter + i) % 4;
	    if (endPattern[i] != _lastFour[j]) {
	        bIsEnd = false;
	        break;
	    }
	}

	// If request is finished and valid, write back the resource requested.
	if (bIsEnd) _sendResponse();
}

void ClientSession::_sendResponse() {
	// parse the request.
	std::cout << "printing response";

	// TODO: the stringstream probably isn't necessary.
	std::string method, path;
	_request >> method;
	
	// good() is on most recent stream operation
	if (!_request.good()) return _sendBadRequest("HTTP request malformed");
	if (method != "GET") return _sendBadRequest("Method not supported");

	_request >> path;
	std::cout << "got path" << path << std::endl;

	auto fileIter = _fileMap->find(path);
	if (fileIter == _fileMap->end()) return _sendNotFound(path);

	std::cout << "ok sending the file" << std::endl;

	InMemoryFile& fileEntry = fileIter->second;

	std::stringstream headerStream;
	headerStream << "HTTP/1.1 200 OK\r\nContent-Type: " << fileEntry.mimeType
		<< " \r\nContent-Length:" << fileEntry.fileSize << "\r\n\r\n";

	_responseHeader = std::move(headerStream.str());

	// Set up the write buffers for the header and body.
	int headerNumBuffers = getNumBuffers(_responseHeader.size());
	int bodyNumBuffers = getNumBuffers(fileEntry.fileSize);
	std::cout << "bodyNumBuffers=" << bodyNumBuffers << std::endl;
	_writeBuffers = new uv_buf_t[headerNumBuffers + bodyNumBuffers];
	putDataInBuffers((char*)_responseHeader.data(), _responseHeader.size(), _writeBuffers);
	putDataInBuffers(fileEntry.rawFileData, fileEntry.fileSize, _writeBuffers + headerNumBuffers);

	std::cout << "writing file " << std::endl;
	// Write the entire file (in buffer chunks).
	uv_write(&_writeReq, (uv_stream_t*)&_clientSocket, _writeBuffers, headerNumBuffers + bodyNumBuffers, _onWriteFinish);
}

void ClientSession::_sendNotFound(const std::string& path) {
	std::stringstream responseStream;
	responseStream << "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: "
		<< (14 + path.size()) << "\r\n\r\n404 " << path << " not found";
	// todo: _responseHeader now contains body...
	_responseHeader = responseStream.str();
	int numBuffers = getNumBuffers(_responseHeader.size());
	_writeBuffers = new uv_buf_t[numBuffers];
	putDataInBuffers((char*)_responseHeader.data(), _responseHeader.size(), _writeBuffers);
	uv_write(&_writeReq, (uv_stream_t*)&_clientSocket, _writeBuffers, numBuffers, _onWriteFinish);
}

void ClientSession::_sendBadRequest(const std::string& msg) {
	std::stringstream responseStream;
	responseStream << "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nContent-Length: "
		<< msg.size() << "\r\n\r\n" << msg;
	// todo: _responseHeader now contains body...
	_responseHeader = responseStream.str();
	int numBuffers = getNumBuffers(_responseHeader.size());
	_writeBuffers = new uv_buf_t[numBuffers];
	putDataInBuffers((char*)_responseHeader.data(), _responseHeader.size(), _writeBuffers);
	uv_write(&_writeReq, (uv_stream_t*)&_clientSocket, _writeBuffers, numBuffers, _onWriteFinish);
}


} // namespace staticserver
