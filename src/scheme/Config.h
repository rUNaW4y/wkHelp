#ifndef WKSCHEMEINFO_CONFIG_H
#define WKSCHEMEINFO_CONFIG_H

#include <filesystem>
#include <string>

class Config {
public:
	static inline const std::string iniFile = "wkHelp.ini";
	static inline const std::string cacheFile = "wkHelpScheme.cache";
	static inline const std::string moduleName = "wkHelp";

private:
	static inline bool moduleEnabled = true;
	static inline bool ignoreVersionCheck = false;
	static inline bool useOffsetCache = true;
	static inline std::filesystem::path waDir;

public:
	static void readConfig();
	static bool isModuleEnabled();
	static int waVersionCheck();
	static const std::filesystem::path &getWaDir();

	static std::string getVersionStr();
	static std::string getBuildStr();
	static std::string getModuleStr();
	static std::string getFullStr();

	static bool isUseOffsetCache();
	static std::string getWaVersionAsString();
	static const std::string &getCacheFile();
};

#endif
