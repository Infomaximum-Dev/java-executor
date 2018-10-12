#pragma once

#include "Error.hpp"
#include <string>
#include <vector>
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
	Error OpenRead(const std::wstring& path);
	Error Write(const uint8_t* pBuffer, const DWORD dwBytesToWrite);
	Error Read(std::vector<uint8_t>& dst);
	static Error Delete(const std::wstring& path);

private:

	Error Read(BYTE* pBuffer, const DWORD bufferSize, DWORD& readCount) const;
	void Close();

private:

	HANDLE	descriptor;
};

