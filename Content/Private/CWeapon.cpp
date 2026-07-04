#include "ContentPCH.h"
#include "CWeapon.h"

namespace
{
	constexpr _float WeaponDissolveTime{ 0.5f };

	void Set_Renderer_Active(const list<WComponent<CMeshRenderer>*>& _listRenderer, _bool _bActive)
	{
		for (auto pRendererWrapper : _listRenderer) {
			if (auto pRenderer = pRendererWrapper->Get()) {
				pRenderer->SetActive(_bActive);
			}
		}
	}

	void Set_DissolveAmount(const list<WComponent<CMeshRenderer>*>& _listRenderer, _float _fAmount)
	{
		for (auto pRendererWrapper : _listRenderer) {
			if (auto pRenderer = pRendererWrapper->Get()) {
				auto pShaderParm = pRenderer->Get_ShaderParam("g_fDissolveAmount");
				if (pShaderParm != nullptr) {
					pShaderParm->Set(_fAmount);
				}
			}
		}
	}
}

CWeapon::CWeapon(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CGameObject(_pDevice, _pContext) {
}

HRESULT CWeapon::Awake()
{
	if (FAILED(__super::Awake())) {
		return E_FAIL;
	}

	m_pAnimationController = GetComponent<CAnimationController>();
	m_pObjectFilter = GetComponent<CObjectFilter>();

	return S_OK;
}

void CWeapon::Start() {
	auto listSkinnedMeshRenderer = GetComponents<CSkinnedMeshRenderer>();
	auto listMeshRenderer = GetComponents<CMeshRenderer>();

	for (auto pSkinnedMeshRenderer : listSkinnedMeshRenderer) {
		auto pWrapper = WComponent<CMeshRenderer>::Create(pSkinnedMeshRenderer);

		m_listRenderer.push_back(pWrapper);
	}

	for (auto pMeshRenderer : listMeshRenderer) {
		auto pWrapper = WComponent<CMeshRenderer>::Create(pMeshRenderer);

		m_listRenderer.push_back(pWrapper);
	}

	Set_DissolveAmount(m_listRenderer, m_bVisible ? 0.f : 1.f);
	Set_Renderer_Active(m_listRenderer, m_bVisible || m_fRatio > 0.f);
}

void CWeapon::Update(_float _fTimeDelta)
{
}

void CWeapon::Late_Update(_float _fTimeDelta) {
	if (m_bVisible) {
		Set_Renderer_Active(m_listRenderer, true);
	}

	m_fRatio = clamp(m_fRatio + (m_bVisible ? _fTimeDelta / WeaponDissolveTime : -_fTimeDelta / WeaponDissolveTime), 0.f, 1.f);

	_float fRate = WEasing::SmoothStep(m_fRatio);
	Set_DissolveAmount(m_listRenderer, 1.f - fRate);

	if (!m_bVisible && m_fRatio <= 0.f) {
		Set_Renderer_Active(m_listRenderer, false);
	}
}

void CWeapon::Notify_Weapon(const _float4& _vValue) {
	m_bVisible = static_cast<_bool>(_vValue.x);

	if (m_bVisible) {
		Set_Renderer_Active(m_listRenderer, true);
	}

	if (_vValue.y == 1.f) {
		if (m_bVisible) {
			m_fRatio = 1.f;
			Set_DissolveAmount(m_listRenderer, 0.f);
		}
		else {
			m_fRatio = 0.f;
			Set_DissolveAmount(m_listRenderer, 1.f);
			Set_Renderer_Active(m_listRenderer, false);
		}
	}
}

void CWeapon::Notify_WeaponEvent(const _float4& _vValue) {
	Notify_Weapon(_vValue);
}

void CWeapon::OnGui_Inspector_Context() {
	__super::OnGui_Inspector_Context();

	_bool bVisible = m_bVisible;
	if (ImGui::Checkbox("Visible", &bVisible)) {
		Notify_Weapon(_float4{ bVisible ? 1.f : 0.f, 1.f, 0.f, 0.f });
	}
}

void CWeapon::Free()
{
	__super::Free();

	for (auto pWrapper : m_listRenderer) {
		Safe_Release(pWrapper);
	}
	m_listRenderer.clear();
}
