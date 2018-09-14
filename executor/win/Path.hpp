#pragma once

#include "Error.hpp"
#include <string>
#include <random>
#include <Windows.h>

struct Path
{
	static Error CreateDir(const std::wstring& dirPath)
	{
		if (!CreateDirectory(dirPath.c_str(), NULL))
		{
			const DWORD err = GetLastError();
			return err == ERROR_ALREADY_EXISTS ? Error() : Error(err);
		}

		return Error();
	}

	static Error GetTempDirPath(const std::wstring& prefix, std::wstring& destination)
	{
		destination.clear();

		wchar_t tempPath[MAX_PATH + 1];
		DWORD len = GetTempPathW(MAX_PATH + 1, tempPath);
		if (len == 0)
		{
			return Error(GetLastError());
		}

		std::wstring dirPath(tempPath);
		dirPath.append(prefix);
		const size_t prevSize = dirPath.size();

		std::random_device	rd;
		std::mt19937		gen(rd());
		while (true)
		{
			dirPath.append(std::to_wstring(gen()));
			if (CreateDirectoryW(dirPath.c_str(), NULL))
			{
				break;
			}

			if (GetLastError() != ERROR_ALREADY_EXISTS)
			{
				return Error(GetLastError());
			}
			dirPath.resize(prevSize);
		}

		destination = std::move(dirPath);
		return Error();
	}

	static Error GetApplicationFilePath(std::wstring& destination)
	{
		const size_t	CAPACITY_INCREMENT = 128;

		destination.clear();

		for (;;)
		{
			destination.resize(destination.capacity() + CAPACITY_INCREMENT);
			const size_t countChar = GetModuleFileNameW(NULL, (wchar_t*)destination.data(), static_cast<DWORD>(destination.capacity()));
			if (countChar == 0)
			{
				return Error(GetLastError());
			}

			if (countChar == destination.capacity() && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
			{
				continue;
			}

			destination.resize(countChar);
			break;
		}

		return Error();
	}

	static std::wstring GetDumpDir()
	{
		const std::wstring defaultDir(L"C:");

		std::wstring dir;
		Error err = GetApplicationFilePath(dir);
		if (!err.Succeeded())
		{
			return defaultDir;
		}

		size_t pos = dir.find_last_of(L'\\');
		if (pos == std::wstring::npos)
		{
			return defaultDir;
		}

		dir.erase(pos);
		return dir;
	}

	static std::wstring GetStdoutFilePath(const std::wstring& filename)
	{
		std::wstring tempDir;
		wchar_t tempPath[MAX_PATH + 1];
		DWORD len = GetTempPathW(MAX_PATH + 1, tempPath);
		if (len == 0)
		{
			tempDir = L"C:\\";
		}
		else
		{
			tempDir.assign(tempPath, len);
		}

		tempDir.append(filename);
		return tempDir;
	}
};