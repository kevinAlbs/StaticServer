#pragma once

#include <unordered_map>
#include <utility> // std::pair
#include <string>

// This is passed by value and copyable.
struct InMemoryFile {
	char* rawFileData;
	std::string mimeType; // todo: change to char*?
	int fileSize;
};

typedef std::unordered_map<std::string, InMemoryFile> FileMap;