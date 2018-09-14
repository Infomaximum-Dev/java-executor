#pragma once

#include "Error.hpp"
#include <string>
#include <Windows.h>

class File
{
public:

	File();
	~File();

	File(File&& other) = delete;
	File(const File& other) = delete;
	File& operator=(const File* pSource) = delete;
	File& operator=(File&& source) noexcept = delete;

	Error OpenWrite(const std::wstring& path);
	Error Write(const uint8_t* pBuffer, const DWORD dwBytesToWrite);
	static Error Delete(const std::wstring& path);

private:

	void Close();

private:

	HANDLE	descriptor;
};

