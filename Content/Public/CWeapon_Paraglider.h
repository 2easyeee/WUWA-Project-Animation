
#pragma once

#include "Content_Define.h"
#include "CWeapon.h"

BEGIN(Content)

class CONTENT_DLL CWeapon_Paraglider final : public CWeapon
{
    INSTANTIABLE(OBJECT)

private:
    explicit CWeapon_Paraglider(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CWeapon_Paraglider() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    REGISTER_FUNCTION(CWeapon_Paraglider, Play_WeaponAnim, _int);
    REGISTER_FUNCTION(CWeapon_Paraglider, Pause_WeaponAnim, monostate);
    REGISTER_FUNCTION(CWeapon_Paraglider, Reset_WeaponAnim, monostate)

private:
    _bool m_bEnd = { false };

private:
    virtual void Free() override;
};

END
