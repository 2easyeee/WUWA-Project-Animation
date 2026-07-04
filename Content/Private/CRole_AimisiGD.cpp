#include "ContentPCH.h"
#include "CRole_AimisiGD.h"
#include "CGameInstance.h"
#include <CCharacterCamera.h>
#include <CVirtualCamera.h>

namespace {
	constexpr _float AIMISI_GD_BASIS_EPSILON = 1e-6f;

	_vector NormalizeOr(_fvector _vValue, _fvector _vFallback) {
		if (XMVectorGetX(XMVector3LengthSq(_vValue)) <= AIMISI_GD_BASIS_EPSILON)
			return XMVector3Normalize(_vFallback);

		return XMVector3Normalize(_vValue);
	}

	_vector RemoveAxisComponent(_fvector _vValue, _fvector _vAxis) {
		return _vValue - _vAxis * XMVector3Dot(_vValue, _vAxis);
	}

	_vector GetTransformBaseUp(CTransform* _pTransform) {
		if (_pTransform == nullptr)
			return XMVectorSet(0.f, 1.f, 0.f, 0.f);

		return NormalizeOr(XMLoadFloat3(&_pTransform->Get_BaseUp()), XMVectorSet(0.f, 1.f, 0.f, 0.f));
	}

	_vector ProjectOnBasePlane(_fvector _vValue, _fvector _vBaseUp, _fvector _vFallback) {
		_vector vProjected = RemoveAxisComponent(_vValue, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > AIMISI_GD_BASIS_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(_vFallback, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > AIMISI_GD_BASIS_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(XMVectorSet(0.f, 0.f, 1.f, 0.f), _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > AIMISI_GD_BASIS_EPSILON)
			return XMVector3Normalize(vProjected);

		return XMVector3Normalize(RemoveAxisComponent(XMVectorSet(1.f, 0.f, 0.f, 0.f), _vBaseUp));
	}

	void SyncBaseUp(CTransform* _pDstTransform, CTransform* _pSrcTransform) {
		if (_pDstTransform == nullptr || _pSrcTransform == nullptr)
			return;

		_pDstTransform->Set_BaseUp(_pSrcTransform->Get_BaseUp());
	}

	void SyncPositionAndBaseUp(CRole_AimisiGD* _pDstRole, CTransform* _pDstTransform, CTransform* _pSrcTransform) {
		if (_pDstRole == nullptr || _pDstTransform == nullptr || _pSrcTransform == nullptr)
			return;

		SyncBaseUp(_pDstTransform, _pSrcTransform);

		const _float3 vPosition = _pSrcTransform->GetWorldPosition();
		if (auto* pDstCCT = _pDstRole->GetComponent<CCCT>())
			pDstCCT->Set_Position(vPosition);
		else
			_pDstTransform->Set_Position(vPosition);
	}

	_bool TurnOnSharedBase(CTransform* _pDstTransform, CTransform* _pSrcTransform, _fvector _vDirection, _float _fRandomYaw) {
		if (_pDstTransform == nullptr || _pSrcTransform == nullptr)
			return false;

		SyncBaseUp(_pDstTransform, _pSrcTransform);

		_vector vBaseUp = GetTransformBaseUp(_pSrcTransform);
		_vector vDir = RemoveAxisComponent(_vDirection, vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vDir)) <= AIMISI_GD_BASIS_EPSILON)
			return false;

		vDir = XMVector3Normalize(vDir);

		_matrix matRandomYaw = XMMatrixRotationAxis(vBaseUp, _fRandomYaw);
		_vector vTgtLook = ProjectOnBasePlane(XMVector3TransformNormal(vDir, matRandomYaw), vBaseUp, vDir);
		_float3 vScale = _pDstTransform->GetScale();
		_pDstTransform->Turn(_pDstTransform->Get_World(STATE::POSITION) + vTgtLook, vBaseUp);
		_pDstTransform->Set_Scale(vScale);

		return true;
	}
}

CRole_AimisiGD::CRole_AimisiGD(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
    : CRole_Base(_pDevice, _pContext)
{
	m_tRoleDesc.eElement = DAMAGE_FONT_ELEMENT::Fire;
}

HRESULT CRole_AimisiGD::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_AimisiGD::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CRole_AimisiGD::Start()
{
	if (!m_pAimisi || !m_pGD || !m_pActiveType)
		Register_Type();

	__super::Start();
}

void CRole_AimisiGD::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CRole_AimisiGD::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

CRole_AimisiGD* CRole_AimisiGD::Get_Role_Object(E_AIMISI_TYPE _eType) const
{
	switch (_eType)
	{
	case E_AIMISI_TYPE::AIMISI:
		return m_pAimisi;
	case E_AIMISI_TYPE::GUNDAM:
		return m_pGD;
	}

	return nullptr;
}

CRole_AimisiGD::E_AIMISI_TYPE CRole_AimisiGD::Get_Opposite_Type(E_AIMISI_TYPE _eType) const
{
	switch (_eType)
	{
	case E_AIMISI_TYPE::AIMISI:
		return E_AIMISI_TYPE::GUNDAM;
	case E_AIMISI_TYPE::GUNDAM:
		return E_AIMISI_TYPE::AIMISI;
	}

	return E_AIMISI_TYPE::AIMISI;
}

_bool CRole_AimisiGD::Try_Get_ActiveAndInactiveRoles(CRole_AimisiGD*& _pActiveRole, CRole_AimisiGD*& _pInactiveRole) const
{
	if (!m_pAimisi || !m_pGD)
		return false;

	_pActiveRole = Get_Role_Object(m_eActiveType);
	_pInactiveRole = Get_Role_Object(Get_Opposite_Type(m_eActiveType));

	return _pActiveRole != nullptr && _pInactiveRole != nullptr;
}

void CRole_AimisiGD::Reset_SkillCameraDesc(CRole_AimisiGD* _pRole)
{
	if (_pRole == nullptr)
		return;

	_pRole->m_tSkillCameraTransitionDesc = {};
	_pRole->m_tSkillCameraTransitionDesc.eCameraOrder = CAMERA_ORDER::CUTSCENE;
	_pRole->m_tSkillCameraTransitionDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
	_pRole->m_tSkillCameraTransitionDesc.fTransitionTime = 0.15f;
	_pRole->m_tSkillCameraTransitionDesc.fFov = 72.f;
	_pRole->m_tSkillCameraTransitionDesc.iFlag =
		ETOU(CAMERA_BLEND_MASK::POS) |
		ETOU(CAMERA_BLEND_MASK::ROT) |
		ETOU(CAMERA_BLEND_MASK::FOV);
	_pRole->m_tSkillCameraTransitionDesc.bHandoffCorrection = true;
	_pRole->m_ePushedSkillCameraOrder = CAMERA_ORDER::CNT;
	_pRole->m_fSkillCameraFovOverride = -1.f;
}

void CRole_AimisiGD::Reset_ControlCameraSettings()
{
	__super::Reset_ControlCameraSettings();

	auto funcResetRoleCamera = [this](CRole_AimisiGD* _pRole) {
		if (_pRole == nullptr || _pRole == this)
			return;

		_pRole->Force_PopSkillCamera();
		_pRole->Reset_CameraSettings();
		};

	funcResetRoleCamera(m_pAimisi);
	funcResetRoleCamera(m_pGD);
}

void CRole_AimisiGD::Push_TargetCharacterCamera(CRole_AimisiGD* _pRole, const monostate& _rValue)
{
	if (_pRole == nullptr)
		return;

	if (_pRole->m_pCharacterCamera)
		_pRole->m_pCharacterCamera->SetActive(true);

	_pRole->Push_Camera(_rValue);
}

void CRole_AimisiGD::Handoff_InternalCharacterCamera(CRole_AimisiGD* _pSourceRole, CRole_AimisiGD* _pTargetRole)
{
	if (_pTargetRole == nullptr || _pTargetRole->m_pCharacterCamera == nullptr)
		return;

	auto* pTargetCamera = _pTargetRole->m_pCharacterCamera;
	auto* pTargetVirtualCamera = pTargetCamera->Get_VirtualCamera();
	if (pTargetVirtualCamera == nullptr)
		return;

	_float fFov = 72.f;
	if (_pSourceRole && _pSourceRole->m_pCharacterCamera)
	{
		if (auto* pSourceVirtualCamera = _pSourceRole->m_pCharacterCamera->Get_VirtualCamera())
			fFov = pSourceVirtualCamera->Get_CurPose().fFov;

		pTargetCamera->Set_CameraRotation(_pSourceRole->m_pCharacterCamera);
	}

	pTargetCamera->SetActive(true);

	CAMERA_TRANSITION_DESC tCamTransitionDesc{};
	tCamTransitionDesc.eCameraOrder = CAMERA_ORDER::IN_GAME;
	tCamTransitionDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
	tCamTransitionDesc.fFov = fFov;
	tCamTransitionDesc.iFlag =
		ETOU(CAMERA_BLEND_MASK::POS) |
		ETOU(CAMERA_BLEND_MASK::ROT) |
		ETOU(CAMERA_BLEND_MASK::FOV);
	tCamTransitionDesc.pVirtualCamera = pTargetVirtualCamera;
	tCamTransitionDesc.fTransitionTime = 0.1f;

	m_pGameInstance->Push_Camera(tCamTransitionDesc);

	if (_pSourceRole && _pSourceRole != _pTargetRole && _pSourceRole->m_pCharacterCamera)
		_pSourceRole->m_pCharacterCamera->SetActive(false);
}

void CRole_AimisiGD::Set_MonsterBossLayerDelta(_float _fTargetDelta)
{
	OW_DELTA_ANIM_DESC tDesc{};
	tDesc.eEnter = E_EASING::STEP;
	tDesc.fBlendTime = 0.f;
	tDesc.fTargetDelta = max(0.f, _fTargetDelta);
	tDesc.Add_Layer(OBJECT_LAYER::MONSTER);
	tDesc.Add_Layer(OBJECT_LAYER::BOSS);

	m_pGameInstance->Change_DeltaTime_OneWay(tDesc);
}

void CRole_AimisiGD::Begin_SkillCutsceneMonsterFreeze()
{
	CRole_AimisiGD* pOwner = m_pAimisi ? m_pAimisi : this;
	if (pOwner == nullptr)
		return;

	if (pOwner->m_iSkillCutsceneMonsterFreezeRef++ == 0)
		pOwner->Set_MonsterBossLayerDelta(0.f);
}

void CRole_AimisiGD::End_SkillCutsceneMonsterFreeze()
{
	CRole_AimisiGD* pOwner = m_pAimisi ? m_pAimisi : this;
	if (pOwner == nullptr)
		return;

	pOwner->m_iSkillCutsceneMonsterFreezeRef = max(0, pOwner->m_iSkillCutsceneMonsterFreezeRef - 1);
	if (pOwner->m_iSkillCutsceneMonsterFreezeRef == 0)
		pOwner->Set_MonsterBossLayerDelta(1.f);
}

void CRole_AimisiGD::Notify_Switch_Start (const _int& _iSkillIndex)
{
	/* A : Aimisi , G : Gundam */

	if (!m_pAimisi || !m_pGD)
		return;

	_bool bAimisiActive = (m_eActiveType == E_AIMISI_TYPE::AIMISI);

	CRole_AimisiGD* pSourceType = bAimisiActive ? m_pAimisi : m_pGD;
	CRole_AimisiGD* pTargetType = bAimisiActive ? m_pGD : m_pAimisi;
	E_AIMISI_TYPE eNextType = bAimisiActive ? E_AIMISI_TYPE::GUNDAM : E_AIMISI_TYPE::AIMISI;

	_int iSkillIndex = _iSkillIndex;
	if (Get_Desc().Check_Identity_Eneergy())
	{
		iSkillIndex = 2;
	}

	// 현재 내 활성화된 AIMISI/GD 로 스킬 결정 ex. AIMISI
	const _tchar* pTargetSkillName = { nullptr };
	
	switch (iSkillIndex)
	{
	case 0:
	{
		pTargetSkillName = L"Skill_06";
	}
	break;
	case 1:
	{
		// E1
		pTargetSkillName = bAimisiActive ? L"Skill_02" : L"Skill_01";
	}
	break;
	case 2:
	{
		/* 50 공명 */
		pTargetSkillName = bAimisiActive ? L"Skill_04" : L"Skill_03";
	}
	break;
	}

	if (!pTargetType || !pTargetSkillName)
		return;

	auto pTargetStateController = pTargetType->GetComponent<CStateController>();
	if (!pTargetStateController)
		return;
	pTargetStateController->ChangeAnimState(pTargetSkillName, true);

	// 현재 활성화된 AIMISI/GD 의 반대 타입으로 switch ex. GD
	Switch_Type(eNextType);

	if (iSkillIndex != 2)
		Handoff_InternalCharacterCamera(pSourceType, pTargetType);
}

void CRole_AimisiGD::Skill_Cut_Scene_End(const monostate&)
{
	/* RootMotion All ON */
	if (!m_pAimisi || !m_pGD)
		return;

	auto* pAimisiCtrl = m_pAimisi->GetComponent<CAnimationController>();
	auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>();

	/* 1. RootMotion */
	if (pAimisiCtrl)
	{
		pAimisiCtrl->Set_ApplyRootMotion(true);
		pAimisiCtrl->Set_ApplyRootMotionX(true);
		pAimisiCtrl->Set_ApplyRootMotionZ(true);
	}
	if (pGDCtrl)
	{
		pGDCtrl->Set_ApplyRootMotion(true);
		pGDCtrl->Set_ApplyRootMotionX(true);
		pGDCtrl->Set_ApplyRootMotionZ(true);
	}
}

void CRole_AimisiGD::Notify_Switch_SyncPosition(const monostate&)
{
	if (!m_pAimisi || !m_pGD)
		return;

	CRole_AimisiGD* pSrcType = { nullptr };
	CRole_AimisiGD* pTgtType = { nullptr };

	// Notify_Switch_Start 이후라고 가정 (이미 활성화된 타입이 바뀐 상태)
	// Tgt 을 현재 Src 위치에 set
	switch (m_eActiveType)
	{
	case E_AIMISI_TYPE::AIMISI:
	{
		pSrcType = m_pAimisi;
		pTgtType = m_pGD;
	}
		break;
	case E_AIMISI_TYPE::GUNDAM:
	{
		pSrcType = m_pGD;
		pTgtType = m_pAimisi;
	}
		break;
	}

	if (!pSrcType || !pTgtType)
		return;

	auto* pSrcTransform = pSrcType->GetTransform();
	auto* pTgtTransform = pTgtType->GetTransform();
	if (!pSrcTransform || !pTgtTransform)
		return;

	/* 1. Set Position */
	SyncPositionAndBaseUp(pTgtType, pTgtTransform, pSrcTransform);
}

void CRole_AimisiGD::Notify_Switch_SyncRotation(const monostate&)
{
	if (!m_pAimisi || !m_pGD)
		return;

	CRole_AimisiGD* pSrcType = { nullptr };
	CRole_AimisiGD* pTgtType = { nullptr };

	// Notify_Switch_Start 이후라고 가정 (이미 활성화된 타입이 바뀐 상태)
	// Tgt 을 현재 Src 위치에 set
	switch (m_eActiveType)
	{
	case E_AIMISI_TYPE::AIMISI:
	{
		pSrcType = m_pAimisi;
		pTgtType = m_pGD;
	}
	break;
	case E_AIMISI_TYPE::GUNDAM:
	{
		pSrcType = m_pGD;
		pTgtType = m_pAimisi;
	}
	break;
	}

	if (!pSrcType || !pTgtType)
		return;

	auto* pSrcTransform = pSrcType->GetTransform();
	auto* pTgtTransform = pTgtType->GetTransform();
	if (!pSrcTransform || !pTgtTransform)
		return;

	_float fRandomYaw = XMConvertToRadians(m_pGameInstance->Random(-5.f, 5.f));

	_vector vSrcLook = pSrcTransform->Get_World(STATE::LOOK);
	if (!TurnOnSharedBase(pTgtTransform, pSrcTransform, vSrcLook, fRandomYaw)) {
		if (auto* pTgtCCT = pTgtType->GetComponent<CCCT>())
			pTgtCCT->Clear_PlanarVelocity();
	}
}

void CRole_AimisiGD::Aimisi_Notify_Switch_Start(const _int& _iSkillIndex)
{
	if (m_pAimisi)
		m_pAimisi->Notify_Switch_Start(_iSkillIndex);
}

void CRole_AimisiGD::Aimisi_Notify_Switch_SyncPosition(const monostate& _rValue)
{
	if (m_pAimisi)
		m_pAimisi->Notify_Switch_SyncPosition(_rValue);
}

void CRole_AimisiGD::Aimisi_Set_Aimisi_RootMotion(const _int& _iApply)
{
	if (m_pAimisi)
		m_pAimisi->Set_Aimsi_RootMotion(_iApply);
}

void CRole_AimisiGD::Aimisi_Set_SkillCameraRig_LayerDelta(const _float4& _vDesc)
{
	if (m_pAimisi)
		m_pAimisi->Set_SkillCameraRig_LayerDelta(_vDesc);
}

void CRole_AimisiGD::Aimisi_Set_Skill_Camera_Desc(const _float4& _vDesc)
{
	if (m_pAimisi)
		m_pAimisi->Set_Skill_Camera_Desc(_vDesc);
}

void CRole_AimisiGD::Aimisi_Push_Skill_Camera(const monostate& _rValue)
{
	if (m_pAimisi)
		m_pAimisi->Push_Skill_Camera(_rValue);
}

void CRole_AimisiGD::Aimisi_Pop_Skill_Camera(const _int& _iForced)
{
	if (m_pAimisi)
	{
		m_pAimisi->Pop_Skill_Camera(_iForced);
		Reset_SkillCameraDesc(m_pAimisi);
	}
}

void CRole_AimisiGD::Aimisi_Pop_Skill_Camera_Smooth(const _float4& _vDesc)
{
	if (m_pAimisi)
	{
		m_pAimisi->Pop_Skill_Camera_Smooth(_vDesc);
		Reset_SkillCameraDesc(m_pAimisi);
	}
}

void CRole_AimisiGD::Aimisi_Push_Camera(const monostate& _rValue)
{
	Push_TargetCharacterCamera(m_pAimisi, _rValue);
}

void CRole_AimisiGD::Aimisi_Disable_State(const monostate& _rValue)
{
	if (m_pAimisi)
		m_pAimisi->Disable_State(_rValue);
}

void CRole_AimisiGD::Aimisi_Skill_Cut_Scene_End(const monostate& _rValue)
{
	if (m_pAimisi)
		m_pAimisi->Skill_Cut_Scene_End(_rValue);
}

void CRole_AimisiGD::GD_Notify_Switch_Start(const _int& _iSkillIndex)
{
	if (m_pGD)
		m_pGD->Notify_Switch_Start(_iSkillIndex);
}

void CRole_AimisiGD::GD_Notify_Switch_SyncPosition(const monostate& _rValue)
{
	if (m_pGD)
		m_pGD->Notify_Switch_SyncPosition(_rValue);
}

void CRole_AimisiGD::GD_Set_GD_RootMotion(const _int& _iApply)
{
	if (m_pGD)
		m_pGD->Set_GD_RootMotion(_iApply);
}

void CRole_AimisiGD::GD_Set_GD_RootMotionMultiplier(const _float2& _vMultiplier)
{
	if (m_pGD)
		m_pGD->Set_GD_RootMotionMultiplier(_vMultiplier);
}

void CRole_AimisiGD::GD_Set_SkillCameraRig_LayerDelta(const _float4& _vDesc)
{
	if (m_pGD)
		m_pGD->Set_SkillCameraRig_LayerDelta(_vDesc);
}

void CRole_AimisiGD::GD_Set_Skill_Camera_Desc(const _float4& _vDesc)
{
	if (m_pGD)
		m_pGD->Set_Skill_Camera_Desc(_vDesc);
}

void CRole_AimisiGD::GD_Push_Skill_Camera(const monostate& _rValue)
{
	if (m_pGD)
		m_pGD->Push_Skill_Camera(_rValue);
}

void CRole_AimisiGD::GD_Pop_Skill_Camera(const _int& _iForced)
{
	if (m_pGD)
	{
		m_pGD->Pop_Skill_Camera(_iForced);
		Reset_SkillCameraDesc(m_pGD);
	}
}

void CRole_AimisiGD::GD_Pop_Skill_Camera_Smooth(const _float4& _vDesc)
{
	if (m_pGD)
	{
		m_pGD->Pop_Skill_Camera_Smooth(_vDesc);
		Reset_SkillCameraDesc(m_pGD);
	}
}

void CRole_AimisiGD::GD_Push_Camera(const monostate& _rValue)
{
	Push_TargetCharacterCamera(m_pGD, _rValue);
}

void CRole_AimisiGD::GD_Disable_State(const monostate& _rValue)
{
	if (m_pGD)
		m_pGD->Disable_State(_rValue);
}

void CRole_AimisiGD::GD_Skill_Cut_Scene_End(const monostate& _rValue)
{
	if (m_pGD)
		m_pGD->Skill_Cut_Scene_End(_rValue);
}

void CRole_AimisiGD::Sync_AimisiTransformToGD(const monostate&)
{
	if (!m_pAimisi || !m_pGD)
		return;

	auto pAimisiTransform = m_pAimisi->GetTransform();
	auto pGDTransform = m_pGD->GetTransform();
	if (!pAimisiTransform || !pGDTransform)
		return;

	auto* pGDCCT = m_pGD->GetComponent<CCCT>();

	/* 1. Set Position */
	SyncPositionAndBaseUp(m_pGD, pGDTransform, pAimisiTransform);

	/* 2. Rotation */
	_float fRandomYaw = XMConvertToRadians(m_pGameInstance->Random(-5.f, 5.f));
	if (!TurnOnSharedBase(pGDTransform, pAimisiTransform, XMLoadFloat3(&m_pAimisi->m_vTargetDirection), fRandomYaw)) {
		if (pGDCCT)
			pGDCCT->Clear_PlanarVelocity();

		return;
	}
}

void CRole_AimisiGD::Sync_GDTransformToAimisi(const monostate&)
{
	if (!m_pAimisi || !m_pGD)
		return;

	auto pAimisiTransform = m_pAimisi->GetTransform();
	auto pGDTransform = m_pGD->GetTransform();
 	if (!pAimisiTransform || !pGDTransform)
		return;

	auto* pAimisiCCT = m_pAimisi->GetComponent<CCCT>();

	/* 1. Set Position */
	SyncPositionAndBaseUp(m_pAimisi, pAimisiTransform, pGDTransform);

	/* 2. Rotation */
	_float fRandomYaw = XMConvertToRadians(m_pGameInstance->Random(-5.f, 5.f));
	if (!TurnOnSharedBase(pAimisiTransform, pGDTransform, XMLoadFloat3(&m_pGD->m_vTargetDirection), fRandomYaw)) {
		if (pAimisiCCT)
			pAimisiCCT->Clear_PlanarVelocity();

		return;
	}
}

void CRole_AimisiGD::Notify_RootMotion_All_ON(const monostate&)
{
	/* RootMotion All ON */
	if (!m_pAimisi || !m_pGD)
		return;

	auto* pAimisiCtrl = m_pAimisi->GetComponent<CAnimationController>();
	auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>();

	/* RootMotion */
	if (pAimisiCtrl)
	{
		pAimisiCtrl->Set_ApplyRootMotion(true);
		pAimisiCtrl->Set_ApplyRootMotionX(true);
		pAimisiCtrl->Set_ApplyRootMotionZ(true);
	}
	if (pGDCtrl)
	{
		pGDCtrl->Set_ApplyRootMotion(true);
		pGDCtrl->Set_ApplyRootMotionX(true);
		pGDCtrl->Set_ApplyRootMotionZ(true);
	}
}

void CRole_AimisiGD::Notify_RootMotion_All_OFF(const monostate&)
{
	/* RootMotion All ON */
	if (!m_pAimisi || !m_pGD)
		return;

	auto* pAimisiCtrl = m_pAimisi->GetComponent<CAnimationController>();
	auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>();

	/* RootMotion */
	if (pAimisiCtrl)
	{
		pAimisiCtrl->Set_ApplyRootMotion(false);
		pAimisiCtrl->Set_ApplyRootMotionX(false);
		pAimisiCtrl->Set_ApplyRootMotionZ(false);
	}
	if (pGDCtrl)
	{
		pGDCtrl->Set_ApplyRootMotion(false);
		pGDCtrl->Set_ApplyRootMotionX(false);
		pGDCtrl->Set_ApplyRootMotionZ(false);
	}
}

void CRole_AimisiGD::Set_Switch_Target_RootMotion(const _int& _iApply)
{
	CRole_AimisiGD* pCurrentActiveRole = { nullptr };
	CRole_AimisiGD* pCurrentInactiveRole = { nullptr };

	if (!Try_Get_ActiveAndInactiveRoles(pCurrentActiveRole, pCurrentInactiveRole))
		return;

	auto* pSwitchTargetCtrl = pCurrentActiveRole->GetComponent<CAnimationController>();
	if (!pSwitchTargetCtrl)
		return;

	const _bool bApply = (_iApply != 0);
	pSwitchTargetCtrl->Set_ApplyRootMotion(bApply);
	pSwitchTargetCtrl->Set_ApplyRootMotionX(bApply);
	pSwitchTargetCtrl->Set_ApplyRootMotionZ(bApply);
}

void CRole_AimisiGD::Set_Switch_Source_RootMotion(const _int& _iApply)
{
	CRole_AimisiGD* pCurrentActiveRole = { nullptr };
	CRole_AimisiGD* pCurrentInactiveRole = { nullptr };

	if (!Try_Get_ActiveAndInactiveRoles(pCurrentActiveRole, pCurrentInactiveRole))
		return;

	auto* pSwitchSourceCtrl = pCurrentActiveRole->GetComponent<CAnimationController>();
	if (!pSwitchSourceCtrl)
		return;

	const _bool bApply = (_iApply != 0);
	pSwitchSourceCtrl->Set_ApplyRootMotion(bApply);
	pSwitchSourceCtrl->Set_ApplyRootMotionX(bApply);
	pSwitchSourceCtrl->Set_ApplyRootMotionZ(bApply);
}

void CRole_AimisiGD::Set_Aimsi_RootMotion(const _int& _iApply)
{
	if (!m_pAimisi)
		return;

	/* RootMotion */
	auto* pAimisiCtrl = m_pAimisi->GetComponent<CAnimationController>();

	if (pAimisiCtrl)
	{
		const _bool bApply = (_iApply != 0);
		pAimisiCtrl->Set_ApplyRootMotion(bApply);
		pAimisiCtrl->Set_ApplyRootMotionX(bApply);
		pAimisiCtrl->Set_ApplyRootMotionZ(bApply);
	}
}

void CRole_AimisiGD::Set_GD_RootMotion(const _int& _iApply)
{
	if (!m_pGD)
		return;

	/* RootMotion */
	auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>();

	if (pGDCtrl)
	{
		const _bool bApply = (_iApply != 0);
		pGDCtrl->Set_ApplyRootMotion(bApply);
		pGDCtrl->Set_ApplyRootMotionX(bApply);
		pGDCtrl->Set_ApplyRootMotionZ(bApply);
	}
}

void CRole_AimisiGD::Fix_GD_Skill(const _int& _iPivotIndex)
{
	if (m_pGD == nullptr || m_pAimisi == nullptr)
		return;

	CObjectFilter* pObjectFilter = m_pAimisi->GetComponent<CObjectFilter>();
	if (pObjectFilter == nullptr)
		return;

	CObject* pObject = pObjectFilter->Get_Object(_iPivotIndex); //GD_Pivot
	if (pObject == nullptr)
		return;

	CTransform* pPivotTransform = pObject->GetTransform();
	CTransform* pAimisiTransform = m_pAimisi->GetTransform();
	CTransform* pGDTransform = m_pGD->GetTransform();
	if (pPivotTransform == nullptr || pAimisiTransform == nullptr || pGDTransform == nullptr)
		return;

	pGDTransform->Set_BaseUp(pAimisiTransform->Get_BaseUp());

	_float3 vGDScale = pGDTransform->GetScale();
	_vector vRight = XMVector3Normalize(pPivotTransform->Get_World(STATE::RIGHT)) * vGDScale.x;
	_vector vUp = XMVector3Normalize(pPivotTransform->Get_World(STATE::UP)) * vGDScale.y;
	_vector vLook = XMVector3Normalize(pPivotTransform->Get_World(STATE::LOOK)) * vGDScale.z;
	_vector vPosition = pPivotTransform->Get_World(STATE::POSITION);

	_matrix matWorld(
		XMVectorSetW(vRight, 0.f),
		XMVectorSetW(vUp, 0.f),
		XMVectorSetW(vLook, 0.f),
		XMVectorSetW(vPosition, 1.f)
	);

	if (auto* pParent = pGDTransform->GetParent())
	{
		_matrix matParentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&pParent->GetWorldMatrix()));
		pGDTransform->Set_LocalMatrix(matWorld * matParentInv);
	}
	else
	{
		pGDTransform->Set_LocalMatrix(matWorld);
	}

	_float3 vFinalPos{};
	XMStoreFloat3(&vFinalPos, vPosition);

	if (CCCT* pCCCT = m_pGD->GetComponent<CCCT>())
	{
		pCCCT->Set_Position(vFinalPos);
		pCCCT->Clear_PlanarVelocity();
	}
}

void CRole_AimisiGD::Set_Fix_GD_Skill(const _int& _iPivotIndex)
{
		Fix_GD_Skill(_iPivotIndex);
}

void CRole_AimisiGD::Set_GD_RootMotionMuteWeight(const _float& _fWeight)
{
	if (m_pGD == nullptr)
		return;

	if (auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>())
		pGDCtrl->Set_RootMotionMute(clamp(_fWeight, 0.f, 1.f));
}

void CRole_AimisiGD::Set_GD_RootMotionMultiplier(const _float2& _vMultiplier)
{
	if (m_pGD == nullptr)
		return;

	auto* pGDCtrl = m_pGD->GetComponent<CAnimationController>();
	if (pGDCtrl == nullptr)
		return;

	ANIMATION_INFO* pCurAnimation = pGDCtrl->Get_CurAnimation();
	if (pCurAnimation == nullptr)
		return;

	pCurAnimation->fRootMotionMultiplierX = max(0.f, _vMultiplier.x);
	pCurAnimation->fRootMotionMultiplierZ = max(0.f, _vMultiplier.y);
}

void CRole_AimisiGD::Switch_Type(E_AIMISI_TYPE _eType)
{
	CRole_AimisiGD* pNextType = { nullptr };

	switch (_eType)
	{
	case E_AIMISI_TYPE::AIMISI:
	{
		pNextType = m_pAimisi;
	}
		break;
	case E_AIMISI_TYPE::GUNDAM:
	{
		pNextType = m_pGD;
	}
		break;
	}

	if (!pNextType)
		return;

	if (m_pAimisi)
	{
		m_pAimisi->m_eActiveType = _eType;
		m_pAimisi->m_pAimisi = m_pAimisi;
		m_pAimisi->m_pGD = m_pGD;
		m_pAimisi->m_pActiveType = pNextType;
		m_pAimisi->m_bIsMainRole = (_eType == E_AIMISI_TYPE::AIMISI);
		m_pAimisi->m_eLayer = (_eType == E_AIMISI_TYPE::AIMISI) ? OBJECT_LAYER::MAIN_PLAYER : OBJECT_LAYER::SUB_PLAYER;
		for (auto pRendererWrapper : m_pAimisi->m_listRenderer)
		{
			if (auto pRenderer = pRendererWrapper->Get())
				pRenderer->GetOwner()->Set_Layer(m_pAimisi->m_eLayer);
		}
	}
	if (m_pGD)
	{
		m_pGD->m_eActiveType = _eType;
		m_pGD->m_pAimisi = m_pAimisi;
		m_pGD->m_pGD = m_pGD;
		m_pGD->m_pActiveType = pNextType;
		m_pGD->m_bIsMainRole = (_eType == E_AIMISI_TYPE::GUNDAM);
		m_pGD->m_eLayer = (_eType == E_AIMISI_TYPE::GUNDAM) ? OBJECT_LAYER::MAIN_PLAYER : OBJECT_LAYER::SUB_PLAYER;
		for (auto pRendererWrapper : m_pGD->m_listRenderer)
		{
			if (auto pRenderer = pRendererWrapper->Get())
				pRenderer->GetOwner()->Set_Layer(m_pGD->m_eLayer);
		}
	}

	/* refactor : 명시적 디더링 복구 (건담) */
	pNextType->m_bSwitchingOut = false;
	pNextType->m_bSwitchingIn = false;
	pNextType->m_bDitheringStarted = false;
	pNextType->m_vDitheringTimer.x = 0.f;
	pNextType->Set_Disslove(0.f);
	
	for (auto pRendererWrapper : pNextType->m_listRenderer)
	{
		if (auto pRenderer = pRendererWrapper->Get())
			pRenderer->Set_Renderer_Active(true);
	}

	m_eActiveType = _eType;
	m_pActiveType = pNextType;
	m_bIsMainRole = (this == pNextType);
}

void CRole_AimisiGD::Switch_Type_ForSwitchOut()
{
	m_eActiveType = E_AIMISI_TYPE::AIMISI;
	m_pActiveType = m_pAimisi;

	if (m_pAimisi)
	{
		m_pAimisi->m_eActiveType = E_AIMISI_TYPE::AIMISI;
		m_pAimisi->m_pActiveType = m_pAimisi;
	}
	
	if (m_pGD)
	{
		m_pGD->m_eActiveType = E_AIMISI_TYPE::GUNDAM;
		m_pGD->m_pActiveType = m_pAimisi;
	}
}

void CRole_AimisiGD::Init_Role_Desc()
{
	/* 에이메스 : 24칸, 공명률 50%(12칸) 씩 소모(E) */
	Get_Desc().fHp = 11025.f;
	Get_Desc().fMaxHp = 11025.f;
	Get_Desc().vIdentity_Energy = _float4(0.f, 24.f, 2.f, 12.f);
	Get_Desc().vHarmony_Energy = _float2(0.f, 100.f);
	Get_Desc().vUltra_Energy = _float2(0.f, 100.f);

	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_T)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_E)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_Q)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_R)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SWITCH)] = _float2(0.f, 1.f);
}

void CRole_AimisiGD::Register_Type()
{
	auto wAimisi = m_wAimisiObj ? m_wAimisiObj->Get() : nullptr;
	auto wGD = m_wGDObj ? m_wGDObj->Get() : nullptr;

	/* Cache */
	// Update 에서 check
	m_pAimisi = wAimisi ? static_cast<CRole_AimisiGD*>(wAimisi) : nullptr;
	m_pGD = wGD ? static_cast<CRole_AimisiGD*>(wGD) : nullptr;

	/* Init */
	m_eActiveType = E_AIMISI_TYPE::AIMISI;

	/* Active Type */
	switch (m_eActiveType)
	{
	case E_AIMISI_TYPE::AIMISI:
		m_pActiveType = m_pAimisi;
		break;
	case E_AIMISI_TYPE::GUNDAM:
		m_pActiveType = m_pGD;
		break;
	}
}

void CRole_AimisiGD::Update_Role_Desc(_float _fTimeDelta)
{
	if (this != m_pAimisi)
		return;

	__super::Update_Role_Desc(_fTimeDelta);
}
 
void CRole_AimisiGD::Init_Change_State_Skill()
{
	/* Skill (A <-> G) */
	auto funcIdle = [this]() { return (this == m_pActiveType) && !Has_RunInput(); };
	auto funcRun = [this]() { return (this == m_pActiveType) && Has_RunInput(); };
	auto funcSkillOut = [this]() { 
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (Get_Desc().Check_Identity_Eneergy())
			return false;
		return (this == m_pActiveType) && m_pGameInstance->Key_Down(DIK_E); 
		};
	auto funcAimisiSkillOut = [this]() { 
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (Get_Desc().Check_Identity_Eneergy())
			return false;
		return (this == m_pAimisi) && (this == m_pActiveType) && m_pGameInstance->Key_Down(DIK_E); 
		};
	auto funcGDSkillOut = [this]() { 
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (Get_Desc().Check_Identity_Eneergy())
			return false;
		return (this == m_pGD) && (this == m_pActiveType) && m_pGameInstance->Key_Down(DIK_E); 
		};
	auto funcAimisiHalfSkillOut = [this]() {
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (!Get_Desc().Check_Identity_Eneergy())
			return false;
		return (this == m_pAimisi) && (this == m_pActiveType) && m_pGameInstance->Key_Down(DIK_E);
		};
	auto funcGDHalfSkillOut = [this]() {
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (!Get_Desc().Check_Identity_Eneergy())
			return false;
		return (this == m_pGD) && (this == m_pActiveType) && m_pGameInstance->Key_Down(DIK_E);
		};

	// Attack 1
	{
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_05", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_05", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_05", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Skill_05", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_05", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Skill_05", new CChangeStatement(L"Idle", 1.f));

		m_pStateController->InsertChangeStatement(L"Skill_06", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_06", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}

	// 공명률 50%
	{
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_04", funcAimisiHalfSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_03", funcGDHalfSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Skill_04", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_04", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_03", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_03", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}

	// Attack 2,3,4
	{
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_02", funcAimisiSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_02", funcAimisiSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_02", funcAimisiSkillOut, true));
		
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_01", funcGDSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_01", funcGDSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_01", funcGDSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_01", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_01", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}
}

_float CRole_AimisiGD::Get_SwitchDitheringTime()
{
	//if (!m_pAimisi) return 2.f;
	//return m_pAimisi->Is_AttackState() ? 2.f : 0.0001f;

	return this->Is_AttackState() ? 2.f : 0.0001f;
}

void CRole_AimisiGD::OnGui_Object_Inspector_Context()
{
	__super::OnGui_Object_Inspector_Context();

	if (!m_wAimisiObj || !m_wGDObj)
		return;

	if (ImGui::CollapsingHeader("AIMISI / GD Binding", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_wAimisiObj->OnGui_Inspector_Context("Aimisi")) {
			if (auto pObject = m_wAimisiObj->Get()) {

			}
		}
		ImGui::SameLine(); ImGui::TextUnformatted("Aimisi");

		if (m_wGDObj->OnGui_Inspector_Context("GD")) {
			if (auto pObject = m_wGDObj->Get()) {

			}
		}
		ImGui::SameLine(); ImGui::TextUnformatted("GD");
	}
}

Json::Value CRole_AimisiGD::Serialize()
{
	Json::Value jsonValue;

	if (m_wAimisiObj)
		jsonValue["m_wAimisiObj"] = m_wAimisiObj->Serialize();
	else
		jsonValue["m_wAimisiObj"] = Json::nullValue;

	if (m_wGDObj)
		jsonValue["m_wGDObj"] = m_wGDObj->Serialize();
	else
		jsonValue["m_wGDObj"] = Json::nullValue;

	return jsonValue;
}

void CRole_AimisiGD::Deserialize(Json::Value& _jsonValue)
{
	if (!m_wAimisiObj)
		m_wAimisiObj = WObject<CObject>::Create(nullptr); // 생성 시점 다름

	if (_jsonValue.isMember("m_wAimisiObj") && !_jsonValue["m_wAimisiObj"].isNull())
		m_wAimisiObj->Deserialize(_jsonValue["m_wAimisiObj"]);

	if (!m_wGDObj)
		m_wGDObj = WObject<CObject>::Create(nullptr); // 생성 시점 다름

	if (_jsonValue.isMember("m_wGDObj") && !_jsonValue["m_wGDObj"].isNull())
		m_wGDObj->Deserialize(_jsonValue["m_wGDObj"]);
}

void CRole_AimisiGD::Free()
{
	__super::Free();

	Safe_Release(m_wAimisiObj);
	Safe_Release(m_wGDObj);
}

//Shader Camera Test
_bool CRole_AimisiGD::Can_Process_Input() const
{
	return nullptr == m_pActiveType || this == m_pActiveType;
}
