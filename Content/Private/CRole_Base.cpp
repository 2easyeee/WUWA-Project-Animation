#include "ContentPCH.h"
#include "CRole_Base.h"
#include <CEffectHolder.h>
#include <CCharacterCamera.h>
#include <CActionController.h>
#include <CSkinnedMeshRenderer.h>
#include <CVirtualCamera.h>
#include <CGameManager.h>
#include <CBGMController.h>
#include <CInteractive_Grapple.h>
#include <CHook_Effect.h>
#include <CInstanceEffect.h>
#include <CWeapon.h>
#include <CHoliyDarkFire.h>
#include <CPostProcessEventController.h>
#include <CUI_Manager.h>
#include <CCamera.h>
#include "CBoss_Leviathan.h"
#include "CBoss_Leviathan_Clone.h"
#include "CBoss_Levi_DDong.h"
#include "CWeapon_Paraglider.h"

constexpr _ullong hState = Get_HashCode("State");
constexpr _ullong hHasInput = Get_HashCode("HasInput");
constexpr _ullong hInput_X = Get_HashCode("Input_X");
constexpr _ullong hInput_Z = Get_HashCode("Input_Z");

namespace {
	constexpr _float ROLE_BASE_EPSILON = 1e-6f;

	_vector NormalizeOr(_fvector _vValue, _fvector _vFallback) {
		if (XMVectorGetX(XMVector3LengthSq(_vValue)) <= ROLE_BASE_EPSILON)
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
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > ROLE_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(_vFallback, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > ROLE_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(XMVectorSet(0.f, 0.f, 1.f, 0.f), _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > ROLE_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		return XMVector3Normalize(RemoveAxisComponent(XMVectorSet(1.f, 0.f, 0.f, 0.f), _vBaseUp));
	}

	_float GetBaseUpVelocity(CTransform* _pTransform, _fvector _vVelocity) {
		return XMVectorGetX(XMVector3Dot(_vVelocity, GetTransformBaseUp(_pTransform)));
	}

	_vector SetBaseUpVelocity(CTransform* _pTransform, _fvector _vVelocity, _float _fVelocity) {
		_vector vBaseUp = GetTransformBaseUp(_pTransform);
		return RemoveAxisComponent(_vVelocity, vBaseUp) + vBaseUp * _fVelocity;
	}

	void RemoveNonYawRotationOnBaseUp(CTransform* _pTransform) {
		if (_pTransform == nullptr)
			return;

		_vector vBaseUp = GetTransformBaseUp(_pTransform);
		_vector vLook = ProjectOnBasePlane(_pTransform->Get_World(STATE::LOOK), vBaseUp, XMVectorSet(0.f, 0.f, 1.f, 0.f));
		_vector vRight = NormalizeOr(XMVector3Cross(vBaseUp, vLook), XMVectorSet(1.f, 0.f, 0.f, 0.f));
		vLook = NormalizeOr(XMVector3Cross(vRight, vBaseUp), vLook);

		_matrix matParent = XMMatrixIdentity();
		if (auto pParent = _pTransform->GetParent())
			matParent = XMLoadFloat4x4(&pParent->GetWorldMatrix());

		XMVECTOR vParentScale, qParent, vParentPosition;
		XMMatrixDecompose(&vParentScale, &qParent, &vParentPosition, matParent);
		qParent = XMQuaternionNormalize(qParent);
		_vector qInvParent = XMQuaternionInverse(qParent);

		_float3 vScale = _pTransform->GetScale();
		vScale.x = (vScale.x > ROLE_BASE_EPSILON) ? vScale.x : 1.f;
		vScale.y = (vScale.y > ROLE_BASE_EPSILON) ? vScale.y : 1.f;
		vScale.z = (vScale.z > ROLE_BASE_EPSILON) ? vScale.z : 1.f;

		_pTransform->Set_State_NoDirty(STATE::RIGHT, XMVectorSetW(XMVector3Rotate(vRight, qInvParent) * vScale.x, 0.f));
		_pTransform->Set_State_NoDirty(STATE::UP, XMVectorSetW(XMVector3Rotate(vBaseUp, qInvParent) * vScale.y, 0.f));
		_pTransform->Set_State_NoDirty(STATE::LOOK, XMVectorSetW(XMVector3Rotate(vLook, qInvParent) * vScale.z, 0.f));
		_pTransform->Calculate_World();
	}

	_bool IsBossTargetMonster(CMonster* _pMonster) {
		if (_pMonster == nullptr)
			return false;

		return _pMonster->Get_Layer() == OBJECT_LAYER::BOSS;
	}

	CMonster* Find_MonsterFromHierarchy(CObject* _pObject) {
		for (auto pTransform = _pObject ? _pObject->GetTransform() : nullptr; pTransform != nullptr; pTransform = pTransform->GetParent()) {
			auto pOwner = pTransform->GetOwner();
			if (pOwner == nullptr)
				continue;

			if (auto pMonster = dynamic_cast<CMonster*>(pOwner))
				return pMonster;
		}

		return nullptr;
	}

	CTransform* Get_MonsterTargetTransform(CMonster* _pMonster) {
		if (_pMonster == nullptr)
			return nullptr;

		if (auto pHitCase = _pMonster->Get_HitCase()) {
			if (auto pHitTransform = pHitCase->GetTransform())
				return pHitTransform;
		}

		return _pMonster->GetTransform();
	}

}
CRole_Base::CRole_Base(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CCharacter(_pDevice, _pContext) {
	m_eCharacterType = CHARACTER_TYPE::PLAYER;
	Reset_SkillCameraDesc();
}

HRESULT CRole_Base::Awake() {
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_Base::Awake");
		return E_FAIL;
	}

	m_pAnimationStateMachine = CreateComponent<CAnimationStateMachine>();
	m_pObjectFilter = CreateComponent<CObjectFilter>();
	m_pCCT = CreateComponent<CCCT>();

	m_pTrigger = CreateComponent<CTrigger_Capsule>();

	m_pPostProcessController = CreateComponent<Engine::CPostProcessEventController>();

	// 많아졌는데 컨테이너로 돌릴 수 있으면 그러는게 나을 것 같은데
	m_pTargetMonster = WObject<CMonster>::Create(nullptr);
	m_pParaglider = WObject<CObject>::Create(nullptr);
	m_pHandBone = WObject<CObject>::Create(nullptr);
	m_pGrappleObj = WObject<CGameObject>::Create(nullptr);
	m_pInteractionObj = WObject<CGameObject>::Create(nullptr);
	m_pWeapon = WObject<CWeapon>::Create(nullptr);
	m_pScabbard = WObject<CWeapon>::Create(nullptr);
	m_pBackHolder = WObject<CObject>::Create(nullptr);

	m_pVehicle = WComponent<CTransform>::Create(nullptr);

	m_pSeqCam = WObject<>::Create(nullptr);
	m_pBurstVC = WComponent<CVirtualCamera>::Create(nullptr);

	return S_OK;
}

void CRole_Base::Start() {
	__super::Start();

	if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
		auto listVC = pSQCamObj->GetComponents<CVirtualCamera>();
		if (!listVC.empty()) {
			if (auto pVC = listVC.front()) {
				m_pBurstVC->Set(pVC);
			}
		}
	}

	Init_Role_Desc();
	Init_AttackRootMotionLimit();
	Init_Forced_Return_EffectPool();

	Init_Collision();

	/* Register State (Common) */
	Init_Disable_State();
	Init_Idle_State();
	Init_Run_State();
	Init_Walk_State();
	Init_Sprint_State();
	Init_Move_State();
	Init_Move_Limit_State();
	Init_Attack_L_Loop_State(1.5f);
	Init_Attack_L_End_State(1.f);
	Init_Execute_State();
	Init_Attack_Air_Start_State(0.667f);
	Init_Attack_Air_End_State(0.5f);
	Init_Behit_State();

	Init_Hook_State();
	Init_Fall_State();
	Init_Throw_State();
	Init_Sit_State();

	/* Register State (override) */
	Init_Jump_Start_State();
	Init_Jump_Loop_State();
	Init_Jump_Land_State();
	Init_Jump_Second_State();
	Init_Fly_Start_State();
	Init_Fly_Loop_State();
	Init_Attack01_State(0.5f);
	Init_Attack02_State(0.5f);
	Init_Attack03_State(1.5f);
	Init_Attack04_State(0.85f);
	Init_Attack05_State(1000.f);
	Init_Attack06_State(1000.f);
	Init_Skill01_State(1.f);
	Init_Skill02_State(1.f);
	Init_Skill03_State(4.f);
	Init_Skill04_State(5.f);
	Init_Skill05_State(0.1f);
	Init_Skill06_State(1.f);
	Init_Burst01_State(2.f);
	Init_Switch_QTE_State(1.f);

	/* Register Change State (override) */
	Init_Change_State_Jump();
	Init_Change_State_Fly();
	Init_Change_State_Skill();
	Init_Change_State_Switch_QTE();
	Init_Change_State_Behit();
	Init_Change_State_Attack_Combo(0.3f, 0.3f, 0.3f);
	Init_Change_State_Attack_L();

	/* Register Change State (Common) */
	Init_Change_State_Common();
	Init_Change_State_Locomotion();
	Init_Change_State_Common_Attack();

	/* Camera */
	m_pCharacterCamera = CreateObject<CCharacterCamera>();
	m_pCharacterCamera->Start_If_Not_Initialized();
	m_pCharacterCamera->Set_TargetObject(this);
	Init_Push_Camera();

	/* Detact Monster */
	auto pDetactTriggerObj = CreateObject<CGameObject>(m_pTransform, { 0.f, 0.f, 0.f });
	pDetactTriggerObj->Set_Name(L"Detact Trigger");
	if (auto pTrigger = pDetactTriggerObj->CreateComponent<CTrigger_Sphere>()) {
		pTrigger->Set_Radius(7.5f);
		pTrigger->Set_Filter(ColliderTag_Bit(ETOU(COLLIDER_TAG::MONSTER)) | ColliderTag_Bit(ETOU(COLLIDER_TAG::BOSS)));
		pTrigger->Set_CollidedCallback(bind(&CRole_Base::Monster_Detacted, this, placeholders::_1, placeholders::_2));
	}
}

void CRole_Base::Update(_float _fTimeDelta) {

	Update_Switch(_fTimeDelta);

	Reset_Input(_fTimeDelta);

	if (Update_PlayerControlLock())
		return;

	Update_LandLock(_fTimeDelta);

	Update_Lock_Move(_fTimeDelta);

	Update_IFrameTime(_fTimeDelta);

	Update_Role_Desc(_fTimeDelta);

	if (m_bIsMainRole && !m_bInputLock) {
		Locomotion_Input(_fTimeDelta);

		Attack_Input(_fTimeDelta);

		Consume_Input();

		Record_VideoInput();

		//Debug_Test(_fTimeDelta);
	}

	Expire_Input(_fTimeDelta);

	Update_AttackRootMotionLimit();
}

void CRole_Base::Late_Update(_float _fTimeDelta) {
	__super::Late_Update(_fTimeDelta);

	m_pAnimationStateMachine->Set_Int(hState, m_iState);
	m_pAnimationStateMachine->Set_Bool(hHasInput, m_bHasInput);
	m_pAnimationStateMachine->Set_Float(hInput_X, m_iInput_X);
	m_pAnimationStateMachine->Set_Float(hInput_Z, m_iInput_Z);

	if (auto pMonster = m_pTargetMonster->Get()) {
		auto pMonsterTransform = Get_MonsterTargetTransform(pMonster);
		if (m_pTransform == nullptr) {
			m_pTargetMonster->Set(nullptr);
			return;
		}

		if (pMonsterTransform == nullptr) {
			m_pTargetMonster->Set(nullptr);
		}
		else {
			auto vDif = pMonsterTransform->Get_World(STATE::POSITION) - m_pTransform->Get_World(STATE::POSITION);
			_float fDist = XMVectorGetX(XMVector3Length(vDif));
			_bool bBossTarget = IsBossTargetMonster(pMonster);

			if (pMonster->Get_Desc().fHp <= 0.f || (!bBossTarget && fDist > 9.f)) {
				m_pTargetMonster->Set(nullptr);
			}
		}
	}

	Update_BossCameraArm(_fTimeDelta);

	//pMonster->Get_Desc().fHp <= 0.f && 9.f;

	// if (m_pStateController->GetCurStateName() == L"Idle")
	//	m_fWeaponVisualizeDurtaion = max(m_fWeaponVisualizeDurtaion - _fTimeDelta, 0.f);
	
	// if (m_fWeaponVisualizeDurtaion <= 0.f) {
	// 	Set_Weapon_Visual({ 0 });
	// 	Set_Scabbard_Visual({ 0 });
	// }
}

void CRole_Base::Record_VideoInput()
{
	if (m_pGameInstance->Key_Down(DIK_F7))
	{
		if (!m_pTransform)
			return;

		_float4x4 matTeleport = m_pTransform->GetWorldMatrix();
		matTeleport._41 = 236.710464f;
		matTeleport._42 = 511.823975f;
		matTeleport._43 = 508.720215f;

		Teleport(matTeleport);
	}
}

void CRole_Base::Register_Meta_Bone() {
	if (!m_pTransform || !m_pHitCase || !m_pParaglider || !m_pHandBone)
		return;

	m_pHitCase->Set(nullptr);
	m_pParaglider->Set(nullptr);
	m_pHandBone->Set(nullptr);
	m_pWeapon->Set(nullptr);
	m_pScabbard->Set(nullptr);
	m_pBackHolder->Set(nullptr);
	m_pMiddleCase->Set(nullptr);

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

		switch (pOwner->Get_Layer()) {
		case OBJECT_LAYER::HIT_CASE:
			if (pHitCase == nullptr)
				pHitCase = pCur;
			break;
		case OBJECT_LAYER::PARAGLIDER:
			if (m_pParaglider->Get() == nullptr)
				m_pParaglider->Set(pOwner);
			break;
		case OBJECT_LAYER::RIGHT_HAND:
			if (m_pHandBone->Get() == nullptr)
				m_pHandBone->Set(pOwner);
			break;
		case OBJECT_LAYER::WEAPON:
			if (m_pWeapon->Get() == nullptr) {
				if (auto pWeapon = pOwner->As<CWeapon>())
					m_pWeapon->Set(pWeapon);
			}
			break;
		case OBJECT_LAYER::SCABBARD:
			if (m_pScabbard->Get() == nullptr) {
				if (auto pScabbard = pOwner->As<CWeapon>())
					m_pScabbard->Set(pScabbard);
			}
			break;
		case OBJECT_LAYER::BACK_HOLDER:
			if (m_pBackHolder->Get() == nullptr)
				m_pBackHolder->Set(pOwner);
			break;

		case OBJECT_LAYER::MIDDLE_CASE:
			if (m_pMiddleCase->Get() == nullptr)
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

void CRole_Base::Enter_SideViewMode(const SIDEVIEW_MODE_DESC& _tSideViewDesc) {
	m_ePlayMode = E_PLAY_MODE::SIDEVIEW;

	m_tSideViewModeDesc = _tSideViewDesc;
}

void CRole_Base::Exit_SideViewMode() {
	m_ePlayMode = E_PLAY_MODE::COMMON;

	CAMERA_TRANSITION_DESC tCamTransDesc{};
	tCamTransDesc.eCameraOrder = CAMERA_ORDER::IN_GAME;
	tCamTransDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
	tCamTransDesc.fTransitionTime = 1.f;
	tCamTransDesc.fDampingPower = 5.f;
	tCamTransDesc.iFlag = ETOU(CAMERA_BLEND_MASK::POS) | ETOU(CAMERA_BLEND_MASK::ROT) | ETOU(CAMERA_BLEND_MASK::FOV);
	tCamTransDesc.pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera();

	m_pGameInstance->Push_Camera(tCamTransDesc);
}

void CRole_Base::Teleport(const _float4x4& _matWorld) {
	_float3 vPos{};
	vPos.x = _matWorld._41;
	vPos.y = _matWorld._42;
	vPos.z = _matWorld._43;

	m_pCCT->Set_Position(vPos);
	m_pTransform->Set_LocalMatrix(_matWorld);
	if (m_pCharacterCamera) {
		m_pCharacterCamera->Init_Camera_Pos();
	}

	m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
	m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
}

void CRole_Base::Apply_FallBackPose(const _float3& _vBaseUp, const _float3& _vPosition, const _float4& _vRotation) {
	if (m_pTransform) {
		m_pTransform->Set_BaseUp(_vBaseUp);
	}

	if (m_pCCT && m_pCCT->GetActive()) {
		m_pCCT->Set_Position(_vPosition);
		m_pCCT->Stop();
	}
	else if (m_pTransform) {
		m_pTransform->Set_Position(_vPosition);
	}

	if (m_pTransform) {
		m_pTransform->Set_Rotation_Quaternion(_vRotation);
	}

	m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
}

void CRole_Base::Switch_In(CRole_Base* _pPrevRole) {

	m_bSwitchOutComboChain = false;

	if (_pPrevRole)
	{
		_pPrevRole->Prepare_Switch_Out();
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 2.f, 0.25f });
	}

	m_bIsMainRole = true;

	m_eLayer = OBJECT_LAYER::MAIN_PLAYER;
	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->SetActive(true);
			pRenderer->GetOwner()->Set_Layer(OBJECT_LAYER::MAIN_PLAYER);
		}
	}

	Lock_Move(0.1f);
	Clear_AllInput();

	m_bSwitchingOut = false;
	m_bSwitchingIn = true;
	m_bDitheringStarted = false;
	m_vDitheringTimer = { 0.f, 0.2f };
	Set_Disslove(1.f);

	if (_pPrevRole && _pPrevRole->m_ePlayMode == E_PLAY_MODE::SIDEVIEW) {
		m_ePlayMode = E_PLAY_MODE::SIDEVIEW;
		m_tSideViewModeDesc = _pPrevRole->m_tSideViewModeDesc;
		_pPrevRole->m_ePlayMode = E_PLAY_MODE::COMMON;
	}
	else {
		m_ePlayMode = E_PLAY_MODE::COMMON;
	}
	
	if (_pPrevRole) {
		auto pPrevVC = _pPrevRole->m_pCharacterCamera->Get_VirtualCamera();
		auto tPose = pPrevVC->Get_CurPose();

		/* Teleport Custom */
		_float3 vPrevWorldPos = _pPrevRole->GetTransform()->GetWorldPosition();

		_vector vBaseUp = GetTransformBaseUp(_pPrevRole->GetTransform());
		_float3 vBaseUpFloat{};
		XMStoreFloat3(&vBaseUpFloat, vBaseUp);
		m_pTransform->Set_BaseUp(vBaseUpFloat);

		/* Look 방향 맞추기 */
		_vector vPrevLook = ProjectOnBasePlane(_pPrevRole->GetTransform()->Get_World(STATE::LOOK), vBaseUp, XMVectorSet(0.f, 0.f, 1.f, 0.f));

		_vector vSpawnLook = vPrevLook;
		_vector vSpawnPos = XMLoadFloat3(&vPrevWorldPos);

		/* Pos & Target */
		_vector vPrevPos = XMLoadFloat3(&vPrevWorldPos);
		auto pMonster = _pPrevRole->m_pTargetMonster ? _pPrevRole->m_pTargetMonster->Get() : nullptr;
		if (_pPrevRole->Is_AttackState())
		{
			if (pMonster)
			{
				auto pMonsterTransform = Get_MonsterTargetTransform(pMonster);

				_vector vMonsterPos = pMonsterTransform->Get_World(STATE::POSITION);
				_vector vFromMonster = ProjectOnBasePlane(vPrevPos - vMonsterPos, vBaseUp, -vPrevLook);

				Set_SpawnPosByRadius(vMonsterPos, vFromMonster, vBaseUp,
					vPrevPos, vPrevLook,
					m_pGameInstance->Random(90.f, 120.f), m_fSwitchInTargetRadius,
					vSpawnPos, vSpawnLook);

				if (m_pTargetMonster)
					m_pTargetMonster->Set(pMonster);
			}
			else
			{
				const _float fCenterDistance = 1.f;
				const _float fRadius = 2.5f; // 10.f

				_vector vCenterPos = vPrevPos + (vPrevLook * fCenterDistance);
				_vector vFromCenterToPrev = ProjectOnBasePlane(vPrevPos - vCenterPos, vBaseUp, -vPrevLook);

				Set_SpawnPosByRadius(vCenterPos, vFromCenterToPrev, vBaseUp,
					vPrevPos, vPrevLook,
					m_pGameInstance->Random(45.f, 90.f), fRadius,
					vSpawnPos, vSpawnLook);
			}
		}

		_float3 vNextWorldPos = {};
		XMStoreFloat3(&vNextWorldPos, vSpawnPos);
		m_pCCT->Set_Position(vNextWorldPos);

		m_pTransform->Turn(vSpawnPos + vSpawnLook, vBaseUp);
		XMStoreFloat3(&m_vTargetDirection, vSpawnLook);

		/* 카메라의 방향 맞추기 */

		/* Camera */
		m_pCharacterCamera->SetActive(true);

		if (auto pPrevCamera = _pPrevRole->Get_CharacterCamera())
			m_pCharacterCamera->Set_CameraRotation(pPrevCamera);

		m_pCharacterCamera->Snap_ToTargetKeepingRotation();

		CAMERA_TRANSITION_DESC tCamTransitionDesc{};
		tCamTransitionDesc.eCameraOrder = CAMERA_ORDER::IN_GAME;
		tCamTransitionDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
		tCamTransitionDesc.fFov = tPose.fFov;
		tCamTransitionDesc.iFlag = ENUM_TO_UINT(CAMERA_BLEND_MASK::POS) | ENUM_TO_UINT(CAMERA_BLEND_MASK::ROT) | ENUM_TO_UINT(CAMERA_BLEND_MASK::FOV);
		tCamTransitionDesc.pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera();
		tCamTransitionDesc.fTransitionTime = 0.1f;
		m_pGameInstance->Push_Camera(tCamTransitionDesc);
	}

	_bool bIsAttackState = _pPrevRole && _pPrevRole->Is_AttackState();
	_bool bIsSwitchQTE = _pPrevRole && _pPrevRole->Get_Desc().Check_Harmony_Energy();

	if (bIsSwitchQTE)
	{
		if (bIsAttackState)
			_pPrevRole->Begin_Switch_Out_ComboChain();

		_pPrevRole->Get_Desc().Spend_Harmony_Energy(); // 이전 Role 의 하모니 에너지를 쓴다
		m_pStateController->ChangeAnimState(L"Switch_QTE", true);
	}
	else if (bIsAttackState)
	{
		_pPrevRole->Begin_Switch_Out_ComboChain();
		m_pStateController->ChangeAnimState(L"Attack_02", true);
	}
	else
	{
		m_pStateController->ChangeAnimState(L"Idle", true);
	}

	if (_pPrevRole)
		_pPrevRole->Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SWITCH);

	/* 상태 전환 후에 무적 시간 추가 */
	m_bIFrames = true;
	m_fIFramesTimer.x = 0.75f;
}

void CRole_Base::Switch_Out() {
	m_bIsMainRole = false;

	if (m_bSwitchOutComboChain)
	{
		Reset_Locomotion();
		Reset_Input(0.f);

		/* Reset */
		m_pAnimationStateMachine->Set_Bool(hHasInput, false);
		m_pAnimationStateMachine->Set_Float(hInput_X, 0.f);
		m_pAnimationStateMachine->Set_Float(hInput_Z, 0.f);
	}
	else
	{
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);

		/* Reset */
		m_pAnimationStateMachine->Set_Bool(hHasInput, false);
		m_pAnimationStateMachine->Set_Float(hInput_X, 0.f);
		m_pAnimationStateMachine->Set_Float(hInput_Z, 0.f);
	}

	m_eLayer = OBJECT_LAYER::SUB_PLAYER;

	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->GetOwner()->Set_Layer(OBJECT_LAYER::SUB_PLAYER);
		}
	}

	m_pCharacterCamera->SetActive(false);
	m_pGameInstance->Pop_Camera(m_pCharacterCamera->Get_VirtualCamera());

	/* Notify */
	m_bSwitchingOut = true;
	m_bSwitchingIn = false;
	m_bDitheringStarted = false;
	m_vDitheringTimer.x = 0.f;
	m_vDitheringTimer.y = Get_SwitchDitheringTime();
}

void CRole_Base::Set_SpawnPosByRadius(_fvector _vCenterPos, _fvector _vBaseDirection, _fvector _vBaseUp, _fvector _vPrevPos, _fvector _vPrevLook, _float _fYawDegree, _float _fRadius, _vector& _vSpawnPos, _vector& _vSpawnLook)
{
	_float fYaw = XMConvertToRadians(fabs(_fYawDegree));
	if (m_pGameInstance->Random(0.f, 1.f) < 0.5f)
		fYaw = -fYaw;

	_matrix matYaw = XMMatrixRotationAxis(_vBaseUp, fYaw);
	_vector vSpawnDir = ProjectOnBasePlane(XMVector3TransformNormal(_vBaseDirection, matYaw), _vBaseUp, _vBaseDirection);

	_vSpawnPos = _vCenterPos + (vSpawnDir * _fRadius);
	_vSpawnPos = XMVectorSetY(_vSpawnPos, XMVectorGetY(_vPrevPos));
	_vSpawnLook = ProjectOnBasePlane(_vCenterPos - _vSpawnPos, _vBaseUp, _vPrevLook);
}

void CRole_Base::Begin_Switch_Out_ComboChain()
{
	if (!Is_AttackState())
	{
		m_bSwitchOutComboChain = false;
		return;
	}

	m_bSwitchOutComboChain = true;
	m_bComboInput = true;
}

void CRole_Base::Update_Switch_Out_ComboChain(_float _fTimeDelta)
{
	if (!m_bSwitchOutComboChain)
		return;

	if (!Is_AttackState())
	{
		m_bSwitchOutComboChain = false;
		m_bComboInput = false;
		m_bDitheringStarted = false;
		m_vDitheringTimer = { 0.f, 1.f };

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });

		return;
	}

	if (m_iComboIndex > 0 && m_iComboIndex < Get_Switch_Out_ComboMaxCount())
	{
		m_bComboInput = true;
		return;
	}

	m_bComboInput = false;
}

void CRole_Base::Update_Switch(_float _fTimeDelta)
{
	if (m_bSwitchingOut)
	{
		Update_Switch_Out(_fTimeDelta);
		return;
	}

	if (m_bSwitchingIn)
	{
		Update_Switch_In(_fTimeDelta);
	}
}

void CRole_Base::Update_Switch_In(_float _fTimeDelta)
{
	if (!m_bDitheringStarted)
	{
		m_bDitheringStarted = true;
		m_vDitheringTimer.x = 0.f;

		Set_Disslove(1.f); // 1 -> 0
	}

	m_vDitheringTimer.x += _fTimeDelta;

	_float fDitheringRatio = m_vDitheringTimer.x / m_vDitheringTimer.y;
	fDitheringRatio = clamp(fDitheringRatio, 0.f, 1.f);

	_float fCalDitheringRatio = 1.f - fDitheringRatio;
	Set_Disslove(fCalDitheringRatio);
	if (fDitheringRatio >= 1.f)
	{
		m_bSwitchingIn = false;
		m_bDitheringStarted = false;
		m_vDitheringTimer.x = 0.f;

		Set_Disslove(0.f);
	}
}

void CRole_Base::Update_Switch_Out(_float _fTimeDelta)
{
	if (m_bSwitchOutComboChain)
	{
		Reset_Locomotion();
		Reset_Input(0.f);

		m_pAnimationStateMachine->Set_Bool(hHasInput, false);
		m_pAnimationStateMachine->Set_Float(hInput_X, 0.f);
		m_pAnimationStateMachine->Set_Float(hInput_Z, 0.f);

		Update_Switch_Out_ComboChain(_fTimeDelta);

		if (m_bSwitchOutComboChain && Is_AttackState())
		{
			this->Set_Disslove(0.f);
			return;
		}
	}
	else
	{
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);

		m_pAnimationStateMachine->Set_Bool(hHasInput, false);
		m_pAnimationStateMachine->Set_Float(hInput_X, 0.f);
		m_pAnimationStateMachine->Set_Float(hInput_Z, 0.f);

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
	}


	_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
	fRatio = clamp(fRatio, 0.f, 1.f);

	if (!m_bDitheringStarted)
	{
		m_bDitheringStarted = true;
		m_vDitheringTimer.x = 0.f;


		this->Set_Disslove(0.f); // 0 -> 1
	}

	if (m_bDitheringStarted)
	{
		m_vDitheringTimer.x += _fTimeDelta;

		_float fDitheringRatio = m_vDitheringTimer.x / m_vDitheringTimer.y;
		fDitheringRatio = clamp(fDitheringRatio, 0.f, 1.f);
		this->Set_Disslove(fDitheringRatio);

		if (fDitheringRatio >= 1.f)
		{
			m_bSwitchingOut = false;
			m_bSwitchOutComboChain = false;
			m_bDitheringStarted = false;
			m_vDitheringTimer.x = 0.f;

			if (m_pCCT)
			{
				m_pCCT->Clear_PlanarVelocity();
				m_pCCT->Clear_VerticalVelocity();
			}

			for (auto pRendererWrapper : m_listRenderer) {
				if (auto pRenderer = pRendererWrapper->Get()) {
					pRenderer->Set_Renderer_Active(false);
				}
			}

			m_pStateController->ChangeAnimState(L"Disable", true);
		}
	}
}

_float CRole_Base::Get_SwitchDitheringTime()
{
	return Is_AttackState() ? 10.f : 0.0001f;
}

_bool CRole_Base::Check_SideView_Hook_Enable(_fvector _vHookPos) {
	auto vDiff = _vHookPos - m_pTransform->Get_World(STATE::POSITION);
	_float fLen = XMVector3Length(vDiff).m128_f32[0];
	auto vMoveAxis = XMLoadFloat3(&m_tSideViewModeDesc.vMoveAxis);
	auto vTargetDir = XMLoadFloat3(&m_vTargetDirection);

	_float fDot1 = XMVectorGetX(XMVector3Dot(vDiff, vMoveAxis));
	_float fDot2 = XMVectorGetX(XMVector3Dot(vTargetDir, vMoveAxis));

	if (fDot1 * fDot2 > 0.f && fLen < 20.f) {
		return true;
	}

	return false;
}

void CRole_Base::Exit_Grappling() {
	m_bGrappling = false;
 	if (auto pGrappleObj = m_pGrappleObj->Get()) {
		pGrappleObj->GetTransform()->Destroy_Self_And_Child();
		m_pGrappleObj->Set(nullptr);
	}
	m_pInteractionObj->Set(nullptr);

	const auto& vVelocity = m_pCCT->Get_Velocity();

	m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
}

void CRole_Base::Update_IFrameTime(_float _fTimeDelta)
{
	if (m_fIFramesTimer.x > 0.f)
	{
		m_fIFramesTimer.x = max(0.f, m_fIFramesTimer.x - _fTimeDelta);
		m_bIFrames = (m_fIFramesTimer.x > 0.f);
	}
	else
	{
		//Debug_Output(L"!\n");
		m_bIFrames = false;
	}
}

void CRole_Base::Forced_Return_EffectPool(_int _iEffectIndex) {
	if (auto pEffHolderObj = m_pObjectFilter->Get_Object<CEffectHolder>(ETOU(E_OBJECTS::EFFECT_HOLDER))) {
		pEffHolderObj->Stop_Slot_Effect(_iEffectIndex);
	}
}

CCharacterCamera* CRole_Base::Get_CharacterCamera()
{
	return m_pCharacterCamera;
}

void CRole_Base::Auto_Turn_To_Target_Monster(_float _fTurnSpeed, _float _fAutoTurnDuration) {
	if (auto pMonster = m_pTargetMonster->Get()) {
		XMStoreFloat3(&m_vTargetDirection, XMVector3Normalize(pMonster->GetTransform()->Get_World(STATE::POSITION) - m_pTransform->Get_World(STATE::POSITION)));

		Auto_Turn(m_vTargetDirection, _fTurnSpeed, _fAutoTurnDuration);
	}
}

void CRole_Base::Auto_Turn_To_Monster_Event(const _float2& _vParam) {
	Auto_Turn_To_Target_Monster(_vParam.x, _vParam.y);
}

void CRole_Base::Turn_To_Player_Look_Event(const _float2& _vParam) {
	if (m_pCharacterCamera) {
		m_pCharacterCamera->Turn_To_Player_Look(_vParam.x, _vParam.y);
	}
}

void CRole_Base::Set_Attack_RootMotion_Limit(const _float& _fDistance) {
	m_fRootMotionLimitDistance = _fDistance;
	Update_AttackRootMotionLimit();

	// 기본 Distance 로 매번 돌아가야하는지 고려 (Default Distance set)
}

void CRole_Base::Reset_Input(_float _fTimeDelta)
{
	m_bHasInput = false;
	m_iInput_X = 0;
	m_iInput_Z = 0;
}

void CRole_Base::Reset_Locomotion()
{
	if (m_pCCT)
		m_pCCT->Clear_PlanarVelocity();

	XMStoreFloat3(&m_vTargetDirection, XMVectorZero());
	m_fSpeedFactor = 0.f;
	m_bWasLocomotionMoving = false;
}

void CRole_Base::Reset_Gravity()
{
	m_fJumpGravityScale = 1.2f;
	m_fFlyGravityScale = 0.25f;
}

void CRole_Base::Reset_CameraSettings()
{
	if (!m_pCharacterCamera)
		return;

	m_pCharacterCamera->Set_ObjectOffset(_float3{ 0.f, 1.4f, 0.f });
	m_pCharacterCamera->Set_CameraArmLength(3.5f);
	m_pCharacterCamera->Set_CameraArm_Speed(DEFAULT_ARM_SPEED);
	m_pCharacterCamera->Reset_DestFov();
}

void CRole_Base::Clear_AllInput()
{
	Reset_Locomotion();
	Reset_Input(0.f);

	m_bAttackQueued = false;
	m_bComboInput = false;
	m_bLMousePressing = false;
	m_fLMousePressingTime = 0.f;
	m_iComboIndex = 0;

	while (!m_InputQueue.empty())
		m_InputQueue.pop();
}

_bool CRole_Base::Is_PlayerControlLocked() const
{
	if (auto pUIManager = CUI_Manager::GetInstance())
	{
		if (pUIManager->Is_DachaeHwaActive())
			return true;
	}

	return false;
}

void CRole_Base::Locomotion_Input(_float _fTimeDelta) {

	if (m_bSwitchingOut)
		return;

	if (State_Check(ATTACK_01) || State_Check(ATTACK_02) ||
		State_Check(ATTACK_03) || State_Check(ATTACK_04) ||
		State_Check(ATTACK_L_LOOP) ||
		State_Check(ATTACK_AIR_START) || State_Check(EXECUTE))
		return;

	if (State_Check(SKILL_01) || State_Check(SKILL_02) ||
		State_Check(SKILL_03) || State_Check(SKILL_04) ||
		State_Check(SKILL_05) || State_Check(SKILL_06))
		return;

	if (m_ePlayMode == E_PLAY_MODE::SIDEVIEW) {
		//SideView_Lock_Direction();
		SideView_Run_Input(_fTimeDelta);
	}
	else {
		Run_Input(_fTimeDelta);
	}
}

void CRole_Base::SideView_Run_Input(_float _fTimeDelta) {
	_bool bMove = false;
	_vector vNewDirection = XMVectorSet(0.f, 0.f, 0.f, 0.f);
	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vMoveAxis = ProjectOnBasePlane(XMLoadFloat3(&m_tSideViewModeDesc.vMoveAxis), vBaseUp, m_pTransform->Get_World(STATE::LOOK));

	if (m_pGameInstance->Key_Pressed(DIK_A)) {
		vNewDirection = vMoveAxis;
		m_iInput_Z = -1;
		bMove = true;
	}
	else if (m_pGameInstance->Key_Pressed(DIK_D)) {
		vNewDirection = vMoveAxis * -1.f;
		m_iInput_Z = 1;
		bMove = true;
	}

	if (bMove) {
		_vector vMoveDir = ProjectOnBasePlane(vNewDirection, vBaseUp, vMoveAxis);
		_vector vToCenter = XMLoadFloat3(&m_tSideViewModeDesc.vCenterPosition) - m_pTransform->Get_World(STATE::POSITION);
		vToCenter = RemoveAxisComponent(vToCenter, vBaseUp);
		vToCenter = RemoveAxisComponent(vToCenter, vMoveAxis);

		_float fCenterDist = XMVectorGetX(XMVector3Length(vToCenter));
		if (fCenterDist > 0.02f) {
			_float fCenterRatio = clamp(fCenterDist * m_tSideViewModeDesc.fCenterCorrectionScale, 0.f, m_tSideViewModeDesc.fCenterCorrectionMax);
			vMoveDir = NormalizeOr(vMoveDir + vToCenter / fCenterDist * fCenterRatio, vMoveDir);
		}

		XMStoreFloat3(&m_vTargetDirection, vMoveDir);
		m_bHasInput = true;
	}
}

void CRole_Base::Run_Input(_float _fTimeDelta) {
	_bool bMove = false;
	_vector vNewDirection = XMVectorSet(0.f, 0.f, 0.f, 0.f);
	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vFallbackForward = m_pTransform->Get_World(STATE::LOOK);

	if (m_pGameInstance->Key_Pressed(DIK_W)) {
		_vector vInputDir = m_pCharacterCamera->GetTransform()->Get_World(STATE::LOOK);
		vNewDirection += ProjectOnBasePlane(vInputDir, vBaseUp, vFallbackForward);

		m_iInput_Z = 1;
		bMove = true;
	}
	if (m_pGameInstance->Key_Pressed(DIK_A)) {
		_vector vInputDir = -m_pCharacterCamera->GetTransform()->Get_World(STATE::RIGHT);
		vNewDirection += ProjectOnBasePlane(vInputDir, vBaseUp, -m_pTransform->Get_World(STATE::RIGHT));

		m_iInput_X = -1;
		bMove = true;
	}
	if (m_pGameInstance->Key_Pressed(DIK_S)) {
		_vector vInputDir = -m_pCharacterCamera->GetTransform()->Get_World(STATE::LOOK);
		vNewDirection += ProjectOnBasePlane(vInputDir, vBaseUp, -vFallbackForward);

		m_iInput_Z = -1;
		bMove = true;
	}
	if (m_pGameInstance->Key_Pressed(DIK_D)) {
		_vector vInputDir = m_pCharacterCamera->GetTransform()->Get_World(STATE::RIGHT);
		vNewDirection += ProjectOnBasePlane(vInputDir, vBaseUp, m_pTransform->Get_World(STATE::RIGHT));

		m_iInput_X = 1;
		bMove = true;
	}

	if (bMove) {
		XMStoreFloat3(&m_vTargetDirection, ProjectOnBasePlane(vNewDirection, vBaseUp, vFallbackForward));
		m_bHasInput = true;
	}
	else
	{
	}
}

void CRole_Base::Attack_Input(_float _fTimeDelta) {

	if (m_bSwitchingOut)
		return;

	if (m_pGameInstance->Mouse_Down(DI_MB::LBUTTON))
	{
		/* Air Attack */
		if (State_Check(JUMP_START) || State_Check(JUMP_LOOP))
		{
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_AIR;
			m_InputQueue.push(tEvent);
			return;
		}

		m_bLMousePressing = true;
		m_fLMousePressingTime = 0.f;
	}

	/* 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 (Air Attack 占쏙옙占쏙옙) */
	if (State_Check(JUMP_START) || State_Check(JUMP_LOOP) || State_Check(JUMP_SECOND))
		return;

	if (m_bLMousePressing && m_pGameInstance->Mouse_Pressed(DI_MB::LBUTTON))
	{
		m_fLMousePressingTime += _fTimeDelta;

		if (m_fLMousePressingTime >= m_fAttackLMaxTime)
		{
			m_bLMousePressing = false;

			// 占쏙옙占쏙옙占쏙옙
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_L;
			m_InputQueue.push(tEvent);
		}
	}

	if (m_bLMousePressing && m_pGameInstance->Mouse_Up(DI_MB::LBUTTON))
	{
		m_bLMousePressing = false;

		if (m_fLMousePressingTime >= m_fAttackLEnableTime)
		{
			// 占쏙옙占쏙옙占쏙옙
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_L;
			m_InputQueue.push(tEvent);
		}
		else
		{
			// 占싹뱄옙 占쏙옙占쏙옙 Attack 01 ~ 04 (占쏙옙占쏙옙 占쌨븝옙)
			if (m_InputQueue.size() < 3)
			{
				E_INPUT_EVENT tEvent = {};
				tEvent.eType = E_INPUT_TYPE::ATTACK;
				m_InputQueue.push(tEvent);
			}
		}
	}

	if (m_pGameInstance->Key_Down(DIK_R))
	{
		// Burst (占쏙옙占쏙옙占쌔뱄옙) 01 ~ 02
		E_INPUT_EVENT tEvent = {};
		tEvent.eType = E_INPUT_TYPE::BURST;
		m_InputQueue.push(tEvent);
	}

	if (m_pGameInstance->Key_Down(DIK_F))
	{
		// 타占싱뱄옙 占쏙옙占썹서 占쏙옙占쏙옙 1회
		E_INPUT_EVENT tEvent = {};
		tEvent.eType = E_INPUT_TYPE::EXECUTE;
		m_InputQueue.push(tEvent);
	}
}

void CRole_Base::Consume_Input()
{
	if (m_InputQueue.empty())
		return;

	auto& input = m_InputQueue.front();

	switch (input.eType)
	{
	case E_INPUT_TYPE::ATTACK:
	{
		if (!State_Check(ATTACK_01) && !State_Check(ATTACK_02) && !State_Check(ATTACK_03) && !State_Check(ATTACK_04))
		{
			m_bAttackQueued = true;
			m_fInputTimer = 0.f;
			m_InputQueue.pop();
			return;
		}
		else
		{
			_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
			if (fRatio <= m_fReserveRatio)
			{
				m_bComboInput = true;
				m_InputQueue.pop();
			}
			else if (fRatio > m_fReserveRatio && fRatio <= m_fEndRatio)
			{
				m_bComboInput = true;
				m_InputQueue.pop();
			}
			else
			{
				m_InputQueue.pop();
			}
		}
	}
	break;
	case E_INPUT_TYPE::ATTACK_AIR:
	{
		m_pStateController->ChangeAnimState(L"Attack_Air_Start", true);
		m_InputQueue.pop();
	}
	break;
	case E_INPUT_TYPE::ATTACK_L:
	{
		if (!Get_Desc().Check_Stamina(1))
		{
			m_InputQueue.pop();
			break;
		}

		Get_Desc().Use_Stamina(1);

		auto pAnimController = m_pAnimationStateMachine->Get_AnimationController();
		if (pAnimController)
		{
			m_pStateController->ChangeAnimState(L"Attack_L_Loop", true);
			pAnimController->Play();
		}
		m_InputQueue.pop();
	}
	break;
	case E_INPUT_TYPE::EXECUTE:
	{
		if (auto pEvent = m_pGameInstance->Get_EventQueue(ETOU(EVENT_QUEUE_KEY::EXECUTEABLE))) {
			if (pEvent->pDesc == nullptr)
				break;

			if (State_Check(EXECUTE) || !m_bIsMainRole) {
				m_InputQueue.pop();
				break;
			}

			if (pEvent->pDesc) {
				auto pExecuteDesc = static_cast<EXECUTE_DESC*>(pEvent->pDesc);
				m_pTargetMonster->Set(pExecuteDesc->pTargetMonster);

				m_pStateController->ChangeAnimState(L"Execute", true);
			}

			m_pGameInstance->Remove_EventQueue(ETOU(EVENT_QUEUE_KEY::EXECUTEABLE));
		}
		m_InputQueue.pop();
	}
	break;
	default: {
		m_InputQueue.pop();
	}
		   break;
	}
}

void CRole_Base::Expire_Input(_float _fTimeDelta)
{
	if (m_bAttackQueued)
	{
		m_fInputTimer += _fTimeDelta;
		if (m_fInputTimer > 0.2f)
		{
			m_bAttackQueued = false;
			m_fInputTimer = 0.f;
		}
	}
}

void CRole_Base::Interaction_Input() {
	if (m_bInputLock || !m_pGameInstance->Key_Down(DIK_T)) {
		return;
	}

	m_bHasInput = true;

	auto pGameManager = CGameManager::GetInstance();
	if (pGameManager == nullptr)
		return;

	if (auto pFireHoldDesc = m_pGameInstance->Get_EventQueue(ETOU(EVENT_QUEUE_KEY::FIRE_HOLDED))) {
		auto pDesc = static_cast<CHoliyDarkFire::FIRE_HOLDABLE_EVENT*>(pFireHoldDesc->pDesc);
		if (pDesc == nullptr || pDesc->pFire == nullptr) {
			m_pGameInstance->Remove_EventQueue(ETOU(EVENT_QUEUE_KEY::FIRE_HOLDED));
			return;
		}

		m_pInteractionObj->Set(pDesc->pFire);

		m_pStateController->ChangeAnimState(L"Throw", true);

		return;
	}

	if (m_ePlayMode == E_PLAY_MODE::SIDEVIEW) {
		Grapple_Input();
	}
}

void CRole_Base::Grapple_Input() {
	auto funcEventGraplle = [this](const EVENT_QUEUE* _pDesc) {
		auto pGrappleDesc = static_cast<EVENT_GRAPPLE_DESC*>(_pDesc->pDesc);
		if (pGrappleDesc->pGrapple) {
			m_pInteractionObj->Set(pGrappleDesc->pGrapple);

			switch (pGrappleDesc->eType) {
			case GRAPPLE_TYPE::FIXED:
				m_pStateController->ChangeAnimState(L"Fixed_Grapple", true);
				break;
			case GRAPPLE_TYPE::GAORI:
				m_pStateController->ChangeAnimState(L"Gaori_Grapple", true);
				break;
			}

			pGrappleDesc->pGrapple->Grapple_Start();
		}
	};

	if (auto pGrappleEventDesc = m_pGameInstance->Get_EventQueue(ETOU(EVENT_QUEUE_KEY::QTE_HOOK_TRIGGER_ENTER))) {
		funcEventGraplle(pGrappleEventDesc);
		return;
	}

	if (auto pGrappleEventDesc = m_pGameInstance->Get_EventQueue(ETOU(EVENT_QUEUE_KEY::GRAPPLE_FOCUSED))) {
		funcEventGraplle(pGrappleEventDesc);
		return;
	}
}

void CRole_Base::Init_Change_State_Common()
{
	/* Move */
	auto funcRun = [this]() { return Has_RunInput(); };
	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Run", funcRun, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Run", funcRun, true));

	/* Move_Limit */
	//auto funcMoveLimit = [this]() { return State_Check(MOVE_LIMIT); };
	//m_pStateController->InsertChangeStatement(L"Move", new CChangeStatement(L"Move_Limit", funcMoveLimit, true));

	/* Sprint */
	auto funcSprint = [this]() {
		return !m_bMoveLocked && Get_Desc().Check_Stamina(1)
			&& !State_Check(MOVE_LIMIT)
			&& (Has_MoveInput_Down())
			&& (Has_RunInput());
		};
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Move", funcSprint, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Move", funcSprint, true));

	auto funcRunNotMoveLimit = [this]() { return !State_Check(MOVE_LIMIT) && Has_RunInput(); };
	m_pStateController->InsertChangeStatement(L"Move", new CChangeStatement(L"Sprint", funcRunNotMoveLimit, true));
	m_pStateController->InsertChangeStatement(L"Move", new CChangeStatement(L"Run", funcRunNotMoveLimit, true));

	auto funcMoveBack = [this]() {
		return !m_bMoveLocked && Get_Desc().Check_Stamina(1) && Has_MoveInput_Down();
	};
	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Move", funcMoveBack, true));
	m_pStateController->InsertChangeStatement(L"Attack_05", new CChangeStatement(L"Move", funcMoveBack, true));

	auto funcMoveBackMoving = [this]() {
		return !m_bMoveLocked && Get_Desc().Check_Stamina(1) && Has_MoveInput_Down();
		};
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Move", funcMoveBackMoving, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Move", funcMoveBackMoving, true));

	auto funcMoveEnd = [this]() { return !m_bMoveLocked; };
	m_pStateController->InsertChangeStatement(L"Move", new CChangeStatement(L"Idle", funcMoveEnd, true, EQUAL, 0.375f));

	auto funcMoveAgain = [this]() {
		return !m_bMoveLocked && Get_Desc().Check_Stamina(1) && (Has_MoveInput_Down()); };
	m_pStateController->InsertChangeStatement(L"Sprint", new CChangeStatement(L"Move", funcMoveAgain, true));

	auto funcMoveStop = [this]() {
		return (Has_RunInput())
			&& Has_JumpInput();
		};
	m_pStateController->InsertChangeStatement(L"Sprint", new CChangeStatement(L"Idle", funcMoveStop, false));

	m_pStateController->InsertChangeStatement(L"Move_Limit", new CChangeStatement(L"Idle", funcMoveEnd, true, EQUAL, 0.25f));
	m_pStateController->InsertChangeStatement(L"Move_Limit", new CChangeStatement(L"Run", funcRun, true, EQUAL, 0.3f));

	//m_pStateController->InsertChangeStatement(L"Move_Limit", new CChangeStatement(L"Move", funcMoveBack, true, EQUAL, 0.35f));


}

void CRole_Base::Init_Change_State_Locomotion()
{
	auto funcRun = [this]() { return Has_RunInput(); };
	auto funcWalkStop = [this]() {
		_vector vDir = XMLoadFloat3(&m_vTargetDirection);
		_float fDirLength = XMVectorGetX(XMVector3Length(vDir));
		return fDirLength < 0.01f;
		};

	/* Low Priority */
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Walk", funcRun, false));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Idle", funcWalkStop, true));
}

void CRole_Base::Init_Change_State_Common_Attack()
{
	auto funcRun = [this]() { return Has_RunInput(); };

	/* Attack_Air */
	auto funcAirAttackLand = [this]() { return m_bGrounded && !Is_JumpStarting(); };
	m_pStateController->InsertChangeStatement( L"Attack_Air_Start", new CChangeStatement(L"Attack_Air_End", funcAirAttackLand, true) );

	m_pStateController->InsertChangeStatement(L"Attack_Air_End", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_Air_End", new CChangeStatement(L"Run", funcRun, true, EQUAL, 1.f));

	/* Execute */
	m_pStateController->InsertChangeStatement(L"Execute", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Execute", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.8f));
}

void CRole_Base::Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate)
{
	/* Attack */
	auto funcAttack = [this]() { return m_bAttackQueued; };
	auto funcCombo = [this](_int _iStep) {
		return [this, _iStep]() {
			if ((m_iComboIndex == _iStep) && m_bComboInput)
			{
				m_bComboInput = false;
				return true;
			}
			return false; };
		};

	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Attack_01", funcAttack, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Attack_01", funcAttack, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Attack_01", funcAttack, true));

	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Attack_02", funcCombo(1), true, EMORE, _fCombo01ExitRate));
	m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Attack_03", funcCombo(2), true, EMORE, _fCombo02ExitRate));
	m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Attack_04", funcCombo(3), true, EMORE, _fCombo03ExitRate));

	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Idle", 1.f));
}

void CRole_Base::Init_Change_State_Attack_L()
{
	/* Attack_L */
	auto funcAttack = [this]() { return m_bAttackQueued; };
	auto funcAttackL = [this]() { return !m_pGameInstance->Mouse_Pressed(DI_MB::LBUTTON); };
	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Attack_L_Loop", funcAttack, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Attack_L_Loop", funcAttack, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Attack_L_Loop", funcAttack, true));
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Attack_L_End", funcAttackL, true));
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Attack_L_End", 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Idle", 1.f));
}

void CRole_Base::Init_Change_State_Jump()
{
	/* Jump */
	auto funcRun = [this]() { return Has_RunInput(); };
	auto funcLoopRun = [this]() { return m_bGrounded && Has_RunInput() && !Is_JumpStarting(); };
	auto funcLand = [this]() { return m_bGrounded && !Is_JumpStarting(); };
	m_pStateController->InsertChangeStatement(L"Jump_Start", new CChangeStatement(L"Jump_Land", funcLand, true));
	m_pStateController->InsertChangeStatement(L"Jump_Land", new CChangeStatement(L"Idle", 0.95f));

	auto funcJump = [this]() {
		return Has_JumpInput_Down()
			&& m_bGrounded
			&& Check_JumpState()
			&& !State_Check(MOVE)
			&& !Is_JumpState()
			&& !Is_AttackState();
		};
	auto funcJumpSecond = [this]() {
		return Get_Desc().Check_Stamina(1)
			&& (State_Check(JUMP_START) || State_Check(JUMP_LOOP))
			&& (Has_MoveInput_Down());
		};

	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Jump_Start", funcJump, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Jump_Start", funcJump, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Jump_Start", funcJump, true));
	m_pStateController->InsertChangeStatement(L"Sprint", new CChangeStatement(L"Jump_Start", funcJump, true));

	m_pStateController->InsertChangeStatement(L"Slide", new CChangeStatement(L"Jump_Start", funcJump, true));
	//m_pStateController->InsertChangeStatement(L"Slide", new CChangeStatement(L"Jump_Land", [this]() { return m_pCCT != nullptr && Is_Grounded() && (m_pCCT->Get_GroundShapeHit().bHit || m_pCCT->Get_LastShapeHit().bHit) && !Check_Slidable(); }, true, EQUAL));

	m_pStateController->InsertChangeStatement(L"Jump_Start", new CChangeStatement(L"Jump_Second", funcJumpSecond, true));
	m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Jump_Second", funcJumpSecond, true));
	m_pStateController->InsertChangeStatement(L"Jump_Land", new CChangeStatement(L"Jump_Start", funcJump, true));

	auto funcSecondRun = [this]() { return m_bGrounded && Has_RunInput() && !Is_JumpStarting(); };
	m_pStateController->InsertChangeStatement(L"Jump_Second", new CChangeStatement(L"Run", funcSecondRun, true));

	auto funcSecondLand = [this]() { return m_bGrounded && !Is_JumpStarting() && GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity)) <= 0.f; };
	m_pStateController->InsertChangeStatement(L"Jump_Second", new CChangeStatement(L"Jump_Land", funcSecondLand, true, EMORE, 0.95f)); // 占쏙옙占쏙옙

	m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Slide", bind(&CRole_Base::Check_Slidable, this), true, EQUAL));
	m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Run", funcLoopRun, true));
	m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Jump_Land", funcLand, true)); //EMORE, 0.95f

	m_pStateController->InsertChangeStatement(L"Jump_Land", new CChangeStatement(L"Run", funcRun, true));

	auto funcJumpLoop = [this]() { return !m_bGrounded; }; // Is_Falling 占쏙옙占쏙옙
	m_pStateController->InsertChangeStatement(L"Jump_Start", new CChangeStatement(L"Jump_Loop", funcJumpLoop, true, EMORE, 0.5f)); // 占쏙옙占썩서 占십뱄옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 ?

	auto funcJumpSecondLoop = [this]() { return !m_bGrounded && Is_Falling(); }; // 占쏙옙占쏙옙 占쏙옙占?占식쇽옙
	m_pStateController->InsertChangeStatement(L"Jump_Second", new CChangeStatement(L"Jump_Loop", funcJumpSecondLoop, true, EMORE, 0.8f));
}

void CRole_Base::Init_Change_State_Fly()
{
	/* Fly */
	auto funcFlyStart = [this]() {
		return Has_JumpInput_Down()
			&& !m_bGrounded
			&& (State_Check(JUMP_LOOP) || State_Check(FALL) || Is_Falling());
		};
	m_pStateController->InsertChangeStatement(L"Jump_Start", new CChangeStatement(L"Fly_Start", funcFlyStart, true, EMORE, 0.5f));
	m_pStateController->InsertChangeStatement(L"Jump_Loop", new CChangeStatement(L"Fly_Start", funcFlyStart, true));

	auto funcFlyExit = [this]() { return Has_JumpInput_Down(); };

	/* 260513 추가 */
	m_pStateController->InsertChangeStatement(L"Fall", new CChangeStatement(L"Fly_Start", funcFlyStart, true, EMORE));

	m_pStateController->InsertChangeStatement(L"Fly_Start", new CChangeStatement(L"Fly_Loop", 1.f));
	m_pStateController->InsertChangeStatement(L"Fly_Loop", new CChangeStatement(L"Jump_Loop", funcFlyExit, true));
	m_pStateController->InsertChangeStatement(L"Fly_Start", new CChangeStatement(L"Jump_Loop", funcFlyExit, true));

	auto funcFlyLand = [this]() { return m_bGrounded && !Is_JumpStarting(); };
	m_pStateController->InsertChangeStatement(L"Fly_Start", new CChangeStatement(L"Jump_Land", funcFlyLand, true));
	m_pStateController->InsertChangeStatement(L"Fly_Loop", new CChangeStatement(L"Jump_Land", funcFlyLand, true));

	auto funcFlyRun = [this]() { return m_bGrounded && Has_RunInput() && !Is_JumpStarting(); };
	m_pStateController->InsertChangeStatement(L"Fly_Start", new CChangeStatement(L"Run", funcFlyRun, true));
	m_pStateController->InsertChangeStatement(L"Fly_Loop", new CChangeStatement(L"Run", funcFlyRun, true));

	auto funcFlyLoop = [this]() { return !m_bGrounded && Is_Falling() && Has_JumpInput_Down(); };
	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Jump_Start", funcFlyLoop, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Jump_Start", funcFlyLoop, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Jump_Start", funcFlyLoop, true));

	auto funcFlyReStart = [this]() { return !m_bGrounded && Has_JumpInput_Down();};
	m_pStateController->InsertChangeStatement(L"Jump_Land", new CChangeStatement(L"Fly_Start", funcFlyLoop, true));
}

void CRole_Base::Init_Change_State_Skill()
{
}

void CRole_Base::Init_Change_State_Switch_QTE()
{
	auto funcRun = [this]() { return Has_RunInput(); };

	m_pStateController->InsertChangeStatement(L"Switch_QTE", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Switch_QTE", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.7f));
}

void CRole_Base::Init_Change_State_Behit()
{
	auto funcRun = [this]() { return Has_RunInput(); };
	m_pStateController->InsertChangeStatement(L"Behit", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"Behit", new CChangeStatement(L"Run", funcRun, true));
}

void CRole_Base::Init_Disable_State()
{
	auto pDisable = AnimTransform::Create(0.f);
	pDisable->funcStarted = [this]() {
		/* 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙 占싫븝옙占싱곤옙 占쏙옙占쏙옙占?*/
		for (auto pRendererWrapper : m_listRenderer) {
			if (auto pRenderer = pRendererWrapper->Get()) {
				pRenderer->SetActive(false);
			}
		}

		Add_State(CHARACTER_STATE::DISABLE);

		Flush_Forced_Return_EffectPool();

		if (m_pCCT)
		{
			m_pCCT->Clear_PlanarVelocity();
			m_pCCT->Clear_VerticalVelocity();
			m_pCCT->Set_Tag(COLLIDER_TAG::NONE);
			m_pCCT->Set_Filter(0);
			m_pCCT->Set_UseGravity(false);
		}

		if (m_pTrigger)
			m_pTrigger->SetActive(false);

		Clear_AllInput();

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pDisable->funcUpdate = [this](_float _fTimeDelta) {
		Reset_Locomotion();
		Reset_Input(_fTimeDelta);

		Clear_AllInput();
		};
	pDisable->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::DISABLE);
		
		if (m_pCCT)
		{
			m_pCCT->Set_Tag(COLLIDER_TAG::PLAYER);
			m_pCCT->Set_Filter(
				Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::MONSTER) |
				Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::BOSS) |
				Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::STATIC)
			);
			m_pCCT->Set_UseGravity(true);
		}

		if (m_pTrigger)
			m_pTrigger->SetActive(true);

		/* refactor : 복구 */
		this->m_bSwitchingOut = false;
		this->m_bSwitchingIn = false;
		this->m_bDitheringStarted = false;
		this->m_vDitheringTimer.x = 0.f;
		this->Set_Disslove(0.f);
		};

	m_pAnimator->PushAnimInfo(L"Disable", pDisable);
}

void CRole_Base::Init_Idle_State() {
	auto pIdle = AnimTransform::Create(0.f); // 0.f 占쏙옙 占쏙옙占?
	pIdle->funcStarted = [this]() {
		Add_State(IDLE);

		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();
		Reset_ControlCameraSettings();
		RemoveNonYawRotationOnBaseUp(m_pTransform);

		m_fRootMotionLimitDistance = m_fDefaultRootMotionLimitDistance;

		};
	pIdle->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_AirMotion(_fTimeDelta);
		};
	pIdle->funcEnded = [this]() {
		Remove_State(IDLE);
		};

	m_pAnimator->PushAnimInfo(L"Idle", pIdle);

	m_pStateController->ChangeAnimState(L"Idle");
}

void CRole_Base::Init_Run_State() {
	auto pRun = AnimTransform::Create(0.f);
	pRun->funcStarted = [this]() {
		Add_State(RUN);

		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		m_fWeaponVisualizeDurtaion = 1.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pRun->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_Locomotion(_fTimeDelta);
		Turn(_fTimeDelta * 2.f);
		Update_AirMotion(_fTimeDelta);
		};
	pRun->funcEnded = [this]() {
		Remove_State(RUN);
		};

	m_pAnimator->PushAnimInfo(L"Run", pRun);
}

void CRole_Base::Init_Walk_State()
{
	auto pWalk = AnimTransform::Create(0.f);
	pWalk->funcStarted = [this]() {
		Add_State(WALK);

		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		m_fWeaponVisualizeDurtaion = 1.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pWalk->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_Locomotion(_fTimeDelta);
		Update_RemainLocomotion(_fTimeDelta);
		Turn(_fTimeDelta * 1.f);
		Update_AirMotion(_fTimeDelta);
		};
	pWalk->funcEnded = [this]() {
		Remove_State(WALK);
		};

	m_pAnimator->PushAnimInfo(L"Walk", pWalk);
}

void CRole_Base::Init_Sprint_State()
{
	auto pWalk = AnimTransform::Create(0.f);
	pWalk->funcStarted = [this]() {
		Add_State(SPRINT);

		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		m_fWeaponVisualizeDurtaion = 4.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });

		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 14.f, 0.5f });
		};
	pWalk->funcUpdate = [this](_float _fTimeDelta) {
		if (!Get_Desc().Use_Stamina(5.f * _fTimeDelta))
		{
			m_pStateController->ChangeAnimState(L"Run", true);
			return;
		}
		Interaction_Input();

		Update_Locomotion(_fTimeDelta);
		Turn(_fTimeDelta * 2.f);
		Update_AirMotion(_fTimeDelta);
		};
	pWalk->funcEnded = [this]() {
		Remove_State(SPRINT);
		};

	m_pAnimator->PushAnimInfo(L"Sprint", pWalk);
}

void CRole_Base::Init_Move_State()
{
	auto pMove = AnimTransform::Create(1.333f);
	pMove->funcStarted = [this]() {
		Add_State(MOVE);

		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		m_fAutoTurnDuration = 0.f;

		m_bIFrames = true;
		m_fIFramesTimer.x = m_fIFramesTimer.y;

		if (Get_Desc().Use_Stamina(1))
		{
			Lock_Move(m_fMoveCoolDown);
		}
		};
	pMove->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		};
	pMove->funcEnded = [this]() {
		Remove_State(MOVE);
		};

	m_pAnimator->PushAnimInfo(L"Move", pMove);
}

void CRole_Base::Init_Move_Limit_State()
{
	auto pMoveLimit = AnimTransform::Create(1.5f);
	pMoveLimit->funcStarted = [this]() {
		Add_State(MOVE_LIMIT);
		
		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		Reset_Locomotion();

		Get_Desc().fStamina += 20.f;

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });

		if (auto pGameManager = CGameManager::GetInstance()) {
			if (auto pEffectPrefab = pGameManager->Get_CommonEffect(COMMON_EFFECT::EVADE)) {
				if (auto pPerfectEvadeEffect = pEffectPrefab->Instantiate_Prefab()) {
					if (auto pMiddleCase = m_pMiddleCase->Get()) { 
						pPerfectEvadeEffect->GetTransform()->Set_Parent(pMiddleCase->GetTransform());;
					}
					else {
						pPerfectEvadeEffect->GetTransform()->Set_Parent(m_pTransform);
					}
					pPerfectEvadeEffect->GetTransform()->Set_State(STATE::POSITION, XMVectorSet(0.f, 0.f, 0.f, 1.f));
				}
			}
		}

		Apply_DeltaAnim(E_EASING::SMOOTHSTEP, E_EASING::SMOOTHSTEP, 0.3f, 0.12f, 0.12f, 0.12f, true, true);

		};
	pMoveLimit->funcUpdate = [this, pMoveLimit](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		
		};
	pMoveLimit->funcEnded = [this]() {
		Remove_State(MOVE_LIMIT);

		Release_Move_Lock();
		Clear_AllInput();
		Reset_Input(0.f);
		};

	m_pAnimator->PushAnimInfo(L"Move_Limit", pMoveLimit);
}

void CRole_Base::Init_Jump_Start_State()
{
	auto pJump = AnimTransform::Create(0.f);
	pJump->funcStarted = [this]() {
		Add_State(JUMP_START);

		Flush_Forced_Return_EffectPool();

		m_bGrounded = false;
		m_iGroundContect = 0;

		Set_JumpVelocity_Y(m_fJumpForce);
		Apply_JumpVelocity(0.f);

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		
		m_pAnimationController->Set_SubRootMotion_Y(false);

		};
	pJump->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_AirMotion(_fTimeDelta);
		Update_Locomotion(_fTimeDelta);
		Turn(_fTimeDelta);
		};
	pJump->funcEnded = [this]() {
		Remove_State(JUMP_START);
		};

	m_pAnimator->PushAnimInfo(L"Jump_Start", pJump);
}

void CRole_Base::Init_Jump_Loop_State()
{
	auto pLoop = AnimTransform::Create(0.f);
	pLoop->funcStarted = [this]() {
		Add_State(JUMP_LOOP);

		Flush_Forced_Return_EffectPool();

		m_fJumpGravityScale = 2.8f;

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pLoop->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_AirMotion(_fTimeDelta);
		};
	pLoop->funcEnded = [this]() {
		Remove_State(JUMP_LOOP);
		};
	m_pAnimator->PushAnimInfo(L"Jump_Loop", pLoop);
}

void CRole_Base::Init_Jump_Land_State()
{
	auto pLand = AnimTransform::Create(0.75f);
	pLand->funcStarted = [this]() {
		Add_State(JUMP_LAND);
		Reset_Locomotion();

		Flush_Forced_Return_EffectPool();

		m_fJumpGravityScale = 2.8f;
		m_fLandLockTime = m_fLandLockDuration;
		
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pLand->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_AirMotion(_fTimeDelta);
		Update_Locomotion(_fTimeDelta);
		};
	pLand->funcEnded = [this]() {
		Remove_State(JUMP_LAND);
		Reset_Gravity();
		};

	m_pAnimator->PushAnimInfo(L"Jump_Land", pLand);
}

void CRole_Base::Init_Jump_Second_State()
{
	auto pjump = AnimTransform::Create(0.75f);
	pjump->funcStarted = [this]() {
		Add_State(JUMP_SECOND);

		Flush_Forced_Return_EffectPool();

		Get_Desc().Use_Stamina(1);

		m_bGrounded = false;
		m_iGroundContect = 0;

		if (GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity)) < 0.f)
			XMStoreFloat3(&m_vCharacterVelocity, SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity), 0.f));

		Add_JumpVelocity({ 0.f, 5.f, 0.f });

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });

		if (auto pGameManager = CGameManager::GetInstance()) {
			if (auto pCommonEff = pGameManager->Get_CommonEffect(COMMON_EFFECT::SECOND_JUMP)) {
				if (auto pInst = pCommonEff->Instantiate_Prefab()) {
					if (auto pTransform = pInst->GetTransform()) {
						//pTransform->Set_State(STATE::POSITION, m_pTransform->Get_World(STATE::POSITION));

						_vector vBaseUp = GetTransformBaseUp(m_pTransform);
						_vector vLook = ProjectOnBasePlane(
							m_pTransform->Get_World(STATE::LOOK),
							vBaseUp,
							XMVectorSet(0.f, 0.f, 1.f, 0.f)
						);

						_vector vRight = XMVector3Normalize(XMVector3Cross(vBaseUp, vLook));

						_vector vSpawnPos =
							m_pTransform->Get_World(STATE::POSITION)
							+ vLook * 2.f      
							+ vRight * 0.0f     
							+ vBaseUp * 1.f;   

						pTransform->Set_State(STATE::POSITION, vSpawnPos);
						 vBaseUp = GetTransformBaseUp(m_pTransform);
						 vLook = ProjectOnBasePlane(
							m_pTransform->Get_World(STATE::LOOK),
							vBaseUp,
							XMVectorSet(0.f, 0.f, 1.f, 0.f)
						);

						 vRight = XMVector3Normalize(XMVector3Cross(vBaseUp, vLook));
						 vBaseUp = XMVector3Normalize(XMVector3Cross(vLook, vRight));

						_matrix matLook(
							XMVectorSetW(vRight, 0.f),
							XMVectorSetW(vBaseUp, 0.f),
							XMVectorSetW(vLook, 0.f),
							XMVectorSet(0.f, 0.f, 0.f, 1.f)
						);

						 vLook = XMQuaternionNormalize(XMQuaternionRotationMatrix(matLook));
						pTransform->Set_Rotation_Quaternion(vLook);
					}
				}
			}
		}

		};
	pjump->funcUpdate = [this](_float _fTimeDelta) {
		Interaction_Input();
		Update_AirMotion(_fTimeDelta);
		Update_Locomotion(_fTimeDelta);

		};
	pjump->funcEnded = [this]() {
		Remove_State(JUMP_SECOND);
		};

	m_pAnimator->PushAnimInfo(L"Jump_Second", pjump);
}

void CRole_Base::Init_Fly_Start_State()
{
	auto pFly = AnimTransform::Create(0.f);
	pFly->funcStarted = [this]() {
		Add_State(FLY_START);

		Flush_Forced_Return_EffectPool();

		if (auto pParaglider = m_pParaglider ? m_pParaglider->Get() : nullptr)
		{
			if (auto* pWeapon = pParaglider->As<CWeapon_Paraglider>())
				pWeapon->Play_WeaponAnim(0);
		}

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
	};
 	pFly->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		Turn(_fTimeDelta * 0.15f);
		Update_Locomotion(_fTimeDelta);
		};
	pFly->funcEnded = [this]() {
		Remove_State(FLY_START);
		};

	m_pAnimator->PushAnimInfo(L"Fly_Start", pFly);
}

void CRole_Base::Init_Fly_Loop_State()
{
	auto pFly = AnimTransform::Create(0.f);
	pFly->funcStarted = [this]() {
		Add_State(FLY_LOOP);

		Flush_Forced_Return_EffectPool();

		m_fJumpGravityScale = 0.6f;

		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
	};
	pFly->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		Turn(_fTimeDelta * 0.15f);
		Update_Locomotion(_fTimeDelta);
		};
	pFly->funcEnded = [this]() {
		Remove_State(FLY_LOOP);

		if (auto pParaglider = m_pParaglider ? m_pParaglider->Get() : nullptr)
		{
			if (auto pWeapon = pParaglider->As<CWeapon_Paraglider>())
				pWeapon->Play_WeaponAnim(2);
		}
	};

	m_pAnimator->PushAnimInfo(L"Fly_Loop", pFly);
}

_bool CRole_Base::Check_Grapple_Done() {
	auto pGrappleObj = m_pGrappleObj->Get();
	auto pInteractionObj = m_pInteractionObj->Get();

	if (pGrappleObj && pInteractionObj && m_bGrappling) {
		_vector vDiff = m_pTransform->Get_World(STATE::POSITION) - pInteractionObj->GetTransform()->Get_World(STATE::POSITION);
		_float fLen = XMVectorGetX(XMVector3Length(vDiff));

		if (fLen < 1.f) {
			Debug_Output("가까워져서 종료\n");
			Exit_Grappling();

			return true;
		}
	}
	else {
		Debug_Output("객체가 죽어있어서 종료\n");
		Exit_Grappling();

		return true;
	}

	return false;
}

void CRole_Base::Init_Hook_State() {

	{
		auto pFixedHook = AnimTransform::Create(0.625f);
		pFixedHook->funcStarted = [this]() {
			Set_State(CHARACTER_STATE::GRAPPLE);
			m_bGrappling = true;
			m_bInputLock = true;
			m_pCCT->Set_UseGravity(false);

			m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
			m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });

			Reset_Locomotion();
			Flush_Forced_Return_EffectPool();
			};
		pFixedHook->funcUpdate = [this](_float _fDT) {
			};
		pFixedHook->AddFuncTable(new tagFunctionTable(0.55f, [this]() {
			if (auto pObj = m_pGrappleObj->Get()) {
				pObj->GetTransform()->Destroy_Self_And_Child();
			}

			if (auto pGameManager = CGameManager::GetInstance()) {
				if (auto pInteractionObj = m_pInteractionObj->Get()) {
					auto pPrefab = pGameManager->Get_CommonEffect(COMMON_EFFECT::GRAPPLE);
					if (pPrefab == nullptr)
						return;

					auto pInst = pPrefab->Instantiate_Prefab();
					if (pInst == nullptr)
						return;

					pInst->Start_If_Not_Initialized();

					if (auto pEffectInst = pInst->As<CHook_Effect>()) {
						pEffectInst->GetTransform()->Set_State(STATE::POSITION, XMVectorSet(0.f, 0.f, 0.f, 1.f));

						if (auto pHandBone = m_pHandBone->Get()) {
							pEffectInst->SetUp_Hook(this, pInteractionObj->GetTransform(), pHandBone->GetTransform());
						}
						else {
							pEffectInst->SetUp_Hook(this, pInteractionObj->GetTransform(), m_pTransform);
						}
						m_pGrappleObj->Set(pEffectInst);
					}
				}
			}
			}));
		m_pAnimator->PushAnimInfo(L"Fixed_Grapple", pFixedHook);

		auto pFixedHookLoop = AnimTransform::Create(0.75f);
		pFixedHookLoop->funcGraph = &WEasing::OutCubic;
		pFixedHookLoop->funcStarted = [this]() {
		};

		pFixedHookLoop->AddProperty(new AnimAddProperty<_float3>(
			[this](const _float3& _vPos) {
				m_pCCT->Add_Move(_vPos);
			},
			[this](_float3& _s, _float3& _e) {
				_s = { 0.f, 0.f, 0.f };
				_e = { 0.f, 0.f, 0.f };

				auto pInteractionObj = m_pInteractionObj->Get();
				if (!pInteractionObj) {
					Exit_Grappling();
					return;
				}

				_vector vDiff = (pInteractionObj->GetTransform()->Get_World(STATE::POSITION) - m_pTransform->Get_World(STATE::POSITION));
				XMStoreFloat3(&m_vGrappleDir, XMVector3Normalize(vDiff));

				_s = { 0.f, 0.f, 0.f };
				XMStoreFloat3(&_e, vDiff);
			}
		));

		pFixedHookLoop->funcUpdate = [this](_float) {
			Check_Grapple_Done();
		};

		pFixedHookLoop->funcEnded = [this]() {
			Remove_State(CHARACTER_STATE::GRAPPLE);

			Exit_Grappling();

			m_bInputLock = false;
			m_pCCT->Set_UseGravity(true);
			};

		m_pAnimator->PushAnimInfo(L"Fixed_Grapple", pFixedHookLoop);

		m_pStateController->InsertChangeStatement(L"Fixed_Grapple", new CChangeStatement(L"GrappleEnd", [this]() { return !Is_Grappling(); }, true, EQUAL));
	}

	{
		auto pGaoriHook = AnimTransform::Create(0.625f);
		pGaoriHook->funcStarted = [this]() {
			Set_State(CHARACTER_STATE::GRAPPLE);
			m_bGrappling = true;
			m_bInputLock = true;
			m_pCCT->Set_UseGravity(false);

			m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
			m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });

			Reset_Locomotion();
		};
		pGaoriHook->funcUpdate = [this](_float _fDT) {
		};
		pGaoriHook->AddFuncTable(new tagFunctionTable(0.55f, [this]() {
			if (auto pObj = m_pGrappleObj->Get()) {
				pObj->GetTransform()->Destroy_Self_And_Child();
			}

			if (auto pGameManager = CGameManager::GetInstance()) {
				if (auto pInteractionObj = m_pInteractionObj->Get()) {
					auto pPrefab = pGameManager->Get_CommonEffect(COMMON_EFFECT::GRAPPLE);
					if (pPrefab == nullptr)
						return;

					auto pInst = pPrefab->Instantiate_Prefab();
					if (pInst == nullptr)
						return;

					pInst->Start_If_Not_Initialized();

					if (auto pEffectInst = pInst->As<CHook_Effect>()) {
						pEffectInst->GetTransform()->Set_State(STATE::POSITION, XMVectorSet(0.f, 0.f, 0.f, 1.f));

						if (auto pHandBone = m_pHandBone->Get()) {
							pEffectInst->SetUp_Hook(this, pInteractionObj->GetTransform(), pHandBone->GetTransform());
						}
						else {
							pEffectInst->SetUp_Hook(this, pInteractionObj->GetTransform(), m_pTransform);
						}
						m_pGrappleObj->Set(pEffectInst);
					}

				}
			}
		}));
		m_pAnimator->PushAnimInfo(L"Gaori_Grapple", pGaoriHook);

		auto pGaoriHookLoop = AnimTransform::Create(0.f);
		pGaoriHookLoop->funcStarted = [this]() {
			auto pInteractionObj = m_pInteractionObj->Get();
			if (!pInteractionObj || pInteractionObj->GetTransform() == nullptr) {
				Exit_Grappling();
				return;
			}

			_vector vAnchorPos = pInteractionObj->GetTransform()->Get_World(STATE::POSITION);
			_vector vPlayerPos = m_pTransform->Get_World(STATE::POSITION);
			_vector vDiff = vAnchorPos - vPlayerPos;

			constexpr _float fDefaultGaoriRopeLength = 3.65f;
			m_fGaoriHookRopeLength = fDefaultGaoriRopeLength;

			XMStoreFloat3(&m_vGaoriHookPrevAnchor, vAnchorPos);
			XMStoreFloat3(&m_vGrappleDir, NormalizeOr(vDiff, XMLoadFloat3(&m_vGrappleDir)));

			m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
			m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
		};

		pGaoriHookLoop->funcUpdate = [this, pGaoriHookLoop](_float _fTimeDelta) {
			if (_fTimeDelta <= 0.f || m_pCCT == nullptr) {
				return;
			}

			if (m_pGameInstance->Key_Down(DIK_T) || m_pGameInstance->Key_Down(DIK_SPACE)) {
				Exit_Grappling();
				return;
			}

			auto pInteractionObj = m_pInteractionObj->Get();
			if (!pInteractionObj || pInteractionObj->GetTransform() == nullptr) {
				Exit_Grappling();
				return;
			}

			constexpr _float fStartBlendTime = 0.18f;
			constexpr _float fRopeLength = 3.65f;
			constexpr _float fMaxAnchorCarrySpeed = 14.f;
			constexpr _float fBackSwingDistance = 1.15f;
			constexpr _float fHangBias = 0.95f;
			constexpr _float fSwingStiffness = 4.25f;
			constexpr _float fSwingDamping = 0.985f;
			constexpr _float fTangentialGravityScale = 0.72f;
			constexpr _float fMaxRelativeSpeed = 12.f;
			constexpr _float fMaxRopeCorrectionSpeed = 18.f;
			constexpr _float fMinMoveDirSpeedSq = 0.01f;

			m_fGaoriHookRopeLength = fRopeLength;

			_vector vAnchorPos = pInteractionObj->GetTransform()->Get_World(STATE::POSITION);
			_vector vPlayerPos = m_pTransform->Get_World(STATE::POSITION);
			_vector vPrevAnchorPos = XMLoadFloat3(&m_vGaoriHookPrevAnchor);
			_vector vAnchorVelocity = (vAnchorPos - vPrevAnchorPos) / max(_fTimeDelta, 0.0001f);
			_float fAnchorSpeedSq = XMVectorGetX(XMVector3LengthSq(vAnchorVelocity));
			if (fAnchorSpeedSq > fMaxAnchorCarrySpeed * fMaxAnchorCarrySpeed) {
				vAnchorVelocity = XMVector3Normalize(vAnchorVelocity) * fMaxAnchorCarrySpeed;
				fAnchorSpeedSq = fMaxAnchorCarrySpeed * fMaxAnchorCarrySpeed;
			}

			_float fStartRamp = min(pGaoriHookLoop->fEllipsed / fStartBlendTime, 1.f);
			_vector vBaseUp = GetTransformBaseUp(m_pTransform);
			_vector vPlayerVelocity = m_pCCT->Get_XMVelocity();
			_vector vRelativeVelocity = vPlayerVelocity - vAnchorVelocity;
			_vector vAnchorToPlayer = vPlayerPos - vAnchorPos;
			_vector vRopeDir = NormalizeOr(vAnchorToPlayer, -XMLoadFloat3(&m_vGrappleDir));
			_float fDistSq = XMVectorGetX(XMVector3LengthSq(vAnchorToPlayer));
			_float fDist = fDistSq > ROLE_BASE_EPSILON ? sqrtf(fDistSq) : 0.f;

			_vector vPrevTailDir = ProjectOnBasePlane(vRopeDir, vBaseUp, -pInteractionObj->GetTransform()->Get_World(STATE::LOOK));
			_vector vTailDir = fAnchorSpeedSq > fMinMoveDirSpeedSq ? ProjectOnBasePlane(-vAnchorVelocity, vBaseUp, vPrevTailDir) : vPrevTailDir;
			_vector vDesiredRopeDir = NormalizeOr(vTailDir * fBackSwingDistance - vBaseUp * fHangBias, -vBaseUp);
			_vector vDesiredPlayerPos = vAnchorPos + vDesiredRopeDir * fRopeLength;
			_vector vToDesired = vDesiredPlayerPos - vPlayerPos;

			_vector vGravity = -vBaseUp * (m_fGravity * fTangentialGravityScale);
			vRelativeVelocity += RemoveAxisComponent(vGravity, vRopeDir) * _fTimeDelta;
			vRelativeVelocity += vToDesired * (fSwingStiffness * _fTimeDelta * fStartRamp);
			vRelativeVelocity *= powf(fSwingDamping, _fTimeDelta * 60.f);

			_vector vFrameCorrection = XMVectorZero();
			if (fDist > ROLE_BASE_EPSILON) {
				_float fRopeError = fDist - fRopeLength;
				_float fMaxFrameCorrection = fMaxRopeCorrectionSpeed * fStartRamp * _fTimeDelta;
				_float fCorrection = min((fRopeError >= 0.f) ? fRopeError : -fRopeError, fMaxFrameCorrection);

				if (fCorrection > 0.f) {
					vFrameCorrection -= vRopeDir * ((fRopeError > 0.f) ? fCorrection : -fCorrection);
				}

				_float fRadialSpeed = XMVectorGetX(XMVector3Dot(vRelativeVelocity, vRopeDir));
				vRelativeVelocity -= vRopeDir * (fRadialSpeed * 0.85f);
			}

			_float fRelativeSpeedSq = XMVectorGetX(XMVector3LengthSq(vRelativeVelocity));
			if (fRelativeSpeedSq > fMaxRelativeSpeed * fMaxRelativeSpeed)
				vRelativeVelocity = XMVector3Normalize(vRelativeVelocity) * fMaxRelativeSpeed;

			if (XMVectorGetX(XMVector3LengthSq(vFrameCorrection)) > ROLE_BASE_EPSILON)
				m_pCCT->Add_Move(vFrameCorrection);

			_vector vNewVelocity = vRelativeVelocity + vAnchorVelocity;
			XMStoreFloat3(&m_vGaoriHookPrevAnchor, vAnchorPos);
			XMStoreFloat3(&m_vGrappleDir, NormalizeOr(vAnchorPos - vPlayerPos, XMLoadFloat3(&m_vGrappleDir)));
			XMStoreFloat3(&m_vCharacterVelocity, vNewVelocity);
			m_pCCT->Set_Velocity(vNewVelocity);
		};

		pGaoriHookLoop->funcEnded = [this]() {
			Remove_State(CHARACTER_STATE::GRAPPLE);

			Exit_Grappling();

			m_bInputLock = false;
			m_pCCT->Set_UseGravity(true);
		};

		m_pAnimator->PushAnimInfo(L"Gaori_Grapple", pGaoriHookLoop);

		m_pStateController->InsertChangeStatement(L"Gaori_Grapple", new CChangeStatement(L"GrappleEnd", [this]() { return !Is_Grappling(); }, true, EQUAL));
	}

	auto pHookEnd = AnimTransform::Create(0.35f);
	pHookEnd->funcGraph = &WEasing::OutSine;
	pHookEnd->funcStarted = [this]() {
		m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
	};
	pHookEnd->AddProperty(new AnimAddProperty<_float3>(
		[this](const _float3& _vPos) {
			m_pCCT->Add_Move(_vPos);
		},
		[this](_float3& _s, _float3& _e) {
			_s = { 0.f, 0.f, 0.f };

			_vector vDiff = XMLoadFloat3(&m_vGrappleDir);
			_vector vBaseUp = XMLoadFloat3(&m_pTransform->Get_BaseUp());
			XMStoreFloat3(&_e, vDiff * 2.f + vBaseUp * 0.5f);
		}
	));

	pHookEnd->funcEnded = [this]() {
		m_pCCT->Set_UseGravity(true);
	};
	m_pAnimator->PushAnimInfo(L"GrappleEnd", pHookEnd);

	//auto pHookStart = AnimTransform::Create(1.875f);

	m_pStateController->InsertChangeStatement(L"GrappleEnd", new CChangeStatement(L"Fall", [this]() { return !Is_Grounded(); }, true, EQUAL, 1.f));
	m_pStateController->InsertChangeStatement(L"GrappleEnd", new CChangeStatement(L"Sprint", [this]() { return Is_Grounded() && m_bHasInput; }, true, EQUAL, 1.f));
	m_pStateController->InsertChangeStatement(L"GrappleEnd", new CChangeStatement(L"Idle", 1.f));
}

void CRole_Base::Init_Fall_State() {
	auto pFall = AnimTransform::Create(0.f);
	pFall->funcStarted = [this]() {
		Set_State(CHARACTER_STATE::FALL);
		m_pCCT->Set_UseGravity(true);
		Flush_Forced_Return_EffectPool();
	};
	pFall->funcUpdate = [this](_float _fTimeDelta) {
		Turn(_fTimeDelta * 3.f);
	};
	pFall->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::FALL);
	};

	m_pAnimator->PushAnimInfo(L"Fall", pFall);

	// 슬라이딩
	m_pStateController->InsertChangeStatement(L"Fall", new CChangeStatement(L"Slide", bind(&CRole_Base::Check_Slidable, this), true, EQUAL));
	m_pStateController->InsertChangeStatement(L"Fall", new CChangeStatement(L"Jump_Land", [this]() { return Is_Grounded(); }, true, EQUAL));

	auto pSlide = AnimTransform::Create(0.f);
	pSlide->funcStarted = [this]() {
		Set_State(CHARACTER_STATE::SLIDE);

		m_vSlideVelocity = _float3{ 0.f, 0.f, 0.f };
		m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };

		if (m_pCCT) {
			const auto& tGroundHit = m_pCCT->Get_GroundShapeHit();
			const auto& tHit = tGroundHit.bHit ? tGroundHit : m_pCCT->Get_LastShapeHit();
			_vector vInitialVelocity = m_pCCT->Get_XMVelocity();

			_vector vSlideNormal = tHit.bHit ? NormalizeOr(XMLoadFloat3(&tHit.vNormal), GetTransformBaseUp(m_pTransform)) : NormalizeOr(XMLoadFloat3(&m_vSlideNormal), GetTransformBaseUp(m_pTransform));
			vInitialVelocity -= vSlideNormal * XMVectorGetX(XMVector3Dot(vInitialVelocity, vSlideNormal));
			XMStoreFloat3(&m_vSlideNormal, vSlideNormal);

			XMStoreFloat3(&m_vSlideVelocity, vInitialVelocity);
			XMStoreFloat3(&m_vCharacterVelocity, vInitialVelocity);

			m_pCCT->Set_UseGravity(false);
			m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
			m_pCCT->Set_StepOffset(0.f);
			m_pCCT->Set_ContactOffset(0.08f);
			m_pCCT->Set_SlopeLimit(45.f);
		}
	};
	pSlide->funcUpdate = [this](_float _fTimeDelta) {
		if (_fTimeDelta <= 0.f || m_pCCT == nullptr)
			return;

		const auto& tGroundHit = m_pCCT->Get_GroundShapeHit();
		const auto& tHit = tGroundHit.bHit ? tGroundHit : m_pCCT->Get_LastShapeHit();

		constexpr _float fSlideGravity = 120.f;
		constexpr _float fMinSlideSpeed = 3.5f;
		constexpr _float fMaxSlideSpeed = 14.f;
		constexpr _float fSlideFriction = 0.08f;
		constexpr _float fGroundStickSpeed = 1.25f;
		constexpr _float fSlideEpsilon = 1e-6f;

		_vector vUp = GetTransformBaseUp(m_pTransform);
		_vector vNormal = tHit.bHit ? NormalizeOr(XMLoadFloat3(&tHit.vNormal), vUp) : NormalizeOr(XMLoadFloat3(&m_vSlideNormal), vUp);
		XMStoreFloat3(&m_vSlideNormal, vNormal);
		_vector vGravityAccel = -vUp * fSlideGravity;
		_vector vSlideAccel = vGravityAccel - vNormal * XMVectorGetX(XMVector3Dot(vGravityAccel, vNormal));
		_float fAccelLenSq = XMVectorGetX(XMVector3LengthSq(vSlideAccel));

		if (fAccelLenSq <= fSlideEpsilon) {
			m_vSlideVelocity = _float3{ 0.f, 0.f, 0.f };
			m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
			m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });
			return;
		}

		_vector vSlideDir = XMVector3Normalize(vSlideAccel);
		_vector vSlideVelocity = XMLoadFloat3(&m_vSlideVelocity);
		vSlideVelocity -= vNormal * XMVectorGetX(XMVector3Dot(vSlideVelocity, vNormal));
		vSlideVelocity += vSlideAccel * _fTimeDelta;
		vSlideVelocity *= expf(-fSlideFriction * _fTimeDelta);
		vSlideVelocity -= vNormal * XMVectorGetX(XMVector3Dot(vSlideVelocity, vNormal));

		_float fAlongSpeed = XMVectorGetX(XMVector3Dot(vSlideVelocity, vSlideDir));
		if (fAlongSpeed < fMinSlideSpeed)
			vSlideVelocity += vSlideDir * (fMinSlideSpeed - fAlongSpeed);

		_float fSpeedSq = XMVectorGetX(XMVector3LengthSq(vSlideVelocity));
		if (fSpeedSq > fMaxSlideSpeed * fMaxSlideSpeed)
			vSlideVelocity = XMVector3Normalize(vSlideVelocity) * fMaxSlideSpeed;

		XMStoreFloat3(&m_vSlideVelocity, vSlideVelocity);
		XMStoreFloat3(&m_vCharacterVelocity, vSlideVelocity);

		_vector vStickMove = -vNormal * (fGroundStickSpeed * _fTimeDelta);
		m_pCCT->Add_Move(vStickMove);
		m_pCCT->Set_Velocity(vSlideVelocity);
	};
	pSlide->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::SLIDE);

		if (m_pCCT) {
			m_pCCT->Set_UseGravity(true);
			m_pCCT->Set_StepOffset(0.35f);
			m_pCCT->Set_ContactOffset(0.05f);
			m_pCCT->Set_SlopeLimit(45.f);
			m_pCCT->Set_Velocity(_float3(0.f, 0.f, 0.f));
		}

		m_vSlideVelocity = _float3{ 0.f, 0.f, 0.f };
	};

	// 슬라이딩 하다가 점프 / 완만해지면 착지 / 발이 뜨면 점프 루프

	m_pAnimator->PushAnimInfo(L"Slide", pSlide);

	//m_pStateController->InsertChangeStatement(L"Slide", new CChangeStatement(L"Jump_Land", [this]() { return Is_Grounded(); }, true, EQUAL));
	m_pStateController->InsertChangeStatement(L"Slide", new CChangeStatement(L"Fall", [this]() { return m_pCCT == nullptr || (!Is_Grounded() && !m_pCCT->Get_GroundShapeHit().bHit); }, true, EQUAL));

}

void CRole_Base::Init_Throw_State() {
	auto pThrowWait = AnimTransform::Create(0.333f);
	pThrowWait->funcStarted = [this]() {
		Reset_Locomotion();

		Flush_Forced_Return_EffectPool();

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant(false);
		Set_Scabbard_Visual_Instant(false);
		Set_State(THROW);
	};
	m_pAnimator->PushAnimInfo(L"Throw", pThrowWait);

	auto pThrow = AnimTransform::Create(1.8f - 0.333f);
	pThrow->funcStarted = [this]() {
		Remove_State(THROW);
		if (m_pGameInstance->Get_EventQueue(ETOU(EVENT_QUEUE_KEY::FIRE_LAUNCH)) == nullptr) {
			EVENT_QUEUE tEvent{};
			tEvent.fRemainTime = 0.15f;
			tEvent.pToken = Get_Token();
			tEvent.pDesc = new CHoliyDarkFire::FIRE_THROW_EVENT{ this };
			m_pGameInstance->Add_EventQueue(ETOU(EVENT_QUEUE_KEY::FIRE_LAUNCH), tEvent);
		}
	};
	m_pAnimator->PushAnimInfo(L"Throw", pThrow);

	m_pStateController->InsertChangeStatement(L"Throw", new CChangeStatement(L"Idle", 1.f));
}

void CRole_Base::Init_Sit_State() {
	auto pSit = AnimTransform::Create(0.f);
	pSit->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		m_bInputLock = true;
		Clear_AllInput();
		m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };

		if (auto pVehicle = m_pVehicle->Get()) {
			m_pTransform->Set_Parent(pVehicle);
			m_pTransform->Set_LocalMatrix(XMMatrixIdentity());
		}
		Set_State(CHARACTER_STATE::SIT);
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 25.f, 0.5f });
	};
	pSit->funcEnded = [this]() {
		m_bInputLock = false;
		Remove_State(CHARACTER_STATE::SIT);
		m_pCharacterCamera->Set_CameraArmLength(4.f);
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 26.f, 0.5f });
	};

	m_pAnimator->PushAnimInfo(L"Sit", pSit);
}

void CRole_Base::Init_Attack01_State(_float _fEndTime) {
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_01);
		m_iComboIndex = 1;
		m_bAttackQueued = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio > m_fEndRatio)
		{
			if (m_bComboInput)
			{
				m_pStateController->ChangeAnimState(L"Attack_02", true);
			}
			else
			{
				auto pAnimController = m_pAnimationStateMachine->Get_AnimationController();
				if (pAnimController)
				{
					m_pStateController->ChangeAnimState(L"Attack_01", true);
					pAnimController->Play();
				}
			}
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_01);
		m_iComboIndex = 0;
		m_bComboInput = false;

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_01", pAttack);
}

void CRole_Base::Init_Attack02_State(_float _fEndTime)
{
	/* Switch Out (Common Attack) */
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_02);
		m_iComboIndex = 2;
		m_bAttackQueued = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		
		m_fWeaponVisualizeDurtaion = 1.f;
		Set_Weapon_Visual_Instant({ 1 });
		Set_Scabbard_Visual_Instant({ 1 });
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio > m_fEndRatio && m_bComboInput)
		{
			m_pStateController->ChangeAnimState(L"Attack_03", true);
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_02);
		m_iComboIndex = 0;
		m_bComboInput = false;

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_02", pAttack);
}

void CRole_Base::Init_Attack03_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_03);
		m_iComboIndex = 3;
		m_bAttackQueued = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio > m_fEndRatio && m_bComboInput)
		{
			m_pStateController->ChangeAnimState(L"Attack_04", true);
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_03);
		m_iComboIndex = 0;
		m_bComboInput = false;
		};

	m_pAnimator->PushAnimInfo(L"Attack_03", pAttack);
}

void CRole_Base::Init_Attack04_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_04);
		m_iComboIndex = 4;
		m_bAttackQueued = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_04);
		m_iComboIndex = 0;
		m_bComboInput = false;

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_04", pAttack);
}

void CRole_Base::Init_Attack05_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_05);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_05);

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_05", pAttack);
}

void CRole_Base::Init_Attack06_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_06);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_06);
		};

	m_pAnimator->PushAnimInfo(L"Attack_06", pAttack);
}

void CRole_Base::Init_Attack_L_Loop_State(_float _fEndTime)
{
	_fEndTime = 1.5f;
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_L_LOOP);

		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_L_LOOP);

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_L_Loop", pAttack);
}

void CRole_Base::Init_Attack_L_End_State(_float _fEndTime)
{
	_fEndTime = 1.f;
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_L_END);

		Reset_Locomotion();

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(20.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_L_END);

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_L_End", pAttack);
}

void CRole_Base::Init_Execute_State()
{
	_float fEndTime = 3.5f;
	auto pExecute = AnimTransform::Create(fEndTime); // 3.733f
	pExecute->funcStarted = [this, fEndTime]() {
		Add_State(CHARACTER_STATE::EXECUTE);

		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		m_fAdditionalCameraOffsetRatio = 0.25f; // Camera Offset 정도
		Keep_DistanceToTarget();

		/* 1. Delta 조정 */
		Apply_DeltaAnim(E_EASING::SMOOTHSTEP, E_EASING::SMOOTHSTEP, 0.15f, 0.25f, 3.f, 0.25f, false, true);

		/* 2. Execute Camera 시작 */
		m_fExecuteCameraTimer = 0.f;
		m_bExecuteCameraLookMonster = false;
		m_bExecuteCameraReturnPlayer = false;

		/* 3. Player 를 Monster 방향으로 */
		if (auto pMonster = m_pTargetMonster->Get()) {
			pMonster->Get_Desc().Excute();

			m_pTransform->Turn(pMonster->GetTransform()->Get_World(STATE::POSITION), XMLoadFloat3(&m_pTransform->Get_BaseUp()));
		}

		/* 4. CCharacterCamera set */
		if (m_pCharacterCamera) {
			m_pCharacterCamera->Set_TargetObject(this);
			m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);
			m_pCharacterCamera->Begin_Execute();
			m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);
		}

		/* 무적 */
		m_bIFrames = true;
		m_fIFramesTimer.x = fEndTime;
		};
	pExecute->funcUpdate = [this](_float _fTimeDelta) {
		m_fExecuteCameraTimer += _fTimeDelta;

		/* 5. Target 변경해주기 */
		if (!m_bExecuteCameraLookMonster && m_fExecuteCameraTimer >= 0.39f) {
			if (m_pCharacterCamera && m_pTargetMonster) {
				if (auto pMonster = m_pTargetMonster->Get()) {
					Update_AdditionalCameraOffset();

					_vector vPlayerPos = this->GetTransform()->Get_World(STATE::POSITION);
					_vector vMonsterPos = pMonster->GetTransform()->Get_World(STATE::POSITION);
					_vector vToMonster = vMonsterPos - vPlayerPos;

					vToMonster = XMVectorSetY(vToMonster, 0.f);

					_float fToMonsterLength = XMVectorGetX(XMVector3Length(vToMonster));
					if (fToMonsterLength > 0.001f) {
						_float fRatio = 0.12f;
						_float fMaxDistance = 0.6f;
						_float fHeight = 0.9f;

						_float fDistance = min(fToMonsterLength * fRatio, fMaxDistance);

						_vector vOffset = XMVector3Normalize(vToMonster) * fDistance;
						vOffset = XMVectorSetY(vOffset, fHeight);

						_float3 vObjectOffset = {};
						XMStoreFloat3(&vObjectOffset, vOffset);

						m_pCharacterCamera->Set_ObjectOffset(vObjectOffset);
						m_pCharacterCamera->Set_ObjectOffset_Speed(2.f);
					}
				}
			}

			m_bExecuteCameraLookMonster = true;
		}

		/* 6. 다시 플레이어로 돌아오기 */
		if (!m_bExecuteCameraReturnPlayer && m_fExecuteCameraTimer >= 1.165f) {
			if (m_pCharacterCamera) {
				m_pCharacterCamera->Set_TargetObject(this);
				m_pCharacterCamera->End_Execute();
			}

			m_bExecuteCameraReturnPlayer = true;
		}
		};
	pExecute->funcEnded = [this]() {
		/* 7. 정리 */
		if (m_pCharacterCamera) {
			m_pCharacterCamera->Set_TargetObject(this);
			m_pCharacterCamera->Unlock_MouseInput();
		}

		Set_RootMotionLimit(true);
		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);

		Remove_State(EXECUTE);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};

	m_pAnimator->PushAnimInfo(L"Execute", pExecute);
}

void CRole_Base::Init_Skill01_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {

		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_01);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_01);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};

	m_pAnimator->PushAnimInfo(L"Skill_01", pSkill);
}

void CRole_Base::Init_Skill02_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_02);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		
		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_02);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};

	m_pAnimator->PushAnimInfo(L"Skill_02", pSkill);
}

void CRole_Base::Init_Skill03_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_03);

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pVC = m_pBurstVC->Get()) {
				Debug_Output(m_pTransform->Get_BaseUp(), "\n");
				pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
			}
		}

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_03);

		m_vCharacterVelocity = { 0.f, 0.f, 0.f };
		m_pCCT->Clear_VerticalVelocity();
		m_pCCT->Set_UseGravity(true);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;

		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);
		};

	m_pAnimator->PushAnimInfo(L"Skill_03", pSkill);
}

void CRole_Base::Init_Skill04_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_04);

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pVC = m_pBurstVC->Get()) {
				Debug_Output(m_pTransform->Get_BaseUp(), "\n");
				pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
			}
		}

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_04);


		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};

	m_pAnimator->PushAnimInfo(L"Skill_04", pSkill);
}

void CRole_Base::Init_Skill05_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_05);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_05);
		};

	m_pAnimator->PushAnimInfo(L"Skill_05", pSkill);
}

void CRole_Base::Init_Skill06_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::SKILL_06);

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();
		Set_Active_Render_Children({});

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_06);
		};

	m_pAnimator->PushAnimInfo(L"Skill_06", pSkill);
}

void CRole_Base::Init_Burst01_State(_float _fEndTime)
{
	auto pBurst = AnimTransform::Create(_fEndTime);
	pBurst->funcStarted = [this, _fEndTime]() {
		Add_State(CHARACTER_STATE::BURST_01);

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);

		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(false);
			pUIManager->Set_BossHpBar_Active(false);
		}

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_R);
		Get_Desc().Spend_Ultra_Energy();

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;

		OW_DELTA_ANIM_DESC tOWDesc{};
		tOWDesc.eEnter = E_EASING::STEP;
		tOWDesc.fBlendTime = 0.f;
		tOWDesc.fTargetDelta = 0.f;
		tOWDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tOWDesc.Add_Layer(OBJECT_LAYER::BOSS);
		tOWDesc.Add_Layer(OBJECT_LAYER::EFFECT);
		tOWDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);

		m_pGameInstance->Change_DeltaTime_OneWay(tOWDesc);

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pActController = pSQCamObj->GetComponent<CActionController>()) {
				pActController->SetActive(true);
				pActController->Change_ActionSlot(2);

				if (auto pVC = m_pBurstVC->Get()) {
					CAMERA_TRANSITION_DESC tDesc{};
					tDesc.eCameraOrder = CAMERA_ORDER::CUTSCENE;
					tDesc.eTransitionType = CAMERA_TRANSITION::IMMEDIATE;
					pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
					pVC->Push(tDesc);
				}
			}
		}

		//OW_DELTA_ANIM_DESC

	};

	pBurst->funcUpdate = [this](_float _fTimeDelta) {
	};

	pBurst->AddFuncTable(new tagFunctionTable(m_tRoleDesc.vUltZeroRate.x, []() {}));	// 궁극기를 쓸 때 델타가 어디서 멈출건지
	pBurst->AddFuncTable(new tagFunctionTable(m_tRoleDesc.vUltZeroRate.y, []() {}));	// 궁극기를 쓸 때 델타가 어디서 복원 될 건지

	pBurst->funcEnded = [this]() {
		Remove_State(BURST_01);

		OW_DELTA_ANIM_DESC tOWDesc{};
		tOWDesc.eEnter = E_EASING::STEP;
		tOWDesc.fBlendTime = 0.f;
		tOWDesc.fTargetDelta = 1.f;
		tOWDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tOWDesc.Add_Layer(OBJECT_LAYER::BOSS);
		tOWDesc.Add_Layer(OBJECT_LAYER::EFFECT);
		tOWDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);

		m_pGameInstance->Change_DeltaTime_OneWay(tOWDesc);

		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(true);
			pUIManager->Set_BossHpBar_Active(true);
		}

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pActController = pSQCamObj->GetComponent<CActionController>()) {
				pActController->SetActive(false);
			}
		}

		if (auto pVC = m_pBurstVC->Get()) {
			CAMERA_TRANSITION_DESC tDesc{};
			tDesc.eCameraOrder = CAMERA_ORDER::CUTSCENE;
			tDesc.iFlag = ENUM_TO_UINT(CAMERA_BLEND_MASK::POS) | ENUM_TO_UINT(CAMERA_BLEND_MASK::ROT) | ENUM_TO_UINT(CAMERA_BLEND_MASK::FOV);
			tDesc.fTransitionTime = 0.5f;
			tDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
			pVC->Pop(tDesc);
		}
	};

	m_pAnimator->PushAnimInfo(L"Burst_01", pBurst);
}

void CRole_Base::Init_Attack_Air_Start_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime); // 0.667 + 0.667 // 1.334
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_AIR_START);

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_bAirAttackDiveApplied = false;
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();

		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio >= 0.5f)
		{
			if (!m_bAirAttackDiveApplied)
			{
				Set_JumpVelocity_Y(-30.f);
				m_bAirAttackDiveApplied = true;
			}
			Update_AirMotion(_fTimeDelta);

			if (m_pCCT)
			{
				if (m_pCCT->Is_Grounded())
				{
					m_bGrounded = true;
					m_pStateController->ChangeAnimState(L"Attack_Air_End", true);
					return;
				}
			}
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::ATTACK_AIR_START);
		};

	m_pAnimator->PushAnimInfo(L"Attack_Air_Start", pAttack);
}

void CRole_Base::Init_Attack_Air_End_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_AIR_END);
		Reset_Locomotion();
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		Update_AirMotion(_fTimeDelta);
		};
	pAttack->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::ATTACK_AIR_END);
		};

	m_pAnimator->PushAnimInfo(L"Attack_Air_End", pAttack);
}

void CRole_Base::Init_Behit_State()
{
	auto pBehit = AnimTransform::Create(0.25f);
	pBehit->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::BEHIT);

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();
		Reset_ControlCameraSettings();

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });
		};
	pBehit->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		};
	pBehit->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::BEHIT);
		};

	m_pAnimator->PushAnimInfo(L"Behit", pBehit);
}

void CRole_Base::Init_Switch_QTE_State(_float _fEndTime)
{
	/* Switch Out (QTE Attack) */
	auto pQTE = AnimTransform::Create(_fEndTime);
	pQTE->funcStarted = [this]() {
		Reset_ControlCameraSettings();

		Add_State(CHARACTER_STATE::SWITCH_QTE);

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_fWeaponVisualizeDurtaion = 1.f;
		Set_Weapon_Visual_Instant({ 1 });
		Set_Scabbard_Visual_Instant({ 1 });

		OW_DELTA_ANIM_DESC tOWDesc{};
		tOWDesc.eEnter = E_EASING::STEP;
		tOWDesc.fBlendTime = 0.f;
		tOWDesc.fTargetDelta = 0.f;
		tOWDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tOWDesc.Add_Layer(OBJECT_LAYER::BOSS);
		tOWDesc.Add_Layer(OBJECT_LAYER::EFFECT);
		tOWDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);

		m_pGameInstance->Change_DeltaTime_OneWay(tOWDesc);

		m_pCharacterCamera->Set_CameraArmLength(1.75f);
		m_pCharacterCamera->Set_CameraArm_Speed(12.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pQTE->funcUpdate = [this](_float _fTimeDelta) {
		};

	pQTE->AddFuncTable(new tagFunctionTable(0.75f, [this]() {
		OW_DELTA_ANIM_DESC tOWDesc{};
		tOWDesc.eEnter = E_EASING::STEP;
		tOWDesc.fBlendTime = 0.f;
		tOWDesc.fTargetDelta = 1.f;
		tOWDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tOWDesc.Add_Layer(OBJECT_LAYER::BOSS);
		tOWDesc.Add_Layer(OBJECT_LAYER::EFFECT);
		tOWDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);

		m_pGameInstance->Change_DeltaTime_OneWay(tOWDesc);

		Reset_CameraSettings();
	}));

	pQTE->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::SWITCH_QTE);

		OW_DELTA_ANIM_DESC tOWDesc{};
		tOWDesc.eEnter = E_EASING::STEP;
		tOWDesc.fBlendTime = 0.f;
		tOWDesc.fTargetDelta = 1.f;
		tOWDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tOWDesc.Add_Layer(OBJECT_LAYER::BOSS);
		tOWDesc.Add_Layer(OBJECT_LAYER::EFFECT);
		tOWDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);

		m_pGameInstance->Change_DeltaTime_OneWay(tOWDesc);

		Reset_CameraSettings();

		m_pAnimationController->Set_SubRootMotion_Y(false);
	};

	m_pAnimator->PushAnimInfo(L"Switch_QTE", pQTE);
}

void CRole_Base::Priority_Update(_float _fTimeDelta) {
	__super::Priority_Update(_fTimeDelta);

	if (m_pCCT)
	{
		m_bGrounded = m_pCCT->Is_Grounded();
		return;
	}

	m_bGrounded = m_iGroundContect > 0;
	m_iGroundContect = 0;
}

void CRole_Base::Init_Collision() {
	for (auto* pRigid : GetComponents<CRigidbody>()) {
		const auto& tag = pRigid->Get_UserData();
		if (tag == "Physics") m_pPhysicsRigid = pRigid;
		else if (tag == "Attack")  m_pAtckRigid = pRigid;
		else if (tag == "Hit")     m_pHitRigid = pRigid;
	}
	if (m_pPhysicsRigid)
	{

		m_pPhysicsRigid->SetContact([this](const tagCollision& _tCollision, COLLISION_STATE _eState) {
			if (_tCollision.otherTag == COLLIDER_TAG::STATIC) {
				const _float3& vNormal = _tCollision.normal;

				_float fDotGround = XMVectorGetX(XMVector3Dot(XMLoadFloat3(&vNormal), GetTransformBaseUp(m_pTransform)));

				if (fDotGround > 0.72f) {
					m_iGroundContect++;
				}
			}
			});
	}

	if (m_pCCT)
	{
		m_pCCT->Set_Tag(COLLIDER_TAG::PLAYER);
		m_pCCT->Set_Filter(
			Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::MONSTER) |
			Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::BOSS) |
			Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::STATIC)
		);
	}
}

#define MIN_NEW_TARGET 1.5f

_bool CRole_Base::Monster_Detacted(CTrigger* _pSelf, CTrigger* _pCollided) {
	if (_pCollided == nullptr || _pCollided->GetOwner() == nullptr)
		return false;

	auto eTag = _pCollided->Get_Tag();
	if (eTag != COLLIDER_TAG::MONSTER && eTag != COLLIDER_TAG::BOSS)
		return false;

	auto pCollided = Find_MonsterFromHierarchy(_pCollided->GetOwner());
	if (pCollided == nullptr)
		return false;

	if (pCollided->Get_Desc().fHp <= 0.f)
		return false;

	auto pCollidedTransform = Get_MonsterTargetTransform(pCollided);
	if (pCollidedTransform == nullptr || m_pTransform == nullptr)
		return false;

	_vector vNewDif = pCollidedTransform->Get_World(STATE::POSITION) - m_pTransform->Get_World(STATE::POSITION);

	_float fNewDist = XMVectorGetX(XMVector3LengthSq(vNewDif));

	if (auto pMonster = m_pTargetMonster->Get()) {
		auto pMonsterTransform = Get_MonsterTargetTransform(pMonster);
		if (pMonsterTransform == nullptr) {
			m_pTargetMonster->Set(pCollided);
			return true;
		}

		_vector vDif = pMonsterTransform->Get_World(STATE::POSITION) - m_pTransform->Get_World(STATE::POSITION);

		_float fDist = XMVectorGetX(XMVector3LengthSq(vDif));

		// 이미 있던 타겟이 아직 일정 범위 안에 있으면 타깃 변경을 하지 않는다
		if (fDist < MIN_NEW_TARGET * MIN_NEW_TARGET) {
			return true;
		}

		if (fDist > fNewDist) {
			m_pTargetMonster->Set(pCollided);
			return true;
		}
	}
	else {
		m_pTargetMonster->Set(pCollided);
	}

	return true;
}

void CRole_Base::Debug_Test(_float _fTimeDelta)
{
	/* Test */
	ATTACK_DESC tDesc{};
	tDesc.fBaseDamage = 1.f;
	tDesc.fDamageMultiplier = 100.f;

	if (m_pGameInstance->Key_Down(DIK_K))
	{
		this->Assulted(this, &tDesc);
	}

	/* Test */
	if (m_pGameInstance->Key_Down(DIK_F3))
	{
		Get_Desc().vIdentity_Energy.x = Get_Desc().vIdentity_Energy.y;
		Get_Desc().vHarmony_Energy.x = Get_Desc().vHarmony_Energy.y;
		Get_Desc().vUltra_Energy.x = Get_Desc().vUltra_Energy.y;
		Get_Desc().fStamina = Get_Desc().fMaxStamina;
		Get_Desc().Reset_Cooldown();
	}

	/* Invincible */
	if (m_pGameInstance->Key_Down(DIK_F5))
	{
		m_bDebug_Invincible = !m_bDebug_Invincible;
	
		if (m_bDebug_Invincible)
			Debug_Output("Invincible(muzuk) ON\n");
		else
			Debug_Output("Invincible(muzuk) OFF\n");
	}
}

void CRole_Base::Show_HealFont(_float fHeal)
{
	if (fHeal <= 0.f)
		return;

	if (!m_bIsMainRole)
		return;

	/* UI */
	if (auto pUIManager = CUI_Manager::GetInstance()) {
		DAMAGE_FONT_INFO tDesc{};

		tDesc.fDamage = fHeal;

		_float fOffset = 0.5f;
		if (auto pHitCase = m_pHitCase->Get()) {
			tDesc.vWordlPos = pHitCase->GetTransform()->GetWorldPosition();
			fOffset = 0.5f;
		}
		else {
			tDesc.vWordlPos = m_pTransform->GetWorldPosition();
			fOffset = 1.2f;
		}

		_vector vBaseUp = XMLoadFloat3(&m_pTransform->Get_BaseUp());
		_vector vDamagePos = XMLoadFloat3(&tDesc.vWordlPos) + vBaseUp * fOffset; // 살짝 띄우기
		XMStoreFloat3(&tDesc.vWordlPos, vDamagePos);

		tDesc.eStyle = DAMAGE_FONT_STYLE::Normal;
		tDesc.eElement = DAMAGE_FONT_ELEMENT::Heal;

		pUIManager->Set_DamageFont(tDesc);
	}
}

_bool CRole_Base::Is_JumpStarting()
{
	return GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity)) > 0.05f;
}

_bool CRole_Base::Is_Falling()
{
	return GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity)) < -0.05f;
}

_bool CRole_Base::Update_PlayerControlLock()
{
	const _bool bBlocked = Is_PlayerControlLocked();

	/* State */
	if (m_pStateController)
		m_pStateController->SetActive(!bBlocked);

	/* Camera */
	if (m_pCharacterCamera)
	{
		if (bBlocked)
			m_pCharacterCamera->Lock_MouseInput();
		else
			m_pCharacterCamera->Unlock_MouseInput();
	}

	if (!bBlocked)
		return false;

	/* Key Input */
	Clear_AllInput();

	/* Physic */
	if (m_pCCT)
		m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });

	m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };

	return true;
}

_bool CRole_Base::Is_AttackState()
{
	return State_Check(ATTACK_01) || State_Check(ATTACK_02) ||
		State_Check(ATTACK_03) || State_Check(ATTACK_04) || 
		State_Check(ATTACK_05) || State_Check(ATTACK_06) ||
		State_Check(ATTACK_07) ||
		State_Check(ATTACK_L_LOOP) || State_Check(ATTACK_L_END) ||
		State_Check(ATTACK_AIR_START) || State_Check(EXECUTE) ||
		State_Check(SKILL_01) || State_Check(SKILL_02) ||
		State_Check(SKILL_03) || State_Check(SKILL_04) ||
		State_Check(SKILL_05) || State_Check(SKILL_06) ||
		State_Check(SKILL_07) ||
		State_Check(BURST_01) || State_Check(BURST_02);
}

_bool CRole_Base::Is_BurstState()
{
	return State_Check(BURST_01) || State_Check(BURST_02);
}

_bool CRole_Base::Is_QTEState()
{
	return State_Check(SWITCH_QTE);
}

_bool CRole_Base::Is_LocomotionState()
{
	if (m_pStateController && m_pStateController->GetCurStateName() == L"Idle")
		return true;

	return State_Check(WALK) || State_Check(RUN) || State_Check(SPRINT)
		|| State_Check(JUMP_START) || State_Check(JUMP_LOOP) || State_Check(JUMP_LAND);
}

_bool CRole_Base::Is_JumpState()
{
	return State_Check(JUMP_LAND) || State_Check(JUMP_START)
		|| State_Check(JUMP_LOOP) || State_Check(JUMP_SECOND);
}

_bool CRole_Base::Check_JumpState()
{
	return m_fLandLockTime <= 0.f;
}

_bool CRole_Base::Is_FlyState()
{
	return State_Check(FLY_START) || State_Check(FLY_LOOP);
}

_bool CRole_Base::Check_Movestate()
{
	if (State_Check(MOVE))
		return true;

	return false;
}

_bool CRole_Base::Has_RunInput()
{
	if (!m_bIsMainRole)
		return false;

	return m_pGameInstance->Key_Pressed(DIK_W) || m_pGameInstance->Key_Pressed(DIK_A) || m_pGameInstance->Key_Pressed(DIK_S) || m_pGameInstance->Key_Pressed(DIK_D);
}

_bool CRole_Base::Has_MoveInput()
{
	if (!m_bIsMainRole)
		return false;

	return (m_pGameInstance->Key_Pressed(DIK_LSHIFT) || m_pGameInstance->Mouse_Pressed(DI_MB::RBUTTON));
}

_bool CRole_Base::Has_MoveInput_Down()
{
	if (!m_bIsMainRole)
		return false;

	return (m_pGameInstance->Key_Down(DIK_LSHIFT) || m_pGameInstance->Mouse_Down(DI_MB::RBUTTON));
}

_bool CRole_Base::Has_JumpInput()
{
	if (!m_bIsMainRole)
		return false;

	return m_pGameInstance->Key_Pressed(DIK_SPACE);
}

_bool CRole_Base::Has_JumpInput_Down()
{
	if (!m_bIsMainRole)
		return false;

	return m_pGameInstance->Key_Down(DIK_SPACE);
}

_bool CRole_Base::Check_Slidable() {
	if (m_pCCT == nullptr)
		return false;

	if (m_ePlayMode != E_PLAY_MODE::SIDEVIEW)
		return false;

	const auto& tGroundHit = m_pCCT->Get_GroundShapeHit();
	const auto& tSlideHit = tGroundHit.bHit ? tGroundHit : m_pCCT->Get_LastShapeHit();
	if (!tSlideHit.bHit) {
		return false;
	}

	_vector vNormal = XMVector3Normalize(XMLoadFloat3(&tSlideHit.vNormal));
	_vector vBaseUp = XMVector3Normalize(XMLoadFloat3(&m_pTransform->Get_BaseUp()));

	_float fDot = XMVectorGetX(XMVector3Dot(vNormal, vBaseUp));
	fDot = max(-1.f, min(1.f, fDot));

	_float fAngle = XMConvertToDegrees(acosf(fDot));
	_bool bSlidable = (Is_Grounded() || tGroundHit.bHit) && fAngle >= 20.f;
	if (bSlidable)
		XMStoreFloat3(&m_vSlideNormal, vNormal);

	return bSlidable;
}

void CRole_Base::Update_Lock_Move(_float _fTimeDelta)
{
	if (!m_bMoveLocked)
		return;

	m_fMoveTimer -= _fTimeDelta;
	if (m_fMoveTimer <= 0.f)
	{
		m_fMoveTimer = 0.f;
		m_bMoveLocked = false;
	}
}

void CRole_Base::Lock_Move(_float _fCooldown)
{
	m_bMoveLocked = true;
	m_fMoveTimer = _fCooldown;
}

void CRole_Base::Release_Move_Lock() {
	m_bMoveLocked = false;
	m_fMoveTimer = 0.f;
}

void CRole_Base::Flush_Forced_Return_EffectPool()
{
	if (m_vecForcedReturnEffect.empty())
		return;



	for (_int iEffectIndex : m_vecForcedReturnEffect)
		Forced_Return_EffectPool(iEffectIndex);
}

void CRole_Base::Init_Push_Camera()
{
	/* Aimisi */
	m_pCharacterCamera->Set_ObjectOffset(_float3(0.f, 1.4f, 0.f));
	m_pCharacterCamera->Set_CameraArmLength(3.f);
	m_pGameInstance->Push_Camera(m_pCharacterCamera->Get_VirtualCamera());
}

void CRole_Base::Update_Locomotion(_float _fTimeDelta)
{
	auto pTransform = this->GetTransform();
	if (!pTransform)
		return;

	_vector vDir = XMLoadFloat3(&m_vTargetDirection);
	_float fDirLength = XMVectorGetX(XMVector3Length(vDir));

	if (fDirLength < 0.0001f) {
		if (m_pCCT)
			m_pCCT->Clear_PlanarVelocity();

		m_bWasLocomotionMoving = false;
		return;
	}

	if (!m_bWasLocomotionMoving) {
		Reset_ControlCameraSettings();
		m_bWasLocomotionMoving = true;
	}

	/* 占쌕띰옙 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占싱깍옙 */
	_vector vNormalizeDir = XMVector3Normalize(vDir);

	/* Speed Factor */
	_float fSpeedFactor = 1.f;
	if (m_fSpeedFactor >= 0.f)
	{
		fSpeedFactor = m_fSpeedFactor;
	}
	else
	{
		fSpeedFactor = m_bHasInput ? 1.f : 0.7f;
	}

	if (fSpeedFactor <= 0.f)
	{
		if (m_bHasInput)
		{
			if (m_pCCT)
				m_pCCT->Clear_PlanarVelocity();

			return;
		}

		fSpeedFactor = 0.7f;
	}

	/* Final */
	_float fSpeed = m_fBaseRunSpeed * (fSpeedFactor * clamp(fDirLength, 0.f, 1.f));
	_vector vVelocity = vNormalizeDir * fSpeed;

	if (m_pCCT)
	{
		m_pCCT->Set_PlanarVelocity(vVelocity);
		return;
	}

	pTransform->Translate_Vector(vVelocity * _fTimeDelta); // x
}

void CRole_Base::Update_RemainLocomotion(_float _fTimeDelta)
{
	_vector vCurDir = RemoveAxisComponent(XMLoadFloat3(&m_vTargetDirection), GetTransformBaseUp(m_pTransform));

	_float fLerpRatio = clamp(_fTimeDelta * 2.f, 0.f, 1.f);
	vCurDir = XMVectorLerp(vCurDir, XMVectorZero(), fLerpRatio);

	_float fRemainLength = XMVectorGetX(XMVector3Length(vCurDir));
	if (fRemainLength < 0.001f)
		vCurDir = XMVectorZero();

	XMStoreFloat3(&m_vTargetDirection, vCurDir);
}

void CRole_Base::Update_AirMotion(_float _fTimeDelta)
{
	if (m_bGrounded)
	{
		if (GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity)) < 0.f)
			XMStoreFloat3(&m_vCharacterVelocity, SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity), 0.f));

		Apply_JumpVelocity(_fTimeDelta);
		return;
	}

	_float fGravityScale = 1.f;
	if (State_Check(JUMP_LOOP) || State_Check(JUMP_SECOND))
		fGravityScale = m_fJumpGravityScale;
	else if (State_Check(FLY_START))
		fGravityScale = Is_Falling() ? 0.7f : m_fFlyGravityScale;
	else if (State_Check(FLY_LOOP))
		fGravityScale = m_fFlyGravityScale;

	if (m_pCCT->Get_UseGravity()) {
		_vector vGravityDir = -GetTransformBaseUp(m_pTransform);

		XMStoreFloat3(&m_vCharacterVelocity, XMLoadFloat3(&m_vCharacterVelocity) + vGravityDir * m_fGravity * _fTimeDelta * fGravityScale);

		/* Excep. Fly_Start / Fly_Loop / Jump_Loop 속도 제한 */
		_float fMaxFallSpeed = 0.f;

		if (State_Check(JUMP_LOOP))
			fMaxFallSpeed = 16.f;
		else if (State_Check(FLY_START) || State_Check(FLY_LOOP))
			fMaxFallSpeed = 5.f;

		if (fMaxFallSpeed > 0.f)
		{
			_float fBaseUpVelocity = GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity));
			fBaseUpVelocity = clamp(fBaseUpVelocity, -fMaxFallSpeed, FLT_MAX);

			XMStoreFloat3(&m_vCharacterVelocity, SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity), fBaseUpVelocity));
		}
	}

	Apply_JumpVelocity(_fTimeDelta);
}

CObject* CRole_Base::Get_SkillCameraRig()
{
	if (m_pObjectFilter) {
		if (auto pCameraRig = m_pObjectFilter->Get_Object<CObject>(ETOU(E_OBJECTS::SKILL_CAMERA_RIG))) {
			return pCameraRig;
		}
	}

	if (auto pTransform = GetTransform()) {
		if (auto pCameraRigTransform = pTransform->Get_Child_To_Name(L"SQCAm")) {
			return pCameraRigTransform->GetOwner();
		}
	}

	return nullptr;
}

CVirtualCamera* CRole_Base::Get_SkillVirtualCamera(CObject* _pCameraRig)
{
	if (_pCameraRig == nullptr || _pCameraRig->GetTransform() == nullptr)
		return nullptr;

	auto pCameraTransform = _pCameraRig->GetTransform()->Get_ChildToPath(L"Camera Pivot/Camera Shake Root/Virtual Camera");
	if (pCameraTransform == nullptr) {
		pCameraTransform = _pCameraRig->GetTransform()->Get_Child_To_Name(L"Virtual Camera");
	}

	if (pCameraTransform == nullptr || pCameraTransform->GetOwner() == nullptr)
		return nullptr;

	return pCameraTransform->GetOwner()->GetComponent<CVirtualCamera>();
}

CAMERA_TRANSITION_DESC CRole_Base::Build_SkillCameraTransitionDesc(CVirtualCamera* _pVirtualCamera)
{
	CAMERA_TRANSITION_DESC tDesc = m_tSkillCameraTransitionDesc;
	tDesc.pVirtualCamera = _pVirtualCamera;

	if (_pVirtualCamera) {
		if (m_fSkillCameraFovOverride > 0.f) {
			_pVirtualCamera->Set_Fov(m_fSkillCameraFovOverride);
			tDesc.fFov = m_fSkillCameraFovOverride;
		}
		else {
			tDesc.fFov = _pVirtualCamera->Get_Fov();
		}
	}

	return tDesc;
}

void CRole_Base::Restart_SkillCameraAction(CObject* _pCameraRig)
{
	if (_pCameraRig == nullptr)
		return;

	if (auto pVirtualCamera = Get_SkillVirtualCamera(_pCameraRig)) {
		pVirtualCamera->Reset_RuntimeCorrection();
	}

	auto pActionController = _pCameraRig->GetComponent<CActionController>();
	if (pActionController == nullptr)
		return;

	if (auto pActionInfo = pActionController->Get_ActionInfo()) {
		pActionController->Change_Action(pActionInfo->strName);
		pActionController->Update(0.f);
	}
}

void CRole_Base::Reset_SkillCameraDesc()
{
	m_tSkillCameraTransitionDesc = {};
	m_tSkillCameraTransitionDesc.eCameraOrder = CAMERA_ORDER::CUTSCENE;
	m_tSkillCameraTransitionDesc.eTransitionType = CAMERA_TRANSITION::SMOOTHSTEP;
	m_tSkillCameraTransitionDesc.fTransitionTime = 0.15f;
	m_tSkillCameraTransitionDesc.fFov = 72.f;
	m_tSkillCameraTransitionDesc.iFlag = ETOU(CAMERA_BLEND_MASK::POS) | ETOU(CAMERA_BLEND_MASK::ROT) | ETOU(CAMERA_BLEND_MASK::FOV);
	m_tSkillCameraTransitionDesc.bHandoffCorrection = true;
	m_ePushedSkillCameraOrder = CAMERA_ORDER::CNT;
	m_fSkillCameraFovOverride = -1.f;
}

void CRole_Base::Force_PopSkillCamera()
{
	auto pCameraRig = Get_SkillCameraRig();
	auto pVirtualCamera = Get_SkillVirtualCamera(pCameraRig);
	if (pVirtualCamera)
	{
		CAMERA_TRANSITION_DESC tDesc{};
		tDesc.pVirtualCamera = pVirtualCamera;
		m_pGameInstance->Pop_Camera(tDesc, true);
	}

	m_ePushedSkillCameraOrder = CAMERA_ORDER::CNT;
	Reset_SkillCameraDesc();
}

void CRole_Base::Reset_ControlCameraSettings()
{
	Force_PopSkillCamera();
	Reset_CameraSettings();
}

void CRole_Base::Set_Skill_Camera_Order(const _int& _iOrder)
{
	const _int iMax = ETOU(CAMERA_ORDER::CNT) - 1;
	m_tSkillCameraTransitionDesc.eCameraOrder = static_cast<CAMERA_ORDER>(clamp(_iOrder, 0, iMax));
}

void CRole_Base::Set_Skill_Camera_Transition(const _int& _iTransition)
{
	const _int iMax = ETOU(CAMERA_TRANSITION::CNT) - 1;
	m_tSkillCameraTransitionDesc.eTransitionType = static_cast<CAMERA_TRANSITION>(clamp(_iTransition, 0, iMax));
}

void CRole_Base::Set_Skill_Camera_Transition_Time(const _float& _fTime)
{
	m_tSkillCameraTransitionDesc.fTransitionTime = max(0.f, _fTime);
}

void CRole_Base::Set_Skill_Camera_Blend_Mask(const _int& _iMask)
{
	m_tSkillCameraTransitionDesc.iFlag = static_cast<_uint>(max(0, _iMask));
}

void CRole_Base::Set_Skill_Camera_Fov(const _float& _fFov)
{
	m_fSkillCameraFovOverride = _fFov;
	if (_fFov > 0.f) {
		m_tSkillCameraTransitionDesc.fFov = _fFov;
	}
}

void CRole_Base::Set_Skill_Camera_HandoffCorrection(const _int& _iApply)
{
	m_tSkillCameraTransitionDesc.bHandoffCorrection = (_iApply != 0);
}

void CRole_Base::Set_Skill_Camera_Desc(const _float4& _vDesc)
{
	Set_Skill_Camera_Order(static_cast<_int>(_vDesc.x));
	Set_Skill_Camera_Transition(static_cast<_int>(_vDesc.y));
	Set_Skill_Camera_Transition_Time(_vDesc.z);
	Set_Skill_Camera_Blend_Mask(static_cast<_int>(_vDesc.w));
}

void CRole_Base::Push_Skill_Camera(const monostate&)
{
	auto pCameraRig = Get_SkillCameraRig();
	auto pVirtualCamera = Get_SkillVirtualCamera(pCameraRig);
	if (pVirtualCamera == nullptr)
		return;

	Restart_SkillCameraAction(pCameraRig);

	auto tDesc = Build_SkillCameraTransitionDesc(pVirtualCamera);
	m_ePushedSkillCameraOrder = tDesc.eCameraOrder;
	m_pGameInstance->Push_Camera(tDesc);
}

void CRole_Base::Pop_Skill_Camera(const _int& _iForced)
{
	_bool bForced = (_iForced != 0);

	auto pCameraRig = Get_SkillCameraRig();
	auto pVirtualCamera = Get_SkillVirtualCamera(pCameraRig);
	if (pVirtualCamera == nullptr)
		return;

	auto tDesc = Build_SkillCameraTransitionDesc(pVirtualCamera);
	if (m_ePushedSkillCameraOrder != CAMERA_ORDER::CNT) {
		tDesc.eCameraOrder = m_ePushedSkillCameraOrder;
	}

	m_pGameInstance->Pop_Camera(tDesc, bForced);
	m_ePushedSkillCameraOrder = CAMERA_ORDER::CNT;
}

void CRole_Base::Pop_Skill_Camera_Smooth(const _float4& _vDesc)
{
	auto pCameraRig = Get_SkillCameraRig();
	auto pVirtualCamera = Get_SkillVirtualCamera(pCameraRig);
	if (pVirtualCamera == nullptr)
		return;

	auto tDesc = Build_SkillCameraTransitionDesc(pVirtualCamera);

	if (m_ePushedSkillCameraOrder != CAMERA_ORDER::CNT)
	{
		tDesc.eCameraOrder = m_ePushedSkillCameraOrder;
	}
	else
	{
		const _int iMaxOrder = static_cast<_int>(ETOU(CAMERA_ORDER::CNT)) - 1;
		const _int iOrder = std::clamp(static_cast<_int>(_vDesc.x), 0, iMaxOrder);
		tDesc.eCameraOrder = static_cast<CAMERA_ORDER>(iOrder);
	}

	const _int iMaxTransition = static_cast<_int>(ETOU(CAMERA_TRANSITION::CNT)) - 1;
	const _int iTransition = std::clamp(static_cast<_int>(_vDesc.y), 0, iMaxTransition);

	tDesc.eTransitionType = static_cast<CAMERA_TRANSITION>(iTransition);
	tDesc.fTransitionTime = max(0.f, _vDesc.z);
	tDesc.iFlag = static_cast<_uint>(max(0, static_cast<_int>(_vDesc.w)));

	m_pGameInstance->Pop_Camera(tDesc, false);
	m_ePushedSkillCameraOrder = CAMERA_ORDER::CNT;
}


void CRole_Base::Set_SkillCameraRig_LayerDelta(const _float4& _vDesc)
{
	auto* pCameraRig = Get_SkillCameraRig();
	if (pCameraRig == nullptr)
		return;

	const _int iMaxLayer = ETOU(OBJECT_LAYER::CNT) - 1;
	const _int iLayer = clamp(static_cast<_int>(_vDesc.x), 0, iMaxLayer);

	const OBJECT_LAYER eLayer = static_cast<OBJECT_LAYER>(iLayer);
	const _float fTargetDelta = max(0.f, _vDesc.y);
	const _float fBlendTime = max(0.f, _vDesc.z);

	pCameraRig->Set_Layer(eLayer);

	OW_DELTA_ANIM_DESC tDesc{};
	tDesc.Add_Layer(eLayer);
	tDesc.fTargetDelta = fTargetDelta;
	tDesc.fBlendTime = fBlendTime;

	m_pGameInstance->Change_DeltaTime_OneWay(tDesc);
}

void CRole_Base::Play_Sound_SFX(const _float2& _vParam) {
	Play_Sound(E_OBJECTS::SOUND_FILTER_SFX, _vParam);
}

void CRole_Base::Play_Sound_VOX(const _float2& _vParam) {
	Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, _vParam);
}

void CRole_Base::Play_Sound_RUN(const _float3& _vParam) {
	/* x : Ground Run Sound / y : Volume / z : Cloud Run Sound */
	const _float fIndex = (m_ePlayMode == E_PLAY_MODE::SIDEVIEW) ? _vParam.z : _vParam.x;
	const _float fVolume = (m_ePlayMode == E_PLAY_MODE::SIDEVIEW) ? _vParam.y * 5.f : _vParam.y;

	Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, _float2{ fIndex, fVolume});
}

void CRole_Base::Set_Weapon_Visual(const _int& _rParam) {
	if (auto pWeapon = m_pWeapon->Get()) {
		pWeapon->Notify_WeaponEvent(_float4{ static_cast<_float>(_rParam), 0.f, 0.f, 0.f });
	}
}

void CRole_Base::Set_Weapon_Visual_Instant(const _int& _rParam) {
	if (auto pWeapon = m_pWeapon->Get()) {
		pWeapon->Notify_WeaponEvent(_float4{ static_cast<_float>(_rParam), 1.f, 0.f, 0.f });
	}
}

void CRole_Base::Set_Scabbard_Visual(const _int& _rParam) {
	if (auto pScabbard = m_pScabbard->Get()) {
		pScabbard->Notify_WeaponEvent(_float4{ static_cast<_float>(_rParam), 0.f, 0.f, 0.f });
	}
}

void CRole_Base::Set_Scabbard_Visual_Instant(const _int& _rParam) {
	if (auto pScabbard = m_pScabbard->Get()) {
		pScabbard->Notify_WeaponEvent(_float4{ static_cast<_float>(_rParam), 1.f, 0.f, 0.f });
	}
}

void CRole_Base::Play_Sound(E_OBJECTS _eObjects, const _float2& _vParam) {
	_uint _iIndex = static_cast<_uint>(_vParam.x);
	_float _fVolume = _vParam.y;

	if (auto pObj = m_pObjectFilter->Get_Object(_eObjects)) {
		if (auto pSoundFilter = pObj->GetComponent<CSoundFilter>()) {
			auto pSounrAsset = pSoundFilter->Get_Asset(_iIndex);

			if (pSounrAsset) {
				pSounrAsset->Play(_fVolume, 0.f, m_pTransform);
			}
		}
	}
}

void CRole_Base::Play_Sound(E_OBJECTS _eObjects, const _float3& _vParam)
{
	_uint _iIndex = static_cast<_uint>(_vParam.x);
	_float _fVolume = _vParam.y;
	_int iForce = static_cast<_int>(_vParam.z);

	if (auto pObj = m_pObjectFilter->Get_Object(_eObjects)) {
		if (auto pSoundFilter = pObj->GetComponent<CSoundFilter>()) {
			auto pSounrAsset = pSoundFilter->Get_Asset(_iIndex);

			if (pSounrAsset) {
				pSounrAsset->Play_Channel(static_cast<CHANNELID>(_iIndex), _fVolume, iForce, m_pTransform);
			}
		}
	}
}

void CRole_Base::OnBoard_Vehicle(CTransform* _pVehicleTransform) {
	if (_pVehicleTransform == nullptr || m_bOnVehicle)
		return;

	m_pVehicle->Set(_pVehicleTransform);
	m_bOnVehicle = true;

	if (m_pCCT) {
		m_bVehiclePrevCCTActive = m_pCCT->GetActive();
		m_bVehiclePrevCCTUseGravity = m_pCCT->Get_UseGravity();
		m_eVehiclePrevCCTTag = m_pCCT->Get_Tag();
		m_iVehiclePrevCCTFilter = m_pCCT->Get_Filter();

		m_pCCT->Stop();
		m_pCCT->Set_UseGravity(false);
		m_pCCT->Set_Tag(COLLIDER_TAG::NONE);
		m_pCCT->Set_Filter(0);
		m_pCCT->SetActive(false);
	}

	m_pStateController->ChangeAnimState(L"Sit", true);
}

void CRole_Base::GetOff_Vehicle(_float _fFloatVelocity, _float3 _vInheritVelocity) {
	if (!m_bOnVehicle)
		return;

	const _float3 vWorldPosition = m_pTransform->GetWorldPosition();
	const _bool bFloatOff = _fFloatVelocity > 0.f;

	m_pTransform->Detach();
	m_pVehicle->Set(nullptr);
	m_bOnVehicle = false;

	if (auto pGameManager = CGameManager::GetInstance()) {
		if (auto pCommonEff = pGameManager->Get_CommonEffect(COMMON_EFFECT::GET_OUT_GONDOLA)) {
			if (auto pInst = pCommonEff->Instantiate_Prefab()) {
				if (auto pTransform = pInst->GetTransform()) {
					pTransform->Set_State(STATE::POSITION, m_pTransform->Get_World(STATE::POSITION));
				}
			}
		}
	}

	if (m_pCCT) {
		m_pCCT->Set_Position(vWorldPosition);
		m_pCCT->Stop();
		m_pCCT->Set_Tag(m_eVehiclePrevCCTTag);
		m_pCCT->Set_Filter(m_iVehiclePrevCCTFilter);
		m_pCCT->Set_UseGravity(bFloatOff ? true : m_bVehiclePrevCCTUseGravity);
		m_pCCT->SetActive(bFloatOff ? true : m_bVehiclePrevCCTActive);

		if (bFloatOff) {
			_vector vExitVelocity = SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&_vInheritVelocity), _fFloatVelocity);
			XMStoreFloat3(&m_vCharacterVelocity, vExitVelocity);
			m_pCCT->Set_Velocity(vExitVelocity);
			m_pCCT->Add_Move(GetTransformBaseUp(m_pTransform) * 0.05f);

			m_bGrounded = false;
			m_iGroundContect = 0;
		}
	}

	m_pStateController->ChangeAnimState(bFloatOff ? L"Jump_Loop" : L"Idle", true);
}

CRole_Base::EXECUTE_CAMERA_DESC CRole_Base::Get_ExecuteCameraDesc() const
{
	return EXECUTE_CAMERA_DESC{};
}

void CRole_Base::Apply_DeltaAnim(E_EASING _eEnter, E_EASING _eLeave, _float _fTargetDelta, _float _fEnterTime, _float _fDuration, _float _fLeaveTime)
{
	TW_DELTA_ANIM_DESC tDesc{};
	tDesc.eEnter = _eEnter;
	tDesc.fEnterTime = _fEnterTime;

	tDesc.fDuration = _fDuration;
	tDesc.fTargetDelta = _fTargetDelta;

	tDesc.eLeave = _eLeave;
	tDesc.fLeaveTime = _fLeaveTime;

	tDesc.Add_Layer(OBJECT_LAYER::MAIN_PLAYER);
	tDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);
	tDesc.Add_Layer(OBJECT_LAYER::MONSTER);
	tDesc.Add_Layer(OBJECT_LAYER::BOSS);

	m_pGameInstance->Change_DeltaTime_TwoWay(tDesc);
}

void CRole_Base::Apply_DeltaAnim(E_EASING _eEnter, E_EASING _eLeave, _float _fTargetDelta, _float _fEnterTime, _float _fDuration, _float _fLeaveTime, _bool _bApplyToPlayers, _bool _bApplyToMonsters)
{
	TW_DELTA_ANIM_DESC tDesc{};
	tDesc.eEnter = _eEnter;
	tDesc.fEnterTime = _fEnterTime;

	tDesc.fDuration = _fDuration;
	tDesc.fTargetDelta = _fTargetDelta;

	tDesc.eLeave = _eLeave;
	tDesc.fLeaveTime = _fLeaveTime;

	if (_bApplyToPlayers)
	{
		tDesc.Add_Layer(OBJECT_LAYER::MAIN_PLAYER);
		tDesc.Add_Layer(OBJECT_LAYER::SUB_PLAYER);
	}

	if (_bApplyToMonsters)
	{
		tDesc.Add_Layer(OBJECT_LAYER::MONSTER);
		tDesc.Add_Layer(OBJECT_LAYER::BOSS);
	}

	m_pGameInstance->Change_DeltaTime_TwoWay(tDesc);
}

void CRole_Base::Add_JumpVelocity(_float3 _vVelocity)
{
	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vVelocity = XMLoadFloat3(&m_vCharacterVelocity);
	_vector vAdd = XMVectorSet(_vVelocity.x, 0.f, _vVelocity.z, 0.f) + vBaseUp * _vVelocity.y;
	XMStoreFloat3(&m_vCharacterVelocity, vVelocity + vAdd);
}

void CRole_Base::Set_JumpVelocity_Y(_float _fVelocityY)
{
	XMStoreFloat3(&m_vCharacterVelocity, SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity), _fVelocityY));
}

void CRole_Base::Apply_JumpVelocity(_float _fTimeDelta)
{
	_float fBaseUpVelocity = GetBaseUpVelocity(m_pTransform, XMLoadFloat3(&m_vCharacterVelocity));

	if (m_pCCT)
	{
		m_pCCT->Set_VerticalVelocity(fBaseUpVelocity);
		return;
	}

	if (!m_pPhysicsRigid)
		return;

	_float3 vRigidVelocity = m_pPhysicsRigid->Get_Velocity();
	_vector vVelocity = SetBaseUpVelocity(m_pTransform, XMLoadFloat3(&vRigidVelocity), fBaseUpVelocity);
	XMStoreFloat3(&vRigidVelocity, vVelocity);

	m_pPhysicsRigid->Set_Velocity(vRigidVelocity);
}

void CRole_Base::Update_LandLock(_float _fTimeDelta)
{
	m_fLandLockTime = max(0.f, m_fLandLockTime - _fTimeDelta); // Guard dup jump
}

void CRole_Base::Mute_RootMotion(_bool _bApply)
{
	auto* pAnimationCtrl = this->GetComponent<CAnimationController>();
	if (pAnimationCtrl)
		pAnimationCtrl->Set_RootMotionMute(_bApply);
}

void CRole_Base::Set_RootMotionLimit(_bool _bApply)
{
	m_fRootMotionLimit = _bApply;
}

void CRole_Base::Init_AttackRootMotionLimit()
{
	m_fRootMotionLimitDistance = 1.5f;
	m_fRootMotionLimitDistance = m_fDefaultRootMotionLimitDistance;
}

void CRole_Base::Update_AttackRootMotionLimit()
{
	if (!m_fRootMotionLimit)
		return;

	if (!m_pAnimationController)
		return;

	if (!Is_AttackState())
	{
		m_pAnimationController->Set_RootMotionMaxDistance(false);
		return;
	}

	auto pMonster = m_pTargetMonster ? m_pTargetMonster->Get() : nullptr;
	if (!pMonster)
	{
		m_pAnimationController->Set_RootMotionMaxDistance(false);
		return;
	}

	auto pMonsterTransform = Get_MonsterTargetTransform(pMonster);
	auto pPlayerTransform = this->GetTransform();
	if (!pMonsterTransform || !pPlayerTransform)
	{
		m_pAnimationController->Set_RootMotionMaxDistance(false);
		return;
	}

	_vector vPlayerPos = pPlayerTransform->Get_World(STATE::POSITION);
	_vector vMonsterPos = pMonsterTransform->Get_World(STATE::POSITION);
	auto vToPlayer = vMonsterPos - vPlayerPos;
	vToPlayer = XMVectorSetY(vToPlayer, 0.f);

	_float fDistance = XMVectorGetX(XMVector3Length(vToPlayer));
	_float fMaxMoveDistance = max(0.f, fDistance - m_fRootMotionLimitDistance);

	m_pAnimationController->Set_RootMotionMaxDistance(true, fMaxMoveDistance);
}

void CRole_Base::Keep_DistanceToTarget()
{
	/* 공격 시, 몬스터와 거리가 가까우면 거리 유지용 */

	if (!m_pAnimationController)
		return;

	auto pMonster = m_pTargetMonster ? m_pTargetMonster->Get() : nullptr;
	if (!pMonster)
		return;

	auto pMonsterTransform = pMonster->GetTransform();
	auto pPlayerTransform = this->GetTransform();
	if (!pMonsterTransform || !pPlayerTransform)
		return;

	_vector vPlayerPos = pPlayerTransform->Get_World(STATE::POSITION);
	_vector vMonsterPos = pMonsterTransform->Get_World(STATE::POSITION);

	/* 2. Magic Number * Dir 로 거리 유지하기 */
	_float fKeepDistance = 2.5f;

	_vector vToPlayer = vPlayerPos - vMonsterPos;
	vToPlayer = XMVectorSetY(vToPlayer, 0.f);

	_float fDistance = XMVectorGetX(XMVector3Length(vToPlayer));

	/* 거리가 가까울때만 계산 */
	if (fDistance > 0.001f && fDistance < fKeepDistance)
	{
		_vector vDirection = XMVector3Normalize(vToPlayer);
		_vector vKeepPos = vMonsterPos + (vDirection * fKeepDistance);
		vKeepPos = XMVectorSetY(vKeepPos, XMVectorGetY(vPlayerPos));

		_float3 vFinalPos = {};
		XMStoreFloat3(&vFinalPos, vKeepPos);

		if (m_pTransform)
			m_pTransform->Set_State(STATE::POSITION, XMLoadFloat3(&vFinalPos));

		if (m_pCCT)
			m_pCCT->Set_Position(vFinalPos);
	}

	/* 4. RootMotion Limit OFF */
	Set_RootMotionLimit(false);

	/* 5. RootMotion OFF */
	this->m_pAnimationController->Set_ApplyRootMotion(false);
	this->m_pAnimationController->Set_ApplyRootMotionX(false);
	this->m_pAnimationController->Set_ApplyRootMotionZ(false);
}

void CRole_Base::Update_AdditionalCameraOffset()
{
	if (!m_pCharacterCamera)
		return;
	_float3 vAdditionalCameraOffset = { 0.f, 0.f, 0.f };

	auto pMonster = m_pTargetMonster ? m_pTargetMonster->Get() : nullptr;
	if (!pMonster)
	{
		m_pCharacterCamera->Set_AdditionalCameraOffset(vAdditionalCameraOffset);
		return;
	}

	auto pMonsterTransform = pMonster->GetTransform();
	auto pPlayerTransform = this->GetTransform();
	if (!pMonsterTransform || !pPlayerTransform)
		return;

	_vector vPlayerPos = pPlayerTransform->Get_World(STATE::POSITION);
	_vector vMonsterPos = pMonsterTransform->Get_World(STATE::POSITION);

	_vector vDirecton = vMonsterPos - vPlayerPos;
	vDirecton = XMVectorSetY(vDirecton, 0.f);
	vDirecton = XMVector3Normalize(vDirecton);

	auto pCharcterCameraTransform = m_pCharacterCamera->GetTransform();
	_vector vCameraRight = pCharcterCameraTransform->Get_World(STATE::RIGHT);
	vCameraRight = XMVectorSetY(vCameraRight, 0.f);
	vCameraRight = XMVector3Normalize(vCameraRight);

	/* Scalar */
	_float fPlayerScalar = XMVectorGetX(XMVector3Dot(vPlayerPos, vCameraRight));
	_float fMonsterScalar = XMVectorGetX(XMVector3Dot(vMonsterPos, vCameraRight));

	_float fScalarChai = fMonsterScalar - fPlayerScalar;

	/* R Norm * Chai */
	_vector vOffset = vCameraRight * fScalarChai * m_fAdditionalCameraOffsetRatio;
	XMStoreFloat3(&vAdditionalCameraOffset, vOffset);

	m_pCharacterCamera->Set_AdditionalCameraOffset(vAdditionalCameraOffset);
}

void CRole_Base::Update_BossCameraArm(_float _fTimeDelta)
{
	if (!m_pCharacterCamera)
		return;

	(void)_fTimeDelta;

	const _bool bHadBossCameraState =
		m_fBossCameraArmBias > ROLE_BASE_EPSILON ||
		m_fBossCameraArmHoldBias > ROLE_BASE_EPSILON ||
		XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&m_vBossCameraAdditionalOffset))) > ROLE_BASE_EPSILON;

	m_fBossCameraArmBias = 0.f;
	m_fBossCameraArmHoldTimer = 0.f;
	m_fBossCameraArmHoldBias = 0.f;
	m_vBossCameraAdditionalOffset = { 0.f, 0.f, 0.f };

	auto pMonster = m_pTargetMonster ? m_pTargetMonster->Get() : nullptr;
	if (!IsBossTargetMonster(pMonster) || pMonster->Get_Desc().fHp <= 0.f) {
		m_pCharacterCamera->Set_CameraArmLengthBias(0.f);
		if (bHadBossCameraState)
			m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);

		return;
	}

	auto pMonsterTransform = Get_MonsterTargetTransform(pMonster);
	auto pMainCamera = m_pGameInstance ? m_pGameInstance->Get_MainCamera(RENDER_LAYER::WORLD) : nullptr;
	if (!pMonsterTransform || !pMainCamera || !pMainCamera->GetTransform()) {
		m_pCharacterCamera->Set_CameraArmLengthBias(0.f);
		if (bHadBossCameraState)
			m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);

		return;
	}

	_vector vWorld = pMonsterTransform->Get_World(STATE::POSITION);
	_vector vView = XMVector3Transform(vWorld, XMLoadFloat4x4(&pMainCamera->GetViewMatrix()));
	_float fViewZ = XMVectorGetZ(vView);
	if (fViewZ < pMainCamera->Get_Near() || fViewZ > pMainCamera->Get_Far()) {
		m_pCharacterCamera->Set_CameraArmLengthBias(0.f);
		if (bHadBossCameraState)
			m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);

		return;
	}

	_vector vClip = XMVector3Transform(vView, XMLoadFloat4x4(&pMainCamera->GetProjMatrix()));
	_float fClipW = XMVectorGetW(vClip);
	if (fabsf(fClipW) < ROLE_BASE_EPSILON) {
		m_pCharacterCamera->Set_CameraArmLengthBias(0.f);
		if (bHadBossCameraState)
			m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);

		return;
	}

	_vector vNDC = vClip / fClipW;
	const _float fNdcY = XMVectorGetY(vNDC);
	constexpr _float fScreenYLimit = 1.f;
	const _float fScreenYOverflow = fabsf(fNdcY) - fScreenYLimit;

	if (fScreenYOverflow <= 0.f) {
		m_pCharacterCamera->Set_CameraArmLengthBias(0.f);
		if (bHadBossCameraState)
			m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);

		return;
	}

	const _float fHalfVisibleHeight = tanf(XMConvertToRadians(pMainCamera->Get_Fov()) * 0.5f) * fViewZ;
	const _float fWorldOverflow = max(0.f, fHalfVisibleHeight * fScreenYOverflow);
	const _float fScreenYSign = fNdcY >= 0.f ? 1.f : -1.f;

	if (fScreenYSign > 0.f) {
		m_fBossCameraArmBias = clamp(fWorldOverflow * 4.f, 0.f, 25.f);
	}
	else {
		m_fBossCameraArmBias = clamp(fWorldOverflow * 2.f, 0.f, 15.f);

		_vector vCameraUp = pMainCamera->GetTransform()->Get_World(STATE::UP);
		vCameraUp = XMVector3Normalize(vCameraUp);
		_vector vCameraOffset = vCameraUp * clamp(fWorldOverflow * 2.5f, 0.f, 12.f);
		XMStoreFloat3(&m_vBossCameraAdditionalOffset, vCameraOffset);
	}

	m_pCharacterCamera->Set_CameraArmLengthBias(m_fBossCameraArmBias);
	m_pCharacterCamera->Set_AdditionalCameraOffset(m_vBossCameraAdditionalOffset);
}

void CRole_Base::Instantiate_Effect(const _float4& _vParam)
{
	_int iEffectIndex = static_cast<_int>(_vParam.x);
	_int iEffectDetach = static_cast<_int>(_vParam.w);

	if (auto pEffectHolder = m_pObjectFilter->Get_Object<CEffectHolder>(ETOU(E_OBJECTS::EFFECT_HOLDER))) {
		pEffectHolder->Instantiate_Prefab(this, iEffectIndex, iEffectDetach);
	}
}

void CRole_Base::Run_SpeedFactor(const _float& _fSpeed)
{
	m_fSpeedFactor = _fSpeed;
}

void CRole_Base::Pause_Animation(const monostate&)
{
	auto* pCtrl_My = this->GetComponent<CAnimationController>();
	pCtrl_My->Pause();
}

void CRole_Base::Stop_Animation(const monostate&)
{
	auto* pCtrl_My = this->GetComponent<CAnimationController>();
	pCtrl_My->Stop();
}

void CRole_Base::Set_Active_Render_Children(const monostate&)
{
	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->Set_Renderer_Active(true);
		}
	}
}

void CRole_Base::Set_InActive_Render_Children(const monostate&)
{
	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->Set_Renderer_Active(false);
		}
	}
}

void CRole_Base::Disable_State(const monostate&)
{
	m_pStateController->ChangeAnimState(L"Disable", true);
}

void CRole_Base::Push_Camera(const monostate&)
{
	CAMERA_TRANSITION_DESC tCamTransDesc{};
	tCamTransDesc.eCameraOrder = CAMERA_ORDER::IN_GAME;
	tCamTransDesc.eTransitionType = CAMERA_TRANSITION::DAMPING;
	tCamTransDesc.fTransitionTime = 1.f;
	tCamTransDesc.fFov = 72.f;
	tCamTransDesc.iFlag = ETOU(CAMERA_BLEND_MASK::POS) | ETOU(CAMERA_BLEND_MASK::ROT) | ETOU(CAMERA_BLEND_MASK::FOV);
	tCamTransDesc.pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera();
	tCamTransDesc.fDampingPower = 25.f;

	m_pGameInstance->Push_Camera(tCamTransDesc);
}

void CRole_Base::Pop_Camera(const monostate&)
{
	m_pGameInstance->Pop_Camera(m_pCharacterCamera->Get_VirtualCamera());
}

void CRole_Base::Set_CameraArmLength(const _float& _fValue)
{
	m_pCharacterCamera->Set_CameraArmLength(_fValue);
}

void CRole_Base::Set_CameraArmLengthSpeed(const _float& _fValue)
{
	if (m_pCharacterCamera)
	{
		m_pCharacterCamera->Set_CameraArm_Speed(_fValue);
	}
}

void CRole_Base::Set_CameraObjectOffset(const _float3& _vValue)
{
	m_pCharacterCamera->Set_ObjectOffset(_vValue);
}

void CRole_Base::Push_Camera_Shake(const _int& _iPreset)
{
	if (m_pCharacterCamera == nullptr)
		return;

	if (auto pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera())
		pVirtualCamera->Push_Shake_Event(_iPreset);
}

void CRole_Base::Push_Camera_Shake_Intensity(const _float2& _fValue)
{
	if (m_pCharacterCamera == nullptr)
		return;

	if (auto pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera())
		pVirtualCamera->Push_Shake_IntensityEvent(_fValue);
}

void CRole_Base::Clear_Camera_Shake(const _float& _fBlendTime)
{
	if (m_pCharacterCamera == nullptr)
		return;

	if (auto pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera())
		pVirtualCamera->Clear_Shake(_fBlendTime);
}

void CRole_Base::Set_DestFov(const _float& _fFov)
{
	if (m_pCharacterCamera == nullptr)
		return;

	m_pCharacterCamera->Set_DestFov(_fFov);
}

void CRole_Base::Set_DestFovMultiplier(const _float2& _fValue)
{
	if (m_pCharacterCamera == nullptr)
		return;

	m_pCharacterCamera->Set_DestFovMultiplier(_fValue);
}

void CRole_Base::Reset_DestFov(const monostate&)
{
	if (m_pCharacterCamera == nullptr)
		return;

	m_pCharacterCamera->Reset_DestFov();
}

Json::Value CRole_Base::Serialize() {
	auto jsonResult = __super::Serialize();

	//jsonResult["m_tRoleDesc"] = m_tRoleDesc;

	return jsonResult;
}

void CRole_Base::Deserialize(Json::Value& _jsonValue) {
	_jsonValue;
}

void CRole_Base::OnGui_Object_Inspector_Context()
{
	__super::OnGui_Object_Inspector_Context();

	ImGui::Separator();

	_bool bGrounded = m_bGrounded;
	ImGui::Checkbox("Is Grounded", &bGrounded);

	if (ImGui::CollapsingHeader("Role Desc", ImGuiTreeNodeFlags_DefaultOpen))
	{
		Get_Desc().OnGui_Inspector_Context();
		ImGui::DragFloat2("T", &Get_Desc().arrCooldown[ENUM_TO_UINT(COOLDOWN_TYPE::SKILL_T)].x);
		ImGui::DragFloat2("E", &Get_Desc().arrCooldown[ENUM_TO_UINT(COOLDOWN_TYPE::SKILL_E)].x);
		ImGui::DragFloat2("Q", &Get_Desc().arrCooldown[ENUM_TO_UINT(COOLDOWN_TYPE::SKILL_Q)].x);
		ImGui::DragFloat2("R", &Get_Desc().arrCooldown[ENUM_TO_UINT(COOLDOWN_TYPE::SKILL_R)].x);
		ImGui::DragFloat2("QTE", &Get_Desc().arrCooldown[ENUM_TO_UINT(COOLDOWN_TYPE::SWITCH)].x);
	}

	if (auto pTarget = m_pTargetMonster->Get()) {
		ImGui::Text("Target : %s", pTarget->GetName().c_str());
	}

	if (ImGui::CollapsingHeader("Move Limit Delta Test", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto DrawEasingCombo = [](const char* _szLabel, E_EASING& _eValue)
			{
				std::string strPreview = std::string(magic_enum::enum_name(_eValue));

				if (ImGui::BeginCombo(_szLabel, strPreview.c_str()))
				{
					for (E_EASING eValue : magic_enum::enum_values<E_EASING>())
					{
						if (eValue == E_EASING::COUNT)
							continue;

						std::string strName = std::string(magic_enum::enum_name(eValue));
						_bool bSelected = (_eValue == eValue);

						if (ImGui::Selectable(strName.c_str(), bSelected))
							_eValue = eValue;

						if (bSelected)
							ImGui::SetItemDefaultFocus();
					}

					ImGui::EndCombo();
				}
			};

		DrawEasingCombo("Enter Easing", m_eEnterEasing);
		DrawEasingCombo("Leave Easing", m_eLeaveEasing);

		ImGui::DragFloat("Enter Time", &m_fDeltaEnterTime, 0.001f, 0.f, 1.f, "%.3f");
		ImGui::DragFloat("Duration", &m_fDeltaDuration, 0.001f, 0.f, 2.f, "%.3f");
		ImGui::DragFloat("Leave Time", &m_fDeltaLeaveTime, 0.001f, 0.f, 1.f, "%.3f");

		ImGui::DragFloat("Target Delta", &m_fDeltaTarget, 0.001f, 0.01f, 1.f, "%.3f");

		ImGui::Text("Press ENTER to preview"); ImGui::Spacing();
		if (m_pGameInstance->Key_Down(DIK_RETURN))
		{
			Apply_DeltaAnim(m_eEnterEasing, m_eLeaveEasing, m_fDeltaTarget, m_fDeltaEnterTime, m_fDeltaDuration, m_fDeltaLeaveTime);
		}
	}

	ImGui::DragFloat("m_fAdditionalCameraOffsetRatio", &m_fAdditionalCameraOffsetRatio, 0.01f);
}

void CRole_Base::Free() {
	__super::Free();

	Safe_Release(m_pTargetMonster);
	Safe_Release(m_pParaglider);
	Safe_Release(m_pHandBone);
	Safe_Release(m_pGrappleObj);
	Safe_Release(m_pInteractionObj);
	Safe_Release(m_pWeapon);
	Safe_Release(m_pScabbard);
	Safe_Release(m_pBackHolder); 
	
	Safe_Release(m_pVehicle);

	Safe_Release(m_pSeqCam);
	Safe_Release(m_pBurstVC);
}

void CRole_Base::Update_Role_Desc(_float _fTimeDelta)
{
	/* Stamina */
	if (!State_Check(SPRINT))
		Get_Desc().Update_Stamina(_fTimeDelta);

	/* Cooldown */
	Get_Desc().Update_Cooldown(_fTimeDelta);

	/* Heal */
	_float fHeal = Get_Desc().Update_Heal(_fTimeDelta);
	if (fHeal > 0.f)
	{
		m_fHealFontValue += fHeal;
		if (Get_Desc().vHeal.w <= 0.f)
			Get_Desc().vHeal.w = m_pGameInstance->Random(0.22f, 0.3f);

		Get_Desc().vHeal.w -= _fTimeDelta;
		
		if (Get_Desc().vHeal.w <= 0.f)
		{
			Show_HealFont(m_fHealFontValue);
			m_fHealFontValue = 0.f;
			Get_Desc().vHeal.w = m_pGameInstance->Random(0.22f, 0.3f);
		}
	}
	if (Get_Desc().vHeal.z <= 0.f && m_fHealFontValue > 0.f)
	{
		Show_HealFont(m_fHealFontValue);
		m_fHealFontValue = 0.f;
	}
}

_bool CRole_Base::Assulted(CCharacter* _pAssulter, const ATTACK_DESC* _pAttackDesc)
{
	if (!m_bIsMainRole)
		return false;

	if (m_bDebug_Invincible)
		return false;

	if (Check_Movestate())
	{
		m_bIFrames = true;
		m_fIFramesTimer.x = m_fIFramesTimer.y;

		if (m_pStateController->GetCurStateName() == L"Move_Limit")
			return true;

		/* Reset */
		_bool bHasInput = Has_RunInput();
		m_bHasInput = bHasInput;
		m_pAnimationStateMachine->Set_Int(hState, m_iState);
		m_pAnimationStateMachine->Set_Bool(hHasInput, bHasInput);
		m_pAnimationStateMachine->Set_Float(hInput_X, bHasInput ? m_iInput_X : 0.f);
		m_pAnimationStateMachine->Set_Float(hInput_Z, bHasInput ? m_iInput_Z : 0.f);

		m_pStateController->ChangeAnimState(L"Move_Limit");
		return false;
	}

	if (m_bIFrames)
		return false;

	if (Get_Desc().fHp <= 0.f)
		return false;

	_float fDamage = _pAttackDesc->fBaseDamage * _pAttackDesc->fDamageMultiplier;

	fDamage += rand() % static_cast<_int>(fDamage * 0.25f);

	if (fDamage <= 0.f)
		return false;

	Get_Desc().fHp -= fDamage;
	Get_Desc().fHp = clamp(Get_Desc().fHp, 0.f, Get_Desc().fMaxHp);

	// Locomotion 일 때만, Behit 처리
	if (Is_LocomotionState())
		m_pStateController->ChangeAnimState(L"Behit", true);

	/* UI */
	if (auto pUIManager = CUI_Manager::GetInstance()) {
		DAMAGE_FONT_INFO tDesc{};

		tDesc.fDamage = fDamage;

		_float fOffset = 0.5f;
		if (auto pHitCase = m_pHitCase->Get()) {
			tDesc.vWordlPos = pHitCase->GetTransform()->GetWorldPosition();
			fOffset = 0.5f;	
		}
		else {
			tDesc.vWordlPos = m_pTransform->GetWorldPosition();
			fOffset = 1.2f;
		}

		_vector vBaseUp = XMLoadFloat3(&m_pTransform->Get_BaseUp());
		_vector vDamagePos = XMLoadFloat3(&tDesc.vWordlPos) + vBaseUp * fOffset; // 살짝 띄우기
		XMStoreFloat3(&tDesc.vWordlPos, vDamagePos);

		tDesc.eStyle = _pAttackDesc->bCritical ? DAMAGE_FONT_STYLE::Critical : DAMAGE_FONT_STYLE::Normal;
		tDesc.eElement = DAMAGE_FONT_ELEMENT::Light;

		pUIManager->Set_DamageFont(tDesc);
	}

	return true;
}
