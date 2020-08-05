#pragma once

#include <vector>
#include "RE/havok.h"

#include <Physics/Collide/Shape/Query/hkpRayHitCollector.h>
#include <Physics/Collide/Shape/Query/hkpShapeRayCastCollectorOutput.h>
#include <Physics/Collide/Agent/Query/hkpCdPointCollector.h>
#include <Physics/Collide/Agent/Query/hkpCdBodyPairCollector.h>
#include <Physics/Collide/Agent/Collidable/hkpCdPoint.h>

#include "skse64/GameReferences.h"


struct RayHitCollector : public hkpRayHitCollector
{
public:
	RayHitCollector();
	inline void reset();
	void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo) override;

	hkpShapeRayCastCollectorOutput m_closestHitInfo;
	bool m_doesHitExist = false;
	//const hkpCdBody *m_closestCollidable = nullptr;
};

struct AllRayHitCollector : public hkpRayHitCollector
{
public:
	AllRayHitCollector();
	inline void reset();
	void addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo) override;

	std::vector<std::pair<hkpCdBody *, hkpShapeRayCastCollectorOutput>> m_hits;
};

struct CdPointCollector : public hkpCdPointCollector
{
	CdPointCollector();
	void addCdPoint(const hkpCdPoint& point) override;
	inline void reset() override;

	std::vector<std::pair<hkpCdBody *, hkContactPoint>> m_hits;
};

struct CdBodyPairCollector : public hkpCdBodyPairCollector
{
	CdBodyPairCollector();
	void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB) override;
	void reset() override;

	std::vector<hkpCdBody *> m_hits;
};

void ClearCollisionMap();
UInt32 GetSavedCollision(UInt32 id);
UInt32 GetSavedCollisionRefCount(UInt32 id);
void RemoveSavedCollision(UInt32 id);
void SetCollisionInfoForAllCollisionInRefr(TESObjectREFR *refr, UInt32 collisionGroup);
void SetCollisionInfoDownstream(NiAVObject *obj, UInt32 collisionGroup);
void ResetCollisionInfoForAllCollisionInRefr(TESObjectREFR *refr, hkpCollidable *skipNode = nullptr);
void ResetCollisionInfoDownstream(NiAVObject *obj, hkpCollidable *skipNode = nullptr);

void SetVelocityDownstream(NiAVObject *obj, hkVector4 velocity);
void SetVelocityForAllCollisionInRefr(TESObjectREFR *refr, hkVector4 velocity);
