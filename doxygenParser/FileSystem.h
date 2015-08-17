#ifndef FILE_SYSTEM_H__
#define FILE_SYSTEM_H__

#include "types.h"
#include <vector>

struct FileSystem {
	static std::vector<string> GetFiles(const stringRef& directory, const stringRef& extension = nullptr);
	static bool CreateRecursiveDirectory(const stringRef& filepath);
};

#endif // FILE_SYSTEM_H__
