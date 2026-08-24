#include "SchemeInfo.h"
#include "Config.h"
#include "Diagnostics.h"
#include "Hooks.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {
	constexpr size_t PayloadSizeV1 = 0xDD - 5;
	constexpr size_t PayloadSizeV2 = 0x129 - 5;
	constexpr size_t StandardOptionBytes = 36;

	constexpr int ObjectCount5Step = 30;
	constexpr int ObjectCount10Step = 44;
	constexpr int ObjectCountLastStep = 59;

	constexpr int WeaponIndexFreeze = 45;
	constexpr int WeaponIndexSuperBananaBomb = 46;
	constexpr int WeaponIndexMineStrike = 47;
	constexpr int WeaponIndexEarthquake = 49;
	constexpr int WeaponIndexScalesOfJustice = 50;
	constexpr int WeaponIndexMagicBullet = 53;
	constexpr int WeaponIndexNuclearTest = 54;
	constexpr int WeaponIndexSelectWorm = 55;
	constexpr int WeaponIndexSalvationArmy = 56;
	constexpr int WeaponIndexMoleSquadron = 57;
	constexpr int WeaponIndexMBBomb = 58;
	constexpr int WeaponIndexConcreteDonkey = 59;
	constexpr int WeaponIndexSuicideBomber = 60;
	constexpr int WeaponIndexSheepStrike = 61;
	constexpr int WeaponIndexMailStrike = 62;
	constexpr int WeaponIndexArmageddon = 63;

	enum class SchemeSourceKind {
		Unknown,
		Builtin,
		Custom
	};

	DWORD addrSchemeStruct = 0;
	DWORD addrGetSchemeVersion = 0;
	std::string currentSchemeName;
	std::filesystem::path currentSchemePath;
	SchemeSourceKind currentSchemeSource = SchemeSourceKind::Unknown;
	int currentBuiltinSchemeId = 0;

#pragma pack(push, 1)
	struct RawWeaponSetting {
		int8_t ammo;
		uint8_t power;
		int8_t delay;
		int8_t probability;
	};

	struct RawExtendedOptions {
		uint32_t dataVersion;
		uint8_t constantWind;
		int16_t wind;
		uint8_t windBias;
		int32_t gravity;
		int32_t friction;
		uint8_t ropeKnockForce;
		uint8_t bloodAmount;
		uint8_t ropeUpgrade;
		uint8_t groupPlaceAllies;
		uint8_t noCrateProbability;
		uint16_t crateLimit;
		uint8_t suddenDeathNoWormSelect;
		uint8_t suddenDeathTurnDamage;
		uint8_t wormPhasingAlly;
		uint8_t wormPhasingEnemy;
		uint8_t circularAim;
		uint8_t antiLockAim;
		uint8_t antiLockPower;
		uint8_t wormSelectKeepHotSeat;
		uint8_t wormSelectAnytime;
		uint8_t battyRope;
		uint8_t ropeRollDrops;
		uint8_t keepControlXImpact;
		uint8_t keepControlHeadBump;
		uint8_t keepControlSkim;
		uint8_t explosionFallDamage;
		uint8_t objectPushByExplosion;
		uint8_t undeterminedCrates;
		uint8_t undeterminedMineFuse;
		uint8_t firingPausesTimer;
		uint8_t loseControlDoesntEndTurn;
		uint8_t shotDoesntEndTurn;
		uint8_t shotDoesntEndTurnAll;
		uint8_t drillImpartsVelocity;
		uint8_t girderRadiusAssist;
		uint16_t flameTurnDecay;
		uint8_t flameTouchDecay;
		uint16_t flameLimit;
		int32_t projectileMaxSpeed;
		int32_t ropeMaxSpeed;
		int32_t jetpackMaxSpeed;
		int32_t gameSpeed;
		uint8_t indianRopeGlitch;
		uint8_t herdDoublingGlitch;
		uint8_t jetpackBungeeGlitch;
		uint8_t angleCheatGlitch;
		uint8_t glideGlitch;
		uint8_t skipWalk;
		uint8_t roofing;
		uint8_t floatingWeaponGlitch;
		int32_t wormBounce;
		int32_t viscosity;
		uint8_t viscosityWorms;
		int32_t rwWind;
		uint8_t rwWindWorms;
		uint8_t rwGravityType;
		int32_t rwGravity;
		uint8_t crateRate;
		uint8_t crateShower;
		uint8_t antiSink;
		uint8_t weaponsDontChange;
		uint8_t extendedFuse;
		uint8_t autoReaim;
		uint8_t terrainOverlapGlitch;
		uint8_t roundTimeFractional;
		uint8_t autoRetreat;
		uint8_t healthCure;
		uint8_t kaosMod;
		uint8_t sheepHeavenFlags;
		uint8_t conserveUtilities;
		uint8_t expediteUtilities;
		uint8_t doubleTimeCount;
	};
#pragma pack(pop)

	static_assert(sizeof(RawExtendedOptions) == 110, "Unexpected extended options layout");

	std::pair<int, size_t> getSchemeVersionAndSize() {
		if (!addrSchemeStruct || !addrGetSchemeVersion) {
			return {0, 0};
		}

		size_t schemeSize = 0;
		int version = 0;
		_asm lea eax, schemeSize
		_asm push eax
		_asm mov esi, addrSchemeStruct
		_asm call addrGetSchemeVersion
		_asm mov version, eax
		return {version, schemeSize};
	}

	bool getBit(uint8_t value, int bit) {
		return (value & (1u << bit)) != 0;
	}

	float fixedPointToFloat(int32_t value) {
		return static_cast<float>(value) / 65536.0f;
	}

	std::optional<bool> triStateBool(uint8_t value) {
		if (value == 128) {
			return std::nullopt;
		}
		return value != 0;
	}

	uint8_t waterRiseRateFromIndex(uint8_t index) {
		return static_cast<uint8_t>((index * index * 5) % 256);
	}

	std::string boolWord(bool value) {
		return value ? "on" : "off";
	}

	std::string optionalFlag(const std::optional<bool> &value) {
		if (!value.has_value()) {
			return "default";
		}
		return *value ? "on" : "off";
	}

	std::string objectTypesToString(uint8_t objectTypes) {
		switch (objectTypes & 0x3) {
			case 0:
				return "none";
			case 1:
				return "mines";
			case 2:
				return "oil drums";
			case 3:
				return "mines + oil drums";
			default:
				return "unknown";
		}
	}

	std::string stockpilingToString(uint8_t value) {
		switch (value) {
			case 0:
				return "off";
			case 1:
				return "on";
			case 2:
				return "anti";
			default:
				return "unknown";
		}
	}

	std::string wormSelectToString(uint8_t value) {
		switch (value) {
			case 0:
				return "sequential";
			case 1:
				return "manual";
			case 2:
				return "random";
			default:
				return "unknown";
		}
	}

	std::string suddenDeathToString(uint8_t value) {
		switch (value) {
			case 0:
				return "round end";
			case 1:
				return "nuclear strike";
			case 2:
				return "health drop";
			case 3:
				return "water rise";
			default:
				return "unknown";
		}
	}

	std::string sourceKindToString(SchemeSourceKind source) {
		switch (source) {
			case SchemeSourceKind::Builtin:
				return "builtin";
			case SchemeSourceKind::Custom:
				return "custom";
			default:
				return "unknown";
		}
	}

	std::string builtinSchemeNameFromId(int schemeId) {
		switch (schemeId) {
			case 1:
				return "Beginner";
			case 2:
				return "Intermediate";
			case 3:
				return "Pro";
			case 4:
				return "Tournament";
			case 5:
				return "Classic";
			case 6:
				return "Retro";
			case 7:
				return "Artillery";
			case 8:
				return "Sudden Sinking";
			case 9:
				return "Strategic";
			case 10:
				return "The Darkside";
			case 11:
				return "Armageddon";
			case 12:
				return "Blast Zone";
			case 13:
				return "The Full Wormage";
			default: {
				if (schemeId > 0) {
					return "Builtin #" + std::to_string(schemeId);
				}
				return {};
			}
		}
	}

	void decodeObjectCombo(uint8_t raw, int version, int &count, uint8_t &types) {
		if (version == 1) {
			count = 8;
			switch (raw) {
				case 0:
					types = 0;
					break;
				case 2:
					types = 2;
					break;
				case 5:
					types = 3;
					break;
				default:
					types = 1;
					break;
			}
			return;
		}

		switch (raw) {
			case 3:
			case 4:
				raw = 1;
				break;
			case 6:
			case 7:
				raw = 0;
				break;
			default:
				break;
		}

		int step = std::min(raw < 8 ? 8 : raw / 4 - 2, ObjectCountLastStep);
		if (step >= ObjectCount10Step) {
			step = 100 + 10 * (step - ObjectCount10Step);
		} else if (step >= ObjectCount5Step) {
			step = 30 + 5 * (step - ObjectCount5Step);
		}

		count = step;
		types = raw == 5 ? 3 : static_cast<uint8_t>(raw & 0x3);
	}

	void appendParts(std::vector<std::string> &lines, const std::string &label, const std::vector<std::string> &parts, size_t maxLen = 118) {
		std::string current = "[wkSchemeInfo] " + label;
		bool hasPart = false;
		for (const auto &part : parts) {
			if (part.empty()) {
				continue;
			}
			const std::string token = hasPart ? " | " + part : part;
			if (hasPart && current.size() + token.size() > maxLen) {
				lines.push_back(current);
				current = "[wkSchemeInfo] " + part;
			} else {
				current += token;
			}
			hasPart = true;
		}

		if (hasPart) {
			lines.push_back(current);
		}
	}

	void wrapText(std::vector<std::string> &lines, const std::string &label, const std::string &text, size_t maxLen = 118) {
		if (text.empty()) {
			return;
		}

		std::istringstream iss(text);
		std::string word;
		std::string current = "[wkSchemeInfo] " + label;
		bool firstWord = true;
		while (iss >> word) {
			const std::string token = firstWord ? word : " " + word;
			if (!firstWord && current.size() + token.size() > maxLen) {
				lines.push_back(current);
				current = "[wkSchemeInfo] " + word;
			} else {
				current += token;
			}
			firstWord = false;
		}
		if (!firstWord) {
			lines.push_back(current);
		}
	}

	void loadVersion3Extended(const uint8_t *payload, size_t extraSize, SchemeInfo::ExtendedSummary &extended) {
		extended.bytesPresent = std::min(extraSize, sizeof(RawExtendedOptions));
		RawExtendedOptions raw = {};
		raw.wind = 100;
		raw.windBias = 15;
		raw.gravity = 15728;
		raw.friction = 62914;
		raw.ropeKnockForce = 0xFF;
		raw.bloodAmount = 0xFF;
		raw.noCrateProbability = 0xFF;
		raw.crateLimit = 5;
		raw.suddenDeathNoWormSelect = 1;
		raw.suddenDeathTurnDamage = 5;
		raw.objectPushByExplosion = 128;
		raw.undeterminedCrates = 128;
		raw.undeterminedMineFuse = 128;
		raw.drillImpartsVelocity = 128;
		raw.flameTurnDecay = 13106;
		raw.flameTouchDecay = 30;
		raw.flameLimit = 200;
		raw.projectileMaxSpeed = 32 << 16;
		raw.ropeMaxSpeed = 16 << 16;
		raw.jetpackMaxSpeed = 5 << 16;
		raw.gameSpeed = 1 << 16;
		raw.indianRopeGlitch = 128;
		raw.herdDoublingGlitch = 128;
		raw.jetpackBungeeGlitch = 1;
		raw.angleCheatGlitch = 1;
		raw.glideGlitch = 1;
		raw.floatingWeaponGlitch = 1;
		raw.rwGravity = 1 << 16;
		raw.terrainOverlapGlitch = 128;
		raw.healthCure = 1;
		raw.sheepHeavenFlags = 7;
		raw.doubleTimeCount = 1;
		if (payload && extended.bytesPresent) std::memcpy(&raw, payload, extended.bytesPresent);

		extended.dataVersion = raw.dataVersion;
		extended.ropeKnockForce = raw.ropeKnockForce;
		extended.bloodAmount = raw.bloodAmount;
		extended.groupPlaceAllies = raw.groupPlaceAllies != 0;
		extended.suddenDeathNoWormSelect = raw.suddenDeathNoWormSelect != 0;
		extended.suddenDeathTurnDamage = raw.suddenDeathTurnDamage;
		extended.wormPhasingAlly = raw.wormPhasingAlly;
		extended.wormPhasingEnemy = raw.wormPhasingEnemy;
		extended.circularAim = raw.circularAim != 0;
		extended.antiLockAim = raw.antiLockAim != 0;
		extended.antiLockPower = raw.antiLockPower != 0;
		extended.wormSelectKeepHotSeat = raw.wormSelectKeepHotSeat != 0;
		extended.ropeRollDrops = raw.ropeRollDrops;
		extended.keepControlXImpact = raw.keepControlXImpact;
		extended.keepControlHeadBump = raw.keepControlHeadBump != 0;
		extended.keepControlSkim = raw.keepControlSkim;
		extended.explosionFallDamage = raw.explosionFallDamage != 0;
		extended.objectPushByExplosion = triStateBool(raw.objectPushByExplosion);
		extended.shotDoesntEndTurnAll = raw.shotDoesntEndTurnAll != 0;
		extended.drillImpartsVelocity = triStateBool(raw.drillImpartsVelocity);
		extended.girderRadiusAssist = raw.girderRadiusAssist != 0;
		extended.flameTurnDecay = static_cast<float>(raw.flameTurnDecay) / 65536.0f;
		extended.flameTouchDecay = raw.flameTouchDecay;
		extended.flameLimit = raw.flameLimit;
		extended.projectileMaxSpeed = fixedPointToFloat(raw.projectileMaxSpeed);
		extended.ropeMaxSpeed = fixedPointToFloat(raw.ropeMaxSpeed);
		extended.jetpackMaxSpeed = fixedPointToFloat(raw.jetpackMaxSpeed);
		extended.gameSpeed = fixedPointToFloat(raw.gameSpeed);
		extended.indianRopeGlitch = triStateBool(raw.indianRopeGlitch);
		extended.herdDoublingGlitch = triStateBool(raw.herdDoublingGlitch);
		extended.jetpackBungeeGlitch = raw.jetpackBungeeGlitch != 0;
		extended.angleCheatGlitch = raw.angleCheatGlitch != 0;
		extended.glideGlitch = raw.glideGlitch != 0;
		extended.skipWalk = raw.skipWalk;
		extended.roofing = raw.roofing;
		extended.floatingWeaponGlitch = raw.floatingWeaponGlitch != 0;
		extended.terrainOverlapGlitch = triStateBool(raw.terrainOverlapGlitch);
		extended.healthCure = raw.healthCure;
		extended.kaosMod = raw.kaosMod;
		extended.sheepHeavenFlags = raw.sheepHeavenFlags;
		extended.conserveUtilities = raw.conserveUtilities != 0;
		extended.expediteUtilities = raw.expediteUtilities != 0;
		extended.doubleTimeCount = raw.doubleTimeCount;

#define FIELD_AVAILABLE(field) (extended.bytesPresent >= offsetof(RawExtendedOptions, field) + sizeof(raw.field))

		if (FIELD_AVAILABLE(crateRate)) {
			extended.hasCrateRate = true;
			extended.crateRate = raw.crateRate;
		}
		if (FIELD_AVAILABLE(crateLimit)) {
			extended.hasCrateLimit = true;
			extended.crateLimit = raw.crateLimit;
		}
		if (FIELD_AVAILABLE(noCrateProbability) && raw.noCrateProbability != 0xFF) {
			extended.hasNoCrateProbability = true;
			extended.noCrateProbability = raw.noCrateProbability;
		}
		if (FIELD_AVAILABLE(undeterminedCrates)) {
			extended.hasUndeterminedCrates = true;
			extended.undeterminedCrates = triStateBool(raw.undeterminedCrates);
		}
		if (FIELD_AVAILABLE(undeterminedMineFuse)) {
			extended.hasUndeterminedMineFuse = true;
			extended.undeterminedMineFuse = triStateBool(raw.undeterminedMineFuse);
		}
		if (FIELD_AVAILABLE(shotDoesntEndTurn)) {
			extended.hasShotDoesntEndTurn = true;
			extended.shotDoesntEndTurn = raw.shotDoesntEndTurn != 0;
		}
		if (FIELD_AVAILABLE(loseControlDoesntEndTurn)) {
			extended.hasLoseControlDoesntEndTurn = true;
			extended.loseControlDoesntEndTurn = raw.loseControlDoesntEndTurn != 0;
		}
		if (FIELD_AVAILABLE(firingPausesTimer)) {
			extended.hasFiringPausesTimer = true;
			extended.firingPausesTimer = raw.firingPausesTimer != 0;
		}
		if (FIELD_AVAILABLE(ropeUpgrade)) {
			extended.hasRopeUpgrade = true;
			extended.ropeUpgrade = raw.ropeUpgrade != 0;
		}
		if (FIELD_AVAILABLE(crateShower)) {
			extended.hasCrateShower = true;
			extended.crateShower = raw.crateShower != 0;
		}
		if (FIELD_AVAILABLE(antiSink)) {
			extended.hasAntiSink = true;
			extended.antiSink = raw.antiSink != 0;
		}
		if (FIELD_AVAILABLE(weaponsDontChange)) {
			extended.hasWeaponsDontChange = true;
			extended.weaponsDontChange = raw.weaponsDontChange != 0;
		}
		if (FIELD_AVAILABLE(extendedFuse)) {
			extended.hasExtendedFuse = true;
			extended.extendedFuse = raw.extendedFuse != 0;
		}
		if (FIELD_AVAILABLE(autoReaim)) {
			extended.hasAutoReaim = true;
			extended.autoReaim = raw.autoReaim != 0;
		}
		if (FIELD_AVAILABLE(autoRetreat)) {
			extended.hasAutoRetreat = true;
			extended.autoRetreat = raw.autoRetreat != 0;
		}
		if (FIELD_AVAILABLE(roundTimeFractional)) {
			extended.hasRoundTimeFractional = true;
			extended.roundTimeFractional = raw.roundTimeFractional != 0;
		}
		if (FIELD_AVAILABLE(battyRope)) {
			extended.hasBattyRope = true;
			extended.battyRope = raw.battyRope != 0;
		}
		if (FIELD_AVAILABLE(wormSelectAnytime)) {
			extended.hasWormSelectAnytime = true;
			extended.wormSelectAnytime = raw.wormSelectAnytime != 0;
		}
		if (FIELD_AVAILABLE(constantWind)) {
			extended.hasConstantWind = true;
			extended.constantWind = raw.constantWind != 0;
		}
		if (FIELD_AVAILABLE(wind)) {
			extended.hasWind = true;
			extended.wind = raw.wind;
		}
		if (FIELD_AVAILABLE(windBias)) {
			extended.hasWindBias = true;
			extended.windBias = raw.windBias;
		}
		if (FIELD_AVAILABLE(gravity)) {
			extended.hasGravity = true;
			extended.gravity = fixedPointToFloat(raw.gravity);
		}
		if (FIELD_AVAILABLE(friction)) {
			extended.hasFriction = true;
			extended.friction = fixedPointToFloat(raw.friction);
		}
		if (FIELD_AVAILABLE(wormBounce)) {
			extended.hasWormBounce = true;
			extended.wormBounce = fixedPointToFloat(raw.wormBounce);
		}
		if (FIELD_AVAILABLE(viscosity)) {
			extended.hasViscosity = true;
			extended.viscosity = fixedPointToFloat(raw.viscosity);
		}
		if (FIELD_AVAILABLE(viscosityWorms)) {
			extended.hasViscosityWorms = true;
			extended.viscosityWorms = raw.viscosityWorms != 0;
		}
		if (FIELD_AVAILABLE(rwWind)) {
			extended.hasRwWind = true;
			extended.rwWind = fixedPointToFloat(raw.rwWind);
		}
		if (FIELD_AVAILABLE(rwWindWorms)) {
			extended.hasRwWindWorms = true;
			extended.rwWindWorms = raw.rwWindWorms != 0;
		}
		if (FIELD_AVAILABLE(rwGravityType)) {
			extended.hasRwGravityType = true;
			extended.rwGravityType = raw.rwGravityType;
		}
		if (FIELD_AVAILABLE(rwGravity)) {
			extended.hasRwGravity = true;
			extended.rwGravity = fixedPointToFloat(raw.rwGravity);
		}
		if (FIELD_AVAILABLE(healthCure)) {
			extended.hasHealthCure = true;
			extended.healthCure = raw.healthCure;
		}
		if (FIELD_AVAILABLE(sheepHeavenFlags)) {
			extended.hasSheepHeavenFlags = true;
			extended.sheepHeavenFlags = raw.sheepHeavenFlags;
		}
		if (FIELD_AVAILABLE(doubleTimeCount)) {
			extended.hasDoubleTimeCount = true;
			extended.doubleTimeCount = raw.doubleTimeCount;
		}

#undef FIELD_AVAILABLE
	}

	void deriveVersion2RubberWorm(SchemeInfo::Snapshot &snapshot) {
		auto &extended = snapshot.extended;
		extended.derivedFromRubberWorm = true;

		auto readProb = [&snapshot](int weaponIndex) -> uint8_t {
			return static_cast<uint8_t>(snapshot.weapons[weaponIndex].probability);
		};

		uint8_t prob = readProb(WeaponIndexMagicBullet);
		extended.hasCrateLimit = true;
		extended.crateLimit = prob == 0 ? 5 : prob;

		extended.hasCrateRate = true;
		extended.crateRate = readProb(WeaponIndexNuclearTest);

		extended.hasAntiSink = true;
		extended.antiSink = readProb(WeaponIndexSheepStrike) != 0;

		prob = readProb(WeaponIndexMoleSquadron);
		extended.hasShotDoesntEndTurn = true;
		extended.shotDoesntEndTurn = getBit(prob, 0);
		extended.hasLoseControlDoesntEndTurn = true;
		extended.loseControlDoesntEndTurn = getBit(prob, 1);
		extended.hasFiringPausesTimer = true;
		extended.firingPausesTimer = !getBit(prob, 2);
		extended.hasRopeUpgrade = true;
		extended.ropeUpgrade = getBit(prob, 3);
		extended.hasCrateShower = true;
		extended.crateShower = getBit(prob, 4);
		extended.hasWeaponsDontChange = true;
		extended.weaponsDontChange = getBit(prob, 6);
		extended.hasExtendedFuse = true;
		extended.extendedFuse = getBit(prob, 7);

		extended.hasWormSelectAnytime = true;
		extended.wormSelectAnytime = getBit(readProb(WeaponIndexMBBomb), 0);

		prob = readProb(WeaponIndexEarthquake);
		extended.hasAutoReaim = true;
		extended.autoReaim = getBit(prob, 0);

		prob = readProb(WeaponIndexSalvationArmy);
		if (prob > 0) {
			extended.hasFriction = true;
			extended.friction = static_cast<float>(prob) / 100.0f;
		}

		prob = readProb(WeaponIndexConcreteDonkey);
		extended.hasViscosity = true;
		extended.viscosity = static_cast<float>(prob) / 255.0f;
		extended.hasViscosityWorms = true;
		extended.viscosityWorms = (prob & 1) == 1;

		prob = readProb(WeaponIndexSuicideBomber);
		extended.hasRwWind = true;
		extended.rwWind = static_cast<float>(prob) / 255.0f;
		extended.hasRwWindWorms = true;
		extended.rwWindWorms = (prob & 1) == 1;

		extended.hasWormBounce = true;
		extended.wormBounce = static_cast<float>(readProb(WeaponIndexArmageddon)) / 255.0f;
	}

	void updateCurrentCustomScheme(char *path) {
		if (!path || path[0] == '\0') {
			return;
		}

		currentSchemePath = std::filesystem::path(path);
		currentSchemeName = currentSchemePath.stem().string();
		currentSchemeSource = SchemeSourceKind::Custom;
		currentBuiltinSchemeId = 0;
		Diagnostics::log("scheme custom: %s", currentSchemeName.c_str());
	}

	void updateCurrentBuiltinScheme(int schemeId) {
		currentBuiltinSchemeId = schemeId;
		currentSchemeName = builtinSchemeNameFromId(schemeId);
		currentSchemeSource = SchemeSourceKind::Builtin;
		currentSchemePath.clear();
		Diagnostics::log("scheme builtin: id=%d name=%s", schemeId, currentSchemeName.c_str());
	}

	size_t currentPayloadSize(int version, size_t extraSize) {
		switch (version) {
			case 1:
				return PayloadSizeV1;
			case 2:
				return PayloadSizeV2;
			case 3:
				return PayloadSizeV2 + extraSize;
			default:
				return 0;
		}
	}

	bool fileMatchesCurrentScheme(const std::filesystem::path &path, int version, const uint8_t *payload, size_t payloadSize) {
		std::ifstream input(path, std::ios::binary | std::ios::ate);
		if (!input || input.tellg() != static_cast<std::streamoff>(payloadSize + 5)) {
			return false;
		}
		input.seekg(0);
		std::vector<uint8_t> bytes(payloadSize + 5);
		if (!input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
			return false;
		}
		// The first four bytes are the SCHM signature and byte 4 is the format version.
		return bytes[4] == version && std::memcmp(bytes.data() + 5, payload, payloadSize) == 0;
	}

	std::filesystem::path findMatchingSchemeFile(int version, size_t extraSize) {
		if (!addrSchemeStruct) {
			return {};
		}
		const size_t payloadSize = currentPayloadSize(version, extraSize);
		if (!payloadSize) {
			return {};
		}
		const auto *payload = reinterpret_cast<const uint8_t *>(addrSchemeStruct + 0x14);

		// Prefer the path captured by SetWscScheme when it still describes the active bytes.
		if (!currentSchemePath.empty() && fileMatchesCurrentScheme(currentSchemePath, version, payload, payloadSize)) {
			return currentSchemePath;
		}

		const auto schemeDir = Config::getWaDir() / "User" / "Schemes";
		std::error_code error;
		if (!std::filesystem::is_directory(schemeDir, error)) {
			return {};
		}
		for (std::filesystem::recursive_directory_iterator it(
				schemeDir, std::filesystem::directory_options::skip_permission_denied, error), end;
			 it != end; it.increment(error)) {
			if (error) {
				error.clear();
				continue;
			}
			if (!it->is_regular_file(error) || it->path().extension() != ".wsc") {
				continue;
			}
			if (fileMatchesCurrentScheme(it->path(), version, payload, payloadSize)) {
				return it->path();
			}
		}
		return {};
	}

}

void SchemeInfo::install() {
	Config::readConfig();
	Hooks::loadOffsets();
	const DWORD addrGetSchemeSettingsFromWam = _ScanPattern("GetSchemeSettingsFromWam", "\x57\x6A\x04\x68\x00\x00\x00\x00\x8B\xF8\xE8\x00\x00\x00\x00\x83\xF8\xFF\x75\x04\x0B\xC0\x5F\xC3\x8B\x47\x0C\x56\x8B\x35\x00\x00\x00\x00\x50\x6A\x00\x68\x00\x00\x00\x00\x68\x00\x00\x00\x00\xFF\xD6", "????????xxx????xxxxxxxxxxxxxxx????xxxx????x????xx");
	addrGetSchemeVersion = _ScanPattern("GetSchemeVersion", "\xB8\x00\x00\x00\x00\xB9\x00\x00\x00\x00\x2B\xC8\x8D\x64\x24\x00\x8A\x94\x06\x00\x00\x00\x00\x3A\x14\x01\x75\x38\x83\xE8\x01\x75\xEF\x33\xC9\x8D\x86\x00\x00\x00\x00\x8D\xA4\x24\x00\x00\x00\x00", "??????????xxxxxxxxx????xxxxxxxxxxxxxx????xxx????");

	addrSchemeStruct = *reinterpret_cast<DWORD *>(addrGetSchemeSettingsFromWam + 0x4);

	const int builtinId = addrSchemeStruct ? *reinterpret_cast<int *>(addrSchemeStruct + 0x8) : 0;
	if (builtinId > 0) {
		updateCurrentBuiltinScheme(builtinId);
	}

	Diagnostics::log("SchemeInfo read-only install: struct=0x%X getVersion=0x%X builtinId=%d", addrSchemeStruct, addrGetSchemeVersion, builtinId);
	Hooks::saveOffsets();
}

std::string SchemeInfo::normalizeSchemeName(const std::string &name) {
	std::string normalized;
	normalized.reserve(name.size());
	for (unsigned char ch : name) {
		if (std::isalnum(ch)) {
			normalized.push_back(static_cast<char>(std::tolower(ch)));
		}
	}
	return normalized;
}

std::string SchemeInfo::getCurrentSchemeName() {
	if (!currentSchemeName.empty()) {
		return currentSchemeName;
	}

	if (addrSchemeStruct) {
		const int builtinId = *reinterpret_cast<int *>(addrSchemeStruct + 0x8);
		if (builtinId > 0) {
			return builtinSchemeNameFromId(builtinId);
		}
	}

	return {};
}

SchemeInfo::Snapshot SchemeInfo::snapshot() {
	Snapshot result;
	const auto [version, extraSize] = getSchemeVersionAndSize();
	if (!addrSchemeStruct || version < 1 || version > 3) {
		return result;
	}

	result.valid = true;
	result.version = version;
	result.extraOptionsSize = extraSize;
	const auto matchingFile = findMatchingSchemeFile(version, extraSize);
	if (!matchingFile.empty()) {
		result.name = matchingFile.stem().string();
		result.sourceKind = "custom";
	} else {
		result.name = getCurrentSchemeName();
		result.sourceKind = sourceKindToString(currentSchemeSource);
	}
	result.normalizedName = normalizeSchemeName(result.name);
	result.weaponCount = version == 1 ? 45 : 64;

	const auto *payload = reinterpret_cast<const uint8_t *>(addrSchemeStruct + 0x14);
	result.hotSeatTime = payload[0];
	result.retreatTime = payload[1];
	result.retreatTimeRope = payload[2];
	result.showRoundTime = payload[3] != 0;
	result.replays = payload[4] != 0;
	result.fallDamage = static_cast<int>((payload[5] * 50u) % 256u * 2u);
	result.artilleryMode = payload[6] != 0;
	result.schemeEditor = payload[7];
	result.stockpiling = payload[8];
	result.wormSelect = payload[9];
	result.suddenDeathEvent = payload[10];
	result.waterRiseRate = waterRiseRateFromIndex(payload[11]);
	result.weaponCrateProb = static_cast<int8_t>(payload[12]);
	result.donorCards = payload[13] != 0;
	result.healthCrateProb = static_cast<int8_t>(payload[14]);
	result.healthCrateEnergy = payload[15];
	result.utilityCrateProb = static_cast<int8_t>(payload[16]);
	decodeObjectCombo(payload[17], version, result.objectCount, result.objectTypes);
	result.mineDelayRandom = payload[18] == 4 || payload[18] > 127;
	result.mineDelay = result.mineDelayRandom ? 0 : payload[18];
	result.dudMines = payload[19] != 0;
	result.manualWormPlacement = payload[20] != 0;
	result.wormEnergy = payload[21];
	result.turnTimeInfinite = payload[22] > 127;
	result.turnTime = result.turnTimeInfinite ? 0 : payload[22];
	if (payload[23] > 127) {
		result.roundTimeMinutes = 0;
		result.roundTimeSeconds = static_cast<uint8_t>(256 - payload[23]);
	} else {
		result.roundTimeMinutes = payload[23];
		result.roundTimeSeconds = 0;
	}
	result.numberOfWins = payload[24];
	result.blood = payload[25] != 0;
	result.aquaSheep = payload[26] != 0;
	result.sheepHeaven = payload[27] != 0;
	result.godWorms = payload[28] != 0;
	result.indiLand = payload[29] != 0;
	result.upgradeGrenade = payload[30] != 0;
	result.upgradeShotgun = payload[31] != 0;
	result.upgradeCluster = payload[32] != 0;
	result.upgradeLongbow = payload[33] != 0;
	result.teamWeapons = payload[34] != 0;
	result.superWeapons = payload[35] != 0;

	const auto *weapons = reinterpret_cast<const RawWeaponSetting *>(payload + StandardOptionBytes);
	for (int i = 0; i < result.weaponCount; ++i) {
		result.weapons[i] = {weapons[i].ammo, weapons[i].delay, weapons[i].power, weapons[i].probability};
	}

	if (version == 3) {
		loadVersion3Extended(payload + PayloadSizeV2, extraSize, result.extended);
	} else if (version == 2) {
		deriveVersion2RubberWorm(result);
	}

	return result;
}

std::vector<std::string> SchemeInfo::buildInfoLines(const Snapshot &snapshot) {
	std::vector<std::string> lines;
	if (!snapshot.valid) {
		lines.emplace_back("[wkSchemeInfo] No readable scheme is currently available.");
		return lines;
	}

	const std::string schemeName = snapshot.name.empty() ? "Unknown scheme" : snapshot.name;
	const std::string roundTime = snapshot.roundTimeSeconds > 0
		? std::to_string(snapshot.roundTimeSeconds) + "s"
		: std::to_string(snapshot.roundTimeMinutes) + "m";
	const std::string turnTime = snapshot.turnTimeInfinite
		? "infinite"
		: std::to_string(snapshot.turnTime) + "s";

	appendParts(lines, "", {
		schemeName,
		"v" + std::to_string(snapshot.version),
		"source " + snapshot.sourceKind,
		"turn " + turnTime,
		"round " + roundTime,
		"retreat " + std::to_string(snapshot.retreatTime) + "s",
		"rope " + std::to_string(snapshot.retreatTimeRope) + "s"
	});

	std::string mineText = snapshot.mineDelayRandom
		? "random (1-3s)"
		: std::to_string(snapshot.mineDelay) + "s";
	if (snapshot.extended.hasUndeterminedMineFuse) {
		mineText += ", hidden " + optionalFlag(snapshot.extended.undeterminedMineFuse);
	}

	appendParts(lines, "Core: ", {
		"hp " + std::to_string(snapshot.wormEnergy),
		"wins " + std::to_string(snapshot.numberOfWins),
		"placement " + boolWord(snapshot.manualWormPlacement),
		"mines " + mineText,
		"duds " + boolWord(snapshot.dudMines),
		"fall dmg " + std::to_string(snapshot.fallDamage) + "%"
	});

	appendParts(lines, "Map: ", {
		"objects " + std::to_string(snapshot.objectCount) + " " + objectTypesToString(snapshot.objectTypes),
		"stockpiling " + stockpilingToString(snapshot.stockpiling),
		"worm select " + wormSelectToString(snapshot.wormSelect),
		"SD " + suddenDeathToString(snapshot.suddenDeathEvent),
		"water rise " + std::to_string(snapshot.waterRiseRate) + " px/turn",
		"show round " + boolWord(snapshot.showRoundTime)
	});

	std::vector<std::string> crateParts = {
		"W " + std::to_string(snapshot.weaponCrateProb) + "%",
		"U " + std::to_string(snapshot.utilityCrateProb) + "%",
		"H " + std::to_string(snapshot.healthCrateProb) + "%",
		"health +" + std::to_string(snapshot.healthCrateEnergy),
		"donor cards " + boolWord(snapshot.donorCards)
	};
	if (snapshot.extended.hasCrateRate) {
		crateParts.push_back("rate " + std::to_string(snapshot.extended.crateRate) + "/turn");
	}
	if (snapshot.extended.hasCrateLimit) {
		crateParts.push_back("limit " + std::to_string(snapshot.extended.crateLimit));
	}
	if (snapshot.extended.hasNoCrateProbability) {
		crateParts.push_back("no crate " + std::to_string(snapshot.extended.noCrateProbability) + "%");
	}
	if (snapshot.extended.hasUndeterminedCrates) {
		crateParts.push_back("hidden crates " + optionalFlag(snapshot.extended.undeterminedCrates));
	}
	appendParts(lines, "Crates: ", crateParts);

	appendParts(lines, "Flags: ", {
		"artillery " + boolWord(snapshot.artilleryMode),
		"blood " + boolWord(snapshot.blood),
		"aqua sheep " + boolWord(snapshot.aquaSheep),
		"sheep heaven " + boolWord(snapshot.sheepHeaven),
		"god worms " + boolWord(snapshot.godWorms),
		"indi land " + boolWord(snapshot.indiLand),
		"team weapons " + boolWord(snapshot.teamWeapons),
		"super weapons " + boolWord(snapshot.superWeapons),
		"replays " + boolWord(snapshot.replays)
	});

	appendParts(lines, "Upgrades: ", {
		"grenade " + boolWord(snapshot.upgradeGrenade),
		"shotgun " + boolWord(snapshot.upgradeShotgun),
		"cluster " + boolWord(snapshot.upgradeCluster),
		"longbow " + boolWord(snapshot.upgradeLongbow)
	});

	std::vector<std::string> advancedParts;
	if (snapshot.extended.hasShotDoesntEndTurn) {
		advancedParts.push_back("sdet " + boolWord(snapshot.extended.shotDoesntEndTurn));
	}
	if (snapshot.extended.hasLoseControlDoesntEndTurn) {
		advancedParts.push_back("ldet " + boolWord(snapshot.extended.loseControlDoesntEndTurn));
	}
	if (snapshot.extended.hasFiringPausesTimer) {
		advancedParts.push_back("firing pauses " + boolWord(snapshot.extended.firingPausesTimer));
	}
	if (snapshot.extended.hasRopeUpgrade) {
		advancedParts.push_back("rope+ " + boolWord(snapshot.extended.ropeUpgrade));
	}
	if (snapshot.extended.hasBattyRope) {
		advancedParts.push_back("batty rope " + boolWord(snapshot.extended.battyRope));
	}
	if (snapshot.extended.hasAntiSink) {
		advancedParts.push_back("antisink " + boolWord(snapshot.extended.antiSink));
	}
	if (snapshot.extended.hasWeaponsDontChange) {
		advancedParts.push_back("wdca " + boolWord(snapshot.extended.weaponsDontChange));
	}
	if (snapshot.extended.hasExtendedFuse) {
		advancedParts.push_back("fuse+ " + boolWord(snapshot.extended.extendedFuse));
	}
	if (snapshot.extended.hasWormSelectAnytime) {
		advancedParts.push_back("swat " + boolWord(snapshot.extended.wormSelectAnytime));
	}
	if (snapshot.extended.hasAutoReaim) {
		advancedParts.push_back("auto reaim " + boolWord(snapshot.extended.autoReaim));
	}
	if (snapshot.extended.hasAutoRetreat) {
		advancedParts.push_back("auto retreat " + boolWord(snapshot.extended.autoRetreat));
	}
	if (snapshot.extended.hasRoundTimeFractional) {
		advancedParts.push_back("fractional round " + boolWord(snapshot.extended.roundTimeFractional));
	}
	if (!advancedParts.empty()) {
		appendParts(lines, snapshot.extended.derivedFromRubberWorm ? "RW: " : "3.8: ", advancedParts);
	}

	std::vector<std::string> physicsParts;
	if (snapshot.extended.hasConstantWind) {
		physicsParts.push_back("constant wind " + boolWord(snapshot.extended.constantWind));
	}
	if (snapshot.extended.hasWind) {
		physicsParts.push_back("wind " + std::to_string(snapshot.extended.wind));
	}
	if (snapshot.extended.hasWindBias) {
		physicsParts.push_back("wind bias " + std::to_string(snapshot.extended.windBias));
	}
	if (snapshot.extended.hasGravity) {
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss.precision(2);
		ss << "gravity " << snapshot.extended.gravity;
		physicsParts.push_back(ss.str());
	}
	if (snapshot.extended.hasFriction) {
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss.precision(2);
		ss << "friction " << snapshot.extended.friction;
		physicsParts.push_back(ss.str());
	}
	if (snapshot.extended.hasWormBounce) {
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss.precision(2);
		ss << "bounce " << snapshot.extended.wormBounce;
		physicsParts.push_back(ss.str());
	}
	if (snapshot.extended.hasViscosity) {
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss.precision(2);
		ss << "viscosity " << snapshot.extended.viscosity;
		physicsParts.push_back(ss.str());
		if (snapshot.extended.hasViscosityWorms) {
			physicsParts.push_back("visc worms " + boolWord(snapshot.extended.viscosityWorms));
		}
	}
	if (snapshot.extended.hasRwWind) {
		std::ostringstream ss;
		ss.setf(std::ios::fixed);
		ss.precision(2);
		ss << "rw wind " << snapshot.extended.rwWind;
		physicsParts.push_back(ss.str());
		if (snapshot.extended.hasRwWindWorms) {
			physicsParts.push_back("rw wind worms " + boolWord(snapshot.extended.rwWindWorms));
		}
	}
	if (!physicsParts.empty()) {
		appendParts(lines, "Physics: ", physicsParts);
	}

	return lines;
}
