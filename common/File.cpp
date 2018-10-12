#include "File.h"
#include <thread>
#include <Windows.h>

namespace
{

static Error Create(const WCHAR* fileName, DWORD desiredAccess, DWORD shareMode, DWORD createDisposition, DWORD flagsAndAttrs, HANDLE& descriptor)
{
	descriptor = CreateFile(fileName, desiredAccess, shareMode, NULL, createDisposition, flagsAndAttrs, NULL);
	if (descriptor != INVALID_HANDLE_VALUE)
	{
		return Error();
	}

	const DWORD lastError = GetLastError();
	return Error(lastError);
}

static Error Write(HANDLE file, const void* buff, DWORD bytesToWrite, DWORD* numOfWrittenBytes)
{
	*numOfWrittenBytes = 0;
	return WriteFile(file, buff, bytesToWrite, numOfWrittenBytes, NULL) != FALSE ? Error() : Error(GetLastError());
}

Error OpenFile_Write(const std::wstring& path, HANDLE& descriptor)
{
	return Create(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, descriptor);
}

Error OpenFile_Read(const std::wstring& path, HANDLE& descriptor)
{
	return Create(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, descriptor);
}

Error GetFileSize(HANDLE hFile, uint64_t& dest)
{
	dest = 0;
	LARGE_INTEGER size = { 0, 0 };

	if (GetFileSizeEx(hFile, &size))
	{
		static_assert(sizeof(uint64_t) == sizeof(LARGE_INTEGER), "sizeof(uint64_t) != sizeof(LARGE_INTEGER)");
		dest = size.QuadPart;
		return Error();
	}

	return Error(GetLastError());
}

Error FileRead(HANDLE file, void* buff, DWORD bytesToRead, DWORD* numOfReadBytes)
{
	*numOfReadBytes = 0;

	return ReadFile(file, buff, bytesToRead, numOfReadBytes, NULL) != FALSE ? Error() : Error(GetLastError());
}


} // namespace


File::File()
{
	descriptor = INVALID_HANDLE_VALUE;
}

File::~File()
{
	Close();
}

Error File::OpenWrite(const std::wstring& path)
{
	Close();

	return OpenFile_Write(path, descriptor);
}

Error File::OpenRead(const std::wstring& path)
{
	Close();

	return OpenFile_Read(path, descriptor);
}

Error File::Read(std::vector<uint8_t>& dst)
{
	uint64_t fileSize = 0;
	Error err = GetFileSize(descriptor, fileSize);
	if (!err.Succeeded())
	{
		return err;
	}

	dst.resize(fileSize);

	DWORD readCount;
	err = Read(&dst[0], static_cast<DWORD>(fileSize), readCount);
	if (!err.Succeeded())
	{
		return err;
	}

	return Error();
}

Error File::Write(const uint8_t* pBuffer, const DWORD dwBytesToWrite)
{
	DWORD dwNeed = dwBytesToWrite;
	DWORD dwWrite = 0;
	while (dwWrite < dwNeed)
	{
		Error error = ::Write(descriptor, pBuffer, dwNeed, &dwWrite);
		if (!error.Succeeded())
		{
			return error;
		}

		if (dwWrite == 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		}
		else
		{
			pBuffer = pBuffer + dwWrite;
			dwNeed -= dwWrite;
		}
	}

	return Error();
}

Error File::Delete(const std::wstring& file)
{
	return DeleteFile(file.c_str()) != FALSE ? Error() : Error(GetLastError());
}

void File::Close()
{
	if (descriptor != INVALID_HANDLE_VALUE)
	{
		CloseHandle(descriptor);
		descriptor = INVALID_HANDLE_VALUE;
	}
}

Error File::Read(BYTE* pBuffer, const DWORD bufferSize, DWORD& readCount) const
{
	readCount = 0;
	while (readCount < bufferSize)
	{
		DWORD dwRead = 0;
		Error error = FileRead(descriptor, pBuffer + readCount, bufferSize - readCount, &dwRead);
		if (!error.Succeeded())
		{
			return error;
		}

		if (dwRead == 0)
		{
			break;
		}

		readCount += dwRead;
	}

	return Error();
}