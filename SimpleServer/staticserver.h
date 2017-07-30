#pragma once

#include <uv.h>
#include <string>

namespace staticserver
{
// Can't really use C++ classes to encapsulate since the C callbacks won't call non-static member functions.
// Remember, the first arg of a member function is *this.
// However, based on uWebSockets, I could use lambdas. And I can pass objects through the ->data field.
struct Config
{
    bool printInfo;
    std::string filename;
};

void start();

} // namespace staticserver