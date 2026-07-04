#include "ContentPCH.h"
#include "CRole_GD.h"
#include "CSkinnedMeshRenderer.h"
#include <CCharacterCamera.h>

CRole_GD::CRole_GD(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CRole_AimisiGD(_pDevice, _pContext)
{
	m_tRoleDesc.fDamage = 1020.f;
}

HRESULT CRole_GD::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_GD::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CRole_GD::Start()
{
	__super::Start();

	Register_Type();

	m_pStateController->ChangeAnimState(L"Disable", true);
}

void CRole_GD::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);

	Recover_HoverTime(_fTimeDelta);
}

void CRole_GD::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CRole_GD::Init_Push_Camera()
{
	/* GD */
	m_pCharacterCamera->Set_ObjectOffset(_float3(0.f, 2.f, 0.f));
	m_pCharacterCamera->Set_CameraArmLength(7.f);
	m_pGameInstance->Push_Camera(m_pCharacterCamera->Get_VirtualCamera());
	m_pGameInstance->Pop_Camera(m_pCharacterCamera->Get_VirtualCamera());
}

void CRole_GD::Update_Ascend(_float _fTimeDelta)
{
 	if (!m_pCCT)
		return;
}

void CRole_GD::Update_Hover(_float _fTimeDelta)
{
	auto pTransform = this->GetTransform();
	if (!pTransform)
		return;
}

void CRole_GD::Update_Descend(_float _fTimeDelta)
{
	if (!m_pCCT)
		return;
}

void CRole_GD::Reset_CameraSettings()
{
	if (!m_pCharacterCamera)
		return;

	m_pCharacterCamera->Set_ObjectOffset(_float3{ 0.f, 2.f, 0.f });
	m_pCharacterCamera->Set_CameraArmLength(7.f);
	m_pCharacterCamera->Set_CameraArm_Speed(DEFAULT_ARM_SPEED);
	m_pCharacterCamera->Reset_DestFov();
}

void CRole_GD::Init_AttackRootMotionLimit()
{
	m_fRootMotionLimitDistance = 1.5f;
	m_fRootMotionLimitDistance = m_fDefaultRootMotionLimitDistance;
}

void CRole_GD::Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate)
{
	__super::Init_Change_State_Attack_Combo(0.5f, 0.65f, 0.3f);
}

void CRole_GD::Init_Change_State_Jump()
{
	auto funcAscend = [this]() { return m_pGameInstance->Key_Down(DIK_SPACE) && m_fHoverEnableTime >= 1.f; };
	auto funcAscendEnd = [this]() { return !m_pGameInstance->Key_Pressed(DIK_SPACE); };
	auto funcDescend = [this]() { return m_pGameInstance->Key_Pressed(DIK_LCONTROL); };
	
	m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Ascend", funcAscend, true));
	m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Ascend", funcAscend, true));
	m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Ascend", funcAscend, true));
	m_pStateController->InsertChangeStatement(L"Sprint", new CChangeStatement(L"Ascend", funcAscend, true));
	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Ascend", funcAscend, true));

	m_pStateController->InsertChangeStatement(L"Ascend", new CChangeStatement(L"Hover", funcAscendEnd, true));
	
	auto funcAscendAgain = [this]() {
		return m_pGameInstance->Key_Pressed(DIK_SPACE) && m_fHoverEnableTime >= 1.f;
		};
	m_pStateController->InsertChangeStatement(L"Hover", new CChangeStatement(L"Ascend", funcAscendAgain, true));
	
	m_pStateController->InsertChangeStatement(L"Hover", new  CChangeStatement(L"Descend", funcDescend, true));
	m_pStateController->InsertChangeStatement(L"Hover", new CChangeStatement(L"Descend", [this]() { return m_fHoverEnableTime <= 1.f; }, true));
	 
	m_pStateController->InsertChangeStatement(L"Descend", new CChangeStatement(L"Hover", 
		[this]() { return (!m_pGameInstance->Key_Pressed(DIK_LCONTROL) && m_fHoverEnableTime > 0.f); }, true));
	
	m_pStateController->InsertChangeStatement(L"Descend", new CChangeStatement(L"Idle", [this]() { return m_bGrounded; }, true));
	m_pStateController->InsertChangeStatement(L"Ascend", new CChangeStatement(L"Idle", [this]() { return m_bGrounded && !m_pGameInstance->Key_Pressed(DIK_SPACE); }, true));
	m_pStateController->InsertChangeStatement(L"Hover", new CChangeStatement(L"Idle", [this]() { return m_bGrounded; }, true));

	auto funcRun = [this]() { return Has_RunInput(); };
	auto funcSprint = [this]() {
		return !m_bMoveLocked && Get_Desc().Check_Stamina(1)
			&& ((m_pGameInstance->Key_Down(DIK_LSHIFT) || m_pGameInstance->Mouse_Down(DI_MB::RBUTTON))
				&& (Has_RunInput()));
		};
	m_pStateController->InsertChangeStatement(L"Hover", new CChangeStatement(L"Move", funcSprint, true));
	m_pStateController->InsertChangeStatement(L"Move", new CChangeStatement(L"Sprint", funcRun, true));

	m_pStateController->InsertChangeStatement(L"Sprint", new CChangeStatement(L"Hover", [this]() { return m_bHovering; }, true));
}

void CRole_GD::Init_Change_State_Fly()
{	
}

void CRole_GD::Init_Run_State()
{
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

void CRole_GD::Init_Sprint_State()
{
	auto pWalk = AnimTransform::Create(0.f);
	pWalk->funcStarted = [this]() {
		Add_State(SPRINT);

		Flush_Forced_Return_EffectPool();
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 7.f, 0.5f, false });
		};
	pWalk->funcUpdate = [this](_float _fTimeDelta) {
		if (!Get_Desc().Use_Stamina(5.f * _fTimeDelta))
		{
			m_pStateController->ChangeAnimState(m_bHovering ? L"Hover" : L"Run", true);
			return;
		}

		if (m_bHovering)
		{
			_float3 vPos = m_pCCT->Get_Position();
			vPos.y = m_fHoverHeight;
			m_pCCT->Set_UseGravity(false);
			m_pCCT->Clear_VerticalVelocity();
			Set_JumpVelocity_Y(0.f);
			m_pCCT->Set_Position(vPos);

			Update_Locomotion(_fTimeDelta);
			Turn(_fTimeDelta * 2.f);
		}
		else
		{
			Update_Locomotion(_fTimeDelta);
			Turn(_fTimeDelta * 2.f);
			Update_AirMotion(_fTimeDelta);
		}
		};
	pWalk->funcEnded = [this]() {
		Remove_State(SPRINT);
		};

	m_pAnimator->PushAnimInfo(L"Sprint", pWalk);
}

void CRole_GD::Init_Move_State()
{
	auto pMove = AnimTransform::Create(0.f);
	pMove->funcStarted = [this]() {
		Add_State(MOVE);

		Flush_Forced_Return_EffectPool();
		Reset_ControlCameraSettings();

		m_bIFrames = true;
		m_fIFramesTimer.x = m_fIFramesTimer.y;

		if (Get_Desc().Use_Stamina(1))
		{
			Lock_Move(m_fMoveCoolDown);
		}
		};
	pMove->funcUpdate = [this](_float _fTimeDelta) {

		if (m_fIFramesTimer.x > 0.f)
		{
			m_fIFramesTimer.x = max(0.f, m_fIFramesTimer.x - _fTimeDelta);
			m_bIFrames = (m_fIFramesTimer.x > 0.f);
		}

		if (m_bHovering)
		{
			_float3 vPos = m_pCCT->Get_Position();
			vPos.y = m_fHoverHeight;
			m_pCCT->Set_UseGravity(false);
			m_pCCT->Clear_VerticalVelocity();
			Set_JumpVelocity_Y(0.f);
			m_pCCT->Set_Position(vPos);
		}
		else
		{
			Update_AirMotion(_fTimeDelta);
		}

		};
	pMove->funcEnded = [this]() {
		Remove_State(MOVE);
		};

	m_pAnimator->PushAnimInfo(L"Move", pMove);
}

void CRole_GD::Init_Execute_State()
{
	auto pExecute = AnimTransform::Create(2.f);
	pExecute->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::EXECUTE);

		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Keep_DistanceToTarget();

		/* 1. Delta 조정 */
		Apply_DeltaAnim(E_EASING::SMOOTHSTEP, E_EASING::SMOOTHSTEP, 0.15f, 0.25f, 3.f, 0.25f, false, true);

		/* 3. Player 를 Monster 방향으로 */
		if (auto pMonster = m_pTargetMonster->Get()) {
			m_pTransform->Turn(pMonster->GetTransform()->Get_World(STATE::POSITION), XMLoadFloat3(&m_pTransform->Get_BaseUp()));
		}

		};
	pExecute->funcUpdate = [this](_float _fTimeDelta) {
		};
	pExecute->funcEnded = [this]() {
		/* 7. 정리 */
		if (m_pCharacterCamera) {
			m_pCharacterCamera->Unlock_MouseInput();
		}

		Set_RootMotionLimit(true);
		this->m_pAnimationController->Set_ApplyRootMotion(true);
		this->m_pAnimationController->Set_ApplyRootMotionX(true);
		this->m_pAnimationController->Set_ApplyRootMotionZ(true);

		Remove_State(EXECUTE);
		};

	m_pAnimator->PushAnimInfo(L"Execute", pExecute);
}

void CRole_GD::Init_Jump_Start_State()
{
	/* Ascend */
	auto pAscend = AnimTransform::Create(0.f);
	pAscend->funcStarted = [this]() {
 		Add_State(JUMP_START);

		Flush_Forced_Return_EffectPool();

		m_bGrounded = false;

		/* Velocity */
		m_pCCT->Set_UseGravity(false);
		m_pCCT->Clear_VerticalVelocity();
		Set_JumpVelocity_Y(0.f);

		m_bHovering = true;

		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 11.f, 0.4f, false });
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 20.f, 0.4f, false });
		};
	pAscend->funcUpdate = [this](_float _fTimeDelta) {

		m_pCCT->Set_UseGravity(false);
		m_pCCT->Clear_VerticalVelocity();
		Set_JumpVelocity_Y(0.f);

		_float fAscendSpeed = 8.f;
		m_pCCT->Add_Move(_float3 { 0.f, fAscendSpeed * _fTimeDelta, 0.f });

		_bool bHasRunInput = Has_RunInput();
		m_bHasInput = bHasRunInput;
		if (Has_RunInput())
		{
			m_fSpeedFactor = 2.f;
			Update_Locomotion(_fTimeDelta);
			Turn(_fTimeDelta);
		}
		else
		{
			m_pCCT->Clear_PlanarVelocity();
			XMStoreFloat3(&m_vTargetDirection, XMVectorZero());
		}

		};
	pAscend->funcEnded = [this]() {
		Remove_State(JUMP_START);

		m_pCCT->Clear_VerticalVelocity();
		Set_JumpVelocity_Y(0.f);
		};
	
	m_pAnimator->PushAnimInfo(L"Ascend", pAscend);
}

void CRole_GD::Init_Jump_Loop_State()
{
	auto pLoop = AnimTransform::Create(0.f);
	pLoop->funcStarted = [this]() {
		Add_State(JUMP_LOOP);

		m_fHoverHeight = m_pCCT->Get_Position().y;
		m_pCCT->Set_UseGravity(false);
		m_pCCT->Clear_VerticalVelocity();

		m_bHovering = true;
		};
	pLoop->funcUpdate = [this](_float _fTimeDelta) {

		_float3 vPos = m_pCCT->Get_Position();
		vPos.y = m_fHoverHeight;

		m_pCCT->Set_UseGravity(false);
		m_pCCT->Clear_VerticalVelocity();
		m_pCCT->Set_Position(vPos);

		_bool bHasRunInput = Has_RunInput();
		m_bHasInput = bHasRunInput;
		if (Has_RunInput())
		{
			m_fSpeedFactor = 2.f;
			Update_Locomotion(_fTimeDelta);
			Turn(_fTimeDelta);
		}
		else
		{
			m_pCCT->Clear_PlanarVelocity();
			XMStoreFloat3(&m_vTargetDirection, XMVectorZero());
		}

		};
	pLoop->funcEnded = [this]() {
		Remove_State(JUMP_LOOP);
		};

	m_pAnimator->PushAnimInfo(L"Hover", pLoop);
}

void CRole_GD::Init_Jump_Land_State()
{
	auto pLand = AnimTransform::Create(0.f);
	pLand->funcStarted = [this]() {
		Add_State(JUMP_LAND);

		Flush_Forced_Return_EffectPool();

		m_pCCT->Set_UseGravity(true);
		m_pCCT->Clear_VerticalVelocity();

		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 13.f, 0.4f, false });
		Play_Sound(E_OBJECTS::SOUND_FILTER_VOX, { 23.f, 0.4f, false });
		};
	pLand->funcUpdate = [this](_float _fTimeDelta) {
		
		m_pCCT->Set_UseGravity(true);

		_float fDescendSpeed = -15.f;
		_float fDescendLerpSpeed = 5.f;

		_float fcurVelocityY = m_pCCT->Get_Velocity().y;
		fcurVelocityY += (fDescendSpeed - fcurVelocityY) * fDescendLerpSpeed * _fTimeDelta;

		Set_JumpVelocity_Y(fcurVelocityY);
		Update_AirMotion(_fTimeDelta);

		Update_Locomotion(_fTimeDelta);
		Turn(_fTimeDelta);

		};
	pLand->funcEnded = [this]() {
		Remove_State(JUMP_LAND);

		m_pCCT->Clear_VerticalVelocity();
		
		/* RootMotion */
		m_pAnimationController->Set_SubRootMotion_Y(true);

		m_bHovering = false;
		};

	m_pAnimator->PushAnimInfo(L"Descend", pLand);
}

void CRole_GD::Init_Fly_Start_State()
{
}

void CRole_GD::Init_Fly_Loop_State()
{
}

void CRole_GD::Init_Attack01_State(_float _fEndTime)
{
	_float fAnimFullTime = 6.333f;
	_float fAnimEndTime = fAnimFullTime * 0.15f;
	__super::Init_Attack01_State(fAnimEndTime);
}

void CRole_GD::Init_Attack02_State(_float _fEndTime)
{
	_float fAnimFullTime = 8.f;
	_float fAnimEndTime = fAnimFullTime * 0.1f;
	__super::Init_Attack02_State(fAnimEndTime);
}

void CRole_GD::Init_Attack03_State(_float _fEndTime)
{
	_float fAnimFullTime = 9.667f;
	_float fAnimEndTime = fAnimFullTime * 0.27f;
	__super::Init_Attack03_State(fAnimEndTime);
}

void CRole_GD::Init_Attack04_State(_float _fEndTime)
{
	_float fAnimFullTime = 8.167f;
	_float fAnimEndTime = fAnimFullTime * 0.2f;
	__super::Init_Attack04_State(fAnimEndTime);
}

void CRole_GD::Init_Skill03_State(_float _fEndTime)
{
	_fEndTime = 4.f;
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Begin_SkillCutsceneMonsterFreeze();

		Flush_Forced_Return_EffectPool();

		Reset_Locomotion();

		Add_State(CHARACTER_STATE::SKILL_03);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		Get_Desc().Spend_Identity_Energy();

		/* refactor : 명시적 디더링 복구 (건담) */
		this->m_bSwitchingOut = false;
		this->m_bSwitchingIn = false;
		this->m_bDitheringStarted = false;
		this->m_vDitheringTimer.x = 0.f;
		this->Set_Disslove(0.f);
		
		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Clear_AllInput();
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_03);
		
		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;

		End_SkillCutsceneMonsterFreeze();
		};

	m_pAnimator->PushAnimInfo(L"Skill_03", pSkill);
}

void CRole_GD::Init_Skill04_State(_float _fEndTime)
{
	_fEndTime = 5.f;
	auto pSkill = AnimTransform::Create(5.f);
	pSkill->funcStarted = [this, _fEndTime]() {
		Begin_SkillCutsceneMonsterFreeze();

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_04);

		/* refactor : 명시적 디더링 복구 (건담) */
		this->m_bSwitchingOut = false;
		this->m_bSwitchingIn = false;
		this->m_bDitheringStarted = false;
		this->m_vDitheringTimer.x = 0.f;
		this->Set_Disslove(0.f);

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

		End_SkillCutsceneMonsterFreeze();
		};

	m_pAnimator->PushAnimInfo(L"Skill_04", pSkill);
}

void CRole_GD::Init_Burst01_State(_float _fEndTime)
{
	__super::Init_Burst01_State(8.533f);
}

void CRole_GD::Init_Switch_QTE_State(_float _fEndTime)
{
	__super::Init_Switch_QTE_State(1.f);
}

void CRole_GD::Init_Attack_Air_Start_State(_float _fEndTime)
{
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_AIR_START);

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_bAirAttackDiveApplied = false;

		m_bHovering = false;

		m_pCCT->Set_UseGravity(true);
		m_pCCT->Clear_VerticalVelocity();
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
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::ATTACK_AIR_START);
		};

	m_pAnimator->PushAnimInfo(L"Attack_Air_Start", pAttack);
}

void CRole_GD::Init_Attack_Air_End_State(_float _fEndTime)
{
	_fEndTime = 0.5f;
	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_AIR_END);
		Reset_Locomotion();
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		};
	pAttack->funcEnded = [this]() {
		Remove_State(CHARACTER_STATE::ATTACK_AIR_END);
		};

	m_pAnimator->PushAnimInfo(L"Attack_Air_End", pAttack);
}

void CRole_GD::Recover_HoverTime(_float _fTimeDelta)
{
	if (m_bGrounded)
	{
		m_fHoverEnableTime += m_fHoverUseTime * 0.5f * _fTimeDelta;
		m_fHoverEnableTime = min(m_fHoverEnableTime, 100.f);
	}
}

void CRole_GD::Switch_In(CRole_Base* _pPrevRole)
{
	this->m_bSwitchingOut = false;
	this->m_bSwitchingIn = false;
	this->m_bDitheringStarted = false;
	this->m_vDitheringTimer.x = 0.f;
	this->Set_Disslove(0.f);
}

void CRole_GD::Switch_Out()
{
	m_bIsMainRole = false;

	if (!m_bSwitchOutComboChain)
		Clear_AllInput();

	Reset_Locomotion();
	Reset_Input(0.f);

	m_eLayer = OBJECT_LAYER::SUB_PLAYER;
	for (auto pRendererWrapper : m_listRenderer) {
		if (auto pRenderer = pRendererWrapper->Get()) {
			pRenderer->GetOwner()->Set_Layer(OBJECT_LAYER::SUB_PLAYER);
		}
	}

	if (m_pCharacterCamera)
	{
		m_pCharacterCamera->SetActive(false);
		if (auto* pVirtualCamera = m_pCharacterCamera->Get_VirtualCamera())
			m_pGameInstance->Pop_Camera(pVirtualCamera);
	}

	/* Notify */
	m_bSwitchingOut = true;
	m_bSwitchingIn = false;
	m_bDitheringStarted = false;
	m_vDitheringTimer.x = 0.f;
	m_vDitheringTimer.y = Get_SwitchDitheringTime();
}

void CRole_GD::Free()
{
	__super::Free();
}
