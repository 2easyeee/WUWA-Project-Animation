#pragma once

#include "Content_Define.h"
#include "CGameObject.h"
#include "WObject.h"
#include "WComponent.h"
#include <CStateController.h>
#include <CRole_Base.h>
#include <CCharacter.h>

BEGIN(Content)

static constexpr _float DEFAULT_ARM_SPEED{ 5.f };

class CONTENT_DLL CCharacterCamera final : public CGameObject {
    INSTANTIABLE(OBJECT)
private:
    explicit CCharacterCamera(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CCharacterCamera() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Priority_Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

    void Mouse_Input(_float _fTimeDelta);
    void SetUp_CameraDestOffset(const _float3& _vCameraDestOffset, _float _fFov);
    void Reset_CameraDestOffset();

private:
    _float m_fPosSnapEpsilon{ 0.01f };

public:
    void Set_TargetObject(CCharacter* _pNewTarget) {
        m_pTargetObject->Set(_pNewTarget);
    }

    CVirtualCamera* Get_VirtualCamera() const {
        return m_pVirtualCamera;
    }

    void Enter(_float3 _vPrevCameraPos);
    void Leave();

    void Character_Changed(CCharacterCamera* _pPrevPivot);
    void Character_Parried(CCharacterCamera* _pPrevPivot);

    void Begin_QTE();
    void End_QTE();

    void Turn_To_Player_Look_Instant(_float _fPitch);

    void Turn_To_Player_Look(_float _fAnimTime, _float _fPitch);
    void Turn_To_Player_Back(_float _fAnimTime, _float _fPitch);

    void WipeOut();

    void Begin_Execute();
    void End_Execute();

    void Init_Camera_Pos();
    void Snap_ToTargetKeepingRotation();

    void Set_CameraArmLength(_float _fArmLength) {
        m_fDestCameraArmLength = _fArmLength;
    }

    _float Get_CameraArmLength() const {
        return m_fCameraArmLength;
    }

    _float Get_CameraDestArmLength() const {
        return m_fDestCameraArmLength;
    }

    void Set_CameraArmLengthBias(_float _fArmLengthBias) {
        m_fCameraArmLengthBias = _fArmLengthBias < 0.f ? 0.f : _fArmLengthBias;
    }

    void Set_ObjectOffset(const _float3& _vDestCameraOffset) {
        m_vObjectDestOffset = _vDestCameraOffset;
    }

    void Set_ObjectOffset_Speed(_float _fSpeed) {
        m_fObjectOffsetSpeed = _fSpeed;
    }

    void Set_CameraArm_Speed(_float _fSpeed) {
        m_fCameraArmSpeed = _fSpeed;
    }

    void Set_AdditionalCameraOffset(const _float3& _vAdditionalCameraOffset) {
        m_vAdditionalCameraOffset = _vAdditionalCameraOffset;
    }

    void Unlock_MouseInput() {
        m_bMouseInputLocked = false;
    }

    void Lock_MouseInput() {
        m_bMouseInputLocked = true;
    }

    void Set_CameraRotation(class CCharacterCamera* _pDestCamera) {
        m_fYaw = _pDestCamera->m_fYaw;
        m_fPitch = _pDestCamera->m_fPitch;
        m_fRoll = _pDestCamera->m_fRoll;

        m_fDestYaw = _pDestCamera->m_fDestYaw;
        m_fDestPitch = _pDestCamera->m_fDestPitch;
        m_fDestRoll = _pDestCamera->m_fDestRoll;
    }

    void Set_DestFov(const _float _fFov) {
        m_fDestFov = _fFov;
    }

    void Set_DestFovMultiplier(const _float2 _fValue) {
        m_fDestFov = _fValue.x;
        m_fFovDampMultiplier = _fValue.y;
    }

    void Reset_DestFov() {
        m_fDestFov = k_fDefaultFov;
        m_fFovDampMultiplier = 1.f;
    }

private:
    _bool Calc_LocalRot(_float3& _vOutLocalRot);
    void Calc_LocalRotToWorld(const _float3& _vLocalRot);

private:
    CAnimator* m_pAnimator{ nullptr };
    CStateController* m_pStateController{ nullptr };
    CVirtualCamera* m_pVirtualCamera{ nullptr };

private:
    WObject<CCharacter>* m_pTargetObject{ nullptr };
    WComponent<CTransform>* m_pLookTransform{ nullptr };

    _float3 m_vPrevCameraPos{ 0.f, 0.f, 0.f };

    _float3 m_vObjectOffset{ 0.f, 0.7f, 0.f };
    _float3 m_vObjectOriginOffset{ 0.f, 0.7f, 0.f };
    _float3 m_vObjectDestOffset{ 0.f, 0.7f, 0.f };

    const _float k_fDefaultFov{ 72.f };
    _float m_fDestFov = k_fDefaultFov;
    _float m_fFovDampMultiplier{ 1.f };

    _float3 m_vCameraOffset{ 0.f, 0.f, -1.0f };
    _float3 m_vCameraOriginOffset{ 0.f, 0.f, -1.0f };
    _float3 m_vCameraDestOffset{ 0.f, 0.f, -1.0f };

    _float3 m_vAdditionalCameraOffset{ 0.f, 0.f, 0.f };

    _float m_fCameraArmLength{ 2.4f };
    _float m_fDestCameraArmLength{ 2.4f };
    _float m_fCameraArmLengthBias{ 0.f };

    _float m_fObjectOffsetSpeed{ 5.f };

    _float m_fCameraArmSpeed{ DEFAULT_ARM_SPEED };

    _float m_fCameraMoveTime{ 0.5f };
    _float m_fMouseSensor{ 0.1f };
    _bool m_bUpInit{ false };

    _float m_fYaw{ 0.f }, m_fPitch{ 0.f }, m_fRoll{ 0.f };
    _float m_fDestYaw{ 0.f }, m_fDestPitch{ 0.f }, m_fDestRoll{ 0.f };

    _float m_fLocalYaw{ 0.f }, m_fLocalPitch{ 0.f }, m_fLocalRoll{ 0.f };
    _float3 m_vStartLocalRot{ -24.f, 0.f, 0.f };

    AnimTransform* m_pLookAnimation{ nullptr };
    AnimTransform* m_pBackAnimation{ nullptr };

    _float m_fMouseMultiplier{ 1.f };
    _float m_fDestMouseMultiplier{ 1.f };

    bool     m_bRotInit = false;
    XMVECTOR m_qRot = XMQuaternionIdentity();
    _vector m_prevUp = XMVectorSet(0.f, 1.f, 0.f, 0.f);

    float m_fRotDamping = 18.f;

    _bool m_bMouseInputLocked = { false };

private:
    _bool   m_bInitFilter = false;
    _vector m_vTargetFiltered = XMVectorZero();
    float   m_fTargetFilterK = 25.f;

private:
    bool     m_bTargetInit = false;
    XMVECTOR m_vFilteredTarget = XMVectorZero();

    float    m_fTargetDamping = 20.f; // 타겟 필터 강도
    float    m_fPosDamping = 12.f; // 카메라 위치 댐핑
    XMVECTOR m_vCamVel = XMVectorZero(); // velocity (xyz만 사용)

    _bool    m_bCameraCollisionInit{ false };
    _float   m_fCameraCollisionArmLength{ 3.4f };

    _float2   m_fPitchLimit{ -32.f, 25.f };
public:
    void OnGui_Inspector_Context();

public:
    virtual Json::Value Serialize() override;
    virtual void Deserialize(Json::Value& _jsonValue) override;

private:
    virtual void Free() override;
};

END
