#pragma once

#include <string>
#include <algorithm>
#include <cwctype>
#include <Windows.h>

class Error
{
private:

	std::wstring message;

public:

	Error() = default;

	explicit Error(DWORD code)
	{
		if (code != ERROR_SUCCESS) 
		{
			message = ToString(code);
		}
	}

	explicit Error(std::wstring&& msg)
		: message(std::move(msg))
	{
	}

	bool Succeeded() const
	{
		return message.empty();
	}

	const std::wstring& getMessage() const
	{
		return message;
	}

	static Error makeByErrno(errno_t err)
	{
		return Error(ErrnoToString(err));
	}

private:

	static std::wstring ToString(DWORD errorCode)
	{
		const DWORD bufferLen = 2 * 1024;
		wchar_t buffer[bufferLen];

		std::swprintf(buffer, bufferLen, (errorCode < 0x0000FFFF ? L"No. %u" : L"No. 0x%08x"), errorCode);
		
		std::wstring result = buffer;
		
		DWORD len = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buffer, bufferLen, NULL);
		if (len != 0)
		{
			result.append(L"\n");
			result.append(buffer, len);

			result.erase(std::find_if(result.rbegin(), result.rend(), [](int ch) {
				return !std::iswspace(ch);
			}).base(), result.end());
		}

		return result;
	}

	static std::wstring ErrnoToString(errno_t errorCode)
	{
		const DWORD bufferLen = 2 * 1024;
		wchar_t buffer[bufferLen];

		if (_wcserror_s(buffer, bufferLen, errorCode) != 0)
		{
			std::swprintf(buffer, bufferLen, L"errno_t No. %u", errorCode);
		}

		return std::wstring(buffer);
	}
};