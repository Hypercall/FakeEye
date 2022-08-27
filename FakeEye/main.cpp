#include "be_service.hpp"
#include "game_launcher.hpp"
#include "installer.hpp"
#include <TlHelp32.h>

int main()
{

	// install BEService
	{
		std::cout << "Starting BattlEye Service..." << std::endl;
		if (!installer::Uninstall() ||
			!installer::Install())
		{
			std::cout << "Failed to start BattlEye Service " << std::endl;
			std::cout << "Installing BattlEye Service..." << std::endl;
			if (!installer::Uninstall() ||
				!installer::Install())
			{
				std::cout << "Failed to start BattlEye Service (" << GetLastError() << ")" << std::endl;
				std::cin.get();
			}
		}
		std::cout << "Successfully installed BattlEye Service." << std::endl;
	}

	auto BEPipe = new BEService();
	// init pipe
	{
		if (!BEPipe)
		{
			std::cout << "Failed to communicate to BattlEye Service " << std::endl;
			std::cin.get();
		}

		auto EndTime = time(NULL) + 10;
		while (time(NULL) < EndTime)
		{
			Sleep(100);
			if (BEPipe->Open())
				break;
		}

		if (time(NULL) > EndTime ||
			!BEPipe->Write(BEService::PACKET_ID::INIT_SERVICE, 0))
		{
			std::cout << "Failed to communicate to BattlEye Service (" << GetLastError() << ")" << std::endl;
			std::cin.get();
		}

		std::uint8_t PipeBuffer[1024];
		std::memset(PipeBuffer, 0, 1024);

		EndTime = time(NULL) + 10;

		/* Waiting for BEs message so we can start the game */
		while (time(NULL) < EndTime)
		{
			auto BytesRead = BEPipe->Read(PipeBuffer, 1024);
			if (BytesRead != 0 && BytesRead != -1)
				break;

			Sleep(100);
		}

		if (time(NULL) > EndTime)
		{
			std::cout << "BattlEye Service timeout (" << GetLastError() << ")" << std::endl;
			std::cin.get();
		}
	}

	HANDLE hProc = INVALID_HANDLE_VALUE;
	HANDLE hThread = INVALID_HANDLE_VALUE;
	auto Pid = 0;

	// parse arguments and start game
	{
		std::wstring WCommandLine = L"";
		wchar_t ExePath[1024];
		std::memset(ExePath, 0, 1024);

		auto be_config = config_reader::ReadConfig();

		if (!be_config)
		{
			std::cout << "Config is zero" << std::endl;
			std::cin.get();
		}

		std::cout << "Launching game..." << std::endl;

		auto Exe = wcslen(be_config->Exe32) ? be_config->Exe32 : be_config->Exe64;

		/* Get the arguments from the cmdline or from the initialization file*/
		auto argc_ = 0;
		auto argv_ = CommandLineToArgvW(GetCommandLineW(), &argc_);

		// Parse process arguments
		if (argc_ > 1)
		{
			for (int i = 1; i < argc_; i++)
			{
				if (i == 1)
					WCommandLine += argv_[1];
				else
				{
					WCommandLine += L" ";
					WCommandLine += argv_[i];
				}
			}
		}

		GetModuleFileNameW(0, ExePath, 1024);
			

		swprintf_s(ExePath, L"%s/%s", std::filesystem::path(ExePath).parent_path().generic_wstring().c_str(), Exe);
		Exe = ExePath;

		if (argc_ > 1)
		{
			if (!game_handler::StartGame(const_cast<wchar_t*>(WCommandLine.c_str()), Exe, &hProc, &hThread, FALSE))
			{
				std::cout << "Failed to launch the game (" << GetLastError() << ")" << std::endl;
				delete be_config;
				std::cin.get();
			}
		}
		else
		{
			if (!game_handler::StartGame(be_config->BEArg, Exe, &hProc, &hThread, FALSE))
			{
				std::cout << "Failed to launch the game (" << GetLastError() << ")" << std::endl;
				delete be_config;
				std::cin.get();
			}
		}


		Pid = GetProcessId(hProc);
		if (!BEPipe->Write(BEService::PACKET_ID::NOTIFY_GAME_PROCESS, (void*)Pid))
		{
			TerminateProcess(hProc, 1);
			CloseHandle(hProc);
			CloseHandle(hThread);
			std::cout << "Failed to communicate to BattlEye Service (" << GetLastError() << ")" << std::endl;
			delete be_config;
			std::cin.get();
			return 0;
		}
		delete be_config;

	}



	// TODO:

	// do something with full process access:
	// ....
	
	BEPipe->Close();
	delete BEPipe;
	CloseHandle(hThread);
	CloseHandle(hProc);
	hProc = OpenProcess(SYNCHRONIZE, false, Pid);
	WaitForSingleObject(hProc,INFINITE);
	CloseHandle(hProc);
	return true;
}