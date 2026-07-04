#pragma once

#include "Content_Define.h"
#include "CCharacter.h"
#include <CObjectFilter.h>
#include <CAnimationController.h>
#include <CAnimationStateMachine.h>
#include <CVirtualCamera.h>
#include <CCollider_Capsule.h>
#include <CRigidbody.h>
#include <CCCT.h>
#include <CMonster.h>
#include "WObject.h"
#include <CTrigger_Capsule.h>

namespace Engine {
    class CPostProcessEventController;
}

BEGIN(Content)

class CONTENT_DLL CRole_Base : public CCharacter {
    INSTANTIABLE(OBJECT)

protected:
    enum E_OBJECTS {
        CHARACTER_CAMERA,
        EFFECT_HOLDER,
        SKILL_CAMERA_RIG,
        SOUND_FILTER_SFX,
        SOUND_FILTER_VOX,
    };

    enum E_CAMERA_POINT {
        MAIN_CAMERA
    };

    enum class E_INPUT_TYPE {
        ATTACK, ATTACK_L, ATTACK_AIR, 
        BURST, FINALBURST, 
        EXECUTE,
    };

    struct E_INPUT_EVENT {
        E_INPUT_TYPE eType;
        _float fTime = { 0.2f };
    };

public:
    struct ROLE_DESC : public CHARACTER_DESC {
        // x : total value, y : elapsed, z : duration, w : font popup timer
        _float4 vHeal = { 0.f, 0.f, 0.f, 0.f };

        _float fStamina{ 100.f };
        _float fMaxStamina{ 100.f };
        _int iStaminStepCount{ 5 };

        // x : Current, y : Max, z : Step_Count(1 or 2 or 3), w : UsedEnergy_Count
        _float4 vIdentity_Energy{ 0.f, 0.f, 0.f, 0.f };
        
        _float2 vHarmony_Energy{ 0.f, 0.f };

        _float2 vUltra_Energy{ 0.f, 0.f };

        _float fDamage{ 1000.f };

        DAMAGE_FONT_ELEMENT eElement = DAMAGE_FONT_ELEMENT::Light;

        array<_float2, magic_enum::enum_count<COOLDOWN_TYPE>() - 1> arrCooldown{};  // _float2(current, max)
        _int    iCooldownIndex = 0;             // 0 ~ 3
        _float2 vCooldownValue = { 0.f, 0.f };  // current, max

        _float2 vUltZeroRate{ 0.f, 0.f };

        void Update_Stamina(_float _fTimeDelta) {
            /* Stamina */
            fStamina += _fTimeDelta * 10.f;
            fStamina = clamp(fStamina, 0.f, fMaxStamina);
        }

        void Update_Cooldown(_float _fTimeDelta) {
            /* Skill */
            for (_float2& vValue : arrCooldown) {
                vValue.x = clamp(vValue.x - _fTimeDelta, 0.f, vValue.y);
            }
        }

        void Reset_Cooldown() {
            /* Skill */
            for (_float2& vValue : arrCooldown) {
                vValue.x = 0.f;
            }
        }

        _float Update_Heal(_float _fTimeDelta) {
            /* Heal */
            if (vHeal.z <= 0.f)
                return 0.f;

            _float fPrevHp = fHp;
            _float fPrevElapsed = vHeal.y;
            vHeal.y = min(vHeal.y + _fTimeDelta, vHeal.z);

            _float fDeltaRatio = (vHeal.y - fPrevElapsed) / vHeal.z;
            _float fHeal = vHeal.x * fDeltaRatio;
            
            fHp = min(fHp + fHeal, fMaxHp);

            if (vHeal.y >= vHeal.z)
                vHeal = {};

            return fHeal;
        }
        
        /* Identity Energy */
        _bool Check_Identity_Eneergy() {
            return vIdentity_Energy.x >= vIdentity_Energy.w;
        }

        void Gain_Identity_Energy(_float _fValue) {
            if (_fValue <= 0.f)
                return;

            vIdentity_Energy.x += _fValue * 2.f;
            vIdentity_Energy.x = clamp(vIdentity_Energy.x, 0.f, vIdentity_Energy.y);
        }

        _bool Spend_Identity_Energy() {
            if (!Check_Identity_Eneergy())
                return false;

            vIdentity_Energy.x -= vIdentity_Energy.w;
            vIdentity_Energy.x = clamp(vIdentity_Energy.x, 0.f, vIdentity_Energy.y);

            return true;
        }

        /* Harmony Energy */
        _bool Check_Harmony_Energy() {
            return vHarmony_Energy.x >= vHarmony_Energy.y;
        }

        void Gain_Harmony_Energy(_float _fValue) {
            if (_fValue <= 0.f)
                return;

            vHarmony_Energy.x += _fValue * 2.f;
            vHarmony_Energy.x = clamp(vHarmony_Energy.x, 0.f, vHarmony_Energy.y);
        }

        _bool Spend_Harmony_Energy() {
            if (!Check_Harmony_Energy())
                return false;

            vHarmony_Energy.x -= vHarmony_Energy.y;
            vHarmony_Energy.x = clamp(vHarmony_Energy.x, 0.f, vHarmony_Energy.y);

            return true;
        }

        /* Ultra Energy */
        _bool Check_Ultra_Energy() {
            return vUltra_Energy.x >= vUltra_Energy.y;
        }

        void Gain_Ultra_Energy(_float _fValue) {
            if (_fValue <= 0.f)
                return;

            vUltra_Energy.x += _fValue * 2.f;
            vUltra_Energy.x = clamp(vUltra_Energy.x, 0.f, vUltra_Energy.y);
        }

        _bool Spend_Ultra_Energy() {
            if (!Check_Ultra_Energy())
                return false;

            vUltra_Energy.x -= vUltra_Energy.y;
            vUltra_Energy.x = clamp(vUltra_Energy.x, 0.f, vUltra_Energy.y);

            return true;
        }

        /* Skill CoolDown */
        _bool Check_Cooldown(COOLDOWN_TYPE _eType) {
            return arrCooldown[ENUM_TO_UINT(_eType)].x <= 0.f;
        }

        void Spend_SkillCoolDown(COOLDOWN_TYPE _eType) {
                
            arrCooldown[ENUM_TO_UINT(_eType)].x = arrCooldown[ENUM_TO_UINT(_eType)].y;
        }

        /* Stamina */
        _bool Check_Stamina(_int _iStep) {
            if (iStaminStepCount <= 0)
                return false;

            if (_iStep <= 0)
                return true;

            _float fStepValue = fMaxStamina / iStaminStepCount;
            _float fRequiredStamina = fStepValue * _iStep;

            return fStamina >= fRequiredStamina;
        }

        _bool Check_Stamina(_float _fValue) {
            return fStamina >= _fValue;
        }

        _bool Use_Stamina(_int _iStep) {
            if (!Check_Stamina(_iStep))
                return false;

            _float fStepValue = fMaxStamina / iStaminStepCount;
            _float fRequiredStamina = fStepValue * _iStep;

            fStamina -= fRequiredStamina;
            fStamina = clamp(fStamina, 0.f, fMaxStamina);

            return true;
        }

        _bool Use_Stamina(_float _fValue = 0.f) {
            if (!Check_Stamina(_fValue))
                return false;

            fStamina -= _fValue;
            fStamina = clamp(fStamina, 0.f, fMaxStamina);

            return true;
        }

        /* Heal */
        void Start_Heal(_float _fValue, _float _fDuration)
        {
            if (vHeal.z > 0.f)
                return;

            vHeal = { _fValue, 0.f, _fDuration, 0.f };
        }

        /* Inspector for Test */
        void OnGui_Inspector_Context() {
            _float pHp[2] = { fHp, fMaxHp };
            if (ImGui::DragFloat2("Hp", pHp)) {
                fHp = pHp[0];
                fMaxHp = pHp[1];
            }

            _float pStamina[2] = { fStamina, fMaxStamina };
            if (ImGui::DragFloat2("Stamina", pStamina)) {
                fStamina = pStamina[0];
                fMaxStamina = pStamina[1];
            }

            if (ImGui::DragFloat4("Identity Energy", &vIdentity_Energy.x, 0.01f, 0.f)) {

            }

            ImGui::DragFloat2("Harmony_Energy", &vHarmony_Energy.x);

            ImGui::DragFloat2("Ultra_Energy", &vUltra_Energy.x);

            if (ImGui::DragFloat("Damage", &fDamage, 0.01f, 0.f)) {
 
            }

            ImGui::DragInt("Cooldown Index", &iCooldownIndex, 1, 0, static_cast<_int>(arrCooldown.size()) - 1);
            ImGui::DragFloat2("Cooldown Value", &vCooldownValue.x, 0.01f, 0.f);

            if (ImGui::Button("Apply Cooldown"))
            {
                if (0 <= iCooldownIndex && iCooldownIndex < static_cast<_int>(arrCooldown.size()))
                    arrCooldown[iCooldownIndex] = vCooldownValue;
            }
        }
    };

    struct EXECUTE_CAMERA_DESC
    {
        _float3 vStartCameraOffset{ -0.6f, -0.3f, -1.f };
        _float3 vStartObjectOffset{ -0.35f, 1.2f, -0.15f };
        _float fStartArmLength{ 3.f };

        _float3 vMiddleCameraOffset{ -1.f, -0.35f, -1.f };
        _float3 vMiddleObjectOffset{ -0.5f, 1.2f, 0.5f };
        _float fMiddleArmLength{ 2.f };

        _float3 vEndObjectOffset{ 0.f, 1.15f, 0.f };
        _float fEndArmLength{ 4.2f };
        _float fEndPitch{ 35.f };
    };

protected:
    explicit CRole_Base(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_Base() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Priority_Update(_float _fTimeDelta) override;
    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

private:
    void Record_VideoInput();

public:
    template<typename T>
    T* Get_CameraPoint(_uint _iIndex) {
        auto pObject = m_pObjectFilter->Get_Object<CObject>(CHARACTER_CAMERA);
        if (pObject == nullptr) {
            return nullptr;
        }

        if (auto pObjectFilter = pObject->GetComponent<CObjectFilter>()) {
            return pObjectFilter->Get_Object<T>(_iIndex);
        }

        return nullptr;
    }

public:
    virtual const ROLE_DESC& Get_Desc() const {
        return m_tRoleDesc;
    }

    class CCharacterCamera* Get_CharacterCamera();

    void Auto_Turn_To_Target_Monster(_float _fTurnSpeed, _float _fAutoTurnDuration);
    REGISTER_FUNCTION(CRole_Base, Auto_Turn_To_Monster_Event, _float2)
    REGISTER_FUNCTION(CRole_Base, Turn_To_Player_Look_Event, _float2)
    REGISTER_FUNCTION(CRole_Base, Set_Attack_RootMotion_Limit, _float)

    virtual void Register_Meta_Bone() override;
    void Enter_SideViewMode(const SIDEVIEW_MODE_DESC& _tSideViewDesc);
    void Exit_SideViewMode();

    void Teleport(const _float4x4& _matWorld);
    void Apply_FallBackPose(const _float3& _vBaseUp, const _float3& _vPosition, const _float4& _vRotation);

    virtual CRole_Base* Get_CurrentPlayRole() {     // Get_CurrentRole에서 Amisi_GD 특수화를 위한 가상함수
        return this;
    }

    virtual void Switch_In(CRole_Base* _pPrevRole);
    virtual void Switch_Out();

    void Set_SpawnPosByRadius(_fvector _vCenterPos, _fvector _vBaseDirection, _fvector _vBaseUp, _fvector _vPrevPos, _fvector _vPrevLook, _float _fYawDegree, _float _fRadius, _vector& _vSpawnPos, _vector& _vSpawnLook);
    virtual _int Get_Switch_Out_ComboMaxCount() const { return 4; }
    void Begin_Switch_Out_ComboChain();
    void Update_Switch_Out_ComboChain(_float _fTimeDelta);

    void Update_Switch(_float _fTimeDelta);
    void Update_Switch_In(_float _fTimeDelta);
    void Update_Switch_Out(_float _fTimeDelta);

    virtual void Prepare_Switch_Out() {};

    virtual _float Get_SwitchDitheringTime();

    _bool Update_PlayerControlLock();
    _bool Is_AttackState();
    _bool Is_BurstState();
    _bool Is_QTEState();
    _bool Is_LocomotionState();

    _bool Check_SideView_Hook_Enable(_fvector _vHookPos);
    void Exit_Grappling();

    void Update_IFrameTime(_float _fTimeDelta);
    void Forced_Return_EffectPool(_int _iEffectIndex);

public:
    virtual ROLE_DESC& Get_Desc() {
        return m_tRoleDesc;
    }

    E_PLAY_MODE Get_PlayMode() const {
        return m_ePlayMode;
    }

    virtual _bool Is_Grappling() const {
        return m_bGrappling;
    }

    CGameObject* Get_GrappleTargetObject() const {
        return m_pInteractionObj ? m_pInteractionObj->Get() : nullptr;
    }

    CObject* Get_BackHolder() const {
        return m_pBackHolder ? m_pBackHolder->Get() : nullptr;
    }

    void OnBoard_Vehicle(CTransform* _pVehicleTransform);
    void GetOff_Vehicle(_float _fFloatVelocity = 0.f, _float3 _vInheritVelocity = {});
    _bool Is_OnVehicle() const {
        return m_bOnVehicle;
    }

    virtual EXECUTE_CAMERA_DESC Get_ExecuteCameraDesc() const;

protected:
    inline _bool State_Check(_ullong _iState) {
        return m_iState & _iState;
    }

public:
    inline _bool Is_Grounded() const {
        return m_bGrounded;
    }
    virtual _bool Assulted(CCharacter* _pAssulter, const ATTACK_DESC* _pAttackDesc) override;

    void Set_InputLock(_bool _bInputLock) {
        m_bInputLock = _bInputLock;
    }

public:
    void Grapple_Input();

protected:
    void Reset_Input(_float _fTimeDelta);
    void Reset_Locomotion();
    void Reset_Gravity();
    virtual void Reset_CameraSettings();
    void Clear_AllInput();

    _bool Is_PlayerControlLocked() const;

    virtual void Init_Push_Camera();
    void Update_Locomotion(_float _fTimeDelta);
    void Update_RemainLocomotion(_float _fTimeDelta);
    virtual void Update_AirMotion(_float _fTimeDelta);

    void Add_JumpVelocity(_float3 _vVelocity);
    void Set_JumpVelocity_Y(_float _fVelocityY);
    void Apply_JumpVelocity(_float _fTimeDelta);
    void Update_LandLock(_float _fTimeDelta);

    void Mute_RootMotion(_bool _bApply);
    void Set_RootMotionLimit(_bool _bApply);
    virtual void Init_AttackRootMotionLimit();
    void Update_AttackRootMotionLimit();

    void Keep_DistanceToTarget();
    void Update_AdditionalCameraOffset();
    void Update_BossCameraArm(_float _fTimeDelta);

    _bool Is_JumpStarting();
    _bool Is_Falling();

    _bool Is_JumpState();
    _bool Check_JumpState();
    _bool Is_FlyState();
    _bool Check_Movestate();
    _bool Has_RunInput();
    _bool Has_MoveInput();
    _bool Has_MoveInput_Down();
    _bool Has_JumpInput();
    _bool Has_JumpInput_Down();
    _bool Check_Slidable(); // 슬라이드 가능한지 ?

    void Update_Lock_Move(_float _fTimeDelta);
    void Lock_Move(_float _fCooldown);
    void Release_Move_Lock();

    virtual void Init_Forced_Return_EffectPool() {}
    void Flush_Forced_Return_EffectPool();

    /* Role Desc */
    virtual void Init_Role_Desc() {};
    virtual void Update_Role_Desc(_float _fTimeDelta);

    REGISTER_FUNCTION(CRole_Base, Set_Weapon_Visual_Instant, _int)
    REGISTER_FUNCTION(CRole_Base, Set_Scabbard_Visual_Instant, _int)

    void Play_Sound(E_OBJECTS _eObjects, const _float2& _vParam);
    void Play_Sound(E_OBJECTS _eObjects, const _float3& _vParam);

    void Apply_DeltaAnim(E_EASING _eEnter, E_EASING _eLeave, _float _fTargetDelta, 
                _float _fEnterTime, _float _fDuration, _float _fLeaveTime);
    void Apply_DeltaAnim(E_EASING _eEnter, E_EASING _eLeave, _float _fTargetDelta, 
                _float _fEnterTime, _float _fDuration, _float _fLeaveTime, 
                _bool _bApplyToPlayers, _bool _bApplyToMonsters);
    
    /* FX */
    REGISTER_FUNCTION(CRole_Base, Instantiate_Effect, _float4);

    /* Character */
    REGISTER_FUNCTION(CRole_Base, Run_SpeedFactor, _float);
    REGISTER_FUNCTION(CRole_Base, Pause_Animation, monostate);
    REGISTER_FUNCTION(CRole_Base, Stop_Animation, monostate);
    REGISTER_FUNCTION(CRole_Base, Disable_State, monostate);

    /* Render */
    REGISTER_FUNCTION(CRole_Base, Set_Active_Render_Children, monostate);
    REGISTER_FUNCTION(CRole_Base, Set_InActive_Render_Children, monostate);

    /* Camera */
    REGISTER_FUNCTION(CRole_Base, Pop_Camera, monostate);
    REGISTER_FUNCTION(CRole_Base, Push_Camera, monostate);
    REGISTER_FUNCTION(CRole_Base, Set_CameraArmLength, _float);
    REGISTER_FUNCTION(CRole_Base, Set_CameraArmLengthSpeed, _float);
    REGISTER_FUNCTION(CRole_Base, Set_CameraObjectOffset, _float3);
    REGISTER_FUNCTION(CRole_Base, Push_Camera_Shake, _int);
    REGISTER_FUNCTION(CRole_Base, Push_Camera_Shake_Intensity, _float2)
    REGISTER_FUNCTION(CRole_Base, Clear_Camera_Shake, _float);
    REGISTER_FUNCTION(CRole_Base, Set_DestFov, _float);
    REGISTER_FUNCTION(CRole_Base, Set_DestFovMultiplier, _float2);
    REGISTER_FUNCTION(CRole_Base, Reset_DestFov, monostate);
    
    /* Skill Camera */
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Order, _int);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Transition, _int);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Transition_Time, _float);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Blend_Mask, _int);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Fov, _float);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_HandoffCorrection, _int);
    REGISTER_FUNCTION(CRole_Base, Set_Skill_Camera_Desc, _float4);
    REGISTER_FUNCTION(CRole_Base, Push_Skill_Camera, monostate);
    REGISTER_FUNCTION(CRole_Base, Pop_Skill_Camera, _int);
    REGISTER_FUNCTION(CRole_Base, Pop_Skill_Camera_Smooth, _float4);
    REGISTER_FUNCTION(CRole_Base, Set_SkillCameraRig_LayerDelta, _float4);
    
    /* Sound Filter */
    REGISTER_FUNCTION(CRole_Base, Play_Sound_SFX, _float2)
    REGISTER_FUNCTION(CRole_Base, Play_Sound_VOX, _float2)
    REGISTER_FUNCTION(CRole_Base, Play_Sound_RUN, _float3)

    /* Weapon Visual */
    REGISTER_FUNCTION(CRole_Base, Set_Weapon_Visual, _int)
    REGISTER_FUNCTION(CRole_Base, Set_Scabbard_Visual, _int)
    
protected:
    virtual void Init_Change_State_Common();
    virtual void Init_Change_State_Locomotion();
    void Init_Change_State_Common_Attack();
    virtual void Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate);
    virtual void Init_Change_State_Attack_L();
    virtual void Init_Change_State_Jump();
    virtual void Init_Change_State_Fly();
    virtual void Init_Change_State_Skill();
    virtual void Init_Change_State_Switch_QTE();
    void Init_Change_State_Behit();

    virtual void Init_Jump_Start_State();
    virtual void Init_Jump_Loop_State();
    virtual void Init_Jump_Land_State();
    void Init_Jump_Second_State();
    virtual void Init_Fly_Start_State();
    virtual void Init_Fly_Loop_State();

    _bool Check_Grapple_Done();

    void Init_Hook_State();
    void Init_Fall_State();

    virtual void Init_Throw_State();
    virtual void Init_Sit_State();

protected:
    void Locomotion_Input(_float _fTimeDelta);
    void Run_Input(_float _fTimeDelta);
    virtual void Attack_Input(_float _fTimeDelta);
    virtual void Consume_Input();
    void Expire_Input(_float _fTimeDelta);
    void Interaction_Input();

    /* 사이드 뷰 인풋 */
    void SideView_Run_Input(_float _fTimeDelta);

protected:
    void Init_Disable_State();
    void Init_Idle_State();
    virtual void Init_Run_State();
    void Init_Walk_State();
    virtual void Init_Sprint_State();
    virtual void Init_Move_State();
    void Init_Move_Limit_State();

    CObject* Get_SkillCameraRig();
    CVirtualCamera* Get_SkillVirtualCamera(CObject* _pCameraRig);
    CAMERA_TRANSITION_DESC Build_SkillCameraTransitionDesc(CVirtualCamera* _pVirtualCamera);
    void Restart_SkillCameraAction(CObject* _pCameraRig);
    void Reset_SkillCameraDesc();
    void Force_PopSkillCamera();
    virtual void Reset_ControlCameraSettings();

    virtual void Init_Attack01_State(_float _fEndTime);
    virtual void Init_Attack02_State(_float _fEndTime);
    virtual void Init_Attack03_State(_float _fEndTime);
    virtual void Init_Attack04_State(_float _fEndTime);
    virtual void Init_Attack05_State(_float _fEndTime);
    virtual void Init_Attack06_State(_float _fEndTime);
    virtual void Init_Attack_L_Loop_State(_float _fEndTime);
    virtual void Init_Attack_L_End_State(_float _fEndTime);
    virtual void Init_Execute_State();
    virtual void Init_Skill01_State(_float _fEndTime);
    virtual void Init_Skill02_State(_float _fEndTime);
    virtual void Init_Skill03_State(_float _fEndTime);
    virtual void Init_Skill04_State(_float _fEndTime);
    virtual void Init_Skill05_State(_float _fEndTime);
    virtual void Init_Skill06_State(_float _fEndTime);
    virtual void Init_Burst01_State(_float _fEndTime);
    virtual void Init_Attack_Air_Start_State(_float _fEndTime);
    virtual void Init_Attack_Air_End_State(_float _fEndTime);
    void Init_Behit_State();
    virtual void Init_Switch_QTE_State(_float _fEndTime);

    void Init_Collision();

private:
    _bool Monster_Detacted(CTrigger* _pSelf, CTrigger* _pCollided);

    void Debug_Test(_float _fTimeDelta);

    void Show_HealFont(_float fHeal);

protected:
    /* Jump */
    _float m_fJumpForce = { 10.f };
    _float m_fLandLockTime = { 0.f };
    _float m_fLandLockDuration = { 0.15f };

    /* Air Attack */
    _bool m_bAirAttackDiveApplied = { false };

    /* CCCT */
    class CCCT* m_pCCT = { nullptr };

    //Post Process Event Controller
    Engine::CPostProcessEventController* m_pPostProcessController{ nullptr };

    /* Physics */
    _float3 m_vCharacterVelocity = { 0.f, 0.f, 0.f };
    _float m_fGravity = { 9.81f };
    _float m_fJumpGravityScale = { 1.2f };
    _float m_fFlyGravityScale = { 0.25f };

    // 중력 방향 추가
    _float3 m_vGravityDirection = { 0.f, -1.f, 0.f };

    /* Land */
    _bool m_bGrounded = { true };
    _int m_iGroundContect{ 0 };

    /* Camera */
    class CCharacterCamera* m_pCharacterCamera{ nullptr };
    CAMERA_TRANSITION_DESC m_tSkillCameraTransitionDesc{};
    CAMERA_ORDER m_ePushedSkillCameraOrder{ CAMERA_ORDER::CNT };
    _float m_fSkillCameraFovOverride{ -1.f };
    class CTransform* m_pDetachedSkillCameraRigTransform{ nullptr };
    class CTransform* m_pDetachedSkillCameraRigParent{ nullptr };
    _float4x4 m_matDetachedSkillCameraRigLocal{};
    _float m_fBossCameraArmBias{ 0.f };
    _float m_fBossCameraArmHoldTimer{ 0.f };
    _float m_fBossCameraArmHoldBias{ 0.f };
    _float3 m_vBossCameraAdditionalOffset{ 0.f, 0.f, 0.f };

    WComponent<CTransform>* m_pVehicle{ nullptr };
    _bool m_bOnVehicle{ false };
    _bool m_bVehiclePrevCCTActive{ true };
    _bool m_bVehiclePrevCCTUseGravity{ true };
    COLLIDER_TAG m_eVehiclePrevCCTTag{ COLLIDER_TAG::PLAYER };
    _uint m_iVehiclePrevCCTFilter{ 0 };
    WObject<CObject>* m_pParaglider{ nullptr };

    /* Role DESC */
    ROLE_DESC m_tRoleDesc{};
    E_PLAY_MODE m_ePlayMode{ E_PLAY_MODE::COMMON };
    _float m_fHealFontValue = { 0.f };

    /* RootMotion Limit */
    _bool m_fRootMotionLimit = { true };
    _float m_fRootMotionLimitDistance = { 0.f }; // 캐릭터별 조정하기
    _float m_fDefaultRootMotionLimitDistance = { 1.5f };

    /* Switching */
    _bool m_bSwitchingIn = { false };
    _bool m_bSwitchingOut = { false };
    _bool m_bDitheringStarted = { false };
    _float2 m_vDitheringTimer = { 0.f, 1.f };
    _float m_fSwitchInTargetRadius = { 2.5f };
    _bool m_bSwitchOutComboChain = { false };

    _float m_fAdditionalCameraOffsetRatio = { 1.f };

    /* Effect */
    vector<_int> m_vecForcedReturnEffect{};

protected:
    CObjectFilter* m_pObjectFilter{ nullptr };
    CTrigger* m_pTrigger{ nullptr };

    CRigidbody* m_pPhysicsRigid = nullptr;
    CRigidbody* m_pAtckRigid = nullptr;
    CRigidbody* m_pHitRigid = nullptr;

    _bool m_bIsStateEnded{ false };

    /* Input */
    _bool m_bHasInput = { false };
    _float m_iInput_X = { 0 };
    _float m_iInput_Z = { 0 };
    
    queue<E_INPUT_EVENT> m_InputQueue;
    _float m_fInputTimer = { 0.f };
    _float m_fReserveRatio = { 0.2f };
    _float m_fEndRatio = { 0.85f };

    _float m_fLMousePressingTime = 0.f;
    _bool m_bLMousePressing = false;

    /* Run */
    _float m_fBaseRunSpeed = { 4.f };
    _float m_fSpeedFactor = { -1.f };

    /* Attack01 ~ 04 */
    _int m_iComboIndex = { 0 };
    _bool m_bComboInput = { false };
    _float m_fAttackInputBuffer = { 0.f };
    _bool m_bAttackQueued = { false };

    /* Move */
    _float3 m_vMoveDir = { 0.f, 0.f, 0.f };
    _float m_fMoveSpeed = { 0.f };
    _float m_fMoveCoolDown = { 0.2f };
    _float m_fMoveTimer = { 0.f };
    _bool m_bMoveLocked = { false };
    _bool m_bWasLocomotionMoving = { false };
    
    /* Move (I-frames)*/
    _bool m_bIFrames = { false };
    _float2 m_fIFramesTimer = { 0.f, 0.75f };

    _float2 m_vMoveLimitTimer = { 0.f, 0.75f };

    /* ATTACK_L */
    _float m_fAttackLEnableTime = { 0.15f };
    _float m_fAttackLMaxTime = { 0.25f };

    _bool m_bAttackLMove = { false };
    _float m_fAttackLMoveTime = { 0.f };
    _float m_fAttackLMoveDuration = { 0.2f };

    /* Skill */
    _int m_iSkillInputCount = { 0 };
    _float m_fSkillInputTimer = { 0.f };
    _float m_fSkillInputMaxTimer = { 0.3f };
    _float m_fResonanceEnergy = { 0.f };

    _bool m_bIsMainRole{ true }; // 현재 플레이 가능한 캐릭터인지

    set<class CMonster*> m_setDetactMonster{};
    WObject<class CMonster>* m_pTargetMonster{ nullptr };

    SIDEVIEW_MODE_DESC m_tSideViewModeDesc{};

    WObject<class CGameObject>*         m_pInteractionObj{ nullptr };
    WObject<class CGameObject>*         m_pGrappleObj{ nullptr };
    WObject<class CWeapon>*             m_pWeapon{ nullptr };
    WObject<class CWeapon>*             m_pScabbard{ nullptr };
    WObject<class CObject>*             m_pSeqCam{ nullptr };
public:
    WComponent<class CVirtualCamera>*   m_pBurstVC{ nullptr };

    _float3 m_vGrappleDir{};
    _float3 m_vGaoriHookPrevAnchor{};
    _float3 m_vSlideVelocity{};
    _float3 m_vSlideNormal{};
    _float m_fGaoriHookRopeLength = { 0.f };
    _bool m_bGrappling = { false };
    _bool m_bInputLock = { false };

    /* For.DELTA Test */
    E_EASING m_eEnterEasing = { E_EASING::SMOOTHSTEP };
    _float m_fDeltaEnterTime = { 0.25f };
    _float m_fDeltaDuration = { 0.25f };
    _float m_fDeltaTarget = { 0.1f };
    _float m_fDeltaLeaveTime = { 0.25f };
    E_EASING m_eLeaveEasing = { E_EASING::SMOOTHSTEP };

    /* Camera (Execute) */
    _float m_fExecuteCameraTimer = { 0.f };
    _bool m_bExecuteCameraLookMonster = { false };
    _bool m_bExecuteCameraReturnPlayer = { false };

    _float m_fWeaponVisualizeDurtaion{ 2.f };

    /* Debug */
    _bool m_bDebug_Invincible = { false };

public:
    virtual void OnGui_Object_Inspector_Context() override;

    virtual Json::Value Serialize() override;
    virtual void Deserialize(Json::Value& _jsonValue) override;

protected:
    virtual void Free() override;

//Shader Camera Test
protected:
    virtual _bool Can_Process_Input() const { return true; }
};

END
