
#pragma once

#include "Content_Define.h"
#include "CWeapon.h"
#include <CAnimationController.h>
#include <CObjectFilter.h>

BEGIN(Content)

class CONTENT_DLL CWeapon_Aimisi_Huatong final : public CWeapon
{
    INSTANTIABLE(OBJECT)

private:
    explicit CWeapon_Aimisi_Huatong(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CWeapon_Aimisi_Huatong() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    REGISTER_FUNCTION(CWeapon_Aimisi_Huatong, Play_WeaponAnim, _int);
    REGISTER_FUNCTION(CWeapon_Aimisi_Huatong, Reset_WeaponAnim, monostate);

private:
    void Attach_ToWeaponProp(_int _iAnimIndex);

private:
    virtual void Free() override;
};

END
