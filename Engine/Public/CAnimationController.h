#pragma once
#include "CUpdateComponent.h"
#include <WAsset.h>
#include <CAsset_Animation.h>
#include "CAnimationInfo.h"
#include "WObject.h"
#include "WComponent.h"
#include <CCCT.h>

BEGIN(Engine)

class ENGINE_DLL CAnimationController : public CUpdateComponent {
    INSTANTIABLE(COMPONENT)

public:
    typedef struct tagAdditiveLayer
    {
        ANIMATION_INFO* pAnimation = { nullptr };
        ANIMATION_STATE* pState = { nullptr };
        _bool bAdditive = { false };
        _float fWeight = { 0.f };

        vector<_bool> vecBoneAdditive;
    }ADDITIVE_LAYER;

    typedef struct tagBoneNode
    {
        _string strBoneName;
        _string strFullPath;
        _int iParent = { -1 };
        vector<_int> vecChildren;
    }BONE_NODE;

private:
    enum class ANIMATION_SET_MODE { ANIMATION, BLEND, ADDITIVE };

private:
    explicit CAnimationController(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext, CObject* _pOwner);
    virtual ~CAnimationController() = default;

public:
    virtual HRESULT Initialize() override;
    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    void Play();
    void Play(_string _strAnimName, _bool _bLoop = true);
    void Play(_string _strAnimName, _bool _bLoop, _float _fBlendTime);
    void Stop();
    void Pause();
    void Hold(_float _fRatio);
    _bool Is_Playing() const;
    _bool Is_AnimationEnded() const;

    ANIMATION_INFO* Get_CurAnimation() const;
    ANIMATION_STATE* Get_CurAnimationState();
    ANIMATION_INFO* Get_CurAnimationByName(_string _strName) const;
    vector<_string> Get_AnimationNames() const;
    CAsset_Animation* Get_Asset_Animation() const;

    /* Editor */
    void Set_EditorMode(_bool _bEnable);
    _bool Is_EditorMode() const;
    void Set_Time_Editor(_float _fTime);
    void Change_Animation_Editor(ANIMATION_INFO* _pAnimationInfo);
    _float Get_Time() const;

    /* Root Motion */
    void Set_ApplyRootMotion(_bool _bApply);
    void Set_ApplyRootMotionX(_bool _bApply);
    void Set_ApplyRootMotionZ(_bool _bApply);
    void Set_RootMotionTarget(class CObject* _pTarget);
    void Set_RootMotionMute(_float _fWeight);    
    void Set_RootMotionMultiplierX(_float _fMultiplier);
    void Set_RootMotionMultiplierZ(_float _fMultiplier);
    void Set_RootMotionMultiplierZ_C(const _float& _fMultiplier);
    void Set_SubRootMotion_Y(_bool _bApply);
    void Set_RootMotionMaxDistance(_bool _bActive, _float _fDistance = 0.f);

    /* Event */
    void Update_EventFunction(_float _fTimeDelta);

    REGISTER_FUNCTION(CAnimationController, Set_MuteWeight, _float)
    REGISTER_FUNCTION(CAnimationController, Set_DeltaMultiplier, _float)

private:
    WAsset<CAsset_Animation>* m_pAsset_Animation{ nullptr };
    WComponent<CCCT>* m_pCCCT{ nullptr };

    ANIMATION_INFO* m_pCurAnimation{ nullptr };

    ANIMATION_STATE* m_pCurAnimState = { nullptr };
    ANIMATION_STATE* m_pPreAnimState = { nullptr };

    CGameInstance* m_pGameInstance{ nullptr };

    /* Playback */
    _bool m_bPlayEnable = { true };

    /* Blending */
    ANIMATION_SET_MODE m_ePlayMode = ANIMATION_SET_MODE::ANIMATION;
    _bool m_bBlending = { true };
    _float m_fBlendElapsed = { 0.f };
    _float m_fBlendDuration = { 0.f };

    /* Root Motion */
    WObject<CObject>* m_pRootMotion_Target = { nullptr };
    WObject<CObject>* m_pRootMotion_X = { nullptr };
    WObject<CObject>* m_pRootMotion_Z = { nullptr };
    _bool m_bRootMotionEnable_X = { true };
    _bool m_bRootMotionEnable_Z = { true };

    _bool m_bEnableSubRootMotion_Y{ false }; // Root의 Y 움직임도 사용하고 싶을 때 켜기

    _float3 m_vLastPosition{0.f, 0.f, 0.f};

    _bool m_bDebugRootMotionMuteAll = { false };

    _bool m_bRootMotionMaxDistanceEnable = { false };
    _float m_fRootMotionMaxDistance = { 0.f };

    /* Additive */
    vector<ADDITIVE_LAYER> m_vecAdditiveLayers;
    vector<ADDITIVE_LAYER> m_vecPreAdditiveLayers;

    /* Cache (Bone ImGui) */
    vector<BONE_NODE> m_vecBoneNodes;

    /* Cache (Bone Mask) */
    vector<_int> m_vecChannelToBoneIndex;

    /* Editor */
    _bool m_bEditorMode = { false };

    /* Event */
    _float m_fPrevTime = { 0.f };
    _bool m_bEventFirstTick = { false };
    
private:
    void Play_Animation(_float _fTimeDelta);
    void Play_AnimationByEditor(_float _fTimeDelta);

    void NonBlend_Animation(const _string& _strName);

    /* Blend */
    void Blend_Animation(const _string& _strName);
    void Blend_Animation(const _string& _strName, _float _fBlendTime);
    void Change_Animation(ANIMATION_INFO* _pAnim, ANIMATION_STATE* _pState);
    void Change_AnimationByName(const _string& _strName, _int _iLayerIndex);

    /* Loop */
    void Loop_Animation(_float _fTimeDelta, ANIMATION_STATE* _pState);

    /* Additive */
    void Update_Additive(_float _fTimeDelta);
    void Play_Additive(_float _fTimeDelta);
    void Apply_Additive();
    void Reset_Additive();
    void Reset_PreAdditive();
    void Build_Additive();

    /* Mask (UI) */
    void Set_BoneMaskInHierarchy(const _string& _strBoneFullPath, _bool _bEnable, _int _iLayerIndex);
    _bool Is_BoneEnabled(const _string& _strBoneFullPath, _int _iLayerIndex);
    _int Find_BoneIndexByPath(const _string& _strBoneFullPath) const;

    /* Mask */
    void Recursive_BoneMask(_int _iBoneIndex, _bool _bEnable, _int _iLayerIndex);

    /* Mask (Only Build Data) */
    void Build_BoneHierarchy(_int _iLayerMask);

    void Render_BoneNode(const vector<BONE_NODE>& _vecNodes, _int _iIndex, _int _iLayerIndex);

    /* RootMotion */
    void Apply_RootMotionLimit(ANIMATION_STATE* _pState);

public:
    void OnGui_Inspector_Context();

    void Clean_Events();
    virtual Json::Value Serialize() override;
    virtual void Deserialize(Json::Value& _jsonValue) override;

private:
    virtual void Free() override;

};

END