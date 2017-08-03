#pragma once

// Each file served is split up into buffers of this size when writing with libuv.
#define STATICSERVER_WRITE_BUFFER_SIZE 1024

// The maximum size of a request before responding with a 400 status.
#define STATICSERVER_MAX_REQUEST_LEN 1024

// The max queued TCP connections.
#define STATICSERVER_MAX_BACKLOG 128

// How long will we hold a single TCP connection before timing out? (in ms)
#define STATICSERVER_TIMEOUT 3000