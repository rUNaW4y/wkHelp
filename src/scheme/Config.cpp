#include "Config.h"
#include "Debugf.h"
#include <windows.h>
#include <cstdlib>

namespace fs = std::filesystem;

namespace {
	typedef unsigned long long QWORD;
#define MAKELONGLONG(lo,hi) ((LONGLONG(DWORD(lo) & 0xffffffff)) | LONGLONG(DWORD(hi) & 0xffffffff) << 32 )
#define MAKEQWORD(LO2, HI2, LO1, HI1) MAKELONGLONG(MAKELONG(LO2,HI2),MAKELONG(LO1,HI1))
#define QV(V1, V2, V3, V4) MAKEQWORD(V4, V3, V2, V1)

	int readProfileInt(const char *section, const char *key, int fallback, const std::string &path) {
		char buff[64] = {};
		GetPrivateProfileStringA(section, key, "", buff, sizeof(buff), path.c_str());
		if (buff[0] == '\0') {
			return fallback;
		}

		char *end = nullptr;
		long value = std::strtol(buff, &end, 0);
		if (end == buff) {
			return fallback;
		}
		return static_cast<int>(value);
	}

	QWORD getModuleVersion(HMODULE hModule) {
		char waPath[MAX_PATH];
		DWORD handle = 0;
		GetModuleFileNameA(hModule, waPath, MAX_PATH);
		DWORD size = GetFileVersionInfoSizeA(waPath, &handle);
		if (!size) {
			return 0;
		}

		void *buffer = malloc(size);
		if (!buffer) {
			return 0;
		}

		QWORD version = 0;
		if (GetFileVersionInfoA(waPath, handle, size, buffer)) {
			VS_FIXEDFILEINFO *info = nullptr;
			UINT infoSize = 0;
			if (VerQueryValueA(buffer, "\\", reinterpret_cast<LPVOID *>(&info), &infoSize)
				&& info
				&& info->dwSignature == 0xFEEF04BD) {
				version = MAKELONGLONG(info->dwFileVersionLS, info->dwFileVersionMS);
			}
		}

		free(buffer);
		return version;
	}
}

void Config::readConfig() {
	char waBuff[MAX_PATH];
	GetModuleFileNameA(nullptr, waBuff, sizeof(waBuff));
	waDir = fs::path(waBuff).parent_path();
	const auto iniPath = (waDir / iniFile).string();

	moduleEnabled = readProfileInt("general", "EnableModule", 1, iniPath) != 0;
	useOffsetCache = readProfileInt("general", "UseOffsetCache", 1, iniPath) != 0;
	ignoreVersionCheck = readProfileInt("general", "IgnoreVersionCheck", 0, iniPath) != 0;
}

bool Config::isModuleEnabled() {
	return moduleEnabled;
}

int Config::waVersionCheck() {
	if (ignoreVersionCheck) {
		return 1;
	}

	const auto version = getModuleVersion(nullptr);
	char versionStr[64];
	_snprintf_s(versionStr, _TRUNCATE, "Detected game version: %u.%u.%u.%u", PWORD(&version)[3], PWORD(&version)[2], PWORD(&version)[1], PWORD(&version)[0]);
	debugf("%s\n", versionStr);

	const std::string title = getFullStr();
	char buff[512];
	if (version < QV(3, 8, 0, 0)) {
		_snprintf_s(buff, _TRUNCATE, "wkSchemeInfo is not compatible with WA versions older than 3.8.0.0.\n\n%s", versionStr);
		MessageBoxA(nullptr, buff, title.c_str(), MB_OK | MB_ICONERROR);
		return 0;
	}
	if (version >= QV(3, 9, 0, 0)) {
		_snprintf_s(buff, _TRUNCATE, "wkSchemeInfo is not compatible with WA versions 3.9.x.x and newer.\n\n%s", versionStr);
		MessageBoxA(nullptr, buff, title.c_str(), MB_OK | MB_ICONERROR);
		return 0;
	}
	if (version == QV(3, 8, 0, 0) || version == QV(3, 8, 1, 0)) {
		return 1;
	}

	_snprintf_s(buff, _TRUNCATE, "wkSchemeInfo is not designed to work with your WA version and may malfunction.\n\nTo disable this warning set IgnoreVersionCheck=1 in wkSchemeInfo.ini.\n\n%s", versionStr);
	return MessageBoxA(nullptr, buff, title.c_str(), MB_OKCANCEL | MB_ICONWARNING) == IDOK;
}

const fs::path &Config::getWaDir() {
	return waDir;
}

std::string Config::getVersionStr() {
	return "v0.1.0";
}

std::string Config::getBuildStr() {
	return __DATE__ " " __TIME__;
}

std::string Config::getModuleStr() {
	return moduleName;
}

std::string Config::getFullStr() {
	return getModuleStr() + " " + getVersionStr() + " (build: " + getBuildStr() + ")";
}

bool Config::isUseOffsetCache() {
	return useOffsetCache;
}

std::string Config::getWaVersionAsString() {
	 char buff[32];
	 const auto version = getModuleVersion(nullptr);
	 sprintf_s(buff, "%u.%u.%u.%u", PWORD(&version)[3], PWORD(&version)[2], PWORD(&version)[1], PWORD(&version)[0]);
	 return buff;
}

const std::string &Config::getCacheFile() {
	return cacheFile;
}
