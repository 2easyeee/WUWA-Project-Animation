#pragma once

#include "Content_Define.h"
#include "CRole_AimisiGD.h"
#include <CObjectFilter.h>
#include <CPrefabFilter.h>

BEGIN(Content)

class CONTENT_DLL CRole_Aimisi final : public CRole_AimisiGD
{
    INSTANTIABLE(OBJECT)
    friend class CRole_GD;
private:
    explicit CRole_Aimisi(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_Aimisi() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

private:
    virtual void Init_AttackRootMotionLimit() override;

    virtual void Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate) override;

    virtual void Init_Attack01_State(_float _fEndTime) override;
    virtual void Init_Attack02_State(_float _fEndTime) override;
    virtual void Init_Attack03_State(_float _fEndTime) override;
    virtual void Init_Attack04_State(_float _fEndTime) override;

    virtual void Init_Attack_L_Loop_State(_float _fEndTime) {}
    virtual void Init_Attack_L_End_State(_float _fEndTime) {}

    virtual void Init_Skill01_State(_float _fEndTime) override;
    virtual void Init_Skill02_State(_float _fEndTime) override;
    virtual void Init_Skill03_State(_float _fEndTime) override;
    virtual void Init_Skill04_State(_float _fEndTime) override;

    virtual void Init_Attack_Air_End_State(_float _fEndTime);

    virtual void Init_Burst01_State(_float _fEndTime) override;

    virtual void Init_Switch_QTE_State(_float _fEndTime) override;

public:
    virtual void Switch_In(CRole_Base* _pPrevRole) override;
    virtual void Switch_Out() override;
    virtual void Prepare_Switch_Out() override;

private:
    CPrefabFilter* m_pPrefabFilter{ nullptr };

private:
    virtual void Free() override;
};

END
