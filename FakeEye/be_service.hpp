#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include "config_reader.hpp"
#include "installer.hpp"

/**
 * @brief Responsible for the communication between the launcher and BEService
 */
class BEService
{
public:

	/**
	 * @enum PACKET_ID
	 * @brief Packet-Ids discovered in the BattlEye launcher
	 */
	enum class PACKET_ID : std::uint8_t
	{
		/**
		 * Used to transfer the BattlEye configuration
		 */
		INIT_SERVICE = 0,
		/**
		 * Used to notify the BattlEye service about the started game
		 */
		NOTIFY_GAME_PROCESS = 3
	};

	/**
	 * Constructor
	 *
	 */
	BEService()
	{
		hFile = INVALID_HANDLE_VALUE;
	}

	/**
	 * Deconstructor
	 *
	 */
	~BEService()
	{
		if (hFile)
			CloseHandle(hFile);
	}

	/**
	 * Opens the pipe of BEService
	 *
	 * @return result of CreateService
	 */
	auto Open()->boolean
	{
		hFile = CreateFileA(
			"\\\\.\\pipe\\BattlEye",
			GENERIC_READ | GENERIC_WRITE,
			0,
			0,
			OPEN_ALWAYS,
			0,
			0);
		return hFile != INVALID_HANDLE_VALUE;
	}

	/**
	 * Closes the opened pipe of BEService
	 *
	 */
	auto Close()->void
	{
		CloseHandle(hFile);
		hFile = 0;
		return;
	}

	/**
	 * Read any message from the pipe
	 *
	 * @param Out The buffer 
	 * @param Size The size of the buffer
	 * 
	 * @returns The number of bytes read
	 */
	auto Read(void* Out, std::uint32_t Size)->int const
	{
		DWORD dwReaded = 0;
		if (!ReadFile(this->hFile, Out, Size, &dwReaded, 0) && GetLastError() == ERROR_BROKEN_PIPE)
			return -1;
		return dwReaded;
	}

	/**
	 * Checks if the pipe is opened
	 *
	 * @returns whether the pipe is open or not
	 */
	auto IsOpened()->boolean const
	{
		return hFile != INVALID_HANDLE_VALUE;
	}

	/**
	 * Writes a packet to the pipe
	 *
	 * @param Id The specified BE packet id
	 * @param Size An additional argument, for example, the process-id of the game
	 *
	 * @returns The number of bytes read
	 */
	auto Write(PACKET_ID Id, void* Argument)->boolean
	{
		boolean Result = false;
		std::uint8_t* Buffer = nullptr, *StartBuffer = nullptr;
		config_reader::be_config* config = nullptr;
		DWORD dwWritten = 0;
		switch (Id)
		{
		case PACKET_ID::INIT_SERVICE:

			config = config_reader::ReadConfig();
			if (!config)
				break;

			Buffer = (std::uint8_t*)VirtualAlloc(0, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
			if (!Buffer)
				break;

			StartBuffer = Buffer;

			// Build packet buffer
			{
				*(std::uint8_t*)(Buffer) = (std::uint8_t)PACKET_ID::INIT_SERVICE;
				Buffer++;
				auto GameIdLen = wcslen(config->GameID);;
				*(std::uint8_t*)(Buffer) = wcslen(config->GameID);
				Buffer++;
				wmemcpy((wchar_t*)Buffer, config->GameID, GameIdLen);
				Buffer += GameIdLen * 2;
				*(std::uint16_t*)(Buffer) = config->BasePort;
				Buffer += sizeof(std::uint16_t);
				*(std::uint32_t*)(Buffer) = GetCurrentProcessId();
				Buffer += sizeof(std::uint32_t);
				auto Exe = wcslen(config->Exe32) ? config->Exe32 : config->Exe64;
				Buffer++;
				auto ExeLen = wcslen(Exe);
				*(std::uint8_t*)(Buffer) = ExeLen;
				Buffer++;
				wmemcpy((wchar_t*)Buffer, Exe, ExeLen);
				Buffer += ExeLen * sizeof(wchar_t);
				wmemcpy((wchar_t*)Buffer, installer::GetInstallationPath().c_str(), installer::GetInstallationPath().length());
				Buffer += installer::GetInstallationPath().length() * 2;
			}
			// send buffer
			{
				auto PacketSize = (Buffer - StartBuffer);
				Result = WriteFile(hFile, StartBuffer, PacketSize, &dwWritten, 0);
			}
			break;
		case PACKET_ID::NOTIFY_GAME_PROCESS:
			Buffer = (std::uint8_t*)VirtualAlloc(0, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

			if (!Buffer)
				break;

			// construct and send packet
			{
				Buffer = Buffer;
				*(std::uint8_t*)(Buffer) = (std::uint8_t)PACKET_ID::NOTIFY_GAME_PROCESS;
				Buffer++;
				*(std::uint32_t*)(Buffer) = (std::uint32_t)Argument;
				Result = WriteFile(hFile, Buffer, 5, &dwWritten, 0);
			}
			break;
		}

		if(Buffer)
			VirtualFree(Buffer, 0, MEM_RELEASE);
		if (config)
			delete config;

		return Result;
	}

private:
	/**
	 * Pipe-Handle used to read and write data
	 *
	 */
	HANDLE hFile = 0;
};