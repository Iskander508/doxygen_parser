#include "FileSystem.h"
#include <windows.h>
#include <Shlwapi.h>

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

bool FileSystem::CreateRecursiveDirectory(const stringRef& filepath)
{
    bool result = false;
    wchar_t path_copy[MAX_PATH] = {0};
	wcscat_s(path_copy, MAX_PATH, filepath.str());
    std::vector<std::wstring> path_collection;

    for(int level=0; PathRemoveFileSpec(path_copy); level++)
    {
        path_collection.push_back(path_copy);
    }
    for(int i=path_collection.size()-1; i >= 0; i--)
    {
        if(!PathIsDirectory(path_collection[i].c_str()))
            if(CreateDirectory(path_collection[i].c_str(), NULL))
                result = true;
    }
    return result;
};