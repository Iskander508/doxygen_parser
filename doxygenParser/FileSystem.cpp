#include "FileSystem.h"
#include <windows.h>

std::vector<string> FileSystem::GetFiles(const stringRef& directory, const stringRef& extension) {

	string targetDir(directory.str());
	if(targetDir.back() != _T('\\')) {
		targetDir.append(_T("\\"));
	}

	string target(targetDir);
	if (extension) {
		target.append(_T("*.")).append(extension.str());
	} else {
		target.append(_T("*.*"));
	}
	
	std::vector<string> result;
	WIN32_FIND_DATA data;
	HANDLE hFind = FindFirstFile(target.c_str(), &data);
	if (hFind != INVALID_HANDLE_VALUE) {
		do {
			result.push_back(targetDir + data.cFileName);
		} while (FindNextFile(hFind, &data));
		FindClose(hFind);
	}

	return result;
}