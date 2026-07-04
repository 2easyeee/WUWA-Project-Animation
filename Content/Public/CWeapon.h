
#pragma once

#include "Content_Define.h"
#include "CGameObject.h"
#include <CSkinnedMeshRenderer.h>
#include <WComponent.h>
#include <CAnimationController.h>
#include <CObjectFilter.h>

BEGIN(Content)

class CONTENT_DLL CWeapon : public CGameObject
{
    INSTANTIABLE(OBJECT)

protected:
    explicit CWeapon(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CWeapon() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:
    virtual void Notify_Weapon(const _float4& _vParam);
    REGISTER_FUNCTION(CWeapon, Notify_WeaponEvent, _float4);

protected:
    class CAnimationController* m_pAnimationController = { nullptr };
    class CObjectFilter* m_pObjectFilter{ nullptr };

protected:
    list<WComponent<CMeshRenderer>*> m_listRenderer{ };
    _float m_fRatio{ 0.f };
    _bool m_bVisible{ false };

public:
    virtual void OnGui_Inspector_Context() override;

protected:
    virtual void Free() override;
};

END
