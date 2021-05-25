#include <sstream>

#include "physics.h"
#include "offsets.h"
#include "utils.h"
#include "pluginapi.h"
#include "grabber.h"
#include "config.h"

#include "skse64/NiNodes.h"
#include "skse64/gamethreads.h"


CdPointCollector::CdPointCollector()
{
	reset();
}

void CdPointCollector::reset()
{
	m_earlyOutDistance = 1.0f;
	m_hits.clear(); // TODO: Shrink to fit?
}

void CdPointCollector::addCdPoint(const hkpCdPoint& point)
{
	// Note: If this collector is being used for *linear casts* then for optimization 
	// purposes you should set the m_earlyOutDistance to:
	// - 0.0 if you want to get no more hits
	// - point.m_contact.getDistance() if you only want to get closer hits
	// - don't set, if you want to get all hits.

	hkpCdBody *cdBody = const_cast<hkpCdBody *>(&point.m_cdBodyB);
	while (cdBody->m_parent) {
		cdBody = const_cast<hkpCdBody *>(cdBody->m_parent);
	}
	//_MESSAGE("Hit: %x", cdBody->m_shape ? cdBody->m_shape->m_type : HK_SHAPE_INVALID);

	m_hits.push_back(std::make_pair(cdBody, point.getContact()));
	//m_earlyOutDistance = point.m_contact.getDistance(); // Only accept closer hits after this
}

CdBodyPairCollector::CdBodyPairCollector()
{
	reset();
}

void CdBodyPairCollector::reset()
{
	m_earlyOut = false;
	m_hits.clear(); // TODO: Shrink to fit?
}

void CdBodyPairCollector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
	// Note: for optimization purposes this should set the m_earlyOut:
	// - true if you want to get no more hits
	// - false if you want to get more hits (which is the default)

	hkpCdBody *cdBody = const_cast<hkpCdBody *>(&bodyB);
	while (cdBody->m_parent) {
		cdBody = const_cast<hkpCdBody *>(cdBody->m_parent);
	}
	//_MESSAGE("Hit: %x", cdBody->m_shape ? cdBody->m_shape->m_type : HK_SHAPE_INVALID);

	m_hits.push_back(cdBody);
}

SpecificPairCollector::SpecificPairCollector()
{
	reset();
}

void SpecificPairCollector::reset()
{
	m_earlyOut = false;
	m_foundTarget = false;
	m_target = nullptr;
}

void SpecificPairCollector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
	// Note: for optimization purposes this should set the m_earlyOut:
	// - true if you want to get no more hits
	// - false if you want to get more hits (which is the default)

	hkpCdBody *cdBody = const_cast<hkpCdBody *>(&bodyB);
	while (cdBody->m_parent) {
		cdBody = const_cast<hkpCdBody *>(cdBody->m_parent);
	}

	if (cdBody == m_target) {
		m_foundTarget = true;
		m_earlyOut = true;
	}
}

RayHitCollector::RayHitCollector()
{
	reset();
}

void RayHitCollector::reset()
{
	m_earlyOutHitFraction = 1.0f;
	m_doesHitExist = false;
}

void RayHitCollector::addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
{
	// Note: for optimization purposes this should set the m_earlyOutHitFraction to:
	// - 0.0 if you want to get no more hits
	// - 1.0 if you want to get all hits (constructor initializes this value to 1.0 by default)
	// - output.m_hitFraction if you only want to get closer hits than one just found

	//while (cdBody->m_parent) {
	//	cdBody = cdBody->m_parent;
	//}
	//_MESSAGE("Raycast hit: %x", cdBody->m_shape ? cdBody->m_shape->m_type : HK_SHAPE_INVALID);

	//m_closestCollidable = cdBody;
	m_closestHitInfo = hitInfo;
	m_doesHitExist = true;
	m_earlyOutHitFraction = hitInfo.m_hitFraction; // Only accept closer hits after this
}

AllRayHitCollector::AllRayHitCollector()
{
	reset();
}

void AllRayHitCollector::reset()
{
	m_earlyOutHitFraction = 1.0f;
	m_hits.clear(); // TODO: Shrink to fit?
}

void AllRayHitCollector::addRayHit(const hkpCdBody& cdBody, const hkpShapeRayCastCollectorOutput& hitInfo)
{
	// Note: for optimization purposes this should set the m_earlyOutHitFraction to:
	// - 0.0 if you want to get no more hits
	// - 1.0 if you want to get all hits (constructor initializes this value to 1.0 by default)
	// - output.m_hitFraction if you only want to get closer hits than one just found

	//while (cdBody->m_parent) {
	//	cdBody = cdBody->m_parent;
	//}
	//_MESSAGE("Raycast hit: %x", cdBody->m_shape ? cdBody->m_shape->m_type : HK_SHAPE_INVALID);

	//m_closestCollidable = cdBody;

	hkpCdBody *body = const_cast<hkpCdBody *>(&cdBody);
	while (body->m_parent) {
		body = const_cast<hkpCdBody *>(body->m_parent);
	}

	m_hits.push_back(std::make_pair(body, hitInfo));
	//m_earlyOutHitFraction = hitInfo->m_hitFraction; // Only accept closer hits after this
}


struct CreateDetectionEventTask : TaskDelegate
{
	static CreateDetectionEventTask * Create(ActorProcessManager *ownerProcess, Actor *owner, NiPoint3 position, int soundLevel, TESObjectREFR *source) {
		CreateDetectionEventTask * cmd = new CreateDetectionEventTask;
		if (cmd)
		{
			cmd->ownerProcess = ownerProcess;
			cmd->owner = owner;
			cmd->position = position;
			cmd->soundLevel = soundLevel;
			cmd->source = source;
		}
		return cmd;
	}
	virtual void Run() {
		CreateDetectionEvent(ownerProcess, owner, &position, soundLevel, source);
	}
	virtual void Dispose() {
		delete this;
	}

	ActorProcessManager *ownerProcess;
	Actor* owner;
	NiPoint3 position;
	int soundLevel;
	TESObjectREFR *source;
};

void ContactListener::contactPointCallback(const hkpContactPointEvent& evnt)
{
	if (evnt.m_contactPointProperties && (evnt.m_contactPointProperties->m_flags & hkContactPointMaterial::FlagEnum::CONTACT_IS_DISABLED)) {
		// Early out
		return;
	}

	hkpRigidBody *rigidBodyA = evnt.m_bodies[0];
	hkpRigidBody *rigidBodyB = evnt.m_bodies[1];

	UInt32 layerA = rigidBodyA->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo & 0x7f;
	UInt32 layerB = rigidBodyB->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo & 0x7f;
	if (layerA != 56 && layerB != 56) return; // Every collision we care about involves a body with our custom layer (hand, held object...)

	float separatingVelocity = fabs(hkpContactPointEvent_getSeparatingVelocity(evnt));
	if (separatingVelocity < Config::options.collisionMinHapticSpeed) return;

	bhkRigidBody *bhkRigidBodyA = (bhkRigidBody *)rigidBodyA->m_userData;
	bhkRigidBody *bhkRigidBodyB = (bhkRigidBody *)rigidBodyB->m_userData;

	bool isARightHand = bhkRigidBodyA && bhkRigidBodyA == g_rightGrabber->handBody;
	bool isBRightHand = bhkRigidBodyB && bhkRigidBodyB == g_rightGrabber->handBody;
	bool isALeftHand = bhkRigidBodyA && bhkRigidBodyA == g_leftGrabber->handBody;
	bool isBLeftHand = bhkRigidBodyB && bhkRigidBodyB == g_leftGrabber->handBody;

	bool rightHasHeld = g_rightGrabber->HasHeldObject();
	bool leftHasHeld = g_leftGrabber->HasHeldObject();

	bool isAHeldRight = rightHasHeld && &rigidBodyA->m_collidable == g_rightGrabber->selectedObject.collidable;
	bool isBHeldRight = rightHasHeld && &rigidBodyB->m_collidable == g_rightGrabber->selectedObject.collidable;
	bool isAHeldLeft = leftHasHeld && &rigidBodyA->m_collidable == g_leftGrabber->selectedObject.collidable;
	bool isBHeldLeft = leftHasHeld && &rigidBodyB->m_collidable == g_leftGrabber->selectedObject.collidable;

	bool isAWeapRight = bhkRigidBodyA && bhkRigidBodyA == g_rightGrabber->weaponBody;
	bool isBWeapRight = bhkRigidBodyB && bhkRigidBodyB == g_rightGrabber->weaponBody;
	bool isAWeapLeft = bhkRigidBodyA && bhkRigidBodyA == g_leftGrabber->weaponBody;
	bool isBWeapLeft = bhkRigidBodyB && bhkRigidBodyB == g_leftGrabber->weaponBody;

	auto TriggerCollisionHaptics = [separatingVelocity](float inverseMass, bool isLeft) {
		float mass = inverseMass ? 1.0f / inverseMass : 10000.0f;
		if (isLeft) {
			g_leftGrabber->TriggerCollisionHaptics(mass, separatingVelocity);
		}
		else {
			g_rightGrabber->TriggerCollisionHaptics(mass, separatingVelocity);
		}

		HiggsPluginAPI::TriggerCollisionCallbacks(isLeft, mass, separatingVelocity);
	};

	bool isA = isARightHand || isALeftHand || isAHeldRight || isAHeldLeft || isAWeapLeft || isAWeapRight;
	if (isA) {
		bool isLeft = isALeftHand || isAHeldLeft || isAWeapLeft;
		TriggerCollisionHaptics(rigidBodyB->getMassInv(), isLeft);
	}

	bool isB = isBRightHand || isBLeftHand || isBHeldRight || isBHeldLeft || isBWeapLeft || isBWeapRight;
	if (isB) {
		bool isLeft = isBLeftHand || isBHeldLeft || isBWeapLeft;
		TriggerCollisionHaptics(rigidBodyA->getMassInv(), isLeft);
	}

	/*
	TESObjectREFR *ref = FindCollidableRef(&otherBody->m_collidable);
	hkContactPoint *contactPoint = evnt.m_contactPoint;
	if (ref && contactPoint) {
		PlayerCharacter *player = *g_thePlayer;
		NiPoint3 position = HkVectorToNiPoint(contactPoint->getPosition()) * *g_inverseHavokWorldScale;
		// Very Loud == 200, Silent == 0, Normal == 50, Loud == 100
		int soundLevel = 50;
		g_taskInterface->AddTask(CreateDetectionEventTask::Create(player->processManager, player, position, soundLevel, ref));
	}
	*/
}


namespace CollisionInfo
{
	// Map havok entity id -> (saved collisionfilterinfo, state of saved collision)
	std::unordered_map<UInt32, CollisionMapEntry> collisionInfoIdMap;

	void ClearCollisionMap()
	{
		// Should only be called when you're sure there should be nothing in the map
		if (collisionInfoIdMap.size() > 0) {
			collisionInfoIdMap.clear();
		}
	}

	void SetCollisionInfoDownstream(NiAVObject *obj, UInt32 collisionGroup, State reason)
	{
		auto rigidBody = GetRigidBody(obj);
		if (rigidBody) {
			hkpRigidBody *entity = rigidBody->hkBody;
			if (entity->m_world) {
				hkpCollidable *collidable = &entity->m_collidable;

				// Save collisionfilterinfo by entity id
				UInt32 entityId = entity->m_uid;
				if (collisionInfoIdMap.count(entityId) != 0) {
					// Already in the map - state transition

					auto[savedInfo, savedState] = collisionInfoIdMap[entityId];

					if ((savedState == State::HeldLeft && reason == State::HeldRight) ||
						(savedState == State::HeldRight && reason == State::HeldLeft)) {
						// One hand holds the object -> 2 hands hold it

						collisionInfoIdMap[entityId] = { savedInfo, State::HeldBoth };

						bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
						world->worldLock.LockForWrite();

						UInt8 ragdollBits = (UInt8)RagdollLayer::SkipBoth;
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);

						hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

						world->worldLock.UnlockWrite();
					}
					else if (savedState == State::Unheld && (reason == State::HeldLeft || reason == State::HeldRight)) {
						// No hand holds it -> one hand holds it
						// I don't think this case is actually possible, since I try to reset a pull when grabbing, but it's here just in case.

						collisionInfoIdMap[entityId] = { savedInfo, reason };

						bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
						world->worldLock.LockForWrite();

						// We don't set the layer when pulling, so now that we're grabbing the object we need to set it.
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~0x7F; // clear out layer
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= 56; // our custom layer

						UInt8 ragdollBits = (UInt8)(reason == State::HeldLeft ? RagdollLayer::SkipLeft : RagdollLayer::SkipRight);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);

						hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

						world->worldLock.UnlockWrite();
					}
				}
				else {
					// Not in the map yet - init to the necessary state

					collisionInfoIdMap[entityId] = { collidable->m_broadPhaseHandle.m_collisionFilterInfo, reason };

					bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
					world->worldLock.LockForWrite();

					collidable->m_broadPhaseHandle.m_collisionFilterInfo &= 0x0000FFFF;
					collidable->m_broadPhaseHandle.m_collisionFilterInfo |= collisionGroup << 16;

					// set bit 15. This way it won't collide with the player, but _will_ collide with other objects that also have bit 15 set (i.e. other things we pick up).
					collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (1 << 15); // Why bit 15? It's just the way the collision works.

					if (reason == State::HeldLeft || reason == State::HeldRight) {
						// Our collision layer negates collisions with other characters
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~0x7F; // clear out layer
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= 56; // our custom layer

						UInt8 ragdollBits = (UInt8)(reason == State::HeldLeft ? RagdollLayer::SkipLeft : RagdollLayer::SkipRight);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);
					}
					else if (reason == State::Unheld) {
						// When pulling, don't set the layer yet. This way it will still collide with other characters.

						UInt8 ragdollBits = (UInt8)RagdollLayer::SkipNone;
						collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
						collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);
					}

					hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

					world->worldLock.UnlockWrite();
				}
			}
		}

		NiNode *node = obj->GetAsNiNode();
		if (node) {
			for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
				auto child = node->m_children.m_data[i];
				if (child) {
					SetCollisionInfoDownstream(child, collisionGroup, reason);
				}
			}
		}
	}

	void SetCollisionGroupDownstream(NiAVObject *obj, UInt32 collisionGroup, State reason)
	{
		auto rigidBody = GetRigidBody(obj);
		if (rigidBody) {
			hkpRigidBody *entity = rigidBody->hkBody;
			if (entity->m_world) {
				hkpCollidable *collidable = &entity->m_collidable;

				// Save collisionfilterinfo by entity id
				UInt32 entityId = entity->m_uid;
				if (collisionInfoIdMap.count(entityId) != 0) {
					// Already in the map - state transition

					auto[savedInfo, savedState] = collisionInfoIdMap[entityId];

					if ((savedState == State::HeldLeft && reason == State::HeldRight) ||
						(savedState == State::HeldRight && reason == State::HeldLeft)) {
						// One hand holds the object -> 2 hands hold it
						collisionInfoIdMap[entityId] = { savedInfo, State::HeldBoth };
					}
					else if (savedState == State::Unheld && (reason == State::HeldLeft || reason == State::HeldRight)) {
						// No hand holds it -> one hand holds it
						collisionInfoIdMap[entityId] = { savedInfo, reason };
					}
				}
				else {
					// Not in the map yet - init to the necessary state

					collisionInfoIdMap[entityId] = { collidable->m_broadPhaseHandle.m_collisionFilterInfo, reason };

					bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
					world->worldLock.LockForWrite();

					collidable->m_broadPhaseHandle.m_collisionFilterInfo &= 0x0000FFFF;
					collidable->m_broadPhaseHandle.m_collisionFilterInfo |= collisionGroup << 16;

					hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

					world->worldLock.UnlockWrite();
				}
			}
		}

		NiNode *node = obj->GetAsNiNode();
		if (node) {
			for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
				auto child = node->m_children.m_data[i];
				if (child) {
					SetCollisionGroupDownstream(child, collisionGroup, reason);
				}
			}
		}
	}

	void ResetCollisionInfoKeyframed(bhkRigidBody *entity, hkpMotion::MotionType motionType, hkInt8 quality, State reason, bool collideAll)
	{
		UInt32 entityId = entity->hkBody->m_uid;
		if (collisionInfoIdMap.count(entityId) != 0) {
			auto[savedInfo, savedState] = collisionInfoIdMap[entityId];

			if (savedState == State::HeldBoth && (reason == State::HeldRight || reason == State::HeldLeft)) {
				// Two hands hold the object -> one hand lets go, so the other hand holds it

				State otherHand = reason == State::HeldRight ? State::HeldLeft : State::HeldRight;
				collisionInfoIdMap[entityId] = { savedInfo, otherHand };

				// TODO: Lock world for this line?
				UInt8 ragdollBits = (UInt8)(otherHand == State::HeldLeft ? RagdollLayer::SkipLeft : RagdollLayer::SkipRight);
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);

				// use current collision, other hand still has it
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_objectQualityType = HK_COLLIDABLE_QUALITY_CRITICAL; // Will make object collide with other things as motion type is changed
				bhkRigidBody_setMotionType(entity, motionType, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);

				entity->hkBody->m_collidable.m_broadPhaseHandle.m_objectQualityType = quality;
				bhkRigidBody_setMotionType(entity, motionType, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY);
			}
			else if ((savedState == State::HeldLeft && reason == State::HeldLeft) ||
				(savedState == State::HeldRight && reason == State::HeldRight) ||
				(savedState == State::Unheld && reason == State::Unheld)) {
				// One hand holds the object -> one hand lets go of the object, so none hold it
				// Or, no hand was holding, and none is holding now

				collisionInfoIdMap.erase(entityId);

				// Restore only the original layer and ragdoll bits first, so it collides with everything except the player (but still the hands)
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo &= ~0x7f;
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo |= (savedInfo & 0x7f);

				if (collideAll) {
					entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
					entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo |= ((UInt8)RagdollLayer::SkipNone << 8);
				}

				entity->hkBody->m_collidable.m_broadPhaseHandle.m_objectQualityType = HK_COLLIDABLE_QUALITY_CRITICAL; // Will make object collide with other things as motion type is changed
				bhkRigidBody_setMotionType(entity, motionType, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);

				entity->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = savedInfo;
				entity->hkBody->m_collidable.m_broadPhaseHandle.m_objectQualityType = quality;
				bhkRigidBody_setMotionType(entity, motionType, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY);
			}
		}
	}

	void ResetCollisionInfoDownstream(NiAVObject *obj, State reason, hkpCollidable *skipNode, bool collideAll)
	{
		auto rigidBody = GetRigidBody(obj);
		if (rigidBody) {
			hkpRigidBody *entity = rigidBody->hkBody;
			if (entity->m_world) {
				hkpCollidable *collidable = &entity->m_collidable;
				if (collidable != skipNode) {
					UInt32 entityId = entity->m_uid;

					if (collisionInfoIdMap.count(entityId) != 0) {
						auto[savedInfo, savedState] = collisionInfoIdMap[entityId];

						if (savedState == State::HeldBoth && (reason == State::HeldRight || reason == State::HeldLeft)) {
							// Two hands hold the object -> one hand lets go, so the other hand holds it

							State otherHand = reason == State::HeldRight ? State::HeldLeft : State::HeldRight;
							collisionInfoIdMap[entityId] = { savedInfo, otherHand };

							bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
							world->worldLock.LockForWrite();

							UInt8 ragdollBits = (UInt8)(otherHand == State::HeldLeft ? RagdollLayer::SkipLeft : RagdollLayer::SkipRight);
							collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
							collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (ragdollBits << 8);

							hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

							world->worldLock.UnlockWrite();
						}
						else if ((savedState == State::HeldLeft && reason == State::HeldLeft) ||
							(savedState == State::HeldRight && reason == State::HeldRight) ||
							(savedState == State::Unheld && reason == State::Unheld)) {
							// One hand holds the object -> one hand lets go of the object, so none hold it
							// Or, no hand was holding, and none is holding now

							collisionInfoIdMap.erase(entityId);

							bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
							world->worldLock.LockForWrite();

							// Restore only the original layer and ragdoll bits first, so it collides with everything except the player (but still the hands)
							collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~0x7f;
							collidable->m_broadPhaseHandle.m_collisionFilterInfo |= (savedInfo & 0x7f);

							if (collideAll) {
								collidable->m_broadPhaseHandle.m_collisionFilterInfo &= ~(0x1f << 8);
								collidable->m_broadPhaseHandle.m_collisionFilterInfo |= ((UInt8)RagdollLayer::SkipNone << 8);
							}

							hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

							// Do not do a full check. What that means is it won't colide with the player until they stop colliding.
							collidable->m_broadPhaseHandle.m_collisionFilterInfo = savedInfo;
							hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

							world->worldLock.UnlockWrite();
						}
					}
				}
			}
		}

		NiNode *node = obj->GetAsNiNode();
		if (node) {
			for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
				auto child = node->m_children.m_data[i];
				if (child) {
					ResetCollisionInfoDownstream(child, reason, skipNode, collideAll);
				}
			}
		}
	}

	void ResetCollisionGroupDownstream(NiAVObject *obj, State reason, hkpCollidable *skipNode)
	{
		auto rigidBody = GetRigidBody(obj);
		if (rigidBody) {
			hkpRigidBody *entity = rigidBody->hkBody;
			if (entity->m_world) {
				hkpCollidable *collidable = &entity->m_collidable;
				if (collidable != skipNode) {
					UInt32 entityId = entity->m_uid;

					if (collisionInfoIdMap.count(entityId) != 0) {
						auto[savedInfo, savedState] = collisionInfoIdMap[entityId];

						if (savedState == State::HeldBoth && (reason == State::HeldRight || reason == State::HeldLeft)) {
							// Two hands hold the object -> one hand lets go, so the other hand holds it
							State otherHand = reason == State::HeldRight ? State::HeldLeft : State::HeldRight;
							collisionInfoIdMap[entityId] = { savedInfo, otherHand };
						}
						else if ((savedState == State::HeldLeft && reason == State::HeldLeft) ||
							(savedState == State::HeldRight && reason == State::HeldRight) ||
							(savedState == State::Unheld && reason == State::Unheld)) {
							// One hand holds the object -> one hand lets go of the object, so none hold it
							// Or, no hand was holding, and none is holding now

							collisionInfoIdMap.erase(entityId);

							bhkWorld *world = (bhkWorld *)static_cast<ahkpWorld *>(entity->m_world)->m_userData;
							world->worldLock.LockForWrite();

							// Do not do a full check. What that means is it won't colide with the player until they stop colliding.
							collidable->m_broadPhaseHandle.m_collisionFilterInfo = savedInfo;
							hkpWorld_UpdateCollisionFilterOnEntity(entity->m_world, entity, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);

							world->worldLock.UnlockWrite();
						}
					}
				}
			}
		}

		NiNode *node = obj->GetAsNiNode();
		if (node) {
			for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
				auto child = node->m_children.m_data[i];
				if (child) {
					ResetCollisionGroupDownstream(child, reason, skipNode);
				}
			}
		}
	}
}


void AddCustomCollisionLayer(bhkWorld *world)
{
	// Create our own layer in the first ununsed vanilla layer (56)
	bhkCollisionFilter *worldFilter = (bhkCollisionFilter *)world->world->m_collisionFilter;
	UInt64 bitfield = worldFilter->layerBitfields[5]; // copy of L_WEAPON layer bitfield

	bitfield |= ((UInt64)1 << 56); // collide with ourselves
	bitfield &= ~((UInt64)1 << 0x1e); // remove collision with character controllers
	worldFilter->layerBitfields[56] = bitfield;
	worldFilter->layerNames[56] = BSFixedString("L_HANDCOLLISION");
	// Set whether other layers should collide with our new layer
	for (int i = 0; i < 56; i++) {
		if ((bitfield >> i) & 1) {
			worldFilter->layerBitfields[i] |= ((UInt64)1 << 56);
		}
	}
}


void SetVelocityDownstream(NiAVObject *obj, hkVector4 velocity)
{
	auto bRigidBody = GetRigidBody(obj);
	if (bRigidBody) {
		hkpRigidBody *rigidBody = bRigidBody->hkBody;
		if (rigidBody->m_world) {
			hkpMotion *motion = &rigidBody->m_motion;

			bhkRigidBody_setActivated(bRigidBody, true);
			motion->m_linearVelocity = velocity;
		}
	}

	NiNode *node = obj->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				SetVelocityDownstream(child, velocity);
			}
		}
	}
}

void SetAngularVelocityDownstream(NiAVObject *obj, hkVector4 velocity)
{
	auto bRigidBody = GetRigidBody(obj);
	if (bRigidBody) {
		hkpRigidBody *rigidBody = bRigidBody->hkBody;
		if (rigidBody->m_world) {
			hkpMotion *motion = &rigidBody->m_motion;

			bhkRigidBody_setActivated(bRigidBody, true);
			motion->m_angularVelocity = velocity;
		}
	}

	NiNode *node = obj->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				SetAngularVelocityDownstream(child, velocity);
			}
		}
	}
}

void ApplyHardKeyframeDownstream(NiAVObject *obj, hkVector4 pos, hkQuaternion rot, hkReal invDeltaTime)
{
	auto bRigidBody = GetRigidBody(obj);
	if (bRigidBody) {
		hkpRigidBody *rigidBody = bRigidBody->hkBody;
		if (rigidBody->m_world) {
			hkpMotion *motion = &rigidBody->m_motion;

			bhkRigidBody_setActivated(bRigidBody, true);
			hkpKeyFrameUtility_applyHardKeyFrame(pos, rot, invDeltaTime, rigidBody);
		}
	}

	NiNode *node = obj->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				ApplyHardKeyframeDownstream(child, pos, rot, invDeltaTime);
			}
		}
	}
}

float hkpContactPointEvent_getSeparatingVelocity(const hkpContactPointEvent &_this)
{
	if (_this.m_separatingVelocity)
	{
		return *_this.m_separatingVelocity;
	}
	else
	{
		return hkpSimpleContactConstraintUtil_calculateSeparatingVelocity(_this.m_bodies[0], _this.m_bodies[1], _this.m_bodies[0]->getCenterOfMassInWorld(), _this.m_bodies[1]->getCenterOfMassInWorld(), _this.m_contactPoint);
	}
}
