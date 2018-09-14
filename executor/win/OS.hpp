#pragma once

#define NOMINMAX
#include <Shlobj.h>
#include "Version.hpp"

class OS
{
private:

	struct IconResource
	{
		DWORD			threadId;
		const WCHAR*	iconResource;
		bool			isApplied;
	};

	static const WORD ANY_SERVICE_PACK_MAJOR = 0xffff;

public:

	static const DWORD	MAX_PATH_LEN = 32 * 1024;

public:

	static bool IsX64()
	{
		SYSTEM_INFO	info = {0};

		GetNativeSystemInfo(&info);

		return info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
	}

	static bool IsWinXPSP2OrGreater()
	{
		return CheckOSVersion(5, 1, 2);
	}

	static bool IsWinVistaOrGreater()
	{
		return CheckOSVersion(6, 0);
	}

	static bool IsWinVistaSP1OrGreater()
	{
		return CheckOSVersion(6, 0, 1);
	}

	static bool IsWin8OrGreater()
	{
		return CheckOSVersion(6, 2);
	}

	static bool IsWin10OrGreater()
	{
		return CheckOSVersion(10, 0);
	}

	static bool IsWinXP()
	{
		return CheckOSVersion(5, 1, ANY_SERVICE_PACK_MAJOR, VER_EQUAL);
	}

	static bool IsWinVista()
	{
		return CheckOSVersion(6, 0, 0, VER_EQUAL);
	}

private:

	static bool CheckOSVersion(DWORD majorVer, DWORD minorVer, WORD servicePackMajor = ANY_SERVICE_PACK_MAJOR, BYTE condition = VER_GREATER_EQUAL)
	{
		OSVERSIONINFOEX		osVersion = { 0 };
		DWORDLONG			conditionMask = 0;
		DWORD				typeMask = 0;
		
		osVersion.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		osVersion.dwMajorVersion = majorVer;
		osVersion.dwMinorVersion = minorVer;

		VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, condition);
		VER_SET_CONDITION(conditionMask, VER_MINORVERSION, condition);
		typeMask = VER_MAJORVERSION | VER_MINORVERSION;

		if (servicePackMajor != ANY_SERVICE_PACK_MAJOR)
		{
			osVersion.wServicePackMajor = servicePackMajor;

			VER_SET_CONDITION(conditionMask, VER_SERVICEPACKMAJOR, condition);
			typeMask |= VER_SERVICEPACKMAJOR;
		}

		return VerifyVersionInfo(&osVersion, typeMask, conditionMask) != FALSE;
	}

public:

	static DWORD UnpackResource(const WCHAR* resourceId, const WCHAR* type, const WCHAR* pDestinationPath)
	{
		DWORD	error = ERROR_SUCCESS;
		DWORD	resSize;

		HRSRC hResource = FindResourceW(NULL, resourceId, type);
		if (hResource == NULL)
		{
			return GetLastError();
		}

		HGLOBAL hFileResource = LoadResource(NULL, hResource);
		if (hFileResource == NULL)
		{
			return GetLastError();
		}

		void* pResFile = LockResource(hFileResource);
		if (pResFile == NULL)
		{
			return GetLastError();
		}

		resSize = SizeofResource(NULL, hResource);

		HANDLE hFile = CreateFileW(pDestinationPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			return GetLastError();
		}

		HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READWRITE, 0, resSize, NULL); 
		if (hFileMap != NULL)
		{
			void* lpBaseAddress = MapViewOfFile(hFileMap, FILE_MAP_WRITE, 0, 0, 0);
			if (lpBaseAddress != NULL)
			{
				memcpy(lpBaseAddress, pResFile, resSize);
				UnmapViewOfFile(lpBaseAddress);
			}
			else
			{
				error = GetLastError();
			}

			CloseHandle(hFileMap);
		}

		CloseHandle(hFile);

		if (error != ERROR_SUCCESS)
		{
			DeleteFileW(pDestinationPath);
		}

		return error;
	}

	static DWORD GetResourceFrom(const WCHAR* filePath, 
		const WCHAR* type, const WCHAR* name, 
		BYTE* pDestination, DWORD destinationSize)
	{
		DWORD	error;
		HMODULE	hModule;

		hModule = LoadLibraryW(filePath);
		if (hModule == NULL)
		{ 
			return GetLastError();
		}

		error = GetResourceFrom(hModule, type, name, pDestination, destinationSize);

		FreeLibrary(hModule);

		return error;
	}

	static DWORD GetCurrentResource(const WCHAR* type, const WCHAR* name, 
		BYTE* pDestination, DWORD destinationSize)
	{
		return GetResourceFrom((HMODULE)NULL, type, name, pDestination, destinationSize);
	}

	static DWORD SetResourceTo(const WCHAR* filePath, 
		const WCHAR* type, const WCHAR* name, 
		const BYTE* pSource, DWORD sourceSize)
	{
		HANDLE	hUpdateRes;
		bool	result;
		DWORD	error = ERROR_SUCCESS;

		hUpdateRes = BeginUpdateResourceW(filePath, FALSE); 
		if (hUpdateRes == NULL) 
		{ 
			return GetLastError();
		} 

		result = UpdateResourceW(hUpdateRes, type, name, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), (void*)pSource, sourceSize) != FALSE;
		if (!result) 
		{ 
			error = GetLastError();
		} 

		result = EndUpdateResourceW(hUpdateRes, FALSE) != FALSE;
		if (error != ERROR_SUCCESS)
		{
			return error;
		}

		return result ? error : GetLastError();
	}

public:

	static bool ExistFileSystemObject(const WCHAR* objectPath)
	{
		bool				result = false;
		WIN32_FIND_DATAW	findData = {0};
		HANDLE				hSearch;

		hSearch = FindFirstFileW(objectPath, &findData);
		if (hSearch != INVALID_HANDLE_VALUE)
		{
			result = true;
			FindClose(hSearch);
		}

		return result;
	}

	static DWORD DeleteFile(const WCHAR* filePath)
	{
		DWORD	attributes, error = ERROR_SUCCESS;

		attributes = GetFileAttributesW(filePath);
		if (attributes != INVALID_FILE_ATTRIBUTES)
		{
			if (attributes & FILE_ATTRIBUTE_READONLY)
			{
				SetFileAttributesW(filePath, (attributes & ~FILE_ATTRIBUTE_READONLY));
			}

			if (!::DeleteFileW(filePath))
			{
				error = GetLastError();
			}
		}
		else
		{
			error = GetLastError();
		}

		switch(error)
		{
		case ERROR_PATH_NOT_FOUND:
		case ERROR_FILE_NOT_FOUND:
			error = ERROR_SUCCESS;
			break;
		}

		return error;
	}

	static DWORD ClearFolder(const WCHAR* path)
	{
		WCHAR	temp[OS::MAX_PATH_LEN] = {0};
		DWORD	len;

		wcscpy_s(temp, path);

		len = (DWORD)wcslen(temp);
		if (temp[len - 1] != L'\\')
		{
			temp[len] = L'\\';
			++len;
		}

		temp[len] = L'*';
		++len;

		temp[len] = 0;

		return RemoveFolder(temp);
	}

	static DWORD RemoveFolder(WCHAR path[OS::MAX_PATH_LEN])
	{
		int					error;
		_SHFILEOPSTRUCTW	param = {
			NULL, 
			FO_DELETE, 
			path, 
			NULL, 
			FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR,
			FALSE,
			NULL,
			NULL
		};

		path[wcslen(path) + 1] = 0;

		error = SHFileOperationW(&param);
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
		case 0x402:
			error = ERROR_SUCCESS;
			break;
		}

		if (IsWinXP())
		{
			SHChangeNotify(SHCNE_RMDIR, SHCNF_PATHW | SHCNF_FLUSH | SHCNF_NOTIFYRECURSIVE, path, NULL);
			Sleep(10);
		}

		return error;
	}

private:

	static DWORD GetResourceFrom(HMODULE hModule, 
		const WCHAR* type, const WCHAR* name, 
		BYTE* pDestination, DWORD destinationSize)
	{
		DWORD	error = ERROR_SUCCESS;
		DWORD	resSize;

		HRSRC hResource = FindResourceW(hModule, name, type);
		if (hResource == NULL)
		{
			return GetLastError();
		}

		HGLOBAL hFileResource = LoadResource(hModule, hResource);
		if (hFileResource == NULL)
		{
			return GetLastError();
		}

		void* pResFile = LockResource(hFileResource);
		if (pResFile == NULL)
		{
			return GetLastError();					
		}

		resSize = SizeofResource(hModule, hResource);
		if (resSize == 0)
		{
			error = GetLastError();
		}
		else if (resSize > destinationSize)
		{
			error = ERROR_INSUFFICIENT_BUFFER;
		}
		else
		{
			error = ERROR_SUCCESS;
			memcpy(pDestination, pResFile, resSize);
		}

		return error;
	}

	static BOOL CALLBACK SetIconToWindow(HWND hwnd, LPARAM lParam)
	{
		IconResource*	pResource = (IconResource*)lParam;
		HICON			icon;

		if (GetParent(hwnd) != NULL)
		{
			return TRUE;
		}

		icon = LoadIconW(GetModuleHandle(NULL), pResource->iconResource);

		PostMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)icon);
		PostMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)icon);

		pResource->isApplied = true;

		return FALSE;
	}

	static DWORD WINAPI SetIcon(void* pParam)
	{
		IconResource*	pResource = (IconResource*)pParam;

		pResource->isApplied = false;
		do
		{
			EnumThreadWindows(pResource->threadId, SetIconToWindow, (LPARAM)pResource);
			if (!pResource->isApplied)
			{
				Sleep(200);
			}
		}while (!pResource->isApplied);

		return 0;
	}
};