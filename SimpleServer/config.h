#pragma once

// Each file served is split up into buffers of this size when writing with libuv.
#define STATICSERVER_WRITE_BUFFER_SIZE 1024
#define STATICSERVER_MAX_REQUEST_LEN 1024
#define STATICSERVER_MAX_BACKLOG 128