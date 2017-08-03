# StaticServer
A simple static file server written with libuv.

## Basic Usage
```c++
#include "simpleserver.h"

int main() {
    using namespace staticserver;
    Server server;
    server.addFile("test.jpg", "image/jpeg");
    server.addFile("readme.md", "text/plain");
    server.start();
}
```

# TODO
- make a client program
- add IPC and multiple processes
- add better allocation of client sessions (use a fixed array perhaps, and then allocate manually if that is filled)
- test performance against apache, nginx, and node. I predict there shouldn't be a significant difference since network transfer is the real bottleneck.
- conform to google C++ style guide