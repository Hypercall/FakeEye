#pragma once
#include <windows.h>
#include <iostream>

/*! \cond PRIVATE */
struct _OBJECT_HANDLE_FLAG_INFORMATION
{
	BOOLEAN Inherit;
	BOOLEAN ProtectFromClose;
};

typedef struct _LSA_UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} LSA_UNICODE_STRING, * PLSA_UNICODE_STRING, UNICODE_STRING, * PUNICODE_STRING;

typedef struct _RTL_DRIVE_LETTER_CURDIR
{
	WORD Flags;
	WORD Length;
	ULONG TimeStamp;
	UNICODE_STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR;

typedef struct _CURDIR
{
	UNICODE_STRING DosPath;
	PVOID Handle;
} CURDIR, * PCURDIR;

typedef struct _SECTION_IMAGE_INFORMATION
{
	PVOID TransferAddress;
	ULONG ZeroBits;
	ULONG MaximumStackSize;
	ULONG CommittedStackSize;
	ULONG SubSystemType;
	union
	{
		struct
		{
			WORD SubSystemMinorVersion;
			WORD SubSystemMajorVersion;
		};
		ULONG SubSystemVersion;
	};
	ULONG GpValue;
	WORD ImageCharacteristics;
	WORD DllCharacteristics;
	WORD Machine;
	UCHAR ImageContainsCode;
	UCHAR ImageFlags;
	ULONG ComPlusNativeReady : 1;
	ULONG ComPlusILOnly : 1;
	ULONG ImageDynamicallyRelocated : 1;
	ULONG Reserved : 5;
	ULONG LoaderFlags;
	ULONG ImageFileSize;
	ULONG CheckSum;
} SECTION_IMAGE_INFORMATION, * PSECTION_IMAGE_INFORMATION;

typedef struct _RTL_USER_PROCESS_PARAMETERS {
	BYTE           Reserved1[16];
	PVOID          Reserved2[10];
	UNICODE_STRING ImagePathName;
	UNICODE_STRING CommandLine;
} RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS;

typedef struct _CLIENT_ID
{
	PVOID UniqueProcess;
	PVOID UniqueThread;
} CLIENT_ID, * PCLIENT_ID;

typedef struct _RTL_USER_PROCESS_INFORMATION
{
	ULONG Length;
	HANDLE ProcessHandle;
	HANDLE ThreadHandle;
	CLIENT_ID ClientId;
	SECTION_IMAGE_INFORMATION ImageInformation;
} RTL_USER_PROCESS_INFORMATION, * PRTL_USER_PROCESS_INFORMATION;

typedef struct _RTL_RELATIVE_NAME_U {
	UNICODE_STRING RelativeName;
	HANDLE ContainingDirectory;
	PVOID CurDirRef;
} RTL_RELATIVE_NAME_U, * PRTL_RELATIVE_NAME_U;
/*! \endcond */

/**
 * @namespace game_handler
 * @brief Responsible for emulating the game-start process
*/
namespace game_handler
{
	/**
	 * Starts the protected game in the launcher 
	 *
	 * @param Arg The process argument
	 * @param Cmd The path to the BE protected game
	 * @param ProcessHandle A pointer to be written with the full access process handle
	 * @param ThreadHandle A pointer to be written with the full access thread handle
	 * @param Suspended Decides whether the process should remain suspended or not
	 *
	 * @returns A boolean which decides whether the function was successful or not.
	 */
	auto StartGame(wchar_t* Arg, wchar_t* Cmd, OUT PHANDLE ProcessHandle, OUT PHANDLE ThreadHandle, BOOL Suspended)->boolean
	{
		using p_NtSetInformationObject = NTSTATUS(NTAPI*)(HANDLE ObjectHandle, int ObjectInformationClass, PVOID ObjectInformation, ULONG Length);
		static p_NtSetInformationObject NtSetInformationObject = (p_NtSetInformationObject)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetInformationObject"));

		using p_RtlDosPathNameToNtPathName_U = NTSTATUS(NTAPI*)(PCWSTR DosName, PUNICODE_STRING NtName, PCWSTR* PartName, PRTL_RELATIVE_NAME_U RelativeName);
		static p_RtlDosPathNameToNtPathName_U RtlDosPathNameToNtPathName_U = (p_RtlDosPathNameToNtPathName_U)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlDosPathNameToNtPathName_U"));

		using p_RtlInitUnicodeString = NTSTATUS(NTAPI*)(PUNICODE_STRING DestinationString, PCWSTR SourceString);
		static p_RtlInitUnicodeString RtlInitUnicodeString = (p_RtlInitUnicodeString)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlInitUnicodeString"));

		using p_RtlCreateProcessParameters = NTSTATUS(NTAPI*)(PRTL_USER_PROCESS_PARAMETERS* pProcessParameters, PUNICODE_STRING ImagePathName, PUNICODE_STRING DllPath, PUNICODE_STRING CurrentDirectory, PUNICODE_STRING CommandLine, PVOID Environment, PUNICODE_STRING WindowTitle, PUNICODE_STRING DesktopInfo, PUNICODE_STRING ShellInfo, PUNICODE_STRING RuntimeData);
		static p_RtlCreateProcessParameters RtlCreateProcessParameters = (p_RtlCreateProcessParameters)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlCreateProcessParameters"));

		using p_RtlFreeUnicodeString = NTSTATUS(NTAPI*)(PUNICODE_STRING UnicodeString);
		static p_RtlFreeUnicodeString RtlFreeUnicodeString = (p_RtlFreeUnicodeString)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlFreeUnicodeString"));

		using p_RtlCreateUserProcess =  NTSTATUS(NTAPI*)(PUNICODE_STRING ImagePath, ULONG ObjectAttributes, PRTL_USER_PROCESS_PARAMETERS ProcessParameters, PSECURITY_DESCRIPTOR ProcessSecurityDescriptor, PSECURITY_DESCRIPTOR ThreadSecurityDescriptor, HANDLE ParentProcess, BOOLEAN InheritHandles, HANDLE DebugPort, HANDLE ExceptionPort, PRTL_USER_PROCESS_INFORMATION ProcessInformation);
		static p_RtlCreateUserProcess RtlCreateUserProcess =(p_RtlCreateUserProcess)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlCreateUserProcess"));

		using p_RtlDestroyProcessParameters = NTSTATUS(NTAPI*)(PRTL_USER_PROCESS_PARAMETERS ProcessParameters);
		static p_RtlDestroyProcessParameters RtlDestroyProcessParameters = (p_RtlDestroyProcessParameters)(GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlDestroyProcessParameters"));


		wchar_t Path[512], CmdLine[512], NewPath[512];
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;
		PRTL_USER_PROCESS_PARAMETERS UserProcessParam = 0, GameParam = 0;
		HANDLE hProcess = 0, OwnProcess = INVALID_HANDLE_VALUE;
		UNICODE_STRING FileName, CommandLine, FileNameGame;
		RTL_USER_PROCESS_INFORMATION ProcessInfo;
		NTSTATUS Status = 0;
		_OBJECT_HANDLE_FLAG_INFORMATION ObjectHandleAttributeInformation;
		std::uint32_t pId = 0;

		ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));
		ZeroMemory(&ObjectHandleAttributeInformation, sizeof(ObjectHandleAttributeInformation));
		ZeroMemory(&si, sizeof(STARTUPINFOW));
		ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
		ZeroMemory(&UserProcessParam, sizeof(UserProcessParam));
		ZeroMemory(&GameParam, sizeof(GameParam));
		ZeroMemory(&FileName, sizeof(FileName));
		ZeroMemory(&CommandLine, sizeof(CommandLine));
		ZeroMemory(&FileNameGame, sizeof(FileNameGame));
		ZeroMemory(Path, 512);
		ZeroMemory(NewPath, 512);
		ZeroMemory(CmdLine, 512);

		if (!NtSetInformationObject || !RtlDosPathNameToNtPathName_U || !RtlInitUnicodeString || !RtlCreateProcessParameters || !RtlFreeUnicodeString || !RtlCreateUserProcess || !RtlDestroyProcessParameters)
			return false;

		OwnProcess = OpenProcess(PROCESS_CREATE_PROCESS, false, GetCurrentProcessId());
		if (OwnProcess == INVALID_HANDLE_VALUE)
			return false;

		if (!GetModuleFileNameW(0, Path, 512))
		{
			CloseHandle(OwnProcess);
			return false;
		}

		if (Arg)
		{
			swprintf_s(CmdLine, L"\"%ws\" %ws", Cmd, Arg);
			RtlInitUnicodeString(&CommandLine, CmdLine);
		}


		if (!RtlDosPathNameToNtPathName_U(Path, &FileName, NULL, NULL) ||
			!RtlDosPathNameToNtPathName_U(Cmd, &FileNameGame, NULL, NULL))
		{
			CloseHandle(OwnProcess);
			return false;
		}


		if (RtlCreateProcessParameters(&UserProcessParam, &FileName, NULL, NULL, 0, NULL, NULL, NULL, NULL, NULL))
		{
			RtlFreeUnicodeString(&FileNameGame);
			RtlFreeUnicodeString(&FileName);
			CloseHandle(OwnProcess);
			return false;
		}

		if (RtlCreateProcessParameters(&GameParam, &FileNameGame, NULL, NULL, Arg ? &CommandLine : 0, NULL, NULL, NULL, NULL, NULL))
		{
			RtlFreeUnicodeString(&FileNameGame);
			RtlFreeUnicodeString(&FileName);
			RtlDestroyProcessParameters(UserProcessParam);
			CloseHandle(OwnProcess);
			return false;
		}

		Status = RtlCreateUserProcess(&FileName, 0x00000040, UserProcessParam, NULL, NULL, OwnProcess, TRUE, NULL, NULL, &ProcessInfo);
		if (Status)
		{
			RtlFreeUnicodeString(&FileNameGame);
			RtlFreeUnicodeString(&FileName);
			RtlDestroyProcessParameters(UserProcessParam);
			CloseHandle(OwnProcess);
			return false;
		}

		hProcess = ProcessInfo.ProcessHandle;
		if (!CloseHandle(ProcessInfo.ThreadHandle))
		{
			RtlFreeUnicodeString(&FileNameGame);
			RtlFreeUnicodeString(&FileName);
			RtlDestroyProcessParameters(UserProcessParam);
			CloseHandle(OwnProcess);
			CloseHandle(ProcessInfo.ProcessHandle);
			return false;
		}

		Status = RtlCreateUserProcess(&FileNameGame, 0x00000040, GameParam, NULL, NULL, ProcessInfo.ProcessHandle, TRUE, NULL, NULL, &ProcessInfo);
		if (Status)
		{
			RtlFreeUnicodeString(&FileNameGame);
			RtlFreeUnicodeString(&FileName);
			RtlDestroyProcessParameters(UserProcessParam);
			CloseHandle(OwnProcess);
			CloseHandle(ProcessInfo.ProcessHandle);
			CloseHandle(ProcessInfo.ThreadHandle);
			return false;
		}

		TerminateProcess(hProcess, (NTSTATUS)(ProcessInfo.ClientId.UniqueProcess));
		CloseHandle(hProcess);

		if (Suspended == FALSE)
			ResumeThread(ProcessInfo.ThreadHandle);
		pId = (std::uint32_t)(ProcessInfo.ClientId.UniqueProcess);
		if (ThreadHandle)
			*ThreadHandle = ProcessInfo.ThreadHandle;
		if (ProcessHandle)
			*ProcessHandle = ProcessInfo.ProcessHandle;
		if (UserProcessParam)
			RtlDestroyProcessParameters(UserProcessParam);
		if (GameParam)
			RtlDestroyProcessParameters(GameParam);
		if (ProcessInfo.ThreadHandle && !ThreadHandle)
			CloseHandle(ProcessInfo.ThreadHandle);
		if (ProcessInfo.ProcessHandle && !ProcessHandle)
			CloseHandle(ProcessInfo.ProcessHandle);
		if (FileName.Buffer)
			RtlFreeUnicodeString(&FileName);
		if (FileNameGame.Buffer)
			RtlFreeUnicodeString(&FileNameGame);
		if (OwnProcess)
			CloseHandle(OwnProcess);
		Status = true;
		return Status;
	}
};
