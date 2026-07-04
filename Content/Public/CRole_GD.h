#pragma once

#include "Content_Define.h"
#include "CRole_AimisiGD.h"
#include <CObjectFilter.h>

BEGIN(Content)

class CONTENT_DLL CRole_GD final : public CRole_AimisiGD
{
    INSTANTIABLE(OBJECT)
    friend class CRole_Aimisi;

private:
    explicit CRole_GD(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_GD() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

private:
    virtual void Init_Push_Camera() override;

private:
    void Update_Ascend(_float _fTimeDelta);
    void Update_Hover(_float _fTimeDelta);
    void Update_Descend(_float _fTimeDelta);

private:
    virtual void Reset_CameraSettings() override;

    virtual void Init_AttackRootMotionLimit() override;

    virtual void Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate);

    virtual void Init_Change_State_Jump() override;
    virtual void Init_Change_State_Fly() override;

    virtual void Init_Run_State() override;
    virtual void Init_Sprint_State() override;
    virtual void Init_Move_State() override;

    virtual void Init_Execute_State() override;

    virtual void Init_Jump_Start_State() override;
    virtual void Init_Jump_Loop_State() override;
    virtual void Init_Jump_Land_State() override;
    virtual void Init_Fly_Start_State() override;
    virtual void Init_Fly_Loop_State() override;

    virtual void Init_Attack01_State(_float _fEndTime) override;
    virtual void Init_Attack02_State(_float _fEndTime) override;
    virtual void Init_Attack03_State(_float _fEndTime) override;
    virtual void Init_Attack04_State(_float _fEndTime) override;

    virtual void Init_Skill03_State(_float _fEndTime) override;
    virtual void Init_Skill04_State(_float _fEndTime) override;

    virtual void Init_Burst01_State(_float _fEndTime) override;

    virtual void Init_Switch_QTE_State(_float _fEndTime) override;

    virtual void Init_Attack_Air_Start_State(_float _fEndTime) override;
    virtual void Init_Attack_Air_End_State(_float _fEndTime) override;

private:
    void Recover_HoverTime(_float _fTimeDelta);

private:
    _float m_fHoverEnableTime = { 100.f };
    _float m_fHoverUseTime = { 20.f };
    _float m_fHoverHeight = { 0.f };
    _bool m_bHovering = { false };

public:
    virtual void Switch_In(CRole_Base* _pPrevRole) override;
    virtual void Switch_Out() override;

private:
    virtual void Free() override;
};

END
