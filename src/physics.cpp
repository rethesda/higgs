#include <sstream>

#include "RE/offsets.h"
#include "physics.h"
#include "utils.h"
#include "pluginapi.h"
#include "hand.h"
#include "config.h"
#include "main.h"

#include "skse64/NiNodes.h"
#include "skse64/gamethreads.h"
#include "skse64/GameRTTI.h"


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

SpecificPointCollector::SpecificPointCollector()
{
    reset();
}

void SpecificPointCollector::reset()
{
    m_earlyOutDistance = 1.0f;
    m_foundTarget = false;
    m_target = nullptr;
}

void SpecificPointCollector::addCdPoint(const hkpCdPoint& point)
{
    hkpCdBody *cdBody = const_cast<hkpCdBody *>(&point.m_cdBodyB);
    while (cdBody->m_parent) {
        cdBody = const_cast<hkpCdBody *>(cdBody->m_parent);
    }

    if (cdBody == m_target) {
        m_foundTarget = true;
        m_contactPoint = point.getContact();
        m_earlyOutDistance = 0.f;
    }
}

AnyPointCollector::AnyPointCollector()
{
    reset();
}

void AnyPointCollector::reset()
{
    m_earlyOutDistance = 1.0f;
    m_anyHits = false;
}

void AnyPointCollector::addCdPoint(const hkpCdPoint &point)
{
    if (point.getContact().getDistance() > maxDistance) return;

    m_anyHits = true;
    m_earlyOutDistance = 0.f;
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

AnyPairCollector::AnyPairCollector()
{
    reset();
}

void AnyPairCollector::reset()
{
    m_earlyOut = false;
    m_anyHits = false;
}

void AnyPairCollector::addCdBodyPair(const hkpCdBody &bodyA, const hkpCdBody &bodyB)
{
    m_anyHits = true;
    m_earlyOut = true;
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


void IslandDeactivationListener::islandActivatedCallback(hkpSimulationIsland* island) {}

void IslandDeactivationListener::islandDeactivatedCallback(hkpSimulationIsland* island)
{
    int numEntities = island->m_entities.getSize();
    if (numEntities <= 0) return;

    if (g_shadowUpdateFrame == *g_currentFrameCounter) {
        return; // We're already doing an update this frame
    }

    auto SetShadowsToUpdateThisFrame = []() {
        //_MESSAGE("Island deactived on frame %d", *g_currentFrameCounter);
        if (g_savedShadowUpdateFrameDelay == -1) {
            g_savedShadowUpdateFrameDelay = *g_iShadowUpdateFrameDelay;
        }

        // These are values within the game
        *g_nextShadowUpdateFrameCount = *g_currentFrameCounter;
        *g_iShadowUpdateFrameDelay = 1;

        // These are mine, used to keep track of when we want to stop updating
        g_shadowUpdateFrame = *g_currentFrameCounter;
        g_numShadowUpdates = 1;
    };

    if (numEntities > Config::options.maxNumEntitiesPerSimulationIslandToCheck) {
        SetShadowsToUpdateThisFrame();
        return;
    }

    bool anyToProcess = false;
    for (hkpEntity *entity : island->m_entities) {
        if (IsMoveableEntity(entity) &&
            ((entity->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo & 0x7f) != 0x21) && // 0x21 == L_BIPED_NO_CC
            (entity->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo & 0x7f) != 0x20 // 0x20 == L_DEADBIP
            ) {
            anyToProcess = true;
            break;
        }
    }

    if (anyToProcess) {
        PlayerCharacter *player = *g_thePlayer;
        float havokWorldScale = *g_havokWorldScale;
        NiPoint3 playerPos = player->pos * havokWorldScale;

        bool isAnyWithinRadius = false;
        for (hkpEntity *entity : island->m_entities) {
            NiPoint3 pos = HkVectorToNiPoint(entity->m_motion.getTransform().m_translation);
            float distFromPlayer = VectorLength(pos - playerPos);
            if (distFromPlayer < Config::options.maxDistanceOfSimulationIslandToUpdate) {
                isAnyWithinRadius = true;
                break;
            }
        }

        if (isAnyWithinRadius) {
            SetShadowsToUpdateThisFrame();
        }
    }
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

inline bool IsLeftRigidBody(hkpRigidBody *rigidBody)
{
    bhkRigidBody *wrapper = (bhkRigidBody *)rigidBody->m_userData;
    if (!wrapper) return false;
    return wrapper == g_leftHand->handBody || wrapper == g_leftHand->weaponBody || (g_leftHand->HasHeldObject() && wrapper == g_leftHand->selectedObject.rigidBody);
}

inline bool IsRightRigidBody(hkpRigidBody *rigidBody)
{
    bhkRigidBody *wrapper = (bhkRigidBody *)rigidBody->m_userData;
    if (!wrapper) return false;
    return wrapper == g_rightHand->handBody || wrapper == g_rightHand->weaponBody || (g_rightHand->HasHeldObject() && wrapper == g_rightHand->selectedObject.rigidBody);
}

inline bool IsWeaponRigidBody(hkpRigidBody *rigidBody)
{
    bhkRigidBody *wrapper = (bhkRigidBody *)rigidBody->m_userData;
    if (!wrapper) return false;
    return wrapper == g_leftHand->weaponBody || wrapper == g_rightHand->weaponBody;
}

inline bool IsHandRigidBody(hkpRigidBody *rigidBody)
{
    bhkRigidBody *wrapper = (bhkRigidBody *)rigidBody->m_userData;
    if (!wrapper) return false;
    return wrapper == g_leftHand->handBody || wrapper == g_rightHand->handBody;
}

inline bool IsHeldRigidBody(hkpRigidBody *rigidBody)
{
    bhkRigidBody *wrapper = (bhkRigidBody *)rigidBody->m_userData;
    if (!wrapper) return false;
    return (
        (g_leftHand->HasHeldObject() && wrapper == g_leftHand->selectedObject.rigidBody) ||
        (g_rightHand->HasHeldObject() && wrapper == g_rightHand->selectedObject.rigidBody)
    );
}

inline bool IsHiggsRigidBody(hkpRigidBody *rigidBody)
{
    UInt32 collisionLayer = GetCollisionLayer(rigidBody);
    bool isHeld = IsHeldRigidBody(rigidBody);
    if (collisionLayer != 56 && !isHeld) {
        return false;
    }
    return IsHandRigidBody(rigidBody) || IsWeaponRigidBody(rigidBody) || isHeld;
}

std::mutex PhysicsListener::handLocks[2]{};

void TriggerCollisionHaptics(float inverseMass, float speed, HandIndex handIndex) {
    float mass = inverseMass ? 1.0f / inverseMass : 10000.0f;

    if (g_rightHand->IsTwoHanding() || g_leftHand->IsTwoHanding()) {
        handIndex = HandIndex::Both;
    }

    if (handIndex == HandIndex::Both) {
        g_leftHand->TriggerCollisionHaptics(mass, speed);
        g_rightHand->TriggerCollisionHaptics(mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(true, mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(false, mass, speed);
    }
    else if (handIndex == HandIndex::Left) {
        g_leftHand->TriggerCollisionHaptics(mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(true, mass, speed);
    }
    else {
        g_rightHand->TriggerCollisionHaptics(mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(false, mass, speed);
    }
};

void TriggerCollisionHapticsUsingHandSpeed(float inverseMass, bool isLeft) {
    float mass = inverseMass ? 1.0f / inverseMass : 10000.0f;

    if (g_rightHand->IsTwoHanding() || g_leftHand->IsTwoHanding()) {
        // Both hands are holding an equipped weapon - play haptics for both
        float speed = g_rightHand->avgPlayerSpeedWorldspace + max(g_rightHand->controllerData.avgSpeed, g_leftHand->controllerData.avgSpeed);
        g_leftHand->TriggerCollisionHaptics(mass, speed);
        g_rightHand->TriggerCollisionHaptics(mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(true, mass, speed);
        HiggsPluginAPI::TriggerCollisionCallbacks(false, mass, speed);
    }
    else {
        if (isLeft) {
            float speed = g_leftHand->avgPlayerSpeedWorldspace + g_leftHand->controllerData.avgSpeed;
            g_leftHand->TriggerCollisionHaptics(mass, speed);
            HiggsPluginAPI::TriggerCollisionCallbacks(isLeft, mass, speed);
        }
        else {
            float speed = g_rightHand->avgPlayerSpeedWorldspace + g_rightHand->controllerData.avgSpeed;
            g_rightHand->TriggerCollisionHaptics(mass, speed);
            HiggsPluginAPI::TriggerCollisionCallbacks(isLeft, mass, speed);
        }
    }
}

HandIndex GetRigidBodyHandIndex(hkpRigidBody *rigidBody)
{
    bool isLeft = IsLeftRigidBody(rigidBody);
    bool isRight = IsRightRigidBody(rigidBody);
    return (isLeft && isRight) ? HandIndex::Both : (isLeft ? HandIndex::Left : HandIndex::Right);
}

void PhysicsListener::RegisterHandCollision(hkpRigidBody *body, float separatingVelocity, bool isLeft)
{
    auto &collidedBodies = handData[isLeft].collidedBodies;
    if (auto it = collidedBodies.find(body); it == collidedBodies.end()) {
        // It's not in the map yet
        std::unique_lock lock(handLocks[isLeft]);
        float collisionVelocity = separatingVelocity;
        if (it = collidedBodies.find(body); it != collidedBodies.end()) {
            // It's been added between when we checked above and when we acquired the lock
            collisionVelocity = max(separatingVelocity, it->second.velocity);
        }
        collidedBodies[body] = { *g_currentFrameCounter, body->getMassInv(), collisionVelocity };
    }
    else {
        // It's already in the map, so just update the collided frame
        it->second.collidedFrame = *g_currentFrameCounter;
    }
}

void PhysicsListener::RegisterHandCollision(hkpRigidBody *body, float separatingVelocity, HandIndex handIndex)
{
    int startIndex = handIndex == HandIndex::Both ? 0 : (handIndex == HandIndex::Left ? 1 : 0);
    int endIndex = handIndex == HandIndex::Both ? 1 : (handIndex == HandIndex::Left ? 1 : 0);

    for (int isLeft = startIndex; isLeft <= endIndex; isLeft++) {
        RegisterHandCollision(body, separatingVelocity, isLeft);
    }
}

void PhysicsListener::DisableContactsTemporarily(hkpRigidBody *bodyA, hkpRigidBody *bodyB, double duration)
{
    ignoreContactPointData[bodyA] = { bodyB, g_currentFrameTime, duration };
    ignoreContactPointData[bodyB] = { bodyA, g_currentFrameTime, duration };
}

void PhysicsListener::HandleIgnoredContact(const hkpContactPointEvent &evnt)
{
    if (ignoreContactPointData.size() == 0) return; // quick check first

    // Only need to check one direction, since we add both directions to the map
    auto it = ignoreContactPointData.find(evnt.m_bodies[0]);
    if (it != ignoreContactPointData.end()) {
        if (g_currentFrameTime - it->second.startTime < it->second.ignoreTime) {
            evnt.m_contactPointProperties->m_flags |= hkContactPointMaterial::FlagEnum::CONTACT_IS_DISABLED;
            return;
        }
    }
}

void PhysicsListener::contactPointCallback(const hkpContactPointEvent& evnt)
{
    if (evnt.m_contactPointProperties->m_flags & hkContactPointMaterial::FlagEnum::CONTACT_IS_DISABLED) {
        // Early out
        return;
    }

    hkpRigidBody *rigidBodyA = evnt.m_bodies[0];
    hkpRigidBody *rigidBodyB = evnt.m_bodies[1];

    bool isAHiggs = IsHiggsRigidBody(rigidBodyA);
    bool isBHiggs = IsHiggsRigidBody(rigidBodyB);

    if (!isAHiggs && !isBHiggs) return; // Neither body is a higgs body, so we don't care about this collision

    HandleIgnoredContact(evnt);
    if (evnt.m_contactPointProperties->m_flags & hkContactPointMaterial::FlagEnum::CONTACT_IS_DISABLED) {
        return;
    }

    // Ensure full manifold callbacks for any higgs collisions
    evnt.m_contactMgr->m_contactPointCallbackDelay = 0;

    float separatingVelocity = fabs(hkpContactPointEvent_getSeparatingVelocity(evnt));

    if (evnt.m_contactPointProperties->wasUsed() && evnt.m_contactPoint->getDistance() < Config::options.collisionMaxInitialContactPointDistance) {
        if (isAHiggs) {
            RegisterHandCollision(rigidBodyB, separatingVelocity, GetRigidBodyHandIndex(rigidBodyA));
        }

        if (isBHiggs) {
            RegisterHandCollision(rigidBodyA, separatingVelocity, GetRigidBodyHandIndex(rigidBodyB));
        }
    }

    if (evnt.m_contactPointProperties->isPotential()) {
        if (separatingVelocity < Config::options.collisionMinHapticSpeed) {
            return;
        }

        if (isAHiggs) {
            TriggerCollisionHaptics(rigidBodyB->getMassInv(), separatingVelocity, GetRigidBodyHandIndex(rigidBodyA));
        }

        if (isBHiggs) {
            TriggerCollisionHaptics(rigidBodyA->getMassInv(), separatingVelocity, GetRigidBodyHandIndex(rigidBodyB));
        }
    }

    /*
    TESObjectREFR *ref = GetRefFromCollidable(&otherBody->m_collidable);
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

void PhysicsListener::postSimulationCallback(hkpWorld* world)
{
    int currentFrame = *g_currentFrameCounter;

    for (int isLeft = 0; isLeft < 2; isLeft++) { // for each hand
        auto &collidedBodies = handData[isLeft].collidedBodies;
        auto &previousCollidedBodies = handData[isLeft].prevCollidedBodies;

        for (auto it = collidedBodies.begin(); it != collidedBodies.end();) {
            auto[body, collisionData] = *it;

            if (currentFrame - collisionData.collidedFrame < Config::options.collisionMaxInactiveFramesToConsiderActive) {
                // Collision is active
                if (!previousCollidedBodies.count(body)) {
                    previousCollidedBodies.insert(body);
                    // No used contact points for this body last frame, but yes this frame
                    TriggerCollisionHaptics(collisionData.inverseMass, collisionData.velocity, HandIndex(isLeft));
                }
            }
            else {
                // Collision is inactive
                if (auto it = previousCollidedBodies.find(body); it != previousCollidedBodies.end()) {
                    // There were used contact points for this body last frame, but not anymore
                    previousCollidedBodies.erase(it);
                }
            }

            if (currentFrame - collisionData.collidedFrame > Config::options.collisionMaxInactiveFramesBeforeCleanup) {
                it = collidedBodies.erase(it);
            }
            else {
                ++it;
            }
        }

        for (auto it = previousCollidedBodies.begin(); it != previousCollidedBodies.end();) {
            hkpRigidBody *body = *it;
            if (!collidedBodies.count(body)) {
                it = previousCollidedBodies.erase(it);
            }
            else {
                ++it;
            }
        }
    }

    g_rightEntityCollisionListener.PostSimulationUpdate();
    g_leftEntityCollisionListener.PostSimulationUpdate();

    // Clear out old ignore contact point data
    for (auto it = ignoreContactPointData.begin(); it != ignoreContactPointData.end();) {
        if (g_currentFrameTime - it->second.startTime > it->second.ignoreTime + 1.0) {
            it = ignoreContactPointData.erase(it);
        }
        else {
            ++it;
        }
    }
}


EntityCollisionListener g_rightEntityCollisionListener(false);
EntityCollisionListener g_leftEntityCollisionListener(true);

void EntityCollisionListener::RegisterCollision(hkpRigidBody *body)
{
    if (auto it = collidedBodies.find(body); it == collidedBodies.end()) {
        // It's not in the map yet
        std::unique_lock lock(collisionLock);
        collidedBodies[body] = *g_currentFrameCounter;
    }
    else {
        // It's already in the map, so just update the collided frame
        it->second = *g_currentFrameCounter;
    }
}

void EntityCollisionListener::contactPointCallback(const hkpContactPointEvent &evnt)
{
    if (evnt.m_contactPointProperties->m_flags & hkContactPointMaterial::FlagEnum::CONTACT_IS_DISABLED) {
        // Early out
        return;
    }

    if (evnt.m_contactPointProperties->wasUsed() && evnt.m_contactPoint->getDistance() < Config::options.collisionMaxInitialContactPointDistance) {
        RegisterCollision(evnt.m_source == hkpContactPointEvent::SOURCE_A ? evnt.m_bodies[0] : evnt.m_bodies[1]);
    }
}

void EntityCollisionListener::PostSimulationUpdate()
{
    for (auto it = collidedBodies.begin(); it != collidedBodies.end();) {
        auto [body, collidedFrame] = *it;

        if (*g_currentFrameCounter - collidedFrame > Config::options.collisionMaxInactiveFramesToConsiderActive) {
            // Collision is inactive
            it = collidedBodies.erase(it);
        }
        else {
            // Collision is active
            ++it;
        }
    }
}

bool EntityCollisionListener::IsColliding()
{
    return collidedBodies.size() > 0;
}

void AddHiggsCollisionLayer(bhkWorld *world)
{
    // Create our own layer in the first unused vanilla layer (56)
    bhkCollisionFilter *worldFilter = (bhkCollisionFilter *)world->world->m_collisionFilter;
    worldFilter->layerBitfields[56] = g_interface001.higgsLayerBitfield;
    worldFilter->layerNames[56] = BSFixedString("L_HIGGSCOLLISION");
    // Set whether other layers should collide with our new layer
    ReSyncLayerBitfields(worldFilter, 56);
}

void EnsureHiggsCollisionLayer(bhkWorld *world)
{
    bhkCollisionFilter *worldFilter = (bhkCollisionFilter *)world->world->m_collisionFilter;
    UInt64 currentHiggsBitfield = worldFilter->layerBitfields[56];
    if (currentHiggsBitfield != g_interface001.higgsLayerBitfield) {
        BSWriteLocker lock(&world->worldLock);
        worldFilter->layerBitfields[56] = g_interface001.higgsLayerBitfield;
        ReSyncLayerBitfields(worldFilter, 56);
    }
}

void ReSyncLayerBitfields(bhkCollisionFilter *filter, UInt8 layer)
{
    UInt64 bitfield = filter->layerBitfields[layer];
    for (int i = 0; i < 64; i++) { // 56 layers in vanilla
        if ((bitfield >> i) & 1) {
            filter->layerBitfields[i] |= ((UInt64)1 << layer);
        }
        else {
            filter->layerBitfields[i] &= ~((UInt64)1 << layer);
        }
    }
}

void ApplyHardKeyframeVelocityClamped(const hkVector4& nextPosition, const hkQuaternion& nextOrientation, hkReal invDeltaTime, bhkRigidBody* body)
{
    hkpRigidBody *hkBody = body->hkBody;
    hkpKeyFrameUtility_applyHardKeyFrame(nextPosition, nextOrientation, invDeltaTime, hkBody);

    if (VectorLength(HkVectorToNiPoint(hkBody->getLinearVelocity())) > bhkRigidBody_GetMaxLinearVelocityMetersPerSecond(body)) {
        if (ahkpWorld *world = body->GetHavokWorld_2()) {
            if (bhkWorld *worldWrapper = world->m_userData) {
                BSWriteLocker lock(&worldWrapper->worldLock);
                hkpRigidBody_setPosition(hkBody, nextPosition);
            }
        }
    }
    if (VectorLength(HkVectorToNiPoint(hkBody->getAngularVelocity())) > bhkRigidBody_GetMaxAngularVelocity(body)) {
        if (ahkpWorld *world = body->GetHavokWorld_2()) {
            if (bhkWorld *worldWrapper = world->m_userData) {
                BSWriteLocker lock(&worldWrapper->worldLock);
                hkpRigidBody_setRotation(hkBody, nextOrientation);
            }
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

void hkpWorld_removeContactListener(hkpWorld *_this, hkpContactListener* worldListener)
{
    hkArray<hkpContactListener *> &listeners = _this->m_contactListeners;

    for (int i = 0; i < listeners.getSize(); i++) {
        hkpContactListener *listener = listeners[i];
        if (listener == worldListener) {
            listeners[i] = nullptr;
            return;
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

bool IsColliding(const hkpRigidBody *rigidBody, float tolerance)
{
    hkpWorld *world = rigidBody->getWorld();
    if (!world) return false;

    const hkpCollidable *collidable = rigidBody->getCollidable();

    static AnyPointCollector collector{};
    collector.reset();
    collector.maxDistance = tolerance;

    hkpWorld_GetClosestPoints(world, collidable, world->getCollisionInput(), &collector);
    return collector.m_anyHits;
}

