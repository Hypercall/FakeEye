#pragma once
#include <windows.h>
#include <iostream>
#include <Shlobj.h>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi")

/**
 * @namespace installer
 * @brief Responsible for managing the service
*/
namespace installer
{
	/**
	 * Stores the path of the installed service
	 * 
	 */
	static inline std::wstring service_path;

	/**
	 * Installs the service of BattlEye
	 *
	 * @returns The result as boolean
	 */
	auto Install()->boolean 
	{
		wchar_t FilePath[1024], CommonPath[MAX_PATH], ComPath[MAX_PATH];
		SYSTEM_INFO si;
		DWORD dwAttributes = 0, bytesNeeded = 0;
		SC_HANDLE hService = 0, hSCM = 0;
		SERVICE_STATUS_PROCESS ss = { 0,0,0,0,0,0,0,0,0 };
		HANDLE hFile = INVALID_HANDLE_VALUE;


		std::memset(FilePath, 0, 1024);
		std::memset(CommonPath, 0, MAX_PATH);
		std::memset(ComPath, 0, MAX_PATH);

		// check if the service is already installed
		{
			std::memset(&si, 0, sizeof(si));
			hSCM = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
			if (hSCM == 0)
				return false;

			hService = OpenServiceA(hSCM, "BEService", SERVICE_ALL_ACCESS);
			if (hService != 0)
			{
				if (QueryServiceStatusEx(hService, (SC_STATUS_TYPE)0, (LPBYTE)&ss, sizeof(ss), &bytesNeeded))
				{
					if (ss.dwCurrentState == SERVICE_RUNNING)
					{
						CloseServiceHandle(hService);
						return true;
					}
					if (StartServiceA(hService, 0, 0) == FALSE)
					{
						CloseServiceHandle(hService);
						return false;
					}

					while (ss.dwCurrentState != SERVICE_RUNNING)
					{
						if (!QueryServiceStatusEx(hService, (SC_STATUS_TYPE)0, (LPBYTE)&ss, sizeof(ss), &bytesNeeded))
						{
							CloseServiceHandle(hService);
							return false;
						}
					}
					CloseServiceHandle(hService);
					return true;
				}
				CloseServiceHandle(hService);
			}
		}

		// get service path
		{
			GetNativeSystemInfo(&si);
			auto is_64_pc = (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);

			if (!GetModuleFileNameW(0, FilePath, 1024))
			{
				CloseServiceHandle(hSCM);
				return false;
			}

			if (!SHGetSpecialFolderPathW(0, CommonPath, is_64_pc ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES, FALSE))
			{
				CloseServiceHandle(hSCM);
				return false;
			}

			if (!PathRemoveFileSpecW(FilePath))
			{
				CloseServiceHandle(hSCM);
				return false;
			}

			swprintf_s(FilePath, is_64_pc ? L"%s\\BattlEye\\BEService_x64.exe" : L"%s\\BattlEye\\BEService.exe", FilePath);
			if (!SHGetSpecialFolderPathW(0, CommonPath, is_64_pc ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES, FALSE))
			{
				CloseServiceHandle(hSCM);
				return false;
			}
		}

		// copy service
		{

			swprintf_s(ComPath, L"%s\\Common Files\\BattlEye", CommonPath);
			dwAttributes = GetFileAttributesW(ComPath);
			if (!(dwAttributes != INVALID_FILE_ATTRIBUTES && (dwAttributes & FILE_ATTRIBUTE_DIRECTORY)))
			{
				if (!CreateDirectoryW(ComPath, 0))
				{
					CloseServiceHandle(hSCM);
					return false;
				}
			}

			swprintf_s(ComPath, L"%s\\Common Files\\BattlEye\\BEService.exe", CommonPath);
			hFile = CreateFileW(ComPath, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
			if (hFile != INVALID_HANDLE_VALUE)
			{
				CloseHandle(hFile);
				if (!DeleteFileW(ComPath))
				{
					CloseHandle(hFile);
					CloseServiceHandle(hSCM);
					return false;
				}
			}

			if (service_path.length() == 0)
			{
				wchar_t ServicePath[1024];
				std::memset(ServicePath, 0, 1024);
				wmemcpy(ServicePath, FilePath, wcslen(FilePath));
				service_path = std::wstring(ServicePath);
			}

			
			if (!CopyFileW(FilePath, ComPath, false))
			{
				CloseServiceHandle(hSCM);
				return false;
			}
		}
		// start service
		{
			hService = 0;
			hService = CreateServiceW(hSCM,
				L"BEService",
				L"BattlEye Service",
				SERVICE_ALL_ACCESS,
				SERVICE_WIN32_OWN_PROCESS,
				SERVICE_DEMAND_START,
				SERVICE_ERROR_NORMAL,
				ComPath,
				0,
				0,
				0,
				0,
				0);
			if (hService == 0)
			{
				CloseServiceHandle(hSCM);
				return false;
			}


			if (StartServiceA(hService, 0, 0) == FALSE)
			{
				CloseServiceHandle(hService);
				CloseServiceHandle(hSCM);
				return false;
			}

			while (ss.dwCurrentState != SERVICE_RUNNING)
			{
				if (!QueryServiceStatusEx(hService, (SC_STATUS_TYPE)0, (LPBYTE)&ss, sizeof(ss), &bytesNeeded))
				{
					CloseServiceHandle(hService);
					CloseServiceHandle(hSCM);
					return false;
				}
			}
			if (hFile)
				CloseHandle(hFile);
			if (hService != 0 && hService != INVALID_HANDLE_VALUE)
				CloseServiceHandle(hService);
			if (hSCM != 0)
				CloseServiceHandle(hSCM);
		}
		return true;
	};


	/**
	 * Removes the service of BattlEye
	 *
	 * @returns The result as boolean
	 */
	auto Uninstall()->boolean
	{
		BOOL Status = false;
		SC_HANDLE hService = 0, hSCM = 0;
		DWORD cbNeeded = 0;
		SERVICE_STATUS_PROCESS ss = { 0,0,0,0,0,0,0,0,0 };
		SERVICE_STATUS ssc = { 0,0,0,0,0,0,0 };

		hSCM = OpenSCManagerA(0, 0, SC_MANAGER_ALL_ACCESS);
		if (hSCM == 0)
			return false;

		hService = OpenServiceA(hSCM, "BEService", SERVICE_ALL_ACCESS);

		if (hService != 0 &&
			QueryServiceStatusEx(hService, (SC_STATUS_TYPE)0, (LPBYTE)&ss, sizeof(ss), &cbNeeded))
		{
			// stop driver
			{
				if (ss.dwCurrentState == SERVICE_RUNNING)
				{
					if (ControlService(hService, SERVICE_CONTROL_STOP, &ssc) == FALSE)
					{
						CloseServiceHandle(hSCM);
						CloseServiceHandle(hService);
						return false;
					}
					while (ss.dwCurrentState != SERVICE_STOPPED)
					{
						if (!QueryServiceStatusEx(hService, (SC_STATUS_TYPE)0, (LPBYTE)&ss, sizeof(ss), &cbNeeded))
						{
							CloseServiceHandle(hSCM);
							CloseServiceHandle(hService);
							return false;
						}
					}
				}
			}

			if (DeleteService(hService) == FALSE)
			{
				CloseServiceHandle(hSCM);
				CloseServiceHandle(hService);
				return false;
			}
			// wait for driver exit
			{
				CloseServiceHandle(hService);
				hService = 0;
				while (hService != 0)
				{
					Sleep(120);
					hService = OpenServiceA(hSCM, "BEService", SERVICE_ALL_ACCESS);
					CloseServiceHandle(hService);
				}
			}
		}
		return true;
	};

	/**
	 * Gets the path where BEService is installed
	 *
	 */
	auto GetInstallationPath()->std::wstring { return service_path; };
}