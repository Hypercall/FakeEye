#pragma once
#include <windows.h>
#include <iostream>
#include <filesystem>

/**
 * @namespace config_reader
 * @brief Responsible for reading the configuration of BattlEye
*/
namespace config_reader
{
	/**
	 * @brief Holds the attributes found in the BattlEye configuration
	 */
	struct be_config
	{
		/**
		 * Used to identify the game
		 */
		wchar_t GameID[512];
		/**
		 * Used to start the 32 bit game
		 */
		wchar_t Exe32[512];
		/**
		 * Used to start the 64 bit game
		 */
		wchar_t Exe64[512];
		/**
		 * Used as process argument
		 */
		wchar_t BEArg[512];

		/**
		 * Used for communication
		 */
		int BasePort;
	};

	/**
	 * Reads the configuration found in the game folder
	 *
	 * @returns The read config struct
	 */
	auto ReadConfig()->be_config*
	{

		be_config* config = nullptr;

		wchar_t Path[512];
		std::memset(Path, 0, 512);
		wchar_t ExePath[512];
		std::memset(ExePath, 0, 512);


		if (!GetModuleFileNameW(0, ExePath, 512))
			return nullptr;

		config = new be_config();
		if (!config)
			return nullptr;
		std::memset(config->GameID, 0, 512);
		std::memset(config->Exe32, 0, 512);
		std::memset(config->Exe64, 0, 512);
		std::memset(config->BEArg, 0, 512);

		swprintf_s(Path, L"%s/%s", std::filesystem::path(ExePath).parent_path().generic_wstring().c_str(), L"BattlEye/BELauncher.ini");

		config->BasePort = GetPrivateProfileIntW(L"Launcher", L"BasePort", 0, Path);
		GetPrivateProfileStringW(L"Launcher", L"GameID", 0, config->GameID, 512, Path);
		GetPrivateProfileStringW(L"Launcher", L"32BitExe", 0, config->Exe32, 512, Path);
		GetPrivateProfileStringW(L"Launcher", L"64BitExe", 0, config->Exe64, 512, Path);
		GetPrivateProfileStringW(L"Launcher", L"BEArg", 0, config->BEArg, 512, Path);

		return config;
	};
}