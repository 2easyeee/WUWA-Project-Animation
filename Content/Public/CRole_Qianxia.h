
#pragma once

#include "Content_Define.h"
#include "CRole_Base.h"

BEGIN(Content)

class CONTENT_DLL CRole_Qianxia final : public CRole_Base
{
    INSTANTIABLE(OBJECT)

private:
    explicit CRole_Qianxia(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CRole_Qianxia() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    virtual EXECUTE_CAMERA_DESC Get_ExecuteCameraDesc() const override;
    virtual _int Get_Switch_Out_ComboMaxCount() const override { return 5; }

private:
    virtual void Init_Role_Desc() override;
    virtual void Init_Forced_Return_EffectPool() override;

    virtual void Attack_Input(_float _fTimeDelta) override;
    virtual void Consume_Input() override;

    virtual void Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate) override;
    virtual void Init_Change_State_Attack_L() override;

    virtual void Init_Change_State_Skill() override;

    virtual void Init_Attack01_State(_float _fEndTime) override;
    virtual void Init_Attack02_State(_float _fEndTime) override;
    virtual void Init_Attack03_State(_float _fEndTime) override;
    virtual void Init_Attack04_State(_float _fEndTime) override;
    virtual void Init_Attack05_State(_float _fEndTime) override;

    virtual void Init_Attack_L_Loop_State(_float _fEndTime) override;
    virtual void Init_Attack_L_End_State(_float _fEndTime) override;

    virtual void Init_Skill02_State(_float _fEndTime) override;
    virtual void Init_Skill03_State(_float _fEndTime) override;
    virtual void Init_Skill04_State(_float _fEndTime) override;
    virtual void Init_Skill05_State(_float _fEndTime) override;
    virtual void Init_Skill06_State(_float _fEndTime) override;

    virtual void Init_Execute_State() override;

    virtual void Init_Burst01_State(_float _fEndTime) override;

    virtual void Init_Switch_QTE_State(_float _fEndTime) override;

private:
    virtual void Free() override;
};

END
