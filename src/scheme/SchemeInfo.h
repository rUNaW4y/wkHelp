#ifndef WKSCHEMEINFO_SCHEMEINFO_H
#define WKSCHEMEINFO_SCHEMEINFO_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class SchemeInfo {
public:
	struct WeaponSetting {
		int8_t ammo = 0;
		int8_t delay = 0;
		uint8_t power = 0;
		int8_t probability = 0;
	};

	struct ExtendedSummary {
		bool derivedFromRubberWorm = false;
		size_t bytesPresent = 0;
		uint32_t dataVersion = 0;
		uint8_t ropeKnockForce = 255;
		uint8_t bloodAmount = 255;
		bool groupPlaceAllies = false;
		bool suddenDeathNoWormSelect = true;
		uint8_t suddenDeathTurnDamage = 5;
		uint8_t wormPhasingAlly = 0;
		uint8_t wormPhasingEnemy = 0;
		bool circularAim = false;
		bool antiLockAim = false;
		bool antiLockPower = false;
		bool wormSelectKeepHotSeat = false;
		uint8_t ropeRollDrops = 0;
		uint8_t keepControlXImpact = 0;
		bool keepControlHeadBump = false;
		uint8_t keepControlSkim = 0;
		bool explosionFallDamage = false;
		std::optional<bool> objectPushByExplosion;
		bool shotDoesntEndTurnAll = false;
		std::optional<bool> drillImpartsVelocity;
		bool girderRadiusAssist = false;
		float flameTurnDecay = 13106.0f / 65536.0f;
		uint8_t flameTouchDecay = 30;
		uint16_t flameLimit = 200;
		float projectileMaxSpeed = 32.0f;
		float ropeMaxSpeed = 16.0f;
		float jetpackMaxSpeed = 5.0f;
		float gameSpeed = 1.0f;
		std::optional<bool> indianRopeGlitch;
		std::optional<bool> herdDoublingGlitch;
		bool jetpackBungeeGlitch = true;
		bool angleCheatGlitch = true;
		bool glideGlitch = true;
		uint8_t skipWalk = 0;
		uint8_t roofing = 0;
		bool floatingWeaponGlitch = true;
		std::optional<bool> terrainOverlapGlitch;
		uint8_t healthCure = 1;
		uint8_t kaosMod = 0;
		uint8_t sheepHeavenFlags = 7;
		bool conserveUtilities = false;
		bool expediteUtilities = false;
		uint8_t doubleTimeCount = 1;

		bool hasCrateRate = false;
		uint8_t crateRate = 0;
		bool hasCrateLimit = false;
		uint16_t crateLimit = 0;
		bool hasNoCrateProbability = false;
		uint8_t noCrateProbability = 0;
		bool hasUndeterminedCrates = false;
		std::optional<bool> undeterminedCrates;
		bool hasUndeterminedMineFuse = false;
		std::optional<bool> undeterminedMineFuse;
		bool hasShotDoesntEndTurn = false;
		bool shotDoesntEndTurn = false;
		bool hasLoseControlDoesntEndTurn = false;
		bool loseControlDoesntEndTurn = false;
		bool hasFiringPausesTimer = false;
		bool firingPausesTimer = false;
		bool hasRopeUpgrade = false;
		bool ropeUpgrade = false;
		bool hasCrateShower = false;
		bool crateShower = false;
		bool hasAntiSink = false;
		bool antiSink = false;
		bool hasWeaponsDontChange = false;
		bool weaponsDontChange = false;
		bool hasExtendedFuse = false;
		bool extendedFuse = false;
		bool hasAutoReaim = false;
		bool autoReaim = false;
		bool hasAutoRetreat = false;
		bool autoRetreat = false;
		bool hasRoundTimeFractional = false;
		bool roundTimeFractional = false;
		bool hasBattyRope = false;
		bool battyRope = false;
		bool hasWormSelectAnytime = false;
		bool wormSelectAnytime = false;
		bool hasConstantWind = false;
		bool constantWind = false;
		bool hasWind = false;
		int16_t wind = 0;
		bool hasWindBias = false;
		uint8_t windBias = 0;
		bool hasGravity = false;
		float gravity = 0.0f;
		bool hasFriction = false;
		float friction = 0.0f;
		bool hasWormBounce = false;
		float wormBounce = 0.0f;
		bool hasViscosity = false;
		float viscosity = 0.0f;
		bool hasViscosityWorms = false;
		bool viscosityWorms = false;
		bool hasRwWind = false;
		float rwWind = 0.0f;
		bool hasRwWindWorms = false;
		bool rwWindWorms = false;
		bool hasRwGravityType = false;
		uint8_t rwGravityType = 0;
		bool hasRwGravity = false;
		float rwGravity = 0.0f;
		bool hasHealthCure = false;
		bool hasSheepHeavenFlags = false;
		bool hasDoubleTimeCount = false;
	};

	struct Snapshot {
		bool valid = false;
		int version = 0;
		size_t extraOptionsSize = 0;
		int weaponCount = 0;
		std::string name;
		std::string normalizedName;
		std::string sourceKind;

		uint8_t hotSeatTime = 0;
		uint8_t retreatTime = 0;
		uint8_t retreatTimeRope = 0;
		bool showRoundTime = false;
		bool replays = false;
		int fallDamage = 0;
		bool artilleryMode = false;
		uint8_t schemeEditor = 0;
		uint8_t stockpiling = 0;
		uint8_t wormSelect = 0;
		uint8_t suddenDeathEvent = 0;
		uint8_t waterRiseRate = 0;
		int weaponCrateProb = 0;
		bool donorCards = false;
		int healthCrateProb = 0;
		uint8_t healthCrateEnergy = 0;
		int utilityCrateProb = 0;
		int objectCount = 0;
		uint8_t objectTypes = 0;
		bool mineDelayRandom = false;
		uint8_t mineDelay = 0;
		bool dudMines = false;
		bool manualWormPlacement = false;
		uint8_t wormEnergy = 0;
		bool turnTimeInfinite = false;
		uint8_t turnTime = 0;
		uint8_t roundTimeMinutes = 0;
		uint8_t roundTimeSeconds = 0;
		uint8_t numberOfWins = 0;
		bool blood = false;
		bool aquaSheep = false;
		bool sheepHeaven = false;
		bool godWorms = false;
		bool indiLand = false;
		bool upgradeGrenade = false;
		bool upgradeShotgun = false;
		bool upgradeCluster = false;
		bool upgradeLongbow = false;
		bool teamWeapons = false;
		bool superWeapons = false;
		std::array<WeaponSetting, 64> weapons = {};
		ExtendedSummary extended;
	};

	static void install();
	static Snapshot snapshot();
	static std::vector<std::string> buildInfoLines(const Snapshot &snapshot);
	static std::string normalizeSchemeName(const std::string &name);

private:
	static std::string getCurrentSchemeName();
};

#endif
