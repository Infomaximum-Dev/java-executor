#include "PackageManager.h"
#include "Path.hpp"
#include "File.h"
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

const std::wstring RunAsAdminId(L"run_as_admin");
const std::wstring CmdLineId(L"cmd_line");
const std::wstring WorkingDirId(L"working_dir");
const std::wstring BackgroundId(L"background.bmp");
const std::wstring PictureId(L"PICTURE");
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

Error ExecuteProcess(const std::wstring& cmd, const std::wstring& workingDir)
{
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

	WaitForSingleObject(pInfo.hProcess, INFINITE);
	CloseHandle(pInfo.hProcess);
	CloseHandle(pInfo.hThread);
	CloseHandle(hStdout);

	return Error();
}

Error IsCurrentProcessHaveAdminAccessRights(bool& result)
{
	result = false;

	HANDLE hToken = NULL;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
	{
		return Error(GetLastError());
	}

	Error err;
	TOKEN_ELEVATION Elevation;
	DWORD cbSize = sizeof(TOKEN_ELEVATION);
	if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &cbSize)) 
	{
		result = Elevation.TokenIsElevated != FALSE;
	}
	else
	{
		err = Error(GetLastError());
	}

	CloseHandle(hToken);

	return err;
}

Error RunJavaInstaller(const std::wstring& installationDir, const std::wstring& exeFullPath)
{
	std::wstring cmdLine = PackageManager::GetStringFromResource(CmdLineId);
	std::wstring workingDir = PackageManager::GetStringFromResource(WorkingDirId);

	if (cmdLine.empty())
	{
		return Error(L"cmd_line not found in resource");
	}

	boost::replace_all(cmdLine, DirPath, installationDir);
	boost::replace_all(cmdLine, CurrentAppPath, exeFullPath);

	boost::replace_all(workingDir, DirPath, installationDir);

	return ExecuteProcess(cmdLine, workingDir);
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

	std::vector<uint8_t> background = PackageManager::GetBinaryResource(BackgroundId);
	std::list<std::vector<uint8_t>> pictures = PackageManager::GetAllBinaryResources(PictureId);

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

Error UnpackResource(const std::wstring& tempDir)
{
	HANDLE hSplashInitializedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (hSplashInitializedEvent == NULL)
	{
		return Error(GetLastError());
	}

	std::thread unpackThread([&tempDir, &hSplashInitializedEvent]()
	{
		PackageManager::UnpackZipResource(tempDir);

		WaitForSingleObject(hSplashInitializedEvent, INFINITE);
		nana::API::exit_all();
	});

	ShowSplashWindow(hSplashInitializedEvent);

	if (unpackThread.joinable())
	{
		unpackThread.join();
	}

	CloseHandle(hSplashInitializedEvent);

	return Error();
}

int main(int /*argc*/, char** /*argv*/)
{
	std::wstring exeFullPath;
	Error err = Path::GetApplicationFilePath(exeFullPath);
	if (!err.Succeeded())
	{
		ShowError(err);
		return EXIT_FAILURE;
	}

	const std::wstring requireAdmin = PackageManager::GetStringFromResource(RunAsAdminId);
	if (requireAdmin == L"true")
	{
		bool haveAdminRights = false;
		Error err = IsCurrentProcessHaveAdminAccessRights(haveAdminRights);
		if (!err.Succeeded())
		{
			ShowError(err);
			return EXIT_FAILURE;
		}

		if (!haveAdminRights)
		{
			SHELLEXECUTEINFO si = { 0 };

			si.cbSize = sizeof(SHELLEXECUTEINFO);
			si.fMask = SEE_MASK_DEFAULT;
			si.hwnd = NULL;
			si.lpVerb = L"runas";
			si.lpFile = exeFullPath.c_str();
			si.lpParameters = NULL;
			si.lpDirectory = NULL;
			si.nShow = SW_SHOW;
			si.hInstApp = NULL;
			if (!ShellExecuteEx(&si))
			{
				ShowError(Error(GetLastError()));
				return EXIT_FAILURE;
			}

			CloseHandle(si.hProcess);

			return EXIT_SUCCESS;
		}
	}

	std::wstring tempDir;
	err = Error(Path::GetTempDirPath(TmpPrefix, tempDir));
	if (!err.Succeeded())
	{
		ShowError(err);
		return EXIT_FAILURE;
	}

	err = UnpackResource(tempDir);
	if (!err.Succeeded())
	{
		ShowError(err);
		return EXIT_FAILURE;
	}

	err = RunJavaInstaller(tempDir, exeFullPath);
	if (!err.Succeeded())
	{
		ShowError(err);
	}

	RemoveFolder(tempDir);

	return err.Succeeded() ? EXIT_SUCCESS : EXIT_FAILURE;
}