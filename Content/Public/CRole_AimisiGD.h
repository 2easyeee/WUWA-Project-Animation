#pragma once

#include "Content_Define.h"
#include "CRole_Base.h"

BEGIN(Content)

class CONTENT_DLL CRole_AimisiGD : public CRole_Base
{
    INSTANTIABLE(OBJECT)

public:
    enum class E_AIMISI_TYPE { AIMISI, GUNDAM };

protected:
    explicit CRole_AimisiGD(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_AimisiGD() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    virtual CRole_Base* Get_CurrentPlayRole() override {
        return m_pActiveType ? m_pActiveType : this;    // Get_CurrentRole에서 Amisi_GD 특수화를 위한 가상함수(this는 Aimisi 반환)
    }

    const E_AIMISI_TYPE& Get_E_AIMISI_TYPE() const {
        return m_eActiveType;
    }

protected:
    /* WObject */
    WObject<CObject>* m_wAimisiObj = { nullptr };
    WObject<CObject>* m_wGDObj = { nullptr };

    /* Cache */
    CRole_AimisiGD* m_pAimisi = { nullptr };
    CRole_AimisiGD* m_pGD = { nullptr };

    class CRole_AimisiGD* m_pActiveType = { nullptr };
    E_AIMISI_TYPE m_eActiveType = E_AIMISI_TYPE::AIMISI;

protected:
    void Switch_Type(E_AIMISI_TYPE _eType);
    void Switch_Type_ForSwitchOut();

    REGISTER_FUNCTION(CRole_AimisiGD, Notify_Switch_Start, _int)
    //REGISTER_FUNCTION(CRole_AimisiGD, Skill_Cut_Scene_Start, monostate);
    REGISTER_FUNCTION(CRole_AimisiGD, Skill_Cut_Scene_End, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Notify_Switch_SyncPosition, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Notify_Switch_SyncRotation, monostate)

    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Notify_Switch_Start, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Notify_Switch_SyncPosition, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Set_Aimisi_RootMotion, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Set_SkillCameraRig_LayerDelta, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Set_Skill_Camera_Desc, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Push_Skill_Camera, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Pop_Skill_Camera, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Pop_Skill_Camera_Smooth, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Push_Camera, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Disable_State, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, Aimisi_Skill_Cut_Scene_End, monostate);

    REGISTER_FUNCTION(CRole_AimisiGD, GD_Notify_Switch_Start, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Notify_Switch_SyncPosition, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Set_GD_RootMotion, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Set_GD_RootMotionMultiplier, _float2)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Set_SkillCameraRig_LayerDelta, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Set_Skill_Camera_Desc, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Push_Skill_Camera, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Pop_Skill_Camera, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Pop_Skill_Camera_Smooth, _float4)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Push_Camera, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Disable_State, monostate)
    REGISTER_FUNCTION(CRole_AimisiGD, GD_Skill_Cut_Scene_End, monostate)

    REGISTER_FUNCTION(CRole_AimisiGD, Sync_AimisiTransformToGD, monostate);
    REGISTER_FUNCTION(CRole_AimisiGD, Sync_GDTransformToAimisi, monostate);
    
    REGISTER_FUNCTION(CRole_AimisiGD, Notify_RootMotion_All_ON, monostate);
    REGISTER_FUNCTION(CRole_AimisiGD, Notify_RootMotion_All_OFF, monostate);
    REGISTER_FUNCTION(CRole_AimisiGD, Set_Switch_Target_RootMotion, _int); //1 RootMotion On / 0 RootMotion Off
    REGISTER_FUNCTION(CRole_AimisiGD, Set_Switch_Source_RootMotion, _int); //1 RootMotion On / 0 RootMotion Off
    REGISTER_FUNCTION(CRole_AimisiGD, Set_Aimsi_RootMotion, _int); //1 RootMotion On / 0 RootMotion Off
    REGISTER_FUNCTION(CRole_AimisiGD, Set_GD_RootMotion, _int); //1 RootMotion On / 0 RootMotion Off

    void Fix_GD_Skill(const _int& _iPivotIndex);

    REGISTER_FUNCTION(CRole_AimisiGD, Set_Fix_GD_Skill, _int)
    REGISTER_FUNCTION(CRole_AimisiGD, Set_GD_RootMotionMuteWeight, _float)
    REGISTER_FUNCTION(CRole_AimisiGD, Set_GD_RootMotionMultiplier, _float2)

protected:
    virtual void Init_Role_Desc() override;
    void Register_Type();

    virtual void Update_Role_Desc(_float _fTimeDelta) override;
    virtual void Reset_ControlCameraSettings() override;

    virtual void Init_Change_State_Skill() override;
    virtual _float Get_SwitchDitheringTime() override;

private:
    CRole_AimisiGD* Get_Role_Object(E_AIMISI_TYPE _eType) const;
	E_AIMISI_TYPE Get_Opposite_Type(E_AIMISI_TYPE _eType) const;
	_bool Try_Get_ActiveAndInactiveRoles(CRole_AimisiGD*& _pActiveRole, CRole_AimisiGD*& _pInactiveRole) const;
	void Reset_SkillCameraDesc(CRole_AimisiGD* _pRole);
	void Push_TargetCharacterCamera(CRole_AimisiGD* _pRole, const monostate& _rValue);
	void Handoff_InternalCharacterCamera(CRole_AimisiGD* _pSourceRole, CRole_AimisiGD* _pTargetRole);

protected:
	void Begin_SkillCutsceneMonsterFreeze();
	void End_SkillCutsceneMonsterFreeze();
	void Set_MonsterBossLayerDelta(_float _fTargetDelta);

	_int m_iSkillCutsceneMonsterFreezeRef{ 0 };

public:
    virtual const ROLE_DESC& Get_Desc() const override {
        return (!m_pAimisi || this == m_pAimisi) ? m_tRoleDesc : m_pAimisi->Get_Desc();
    }

protected:
    virtual ROLE_DESC& Get_Desc() override {
        return (!m_pAimisi || this == m_pAimisi) ? m_tRoleDesc : m_pAimisi->Get_Desc();
    }

public:
    virtual void OnGui_Object_Inspector_Context() override;
    virtual Json::Value Serialize() override;
    virtual void Deserialize(Json::Value& _jsonValue) override;

protected:
    virtual void Free() override;

//Shader Caemra Test
private:
    virtual _bool Can_Process_Input() const override;
};

END
