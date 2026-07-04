#include "ContentPCH.h"
#include "CWeapon_Paraglider.h"
#include "CAnimationStateMachine.h"

namespace
{
	constexpr _ullong hHasInput = Get_HashCode("HasInput");

    constexpr const _char* PropAnimNames[] =
    {
		"Start",    // 0
		"Loop",     // 1
		"Dash",     // 2  // End 상태에서 쓰는 애니메이션
    };

    constexpr _int PropAnimCount = static_cast<_int>(sizeof(PropAnimNames) / sizeof(PropAnimNames[0]));
	
	enum PARAGLIDER_STATE
	{
		PARAGLIDER_START,
		PARAGLIDER_LOOP,
		PARAGLIDER_END,
	};
}

CWeapon_Paraglider::CWeapon_Paraglider(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
    : CWeapon(_pDevice, _pContext)
{
}

HRESULT CWeapon_Paraglider::Awake()
{
    if (FAILED(__super::Awake())) {
        MSG_BOX("Failed To CWeapon_Paraglider::Awake");
        return E_FAIL;
    }

    return S_OK;
}

void CWeapon_Paraglider::Start()
{
    __super::Start();

	if (m_pAnimationController)
	{
		m_pAnimationController->Pause();
		m_pAnimationController->SetActive(false);
	}

	if (auto pFSM = this->GetComponent<CAnimationStateMachine>())
	{
		pFSM->SetActive(false);
		pFSM->Set_Bool(hHasInput, false);
		pFSM->Reset_Runtime();
	}
}

void CWeapon_Paraglider::Update(_float _fTimeDelta)
{
    __super::Update(_fTimeDelta);

	if (!m_bEnd)
		return;

	auto* pFSM = GetComponent<CAnimationStateMachine>();
	if (!pFSM)
		return;

	auto* pNode = pFSM->Get_CurrentNode();
	if (!pNode)
		return;

	if (pNode->Get_Name() == L"End" && pFSM->Get_CurrentNormalizedTime() >= 1.f)
	{
		m_bEnd = false;
		Notify_Weapon({ 0.f, 1.f, 0.f, 0.f });
		SetActive(false);
	}
}

void CWeapon_Paraglider::Late_Update(_float _fTimeDelta)
{
    __super::Late_Update(_fTimeDelta);
}

void CWeapon_Paraglider::Play_WeaponAnim(const _int& _iAnimIndex)
{
	if (_iAnimIndex < 0 || _iAnimIndex >= PropAnimCount)
		return;

	if (auto pFSM = this->GetComponent<CAnimationStateMachine>())
	{
		SetActive(true);

		pFSM->SetActive(true);

		if (m_pAnimationController)
			m_pAnimationController->SetActive(false);

		if (_iAnimIndex == PARAGLIDER_START)
		{
			m_bEnd = false;

			SetActive(true);
			
			Notify_Weapon({ 1.f, 1.f, 0.f, 0.f });
			
			if (m_pAnimationController)
			{
				m_pAnimationController->Pause();
				m_pAnimationController->SetActive(false);
			}

			pFSM->SetActive(false);
			pFSM->Set_Bool(hHasInput, false);
			pFSM->Reset_Runtime();

			pFSM->SetActive(true);
			pFSM->Set_Bool(hHasInput, true);
			return;
		}

		if (_iAnimIndex == PARAGLIDER_LOOP)
		{
			m_bEnd = false;

			SetActive(true);
			Notify_Weapon({ 1.f, 1.f, 0.f, 0.f });

			pFSM->SetActive(true);
			pFSM->Set_Bool(hHasInput, true);
			return;
		}

		if (_iAnimIndex == PARAGLIDER_END)
		{
			m_bEnd = true;
			Notify_Weapon({ 0.f, 1.f, 0.f, 0.f });
			pFSM->Set_Bool(hHasInput, false);
			return;
		}
	}

	if (!m_pAnimationController)
		return;

	SetActive(true);

	m_bEnd = false;
	m_pAnimationController->SetActive(true);
	m_pAnimationController->Play(PropAnimNames[_iAnimIndex], false);
}

void CWeapon_Paraglider::Pause_WeaponAnim(const monostate&)
{
	if (auto pFSM = this->GetComponent<CAnimationStateMachine>())
		pFSM->SetActive(false);

	if (!m_pAnimationController)
		return;
	m_pAnimationController->Pause();
}

void CWeapon_Paraglider::Reset_WeaponAnim(const monostate&)
{
	m_bEnd = false;

	Notify_Weapon({ 0.f, 1.f, 0.f, 0.f });

	if (auto pFSM = this->GetComponent<CAnimationStateMachine>())
	{
		pFSM->Set_Bool(hHasInput, false);
		pFSM->Reset_Runtime();
	}

	if (m_pAnimationController)
	{
		m_pAnimationController->Pause();
		m_pAnimationController->SetActive(false);
	}

	SetActive(false);
}

void CWeapon_Paraglider::Free()
{
	__super::Free();
}
