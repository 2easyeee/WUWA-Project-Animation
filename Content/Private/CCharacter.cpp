#include "ContentPCH.h"
#include "CCharacter.h"
#include "CTransform.h"
#include "CRenderer.h"
#include <CSkinnedMeshRenderer.h>
#include <CQuestComponent.h>
#include <CGameManager.h>

namespace {
	constexpr _float CHARACTER_BASE_EPSILON = 1e-6f;

	_vector NormalizeOr(_fvector _vValue, _fvector _vFallback) {
		if (XMVectorGetX(XMVector3LengthSq(_vValue)) <= CHARACTER_BASE_EPSILON)
			return XMVector3Normalize(_vFallback);

		return XMVector3Normalize(_vValue);
	}

	_vector GetTransformBaseUp(CTransform* _pTransform) {
		if (_pTransform == nullptr)
			return XMVectorSet(0.f, 1.f, 0.f, 0.f);

		return NormalizeOr(XMLoadFloat3(&_pTransform->Get_BaseUp()), XMVectorSet(0.f, 1.f, 0.f, 0.f));
	}

	_vector RemoveAxisComponent(_fvector _vValue, _fvector _vAxis) {
		return _vValue - _vAxis * XMVector3Dot(_vValue, _vAxis);
	}

	_vector ProjectOnBasePlane(_fvector _vValue, _fvector _vBaseUp, _fvector _vFallback) {
		_vector vProjected = RemoveAxisComponent(_vValue, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > CHARACTER_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(_vFallback, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > CHARACTER_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(XMVectorSet(0.f, 0.f, 1.f, 0.f), _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > CHARACTER_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		return XMVector3Normalize(RemoveAxisComponent(XMVectorSet(1.f, 0.f, 0.f, 0.f), _vBaseUp));
	}
}
CCharacter::CCharacter(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CActor(_pDevice, _pContext) {
}

HRESULT CCharacter::Awake() {
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CCharacter::Awake");
		return E_FAIL;
	}

	m_pHitCase = WObject<CObject>::Create(nullptr);
	m_pMiddleCase = WObject<CObject>::Create(nullptr);

	return S_OK;
}

void CCharacter::Start() {
	__super::Start();

	Register_Meta_Bone(); // 용도가 있는 본들을 따로 캐싱해두는 함수

}

void CCharacter::Update(_float _fTimeDelta) {

}

void CCharacter::Late_Update(_float _fTimeDelta) {
	if (m_fAutoTurnDuration > 0.f) {
		Turn(_fTimeDelta);

		m_fAutoTurnDuration = max(m_fAutoTurnDuration - _fTimeDelta, 0.f);
	}
}

void CCharacter::Set_Disslove(const _float& _fRate) {
	for (auto pRendererWrapper : m_listRenderer)
	{
		if (auto pRenderer = pRendererWrapper->Get()) {
			auto pShaderParm = pRenderer->Get_ShaderParam("g_fDissolveAmount");
			if (pShaderParm != nullptr)
			{
				pShaderParm->Set(_fRate);
			}
		}
	}
}

void CCharacter::Set_Dithering(const _float& _fRate) {
	for (auto pRendererWrapper : m_listRenderer)
	{
		if (auto pRenderer = pRendererWrapper->Get()) {
			auto pShaderParm = pRenderer->Get_ShaderParam("g_fDissolveAmount");
			if (pShaderParm != nullptr)
			{
				pShaderParm->Set(_fRate);
			}
		}
	}
}

void CCharacter::Set_Default_Renderer_Active(const _int& _iActive) {
	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->SetActive(_iActive);
		}
	}
}

void CCharacter::Register_Meta_Bone() {
	if (!m_pTransform || !m_pHitCase)
		return;

	m_pHitCase->Set(nullptr);

	queue<CTransform*> que{};
	que.push(m_pTransform);

	CTransform* pHitCase = nullptr;

	while (!que.empty()) {
		auto* pCur = que.front();
		que.pop();

		if (!pCur)
			continue;

		auto* pOwner = pCur->GetOwner();
		if (!pOwner)
			continue;

		if (pOwner->Get_Layer() == OBJECT_LAYER::HIT_CASE) {
			pHitCase = pCur;
			break;
		}

		if (pOwner->Get_Layer() == OBJECT_LAYER::MIDDLE_CASE) {
			m_pMiddleCase->Set(pOwner);
			break;
		}

		for (auto pChild : pCur->GetChildList()) {
			if (pChild)
				que.push(pChild);
		}
	}

	if (pHitCase == nullptr)
		return;

	m_pHitCase->Set(pHitCase->GetOwner());
}

void CCharacter::Turn(_float _fTimeDelta) {
	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vDirection = ProjectOnBasePlane(m_pTransform->Get_World(STATE::LOOK), vBaseUp, XMVectorSet(0.f, 0.f, 1.f, 0.f));
	_vector vTarget = ProjectOnBasePlane(XMLoadFloat3(&m_vTargetDirection), vBaseUp, vDirection);

	_vector vRightAxis = XMVector3Cross(vDirection, vTarget);
	_float fY = XMVectorGetX(XMVector3Dot(vRightAxis, vBaseUp));
	_float fX = XMVectorGetX(XMVector3Dot(vDirection, vTarget));

	_float fRadianDelta = atan2f(fY, fX);

	m_pTransform->Rotate(vBaseUp, XMConvertToDegrees(fRadianDelta) * _fTimeDelta * m_fTurnSpeed);
}

void CCharacter::Auto_Turn(const _float3& _vTargetDirection, _float _fTurnSpeed, _float _fAutoTurnDuration) {

	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vTargetDirection = ProjectOnBasePlane(XMLoadFloat3(&_vTargetDirection), vBaseUp, m_pTransform->Get_World(STATE::LOOK));
	XMStoreFloat3(&m_vTargetDirection, vTargetDirection);

	m_fTurnSpeed = _fTurnSpeed;
	m_fAutoTurnDuration = _fAutoTurnDuration;
}


void CCharacter::OnQuestNotify(QUEST_EVENT_TYPE eventType, _uint id, _uint count) {
	CGameManager::GetInstance()->GetQuest()->OnNotify(eventType, id, count);
	
	//if (auto* pQuest = Content::CGameManager::GetInstance()->Get_Role(0)->CreateComponent<CQuestComponent>())
	//	pQuest->OnNotify(eventType, id, count);
}

void CCharacter::Set_State(_ullong _iState) {
	m_iState = _iState;
}

void CCharacter::Add_State(_ullong _iState) {
	m_iState |= _iState;
}

void CCharacter::Remove_State(_ullong _iState) {
	if (m_iState & _iState) {
		m_iState ^= _iState;
	}
}

void CCharacter::Free() {
	__super::Free();

	Safe_Release(m_pHitCase);
	Safe_Release(m_pMiddleCase);
}
