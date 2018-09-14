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
