
#pragma once

#include "Content_Define.h"
#include "CWeapon.h"

BEGIN(Content)

class CONTENT_DLL CWeapon_Levi_Yuegong final : public CWeapon
{
    INSTANTIABLE(OBJECT)

private:
    explicit CWeapon_Levi_Yuegong(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CWeapon_Levi_Yuegong() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    virtual void Notify_Weapon(const _float4& _vParam) override;

public:
    REGISTER_FUNCTION(CWeapon_Levi_Yuegong, Play_WeaponAnim, _int);
    REGISTER_FUNCTION(CWeapon_Levi_Yuegong, Pause_WeaponAnim, monostate);
    REGISTER_FUNCTION(CWeapon_Levi_Yuegong, Reset_WeaponAnim, monostate)
    REGISTER_FUNCTION(CWeapon_Levi_Yuegong, Attach_ToObjectFilterBone, _int);



private:
    virtual void Free() override;
};

END
