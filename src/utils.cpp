#include <chrono>
#include <list>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <regex>
#include <map>

#include "skse64/GameRTTI.h"
#include "skse64/NiExtraData.h"
#include "skse64/NiGeometry.h"

#include "utils.h"
#include "offsets.h"
#include "config.h"
#include "math_utils.h"


ITimer g_timer;
double g_currentFrameTime;
double GetTime()
{
	return g_timer.GetElapsedTime();
}

NiAVObject * GetHighestParent(NiAVObject *node)
{
	if (!node->m_parent) {
		return node;
	}
	return GetHighestParent(node->m_parent);
}

UInt32 GetFullFormID(const ModInfo * modInfo, UInt32 formLower)
{
	return (modInfo->modIndex << 24) | formLower;
}

bool IsAllowedCollidable(const hkpCollidable *collidable)
{
	hkpRigidBody *rb = hkpGetRigidBody(collidable);
	if (!rb)
		return false;

	auto motion = &rb->m_motion;
	return (
		motion->m_type == hkpMotion::MotionType::MOTION_DYNAMIC ||
		motion->m_type == hkpMotion::MotionType::MOTION_SPHERE_INERTIA ||
		motion->m_type == hkpMotion::MotionType::MOTION_BOX_INERTIA ||
		motion->m_type == hkpMotion::MotionType::MOTION_THIN_BOX_INERTIA
		);
}

bool HasGeometryChildren(NiAVObject *obj)
{
	NiNode *node = obj->GetAsNiNode();
	if (!node) {
		return false;
	}

	for (int i = 0; i < node->m_children.m_arrayBufLen; ++i) {
		auto child = node->m_children.m_data[i];
		if (child && child->GetAsBSGeometry()) {
			return true;
		}
	}

	return false;
}

bool DoesEntityHaveConstraint(NiAVObject *root, bhkRigidBody *entity)
{
	auto rigidBody = GetRigidBody(root);
	if (rigidBody) {
		int numConstraints = rigidBody->numConstraints;
		if (numConstraints > 0) {
			bhkConstraint **constraints = rigidBody->constraints;
			for (int i = 0; i < numConstraints; i++) {
				bhkConstraint *constraint = constraints[i];
				if (constraint->constraint->getEntityA() == entity->hkBody || constraint->constraint->getEntityB() == entity->hkBody) {
					return true;
				}
			}
		}
	}

	NiNode *rootNode = root->GetAsNiNode();
	if (rootNode) {
		for (int i = 0; i < rootNode->m_children.m_emptyRunStart; i++) {
			NiAVObject *child = rootNode->m_children.m_data[i];
			if (child) {
				if (DoesEntityHaveConstraint(child, entity)) {
					return true;
				}
			}
		}
	}

	return false;
}

bool DoesNodeHaveConstraint(NiNode *rootNode, NiAVObject *node)
{
	bhkRigidBody *entity = GetRigidBody(node);
	if (!entity) {
		return false;
	}

	if (entity->numConstraints > 0) {
		// Easy case: it's a master entity
		return true;
	}

	return DoesEntityHaveConstraint(rootNode, entity);
}

NiAVObject * GetTorsoNode(Actor *actor)
{
	TESRace *race = actor->race;
	BGSBodyPartData *partData = race->bodyPartData;
	if (partData) {
		auto torsoData = partData->part[0];
		if (torsoData && torsoData->unk08.data) {
			NiAVObject *actorNode = actor->GetNiNode();
			if (actorNode) {
				NiAVObject *torsoNode = actorNode->GetObjectByName(&torsoData->unk08.data);
				if (torsoNode) {
					return torsoNode;
				}
			}
		}
	}
	return nullptr;
}

void updateTransformTree(NiAVObject * root, NiAVObject::ControllerUpdateContext *ctx)
{
	root->UpdateWorldData(ctx);

	auto node = root->GetAsNiNode();

	if (node) {
		for (int i = 0; i < node->m_children.m_arrayBufLen; ++i) {
			auto child = node->m_children.m_data[i];
			if (child) updateTransformTree(child, ctx);
		}
	}
}

void UpdateKeyframedNodeTransform(NiAVObject *node, const NiTransform &transform)
{
	NiTransform inverseParent;
	node->m_parent->m_worldTransform.Invert(inverseParent);

	node->m_localTransform = inverseParent * transform;
	NiAVObject::ControllerUpdateContext ctx;
	ctx.flags = 0x2000; // makes havok sim more stable
	ctx.delta = 0;
	NiAVObject_UpdateObjectUpwards(node, &ctx);

	ShadowSceneNode_UpdateNodeList(*g_shadowSceneNode, node, false);
}

bool IsTwoHanded(const TESObjectWEAP *weap)
{
	switch (weap->gameData.type) {
	case TESObjectWEAP::GameData::kType_2HA:
	case TESObjectWEAP::GameData::kType_2HS:
	case TESObjectWEAP::GameData::kType_CBow:
	case TESObjectWEAP::GameData::kType_CrossBow:
	case TESObjectWEAP::GameData::kType_Staff:
	case TESObjectWEAP::GameData::kType_Staff2:
	case TESObjectWEAP::GameData::kType_TwoHandAxe:
	case TESObjectWEAP::GameData::kType_TwoHandSword:
		return true;
	default:
		return false;
	}
}

bool IsBow(const TESObjectWEAP *weap)
{
	UInt8 type = weap->gameData.type;
	return (type == TESObjectWEAP::GameData::kType_Bow || type == TESObjectWEAP::GameData::kType_Bow2);
}

std::pair<bool, bool> AreEquippedItemsValid(Actor *actor)
{
	if (!actor->actorState.IsWeaponDrawn()) {
		return { true, true };
	}

	TESForm *mainhandItem = actor->GetEquippedObject(false);
	TESForm *offhandItem = actor->GetEquippedObject(true);

	return { !mainhandItem, !offhandItem };

	/*
	bool isMainValid = false, isOffhandValid = false;

	if (!mainhandItem || mainhandItem->formType == kFormType_Spell) {
		isMainValid = true;
	}
	TESObjectWEAP *weap = DYNAMIC_CAST(mainhandItem, TESForm, TESObjectWEAP);
	if (weap) {
		if (IsTwoHanded(weap)) {
			return std::make_pair(false, true); // Main hand holds the weapon, offhand is 'free' in VR
		}
		else if (IsBow(weap)) {
			return std::make_pair(true, false); // For bows, the main hand holds the arrow, offhand holds the bow
		}
	}
	if (!offhandItem || offhandItem->formType == kFormType_Spell) {
		isOffhandValid = true;
	}
	return std::make_pair(isMainValid, isOffhandValid);
	*/
}

void PrintVector(const NiPoint3 &p)
{
	_MESSAGE("%.2f, %.2f, %.2f", p.x, p.y, p.z);
}

bool VisitNodes(NiAVObject  *parent, std::function<bool(NiAVObject*, int)> functor, int depth = 0)
{
	if (!parent) return false;
	NiNode * node = parent->GetAsNiNode();
	if (node) {
		if (functor(parent, depth))
			return true;

		auto children = &node->m_children;
		for (UInt32 i = 0; i < children->m_emptyRunStart; i++) {
			NiAVObject * object = children->m_data[i];
			if (object) {
				if (VisitNodes(object, functor, depth + 1))
					return true;
			}
		}
	}
	else if (functor(parent, depth))
		return true;

	return false;
}

std::string PrintNodeToString(NiAVObject *avObj, int depth)
{
	std::stringstream avInfoStr;
	avInfoStr << std::string(depth, ' ') << avObj->GetRTTI()->name;
	if (avObj->m_name) {
		avInfoStr << " " << avObj->m_name;
	}
	BSGeometry *geom = avObj->GetAsBSGeometry();
	if (geom) {
		if (geom->m_spSkinInstance) {
			avInfoStr << " [skinned]";
		}
	}
	auto rigidBody = GetRigidBody(avObj);
	if (rigidBody) {
		avInfoStr.precision(5);
		avInfoStr << " m=" << (1.0f / rigidBody->hkBody->m_motion.getMassInv().getReal());
	}
	if (avObj->m_extraData && avObj->m_extraDataLen > 0) {
		avInfoStr << " { ";
		for (int i = 0; i < avObj->m_extraDataLen; i++) {
			auto extraData = avObj->m_extraData[i];
			auto boolExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiBooleanExtraData);
			if (boolExtraData) {
				avInfoStr << extraData->m_pcName << " (bool): " << boolExtraData->m_data << "; ";
				continue;
			}
			auto intExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiIntegerExtraData);
			if (intExtraData) {
				avInfoStr << extraData->m_pcName << " (int): " << intExtraData->m_data << "; ";
				continue;
			}
			auto stringExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiStringExtraData);
			if (stringExtraData) {
				avInfoStr << extraData->m_pcName << " (str): " << stringExtraData->m_pString << "; ";
				continue;
			}
			auto floatExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiFloatExtraData);
			if (floatExtraData) {
				avInfoStr << extraData->m_pcName << " (flt): " << floatExtraData->m_data << "; ";
				continue;
			}
			auto binaryExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiBinaryExtraData);
			if (binaryExtraData) {
				avInfoStr << extraData->m_pcName << " (bin): " << binaryExtraData->m_data << "; ";
				continue;
			}
			auto floatsExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiFloatsExtraData);
			if (floatsExtraData) {
				std::stringstream extrasStr;
				extrasStr << extraData->m_pcName << " (flts): ";
				for (int j = 0; j < floatsExtraData->m_size; j++) {
					if (j != 0)
						extrasStr << ", ";
					extrasStr << floatsExtraData->m_data[j];
				}
				avInfoStr << extrasStr.str() << "; ";
				continue;
			}
			auto intsExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiIntegersExtraData);
			if (intsExtraData) {
				std::stringstream extrasStr;
				extrasStr << extraData->m_pcName << " (ints): ";
				for (int j = 0; j < intsExtraData->m_size; j++) {
					if (j != 0)
						extrasStr << ", ";
					extrasStr << intsExtraData->m_data[j];
				}
				avInfoStr << extrasStr.str() << "; ";
				continue;
			}
			auto strsExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiStringsExtraData);
			if (strsExtraData) {
				std::stringstream extrasStr;
				extrasStr << extraData->m_pcName << " (strs): ";
				for (int j = 0; j < strsExtraData->m_size; j++) {
					if (j != 0)
						extrasStr << ", ";
					extrasStr << strsExtraData->m_data[j];
				}
				avInfoStr << extrasStr.str() << "; ";
				continue;
			}
			auto vecExtraData = DYNAMIC_CAST(extraData, NiExtraData, NiVectorExtraData);
			if (vecExtraData) {
				std::stringstream extrasStr;
				extrasStr << extraData->m_pcName << " (vec): ";
				for (int j = 0; j < 4; j++) {
					if (j != 0)
						extrasStr << ", ";
					extrasStr << vecExtraData->m_vector[j];
				}
				avInfoStr << extrasStr.str() << "; ";
				continue;
			}
			auto fgAnimExtraData = DYNAMIC_CAST(extraData, NiExtraData, BSFaceGenAnimationData);
			if (fgAnimExtraData) {
				avInfoStr << extraData->m_pcName << " (facegen anim); ";
				continue;
			}
			auto fgModelExtraData = DYNAMIC_CAST(extraData, NiExtraData, BSFaceGenModelExtraData);
			if (fgModelExtraData) {
				avInfoStr << extraData->m_pcName << " (facegen model); ";
				continue;
			}
			auto fgBaseMorphExtraData = DYNAMIC_CAST(extraData, NiExtraData, BSFaceGenBaseMorphExtraData);
			if (fgBaseMorphExtraData) {
				avInfoStr << extraData->m_pcName << " (facegen basemorph); ";
				continue;
			}
			avInfoStr << extraData->m_pcName << "; ";
		}
		avInfoStr << "}";
	}

	return std::regex_replace(avInfoStr.str(), std::regex("\\n"), " ");
}

bool PrintNodes(NiAVObject *avObj, int depth)
{
	gLog.Message(PrintNodeToString(avObj, depth).c_str());
	return false;
}

std::ofstream _file;
bool DumpNodes(NiAVObject *avObj, int depth)
{
	_file << PrintNodeToString(avObj, depth).c_str() << std::endl;
	return false;
}

void PrintSceneGraph(NiAVObject *node)
{
	_file.open("scenegraph.log");
	//VisitNodes(node, PrintNodes);
	VisitNodes(node, DumpNodes);
	_file.close();
}

void PrintToFile(std::string entry, std::string filename)
{
	std::ofstream file;
	file.open(filename);
	file << entry << std::endl;
	file.close();
}

NiPointer<bhkRigidBody> GetRigidBody(NiAVObject *obj)
{
	if (!obj->unk040) return nullptr;

	auto niCollObj = ((NiCollisionObject *)obj->unk040);
	auto collObj = DYNAMIC_CAST(niCollObj, NiCollisionObject, bhkCollisionObject);
	if (collObj) {
		NiPointer<bhkWorldObject> worldObj = collObj->body;
		auto rigidBody = DYNAMIC_CAST(worldObj, bhkWorldObject, bhkRigidBody);
		if (rigidBody) {
			return rigidBody;
		}
	}

	return nullptr;
}

bool DoesNodeHaveNode(NiAVObject *haystack, NiAVObject *target)
{
	if (haystack == target) {
		return true;
	}

	NiNode *node = haystack->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			NiAVObject *child = node->m_children.m_data[i];
			if (child) {
				if (DoesNodeHaveNode(child, target)) {
					return true;
				}
			}
		}
	}
	return false;
}

bool DoesRefrHaveNode(TESObjectREFR *ref, NiAVObject *node)
{
	if (!node || !ref || !ref->loadedState || !ref->loadedState->node) {
		return false;
	}

	return DoesNodeHaveNode(ref->loadedState->node, node);
}

bool IsSkinnedToNodes(NiAVObject *skinnedRoot, const std::unordered_set<NiAVObject *> &targets)
{
	// Check if skinnedRoot is skinned to target
	BSGeometry *geom = skinnedRoot->GetAsBSGeometry();
	if (geom) {
		NiSkinInstancePtr skinInstance = geom->m_spSkinInstance;
		if (skinInstance) {
			NiSkinDataPtr skinData = skinInstance->m_spSkinData;
			if (skinData) {
				UInt32 numBones = *(UInt32*)((UInt64)skinData.m_pObject + 0x58);
				for (int i = 0; i < numBones; i++) {
					NiAVObject *bone = skinInstance->m_ppkBones[i];
					if (bone) {
						if (targets.count(bone) != 0) {
							return true;
						}
					}
				}
			}
		}
		return false;
	}
	NiNode *node = skinnedRoot->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				if (IsSkinnedToNodes(child, targets)) {
					return true;
				}
			}
		}
		return false;
	}
	return false;
}

void PopulateTargets(NiAVObject *root, std::unordered_set<NiAVObject *> &targets)
{
	if (!root) return;

	// Populate targets with the entire subtree but stop when we hit a node with collision

	targets.insert(root);

	NiNode *node = root->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				if (!GetRigidBody(child)) {
					PopulateTargets(child, targets);
				}
			}
		}
	}
}

std::unordered_set<NiAVObject *> targetNodeSet;
bool IsSkinnedToNode(NiAVObject *skinnedRoot, NiAVObject *target)
{
	PopulateTargets(target, targetNodeSet);

	bool result = IsSkinnedToNodes(skinnedRoot, targetNodeSet);

	targetNodeSet.clear();
	return result;
}

void GetAllSkinnedNodes(NiAVObject *root, std::unordered_set<NiAVObject *> &skinnedNodes)
{
	BSGeometry *geom = root->GetAsBSGeometry();
	if (geom) {
		NiSkinInstancePtr skinInstance = geom->m_spSkinInstance;
		if (skinInstance) {
			NiSkinDataPtr skinData = skinInstance->m_spSkinData;
			if (skinData) {
				UInt32 numBones = *(UInt32*)((UInt64)skinData.m_pObject + 0x58);
				for (int i = 0; i < numBones; i++) {
					NiAVObject *bone = skinInstance->m_ppkBones[i];
					if (bone) {
						skinnedNodes.insert(bone);
					}
				}
			}
		}
	}
	NiNode *node = root->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				GetAllSkinnedNodes(child, skinnedNodes);
			}
		}
	}
}

bhkRigidBody * GetFirstCollision(NiAVObject *root)
{
	bhkRigidBody *rigidBody = GetRigidBody(root);
	if (rigidBody) {
		return rigidBody;
	}

	NiNode *node = root->GetAsNiNode();
	if (node) {
		for (int i = 0; i < node->m_children.m_emptyRunStart; i++) {
			auto child = node->m_children.m_data[i];
			if (child) {
				return GetFirstCollision(child);
			}
		}
	}

	return nullptr;
}

UInt32 PlaySoundAtNode(BGSSoundDescriptorForm *sound, NiAVObject *node, const NiPoint3 &location)
{
	UInt32 formId = sound->formID;

	SoundData soundData;
	BSAudioManager_InitSoundData(*g_audioManager, &soundData, formId, 16);
	if (soundData.id == -1) {
		return 0;
	}

	SoundData_SetPosition(&soundData, location.x, location.y, location.z);
	SoundData_SetNode(&soundData, node);
	if (SoundData_Play(&soundData)) {
		return soundData.id;
	}

	return 0;
}
