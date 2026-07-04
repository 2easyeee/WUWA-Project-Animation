#pragma once
#include "CFunctionRegistry.h"
#include <ImGui/ImSequencer.h>
#include <ImGui/ImCurveEdit.h>
#include "WFunction.h"
#include "WComponent.h"
#include "CCCT.h"

BEGIN(Engine)

struct CUSTOM_KEY_FRAME
{
	_float fTrackPosition{0.f};
	union
	{
		_int	iParam;
		_float	fParam;
		_float2 v2Param;
		_float3 v3Param;
		_float4 v4Param;
	};
};

struct EVENT_MODIFIED_TIME
{
	_ullong ullFileTimeUtc = 0;

public:
	void Update_Now();
	_bool Is_Empty() const;
};

struct ENGINE_DLL EVENT_KEY
{
	_string strGUID = "";
	EVENT_MODIFIED_TIME tModifiedTime = {};

	_int iEventLayerIndex = { -1 };
	_string strEventName = { "Unknown" };

	WFunction* pFunction = { nullptr };
	CUSTOM_KEY_FRAME tKF;

public:
	HRESULT Ensure_Meta();
	void Touch_ModifiedTime();
};

struct CUSTOM_ANIM
{
	_int iEventLayerIndex = { -1 };
	_string strCustomAnimName = { "Unknown" };

	_float fStartTime;
	_float fEndTime;

	WFunction* pFunction = { nullptr };
	vector<CUSTOM_KEY_FRAME> vecKF;
	_uint iCurKeyFrameIndex = { 0 };

public:
	ParamValue Evaluate_CustomKeyFrame(_float _fTimeInCustomAnim);
};

typedef struct tagKeyFrame {
	_float	fTrackPosition;
	_float3 vScale;
	_float4 vRotation;
	_float3 vPosition;
} KEY_FRAME;

template<typename T>
struct KeyFrame {
	_float	fTime;
	T		vValue;
};

using KeyVec3 = KeyFrame<_float3>;
using KeyVec4 = KeyFrame<_float4>;

typedef struct tagChannel {
	_string strBoneName = { "" };
	_string strBoneRoute = { "" };

	/* Key Frame (In) */
	vector<KeyVec3> vecScalingKeys;
	vector<KeyVec4> vecRotationKeys;
	vector<KeyVec3> vecPositionKeys;

	/* Key Frame (Out) */
	_uint iNumKeyFrames = { 0 };
	vector<KEY_FRAME> vecKeyFrames;

public:
	void Build_KeyFrames(_float _fDuration, _float _fTickPerSecond);

	KEY_FRAME Get_CurrentKeyFrame(_uint* _pKeyFrameIndex, _float _fTrackPosition);

	/* Deprecated */
	void Apply(_uint* _pCurrentKeyFrameIndex, _float _fCurrentTrackPosition, class CTransform* _pBoneTransform);
	
	void Apply_Blend(KEY_FRAME& _curKeyFrame, KEY_FRAME& _nextKeyFrame, _float _fRatio, class CTransform* _pBoneTransform);
}CHANNEL;

typedef struct tagAdditiveLayerInfo
{
	_string strAdditiveAnimName = { "" };
	_bool bAdditive = { false };
	_float fWeight = { 0.f };

	/* Bone Mask */
	unordered_map<_string, _bool> umBoneMask;
}ADDITIVE_LAYER_INFO;

struct ENGINE_DLL ANIMATION_INFO : public CBase {
	_float fDuration{ 1.f };
	_float fTickPerSecond{ 60.f };

	_string strName{};
	_string strAmartureRoute = { "" };

	/* Channel */
	_uint iNumChannels = { 0 };
	vector<CHANNEL> vecChannels;

	/* Blending */
	_bool bBlending = { true };
	_float fBlendTime = { 0.25f };

	/* Root Motion */
	_string strRootBoneXName = { "Bip001" };
	_string strRootBoneZName = { "Root" };
	_int iRootBoneXCacheIndex = { -1 };
	_int iRootBoneZCacheIndex = { -1 };

	_float fRootMotionMultiplierX{ 1.f };
	_float fRootMotionMultiplierZ{ 1.f };

	_float fRootMotionMuteWeight = { 1.f };

	/* Additive */
	vector<ADDITIVE_LAYER_INFO> vecAdditiveInfos;

	/* Event */
	vector<EVENT_KEY> vecEvents;
	vector<CUSTOM_ANIM> vecCustomAnims;

public:
	void Update_Channel(class CObject* _pOwner, _float _fTime, class CTransform* _pTransform, struct ANIMATION_STATE* _pState);
	void Update_Channel(class CObject* _pOwner, class CObject* _pRootMotionTarget, struct ANIMATION_STATE* _pCurState, struct ANIMATION_STATE* _pNextState, _float _fRatio);
	void Update_Channel(class CObject* _pOwner, class CObject* _pRootMotionTarget, struct ANIMATION_STATE* _pCurState, struct ANIMATION_STATE* _pNextState);
	
	/* Root Motion */
	void Apply_RootMotion(class CObject* _pOwner, class CObject* _pTarget, _float2 _vDelta, struct ANIMATION_STATE* _pCurState);
	_float2 Compute_RootMotionDelta_FromKeyFrame(struct ANIMATION_STATE* _pCurState, struct ANIMATION_STATE* _pNextState, _float _fRatio);
	void Update_SyncWithTarget(class CObject* _pOwner, class CObject* _pTarget);
	void Reset_RootBone(struct ANIMATION_STATE* _pState);

	/* Event */
	void Update_CustomAnim(ANIMATION_STATE* _pState);

	/* Blend */
	void Apply_Blend_State(class CObject* _pOwner, class CObject* _pRootMotionTarget, struct ANIMATION_STATE* _pCurState, struct ANIMATION_STATE* _pNextState, _float _fCurWeight, _float _fNextWeight);

	/* Cache */
	void Cache_RootBoneIndex();

public:	
	virtual void Free() override;
	void Save(vector<_byte>& _vecData);
	HRESULT Load(_uint _iVersion, FILE* _pFile);
};

struct ENGINE_DLL ANIMATION_STATE {
	ANIMATION_INFO* pAnimation = { nullptr };
	CCCT* pCCCT{ nullptr };

	_float fTime = { 0.f };
	_bool bLoop = { true };
	_bool bFinished = { false };
	_bool bRestInit = { false };

	/* Frame */
	vector<_uint> vecKeyFrameIndices;

	/* Cache */
	vector<class CTransform*> vecBoneTransformCache;

	/* Root Motion */
	class CObject* pRootBoneTarget = { nullptr };
	class CTransform* pRootBoneX = { nullptr };
	class CTransform* pRootBoneZ = { nullptr };
	_bool bSwitchRootMotion = { true };
	_bool bRootMotionEnableX = { true };
	_bool bRootMotionEnableZ = { true };

	_bool bEnableSubRootMotion_Y{ false }; // Root의 Y 움직임도 사용하고 싶을 때 켜기

	_float3 vPreRootXPos = { 0.f, 0.f, 0.f };
	_float3 vPreRootZPos = { 0.f, 0.f, 0.f };

	_bool bRootMotionMaxDistanceEnable = { false };
	_float fRootMotionMaxDistance = { 0.f };

	/* Event */
	class CObject* pBindingRoot = { nullptr };
	struct ANIM_BINDING_CONTEXT_CACHE {
		unordered_map<_uint, EventBindContext> umEventBindingContext{};
		unordered_map<_uint, BindingContext> umCustomAnimBindingContext{};
	};
	unordered_map<ANIMATION_INFO*, ANIM_BINDING_CONTEXT_CACHE> m_umBindingContextByAnim{};

	/* Blend */
	_float fBlendElpased = { 0.f };
	_bool bBlending = { false };
	vector<KEY_FRAME> vecPose;

	/* Additive Blend */
	_bool bAdditiveRestInit = { false };
	vector<KEY_FRAME> vecAdditiveRestPose;

public:
	ANIMATION_STATE() {
	}
	~ANIMATION_STATE() {
	}
public:
	void Reset(class CObject* _pOwner);
	void Reset_KeyFrameIndex();

	void Cache_BoneTransform(class CTransform* _pRootTransform);

	void Init_BindingContext(class CObject* _pOwner);
	EventBindContext* Get_EventBindContext(_uint _iEventIndex);
	BindingContext* Get_CustomAnimBindContext(_uint _iCustomAnimIndex);

};

END 
