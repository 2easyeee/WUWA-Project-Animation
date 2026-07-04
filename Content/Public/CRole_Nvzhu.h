
#pragma once

#include "Content_Define.h"
#include "CRole_Base.h"

BEGIN(Content)

class CONTENT_DLL CRole_Nvzhu final : public CRole_Base
{
    INSTANTIABLE(OBJECT)

private:
    explicit CRole_Nvzhu(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_Nvzhu() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

private:
    virtual void Attack_Input(_float _fTimeDelta) override;
    virtual void Consume_Input() override;

    virtual void Init_Role_Desc() override;

    virtual void Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate) override;
    virtual void Init_Change_State_Attack_L() override;
    virtual void Init_Change_State_Skill() override;

    virtual void Init_Attack01_State(_float _fEndTime) override;
    virtual void Init_Attack02_State(_float _fEndTime) override;
    virtual void Init_Attack03_State(_float _fEndTime) override;
    virtual void Init_Attack04_State(_float _fEndTime) override;

    virtual void Init_Skill01_State(_float _fEndTime) override;
    virtual void Init_Skill02_State(_float _fEndTime) override;

    virtual void Init_Burst01_State(_float _fEndTime) override;

    virtual void Init_Switch_QTE_State(_float _fEndTime) override;

private:
    virtual void Free() override;
};

END
