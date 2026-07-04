#include "ContentPCH.h"
#include "CRole_Aimisi.h"
#include <CCharacterCamera.h>
#include <CRole_GD.h>
#include <CUI_Manager.h>

CRole_Aimisi::CRole_Aimisi(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CRole_AimisiGD(_pDevice, _pContext)
{
	m_tRoleDesc.fDamage = 1020.f;
	m_tRoleDesc.eElement = DAMAGE_FONT_ELEMENT::Fire;

	m_fSwitchInTargetRadius = 3.5f;
}

HRESULT CRole_Aimisi::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_Aimisi::Awake");
		return E_FAIL;
	}

	m_pPrefabFilter = CreateComponent<CPrefabFilter>();

	return S_OK;
}

void CRole_Aimisi::Start()
{
	__super::Start();

	m_wAimisiObj->Set(this);
	if (auto pPrefab = m_pPrefabFilter->Get_Prefab(0)) {
		auto pGBObj = pPrefab->Instantiate_Prefab()->As<CRole_GD>();
		pGBObj->SetActive(true);

		m_wGDObj->Set(pGBObj);
		pGBObj->m_wGDObj->Set(pGBObj);
		pGBObj->m_wAimisiObj->Set(this);
	}

	Register_Type();
}

void CRole_Aimisi::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CRole_Aimisi::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CRole_Aimisi::Init_AttackRootMotionLimit()
{
	m_fRootMotionLimitDistance = 3.f;
	m_fRootMotionLimitDistance = m_fDefaultRootMotionLimitDistance;
}

void CRole_Aimisi::Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate)
{
	// 다음 공격으로 캔슬 가능한 최소 시간
	__super::Init_Change_State_Attack_Combo(0.5f, 0.5f, 0.75f);
}

void CRole_Aimisi::Init_Attack01_State(_float _fEndTime)
{
	_float fAnimFullTime = 5.233f;
	_float fAnimEndTime = fAnimFullTime * 0.15f;
	
	auto pAttack = AnimTransform::Create(fAnimEndTime);
	pAttack->funcStarted = [this]() {

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_fWeaponVisualizeDurtaion = 4.f;
		Set_Weapon_Visual_Instant({ 1 });
		Set_Scabbard_Visual_Instant({ 1 });

		Add_State(CHARACTER_STATE::ATTACK_01);
		m_iComboIndex = 1;
		m_bAttackQueued = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		m_fWeaponVisualizeDurtaion = 4.f;
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio > m_fEndRatio) {
			if (m_bComboInput) {
				m_pStateController->ChangeAnimState(L"Attack_02", true);
			}
			else {
				auto pAnimController = m_pAnimationStateMachine->Get_AnimationController();
				if (pAnimController) {
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
		};

	m_pAnimator->PushAnimInfo(L"Attack_01", pAttack);
}

void CRole_Aimisi::Init_Attack02_State(_float _fEndTime)
{
	_float fAnimFullTime = 10.6f;
	_float fAnimEndTime = fAnimFullTime * 0.1f;
	_fEndTime = fAnimEndTime;

	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_fWeaponVisualizeDurtaion = 3.f;
		Set_Weapon_Visual_Instant({ 1 });
		Set_Scabbard_Visual_Instant({ 1 });

		Add_State(CHARACTER_STATE::ATTACK_02);
		m_iComboIndex = 2;
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
			m_pStateController->ChangeAnimState(L"Attack_03", true);
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_02);
		m_iComboIndex = 0;
		m_bComboInput = false;

		this->Set_Disslove(0); // Active

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_02", pAttack);
}

void CRole_Aimisi::Init_Attack03_State(_float _fEndTime)
{
	_float fAnimFullTime = 17.567f;
	_float fAnimEndTime = fAnimFullTime * 0.04f;
	__super::Init_Attack03_State(fAnimEndTime);
}

void CRole_Aimisi::Init_Attack04_State(_float _fEndTime)
{
	_float fAnimFullTime = 14.567f;
	_float fAnimEndTime = fAnimFullTime * 0.1f;
	__super::Init_Attack04_State(fAnimEndTime);
}

void CRole_Aimisi::Init_Skill01_State(_float _fEndTime)
{
	__super::Init_Skill01_State(1.f);
}

void CRole_Aimisi::Init_Skill02_State(_float _fEndTime)
{
	__super::Init_Skill02_State(1.f);
}

void CRole_Aimisi::Init_Skill03_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(3.5f);
	pSkill->funcStarted = [this, _fEndTime]() {
		Begin_SkillCutsceneMonsterFreeze();

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_03);

		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(false);
			pUIManager->Set_BossHpBar_Active(false);
		}

		m_fWeaponVisualizeDurtaion = 0.f;
		Set_Weapon_Visual_Instant({ 0 });
		Set_Scabbard_Visual_Instant({ 0 });

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pVC = m_pBurstVC->Get()) {
				Debug_Output(m_pTransform->Get_BaseUp(), "\n");
				pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
			}
		}

		if (auto pCom = m_wGDObj->Get()) {
			if (auto pFilt = pCom->GetComponent<CObjectFilter>()) {
				if (auto pSQCamObj = pFilt->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
					if (auto pVC = m_pGD->m_pBurstVC->Get()) {
						Debug_Output(m_pTransform->Get_BaseUp(), "\n");
						pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
					}
				}
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

		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(true);
			pUIManager->Set_BossHpBar_Active(true);
		}

		m_vCharacterVelocity = { 0.f, 0.f, 0.f };
		m_pCCT->Clear_VerticalVelocity();
		m_pCCT->Set_UseGravity(true);

		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;

		End_SkillCutsceneMonsterFreeze();
		};

	m_pAnimator->PushAnimInfo(L"Skill_03", pSkill);


}

void CRole_Aimisi::Init_Skill04_State(_float _fEndTime)
{
	auto pSkill = AnimTransform::Create(10.f);
	pSkill->funcStarted = [this, _fEndTime]() {
		Begin_SkillCutsceneMonsterFreeze();

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();	
		
		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(false);
			pUIManager->Set_BossHpBar_Active(false);
		}

		Add_State(CHARACTER_STATE::SKILL_04);

		m_pCCT->Set_Tag(COLLIDER_TAG::PLAYER_EVADE);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		Get_Desc().Spend_Identity_Energy();

		if (auto pSQCamObj = m_pObjectFilter->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
			if (auto pVC = m_pBurstVC->Get()) {
				Debug_Output(m_pTransform->Get_BaseUp(), "\n");
				pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
			}
		}

		if (auto pCom = m_wGDObj->Get()) {
			if (auto pFilt = pCom->GetComponent<CObjectFilter>()) {
				if (auto pSQCamObj = pFilt->Get_Object(E_OBJECTS::SKILL_CAMERA_RIG)) {
					if (auto pVC = m_pGD->m_pBurstVC->Get()) {
						Debug_Output(m_pTransform->Get_BaseUp(), "\n");
						pVC->GetOwner()->GetTransform()->Set_BaseUp(m_pTransform->Get_BaseUp());
					}
				}
			}
		}


		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		m_pCCT->Set_Tag(COLLIDER_TAG::PLAYER);
		Remove_State(SKILL_04);

		if (auto pUIManager = CUI_Manager::GetInstance()) {
			pUIManager->Set_AllUI_Active(true);
			pUIManager->Set_BossHpBar_Active(true);
		}

		m_vCharacterVelocity = { 0.f, 0.f, 0.f };
		m_pCCT->Clear_VerticalVelocity();
		m_pCCT->Set_UseGravity(true);

		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);

		this->Set_Disslove(0); // Active

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;

		End_SkillCutsceneMonsterFreeze();
	};

	m_pAnimator->PushAnimInfo(L"Skill_04", pSkill);
}

void CRole_Aimisi::Init_Attack_Air_End_State(_float _fEndTime)
{
	_float fAnimFullTime = 8.f;
	_float fAnimEndTime = fAnimFullTime * 0.1f;
	__super::Init_Attack_Air_End_State(fAnimEndTime);
}

void CRole_Aimisi::Init_Burst01_State(_float _fEndTime)
{
	__super::Init_Burst01_State(6.833f);
}

void CRole_Aimisi::Init_Switch_QTE_State(_float _fEndTime)
{
	__super::Init_Switch_QTE_State(1.f);
}

void CRole_Aimisi::Switch_In(CRole_Base* _pPrevRole) {

	/* Set Aimisi, Out GD */
	Switch_Type(E_AIMISI_TYPE::AIMISI);

	__super::Switch_In(_pPrevRole);

	if (m_pGD)
	{
		///* refactor : 명시적 디더링 복구 (건담) */
		m_pGD->Switch_In(_pPrevRole);

		Sync_AimisiTransformToGD({});
	}
}

void CRole_Aimisi::Switch_Out() {

	if (m_pGD)
		m_pGD->Switch_Out();

	/* All Out */
	__super::Switch_Out();

	Switch_Type_ForSwitchOut();
}

void CRole_Aimisi::Prepare_Switch_Out()
{
	/* Switch_Out is too late; Switch_In(next role) already reads the previous position. */
	/* Late_Update sync affects CCT */
	if (m_pActiveType == m_pGD)
		Sync_GDTransformToAimisi({});
}

void CRole_Aimisi::Free()
{
	__super::Free();
}
