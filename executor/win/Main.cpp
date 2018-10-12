#include "PackageManager.h"
#include "Path.hpp"
#include "File.h"
#include "ResourceParam.h"
#include <nana/gui/widgets/widget.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/wvl.hpp>
#include <nana/gui/widgets/picture.hpp>
#include <nana/gui/dragger.hpp>
#include <nana/gui/widgets/progress.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/animation.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <thread>

#pragma warning(push)
#pragma warning(disable:4091)
#include <client/windows/handler/exception_handler.h>
#pragma warning(pop)

const std::wstring DirPath(L"<dir_path>");
const std::wstring CurrentAppPath(L"<current_app_path>");
const std::wstring TmpPrefix(L"infomaximum_");

google_breakpad::ExceptionHandler	handler(Path::GetDumpDir(), nullptr, [](const wchar_t* /*dump_path*/, const wchar_t* /*id*/, void* /*context*/,
	EXCEPTION_POINTERS* /*pExcPtr*/,
	MDRawAssertionInfo* /*assertion*/,
	bool succeeded) {
	return succeeded;
}, nullptr, google_breakpad::ExceptionHandler::HANDLER_ALL);

void ShowError(const Error& err)
{
	MessageBoxW(NULL, err.getMessage().c_str(), L"Error", MB_OK | MB_ICONERROR);
}

struct EnumParam
{
	DWORD pid = 0;
	bool visibleWindowExist = false;
};

BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
	EnumParam* param = (EnumParam*)lParam;

	DWORD lpdwProcessId;
	GetWindowThreadProcessId(hwnd, &lpdwProcessId);
	if (lpdwProcessId == param->pid)
	{
		if (IsWindowVisible(hwnd))
		{
			param->visibleWindowExist = true;
			return FALSE;
		}
	}
	return TRUE;
}

Error ExecuteProcess(const std::wstring& cmd, const std::wstring& workingDir, DWORD& exitCode)
{
	exitCode = ERROR_SUCCESS;

	const std::wstring stdoutFilePath = Path::GetStdoutFilePath(std::wstring(TmpPrefix).append(L"stdout.log"));
	File::Delete(stdoutFilePath);

	SECURITY_ATTRIBUTES sa;
	sa.nLength = sizeof(sa);
	sa.lpSecurityDescriptor = NULL;
	sa.bInheritHandle = TRUE;

	HANDLE hStdout = CreateFile(stdoutFilePath.c_str(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		&sa,
		CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (hStdout == INVALID_HANDLE_VALUE)
	{
		hStdout = NULL;
	}

	PROCESS_INFORMATION pInfo = {0};
	STARTUPINFOW sInfo = {0};

	sInfo.cb = sizeof(sInfo);
	sInfo.dwFlags |= STARTF_USESTDHANDLES;
	sInfo.hStdInput = NULL;
	sInfo.hStdError = hStdout;
	sInfo.hStdOutput = hStdout;

	if (!CreateProcessW(NULL, const_cast<wchar_t*>(cmd.c_str()), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, workingDir.c_str(), &sInfo, &pInfo))
	{
		CloseHandle(hStdout);
		return Error(GetLastError());
	}

	DWORD timeoutMs = 100;
	for (;;)
	{
		EnumParam param;
		param.pid = pInfo.dwProcessId;
		EnumWindows(EnumWindowsCallback, (LONG_PTR)&param);
		if (param.visibleWindowExist)
		{
			nana::API::exit_all();
			WaitForSingleObject(pInfo.hProcess, INFINITE);
			break;
		}

		if (WaitForSingleObject(pInfo.hProcess, timeoutMs) != WAIT_TIMEOUT)
		{
			nana::API::exit_all();
			break;
		}
	}

	GetExitCodeProcess(pInfo.hProcess, &exitCode);

	CloseHandle(pInfo.hProcess);
	CloseHandle(pInfo.hThread);
	CloseHandle(hStdout);

	return Error();
}

Error RunJavaInstaller(const std::wstring& installationDir, const std::wstring& exeFullPath, DWORD& exitCode)
{
	std::wstring cmdLine = PackageManager::GetStringResource(ParamType, CmdLineName);
	std::wstring workingDir = PackageManager::GetStringResource(ParamType, WorkingDirName);

	if (cmdLine.empty())
	{
		return Error(L"cmd_line not found in resource");
	}

	boost::replace_all(cmdLine, DirPath, installationDir);
	boost::replace_all(cmdLine, CurrentAppPath, exeFullPath);

	boost::replace_all(workingDir, DirPath, installationDir);

	return ExecuteProcess(cmdLine, workingDir, exitCode);
}

void RemoveFolder(const std::wstring& dir_)
{
	// COMMENT:  Path must be double-null terminated.
	std::wstring dir(dir_);
	dir.push_back(L'\0');

	_SHFILEOPSTRUCTW	param = {
		NULL,
		FO_DELETE,
		dir.c_str(),
		NULL,
		FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI,
		FALSE,
		NULL,
		NULL
	};

	SHFileOperationW(&param);
}

void ShowSplashWindow(HANDLE& hSplashInitializedEvent)
{
	using namespace nana;

	// appearance(bool has_decoration, bool taskbar, bool floating, bool no_activate, bool min, bool max, bool sizable)
	form fm(API::make_center(600, 400), appearance(false, false, true, true, false, false, false));

	std::vector<uint8_t> background = PackageManager::GetBinaryResource(BackgroundName);
	std::list<std::vector<uint8_t>> pictures = PackageManager::GetAllBinaryResources(PictureType);

	frameset fset;
	for (const std::vector<uint8_t>& pic : pictures)
	{
		nana::paint::image img;
		img.open(&pic[0], pic.size());
		fset.push_back(std::move(img));
	}

	animation ani(30);
	ani.push_back(fset);
	ani.output(fm, nana::point(282, 242));
	ani.looped(true);
	ani.play();

	nana::paint::image img;
	img.open(&background[0], background.size());
	drawing dw(fm);
	dw.draw([&img](nana::paint::graphics& graph)
	{
		if (!img.empty())
		{
			img.paste(graph, nana::point{});
		}
	});
	dw.update();

	dragger dg;
	dg.target(fm);
	dg.trigger(fm);

	fm.collocate();
	fm.show();

	SetEvent(hSplashInitializedEvent);
	nana::exec();
}

void ExecuteChildProcess(Error& result, DWORD& exitCode, HANDLE& hSplashInitializedEvent)
{
	result = Error();
	exitCode = ERROR_SUCCESS;

	std::wstring exeFullPath;
	Error err = Path::GetApplicationFilePath(exeFullPath);
	if (!err.Succeeded())
	{
		result = err;
		return;
	}

	std::wstring tempDir;
	err = Error(Path::GetTempDirPath(TmpPrefix, tempDir));
	if (!err.Succeeded())
	{
		result = err;
		return;
	}

	err = PackageManager::UnpackZipResource(tempDir);
	if (!err.Succeeded())
	{
		result = err;
		return;
	}

	WaitForSingleObject(hSplashInitializedEvent, INFINITE);

	result = RunJavaInstaller(tempDir, exeFullPath, exitCode);
	RemoveFolder(tempDir);
}

int main(int /*argc*/, char** /*argv*/)
{
	HANDLE hSplashInitializedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hSplashInitializedEvent == NULL)
	{
		ShowError(Error(GetLastError()));
		return EXIT_FAILURE;
	}

	Error error;
	DWORD exitCode = ERROR_SUCCESS;
	std::thread executeThread(ExecuteChildProcess, std::ref(error), std::ref(exitCode), std::ref(hSplashInitializedEvent));

	ShowSplashWindow(hSplashInitializedEvent);

	if (executeThread.joinable())
	{
		executeThread.join();
	}

	CloseHandle(hSplashInitializedEvent);

	if (!error.Succeeded())
	{
		ShowError(error);
		return EXIT_FAILURE;
	}
	else if (exitCode != ERROR_SUCCESS)
	{
		std::wstring msg;
		msg.append(L"child process failed, error = ").append(Error(exitCode).getMessage());
		ShowError(Error(std::move(msg)));
		return exitCode;
	}
	else
	{
		return EXIT_SUCCESS;
	}
}