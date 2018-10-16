/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "Wind.h"
#include "GlobalSynced.h"
#include "Sim/Units/Unit.h"
#include "Sim/Units/UnitHandler.h"
#include "System/ContainerUtil.h"
#include "System/myMath.h"

CR_BIND(EnvResourceHandler, )

CR_REG_METADATA(EnvResourceHandler, (
	CR_MEMBER(curTidalStrength),
	CR_MEMBER(curWindStrength),
	CR_MEMBER(minWindStrength),
	CR_MEMBER(maxWindStrength),

	CR_MEMBER(curWindVec),
	CR_MEMBER(curWindDir),

	CR_MEMBER(newWindVec),
	CR_MEMBER(oldWindVec),

	CR_MEMBER(windDirTimer),

	CR_MEMBER(generatorIDs)
))


EnvResourceHandler envResHandler;


void EnvResourceHandler::ResetState()
{
	curTidalStrength = 0.0f;
	curWindStrength = 0.0f;
	minWindStrength = 0.0f;
	maxWindStrength = 100.0f;

	curWindDir = RgtVector;
	curWindVec = ZeroVector;
	newWindVec = ZeroVector;
	oldWindVec = ZeroVector;

	windDirTimer = 0;

	generatorIDs.clear();
	generatorIDs.reserve(256);
}

void EnvResourceHandler::LoadWind(float minStrength, float maxStrength)
{
	minWindStrength = std::min(minStrength, maxStrength);
	maxWindStrength = std::max(minStrength, maxStrength);

	curWindVec = mix(curWindDir * GetAverageWindStrength(), RgtVector * GetAverageWindStrength(), curWindDir == RgtVector);
	oldWindVec = curWindVec;
}


bool EnvResourceHandler::AddGenerator(CUnit* u) {
	// duplicates should never happen, no need to check
	spring::VectorInsertUnique(generatorIDs, u->id);

	// start pointing in direction of wind
	u->UpdateWind(curWindDir.x, curWindDir.z, curWindStrength);
	return true;
}

bool EnvResourceHandler::DelGenerator(CUnit* u) {
	return (spring::VectorErase(generatorIDs, u->id));
}



void EnvResourceHandler::Update()
{
	// zero-strength wind does not need updates
	if (maxWindStrength <= 0.0f)
		return;

	if (windDirTimer == 0) {
		oldWindVec = curWindVec;
		newWindVec = oldWindVec;

		// generate new wind direction
		float newStrength = 0.0f;

		do {
			newWindVec.x -= (gsRNG.NextFloat() - 0.5f) * maxWindStrength;
			newWindVec.z -= (gsRNG.NextFloat() - 0.5f) * maxWindStrength;
			newStrength = newWindVec.Length();
		} while (newStrength == 0.0f);

		// normalize and clamp s.t. minWindStrength <= strength <= maxWindStrength
		newWindVec /= newStrength;
		newWindVec *= (newStrength = Clamp(newStrength, minWindStrength, maxWindStrength));

		// update generators
		for (const int unitID: generatorIDs) {
			(unitHandler.GetUnit(unitID))->UpdateWind(newWindVec.x, newWindVec.z, newStrength);
		}
	} else {
		const float mod = smoothstep(0.0f, 1.0f, windDirTimer / float(WIND_UPDATE_RATE));

		// blend between old & new wind directions
		// note: generators added on simframes when timer is 0
		// do not receive a snapshot of the blended direction
		curWindVec = mix(oldWindVec, newWindVec, mod);
		curWindStrength = curWindVec.LengthNormalize();

		curWindDir = curWindVec;
		curWindVec = curWindDir * (curWindStrength = Clamp(curWindStrength, minWindStrength, maxWindStrength));
	}

	windDirTimer = (windDirTimer + 1) % (WIND_UPDATE_RATE + 1);
}

