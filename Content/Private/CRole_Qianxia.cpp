#include "ContentPCH.h"
#include "CRole_Qianxia.h"
#include <CCharacterCamera.h>
#include <CGameManager.h>

CRole_Qianxia::CRole_Qianxia(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CRole_Base(_pDevice, _pContext)
{
	m_tRoleDesc.fDamage = 680.f;

	m_fSwitchInTargetRadius = 1.2f;

	m_tRoleDesc.eElement = DAMAGE_FONT_ELEMENT::Dark;
}

HRESULT CRole_Qianxia::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_Qianxia::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CRole_Qianxia::Start() {
	__super::Start();
}

void CRole_Qianxia::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CRole_Qianxia::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

CRole_Base::EXECUTE_CAMERA_DESC CRole_Qianxia::Get_ExecuteCameraDesc() const
{
	EXECUTE_CAMERA_DESC tDesc = __super::Get_ExecuteCameraDesc();

	tDesc.vStartCameraOffset = { -0.25f, -0.2f, -0.85f };
	tDesc.vStartObjectOffset = { 0.8f, 1.0f, -0.2f };

	tDesc.vMiddleCameraOffset = { -0.55f, -0.22f, -0.8f };
	tDesc.vMiddleObjectOffset = { 1.65f, 1.0f, 0.1f };

	tDesc.fStartArmLength = 2.6f;
	tDesc.fMiddleArmLength = 1.55f;

	return tDesc;
}

void CRole_Qianxia::Init_Role_Desc()
{
	/* 치사 : 40칸, 공명률 100% 씩 소모(E) */
	Get_Desc().fHp = 10775.f;
	Get_Desc().fMaxHp = 10775.f;
	Get_Desc().vIdentity_Energy = _float4(0.f, 40.f, 1.f, 40.f); // 선형적으로
	Get_Desc().vHarmony_Energy = _float2(0.f, 100.f);
	Get_Desc().vUltra_Energy = _float2(0.f, 100.f);

	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_T)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_E)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_Q)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_R)] = _float2(0.f, 10.f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SWITCH)] = _float2(0.f, 1.f);
}

void CRole_Qianxia::Init_Forced_Return_EffectPool()
{
	m_vecForcedReturnEffect = { 3, 4, 7, 20 };
}

void CRole_Qianxia::Attack_Input(_float _fTimeDelta)
{
	if (m_pGameInstance->Mouse_Down(DI_MB::LBUTTON))
	{
		/* Attack_Air */
		if (State_Check(JUMP_START) || State_Check(JUMP_LOOP))
		{
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_AIR;
			m_InputQueue.push(tEvent);
			return;
		}

		/* Attack_L (드리워진 종말) */
		if (State_Check(ATTACK_L_LOOP))
		{
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_L;
			m_InputQueue.push(tEvent);
			return;
		};

		/* 드리워진 종말 중 공격 금지 */
		if (State_Check(ATTACK_L_END))
			return;

		/* Attack */
		E_INPUT_EVENT tEvent = {};
		tEvent.eType = E_INPUT_TYPE::ATTACK;
		m_InputQueue.push(tEvent);

		if (!Is_AttackState())
		{
			m_bLMousePressing = true;
			m_fLMousePressingTime = 0.f;
		}
		else
		{
			m_bComboInput = true;
			m_bLMousePressing = false;
			m_fLMousePressingTime = 0.f;
		}
	}

	if (State_Check(JUMP_START) || State_Check(JUMP_LOOP) || State_Check(JUMP_SECOND))
		return;

	if (State_Check(ATTACK_L_LOOP))
	{
		m_fAttackInputBuffer += _fTimeDelta;
		return;
	}

	if (m_bLMousePressing && State_Check(ATTACK_01) && m_pGameInstance->Mouse_Pressed(DI_MB::LBUTTON))
	{
		m_fLMousePressingTime += _fTimeDelta;
		m_fAttackLEnableTime = 0.15f;
		if (m_fLMousePressingTime >= m_fAttackLEnableTime)
		{
			m_bLMousePressing = false;
			m_fLMousePressingTime = 0.f;

			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_L;
			m_InputQueue.push(tEvent);
		}
	}

	/* Hold 전에 떼면 1타 */
	if (m_bLMousePressing && m_pGameInstance->Mouse_Up(DI_MB::LBUTTON))
	{
		m_bLMousePressing = false;
		m_fLMousePressingTime = 0.f;
	}
}

void CRole_Qianxia::Consume_Input()
{
	if (m_InputQueue.empty())
		return;

	auto& input = m_InputQueue.front();

	switch (input.eType)
	{
	case E_INPUT_TYPE::ATTACK:
	{
		if (State_Check(SKILL_02) || State_Check(SKILL_05) || State_Check(SKILL_03))
		{
			m_bComboInput = true;
			m_bLMousePressing = false;
			m_fLMousePressingTime = 0.f;
			m_InputQueue.pop();
			break;
		}

		if (!Is_AttackState())
		{
			m_bAttackQueued = true;
			m_fInputTimer = 0.f;
		}
		else if (State_Check(ATTACK_L_END))
		{
			_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
			if (fRatio >= 0.5f)
			{
				m_bAttackQueued = true;
				m_fInputTimer = 0.f;
			}
		}
		else
		{
			_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
			if (fRatio <= m_fEndRatio)
			{
				m_bComboInput = true;
				m_bLMousePressing = false;
				m_fLMousePressingTime = 0.f;
			}
		}
		m_InputQueue.pop();
	}
	break;
	case E_INPUT_TYPE::ATTACK_L:
	{
		if (m_bComboInput)
		{
			m_InputQueue.pop();
			break;
		}
		if (!Get_Desc().Check_Stamina(1))
		{
			m_InputQueue.pop();
			break;
		}

		if (State_Check(ATTACK_01))
		{
			Get_Desc().Update_Stamina(1);

			m_iComboIndex = 0;
			m_bComboInput = true;
			m_fAttackInputBuffer = 0.f;
		}
		else if (State_Check(ATTACK_L_LOOP))
		{
			_float fQianxiaAttackLFollowTime = 1.f;
			if (m_fAttackInputBuffer <= fQianxiaAttackLFollowTime)
			{
				Get_Desc().Use_Stamina(1);

				m_bComboInput = true;
				m_fAttackInputBuffer = 0.f;
			}
		}
		m_InputQueue.pop();
	}
	break;
	case E_INPUT_TYPE::ATTACK_AIR:
	{
		m_pStateController->ChangeAnimState(L"Attack_Air_Start", true);
		m_InputQueue.pop();
	}
	break;
	default:
	{
		m_InputQueue.pop();
	}
	break;
	}
}

void CRole_Qianxia::Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate)
{
	__super::Init_Change_State_Attack_Combo(0.75f, 0.85f, 0.65f);

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

	auto funcCombo04To05 = [this]() {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);

		if (m_iComboIndex != 4 || !m_bComboInput)
			return false;

		if (m_bSwitchOutComboChain)
		{
			if (fRatio < 0.08f)
				return false;
		}
		else
		{
			if (fRatio < 0.08f || fRatio > 0.16f)
				return false;
		}

		m_bComboInput = false;
		if (!m_bSwitchOutComboChain)
			m_iComboIndex = 0;
		return true;
		};

	/* 5타 추가 */
	m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Attack_05", funcCombo04To05, true));

	m_pStateController->InsertChangeStatement(L"Attack_05", new CChangeStatement(L"Idle", 1.f));
}

void CRole_Qianxia::Init_Change_State_Attack_L()
{
	auto funcIdle = [this]() { return !Has_RunInput(); };
	auto funcRun = [this]() { return Has_RunInput(); };

	/* Attack_L */
	auto funcAttack = [this]() { return m_bAttackQueued; };
	auto funcCombo = [this](_ullong _iState) {
		return [this, _iState]() {
			if (State_Check(_iState) && m_iComboIndex == 0 && m_bComboInput)
			{
				m_bComboInput = false;
				return true;
			}
			return false;
			};       
		};

	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Attack_L_Loop", funcCombo(ATTACK_01), true, EMORE, 0.45f));

	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Attack_L_End", funcCombo(ATTACK_L_LOOP), true, EMORE, 0.35f));

	auto funcAttackToJumpLoop = [this]() { return !m_bGrounded && !m_pGameInstance->Mouse_Pressed(DI_MB::LBUTTON); };
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Jump_Loop", funcAttackToJumpLoop, true, EMORE, 0.85f));
	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Jump_Loop", funcAttackToJumpLoop, true, EMORE, 0.5f));

	auto funcAttackLToLand = [this]() {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);

		return m_bGrounded
			&& fRatio >= 0.2f
			&& m_vCharacterVelocity.y <= 0.f;
		};
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Jump_Land", funcAttackLToLand, true));

	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Attack_01", [this]() { return m_bAttackQueued; }, true, EMORE, 0.5f));

	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));

	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.85f));
	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
}

void CRole_Qianxia::Init_Change_State_Skill()
{
	auto funcIdle = [this]() { return !Has_RunInput(); };
	auto funcRun = [this]() { return Has_RunInput(); };

	auto funcSkillOut = [this]() {
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		return m_pGameInstance->Key_Down(DIK_E);
		};

	{
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_02", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_02", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_02", funcSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_02", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_02", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_02", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_02", funcSkillOut, true));
	}

	auto funcSkillCombo = [this]() {
		if (m_bComboInput)
		{
			m_bComboInput = false;
			return true;
		}
		return false;
		};
	auto funcSkillComboFail = [this]() { return !m_bComboInput; };

	{
		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Skill_05", funcSkillCombo, true, EMORE, 0.25f));

		m_pStateController->InsertChangeStatement(L"Skill_05", new CChangeStatement(L"Skill_03", funcSkillCombo, true, EMORE, 0.85f)); // End 로 빠지는 것 보다 빠르게
		m_pStateController->InsertChangeStatement(L"Skill_05", new CChangeStatement(L"Skill_04", funcSkillComboFail, true, EMORE, 1.f));

		m_pStateController->InsertChangeStatement(L"Skill_03", new CChangeStatement(L"Skill_06", funcSkillCombo, true, EMORE, 0.85f)); // End 로 빠지는 것 보다 빠르게
		m_pStateController->InsertChangeStatement(L"Skill_03", new CChangeStatement(L"Skill_04", funcSkillComboFail, true, EMORE, 1.f));

		m_pStateController->InsertChangeStatement(L"Skill_04", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_04", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));

		m_pStateController->InsertChangeStatement(L"Skill_06", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_06", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}

	{
		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.55f));
		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}
	
	auto funcROut = [this]() {
		if (!m_bIsMainRole)
			return false;
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_R))
			return false;
		if (!Get_Desc().Check_Ultra_Energy())
			return false;
		return m_pGameInstance->Key_Down(DIK_R);
		};
	{
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Burst_01", funcROut, true));

		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Burst_01", funcROut, true));

		m_pStateController->InsertChangeStatement(L"Burst_01", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.925f));
		m_pStateController->InsertChangeStatement(L"Burst_01", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}
}

void CRole_Qianxia::Init_Attack01_State(_float _fEndTime)
{
	_float fAnimFullTime = 3.067f;
	_float fAnimEndTime = fAnimFullTime * 0.15f;

	auto pAttack = AnimTransform::Create(_fEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_01);
		m_iComboIndex = 1;
		m_bAttackQueued = false;

		m_fWeaponVisualizeDurtaion = 0.f;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);

		if (m_fWeaponVisualizeDurtaion <= 0.f && fRatio >= 0.25f)
		{
			m_fWeaponVisualizeDurtaion = 1.f;
			Set_Weapon_Visual_Instant({ 1 });
			Set_Scabbard_Visual_Instant({ 1 });
		}

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

void CRole_Qianxia::Init_Attack02_State(_float _fEndTime)
{
	_float fAnimFullTime = 5.567f;
	_float fAnimEndTime = fAnimFullTime * 0.17f;

	/* Switch Out (Common Attack) */
	auto pAttack = AnimTransform::Create(fAnimEndTime);
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
		if (fRatio > m_fEndRatio && m_bComboInput) {
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

void CRole_Qianxia::Init_Attack03_State(_float _fEndTime)
{
	_float fAnimFullTime = 7.2f;
	_float fAnimEndTime = fAnimFullTime * 0.23f;
	__super::Init_Attack03_State(fAnimEndTime);
}

void CRole_Qianxia::Init_Attack04_State(_float _fEndTime)
{
	_float fAnimFullTime = 7.933f;
	_float fAnimEndTime = fAnimFullTime * 0.212f;

	auto pAttack = AnimTransform::Create(fAnimEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_04);
		m_iComboIndex = 4;
		m_bAttackQueued = false;
		m_bComboInput = false;

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);

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

void CRole_Qianxia::Init_Attack05_State(_float _fEndTime)
{
	_float fAnimFullTime = 7.667f;
	_float fAnimEndTime = fAnimFullTime * 0.191f;
	
	auto pAttack = AnimTransform::Create(fAnimEndTime);
	pAttack->funcStarted = [this]() {
		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		Add_State(CHARACTER_STATE::ATTACK_05);
		m_iComboIndex = 5;
		m_bAttackQueued = false;

		m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(10.f);
		Get_Desc().Gain_Ultra_Energy(5.f);

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		if (m_bSwitchingOut)
		{
			Set_Weapon_Visual_Instant({ 0 });
		}
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_05);
		m_iComboIndex = 0;
		m_bComboInput = false;

		m_pAnimationController->Set_SubRootMotion_Y(false);
		};

	m_pAnimator->PushAnimInfo(L"Attack_05", pAttack);
}

void CRole_Qianxia::Init_Attack_L_Loop_State(_float _fEndTime)
{
	_float fAnimFullTime = 1.867f;
	auto pAttack = AnimTransform::Create(fAnimFullTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_L_LOOP);

		Flush_Forced_Return_EffectPool();
		Reset_Locomotion();

		m_bGrounded = false;
		m_iGroundContect = 0;

		_float fJumpForceScale = 0.7f; // Adjust jump height here.
		Set_JumpVelocity_Y(m_fJumpForce * fJumpForceScale);
		Apply_JumpVelocity(0.f);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_L_LOOP);

		};

	m_pAnimator->PushAnimInfo(L"Attack_L_Loop", pAttack);
}

void CRole_Qianxia::Init_Attack_L_End_State(_float _fEndTime)
{
	_float fAnimFullTime = 1.533f;
	auto pAttack = AnimTransform::Create(fAnimFullTime);
	pAttack->funcStarted = [this]() {
		Add_State(CHARACTER_STATE::ATTACK_L_END);

		Reset_Locomotion();

		m_bGrounded = false;
		m_iGroundContect = 0;

		m_pCCT->Set_UseGravity(false);

		Get_Desc().Gain_Identity_Energy(0.987654321f);
		Get_Desc().Gain_Harmony_Energy(20.f);
		Get_Desc().Gain_Ultra_Energy(5.f);
		};
	pAttack->funcUpdate = [this](_float _fTimeDelta) {
		m_vCharacterVelocity = _float3{ 0.f, 0.f, 0.f };
		m_pCCT->Set_Velocity(_float3{ 0.f, 0.f, 0.f });

		Update_AirMotion(_fTimeDelta);
		};
	pAttack->funcEnded = [this]() {
		Remove_State(ATTACK_L_END);

		m_pCCT->Set_UseGravity(true);
		};
	m_pAnimator->PushAnimInfo(L"Attack_L_End", pAttack);
}

void CRole_Qianxia::Init_Skill02_State(_float _fEndTime)
{
	/* Skill_02 (E) + Skill_02_Ex */
	auto pSkillStart = AnimTransform::Create(2.233f);
	pSkillStart->funcStarted = [this, _fEndTime]() {
		Add_State(SKILL_02);
		
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		if (Get_Desc().Check_Identity_Eneergy()) // 설계 상, 모두 사용
			Get_Desc().Spend_Identity_Energy();

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkillStart->funcEnded = [this]() {
		Remove_State(SKILL_02);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};
	m_pAnimator->PushAnimInfo(L"Skill_02", pSkillStart);

	_float fAnimFullTime = 3.f;
	_float fAnimEndTime = fAnimFullTime * 0.75f; // 이게idle 이 빨리 되어버리는 원인
	auto pSkillEnd = AnimTransform::Create(fAnimEndTime);
	pSkillEnd->funcStarted = [this]() {
		Add_State(SKILL_02);
		};
	pSkillEnd->funcEnded = [this]() {
		Remove_State(SKILL_02);
		};
	m_pAnimator->PushAnimInfo(L"Skill_02", pSkillEnd);
}

void CRole_Qianxia::Init_Skill03_State(_float _fEndTime)
{
	/* Sp_Attack03 replacement (Excep.Camera 대체) */
	auto pSkill = AnimTransform::Create(1.233f);
	pSkill->funcStarted = [this, _fEndTime]() {
		Add_State(SKILL_03);

		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Set_JumpVelocity_Y(0.f);
		Apply_JumpVelocity(0.f);
		
		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;

		m_pAnimationController->Set_SubRootMotion_Y(true);
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_03);

		m_pAnimationController->Set_SubRootMotion_Y(false);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};
	m_pAnimator->PushAnimInfo(L"Skill_03", pSkill);
}

void CRole_Qianxia::Init_Skill04_State(_float _fEndTime)
{
	/* Sp_Attack03_End replacement */
	_float fAnimFullTime = 15.733f;
	_float fAnimEndTime = fAnimFullTime * 0.035f;
	auto pSkill = AnimTransform::Create(fAnimEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Add_State(SKILL_04);

		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		m_pCCT->Set_UseGravity(true);

		Reset_CameraSettings();
		
		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Update_AirMotion(_fTimeDelta);
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_04);

		if (m_bGrounded)
			Set_JumpVelocity_Y(0.f);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};
	m_pAnimator->PushAnimInfo(L"Skill_04", pSkill);
}

void CRole_Qianxia::Init_Skill05_State(_float _fEndTime)
{
	/* Sp_Attack05 replacement */
	auto pSkill = AnimTransform::Create(1.067f);
	pSkill->funcStarted = [this, _fEndTime]() {
		Add_State(SKILL_05);

		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);

		// TODO : 무기 넣어보고 빼기
		_float fJumpForceScale = 0.18f; // Adjust jump height here.
		Set_JumpVelocity_Y(m_fJumpForce * fJumpForceScale);
		Apply_JumpVelocity(0.f);

		if (m_pCCT)
		{
			m_pCCT->Set_UseGravity(false);
			m_pCCT->Clear_VerticalVelocity();
		}

		Set_JumpVelocity_Y(0.f);
		Apply_JumpVelocity(0.f);

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;

		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		Set_JumpVelocity_Y(0.f);
		Apply_JumpVelocity(0.f);
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_05);

		if (m_pCCT)
			m_pCCT->Set_UseGravity(true);

		Set_JumpVelocity_Y(0.f);
		Apply_JumpVelocity(0.f);

		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};
	m_pAnimator->PushAnimInfo(L"Skill_05", pSkill);
}

void CRole_Qianxia::Init_Skill06_State(_float _fEndTime)
{
	/* Sp_Attack05_Ex replacement (Excep.Camera 대체) */
	_float fAnimFullTime = 17.4f;
	_float fAnimEndTime = fAnimFullTime * 0.144f;
	auto pSkill = AnimTransform::Create(fAnimEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {
		Add_State(SKILL_06);

		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Set_JumpVelocity_Y(0.f);
		Apply_JumpVelocity(0.f);

		// TODO : Character Camera Axis (Auto_Turn_To_Target_Monster())
		if (m_pTargetMonster)
		{
			if (auto pTargetMonster = m_pTargetMonster->Get())
			{
				auto pTargetMonsterTransform = pTargetMonster->GetTransform();

				_vector vBaseUp = XMVector3Normalize(XMLoadFloat3(&m_pTransform->Get_BaseUp()));

				_vector vMyPos = m_pTransform->Get_World(STATE::POSITION);
				_vector vTargetPos = pTargetMonsterTransform->Get_World(STATE::POSITION);

				_vector vLook = vTargetPos - vMyPos;
				vLook = vLook - vBaseUp * XMVector3Dot(vLook, vBaseUp);

				if (XMVectorGetX(XMVector3LengthSq(vLook)) > 0.0001f)
				{
					vLook = XMVector3Normalize(vLook);

					m_pTransform->Turn(vMyPos + vLook, vBaseUp);
					XMStoreFloat3(&m_vTargetDirection, vLook);
				}
			}
		}
		
		m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);

		m_bIFrames = true;
		m_fIFramesTimer.x = _fEndTime;
		};
	pSkill->funcUpdate = [this](_float _fTimeDelta) {
		_float fRatio = m_pAnimationStateMachine->Get_CurrentNormalizedTime(0);
		if (fRatio >= 0.055f) // 0.945초
		{
			if (auto pGameManager = CGameManager::GetInstance())
			{
				for (_uint i = 0; i < 3; ++i)
				{
					if (auto pRole = pGameManager->Get_Role(i))
						pRole->Get_Desc().Start_Heal(2000.f, 3.f);
				}
			}
		}
		};
	pSkill->funcEnded = [this]() {
		Remove_State(SKILL_06);
		
		m_bIFrames = false;
		m_fIFramesTimer.x = 0.f;
		};
	m_pAnimator->PushAnimInfo(L"Skill_06", pSkill);
}

// deprecated
void CRole_Qianxia::Init_Execute_State()
{
	auto pExecute = AnimTransform::Create(3.5f); // 3.733f
	pExecute->funcStarted = [this]() {
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
			m_pTransform->Turn(pMonster->GetTransform()->Get_World(STATE::POSITION), XMLoadFloat3(&m_pTransform->Get_BaseUp()));
		}

		/* 4. CCharacterCamera set */
		if (m_pCharacterCamera) {
			m_pCharacterCamera->Set_TargetObject(this);
			m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);
			m_pCharacterCamera->Begin_Execute();
			m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);
		}
		};
	pExecute->funcUpdate = [this](_float _fTimeDelta) {
		m_fExecuteCameraTimer += _fTimeDelta;

		/* 5. Target 변경해주기 */
		if (!m_bExecuteCameraLookMonster && m_fExecuteCameraTimer >= 0.39f) {
			if (m_pCharacterCamera) {
				m_pCharacterCamera->Set_TargetObject(this);
				m_pCharacterCamera->Turn_To_Player_Look_Instant(0.f);
			}

		if (m_pCharacterCamera && m_pTargetMonster) {
			if (auto pMonster = m_pTargetMonster->Get()) {
				Update_AdditionalCameraOffset();
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
		};

	m_pAnimator->PushAnimInfo(L"Execute", pExecute);
}

void CRole_Qianxia::Init_Burst01_State(_float _fEndTime)
{
	__super::Init_Burst01_State(4.5f);
}

void CRole_Qianxia::Init_Switch_QTE_State(_float _fEndTime)
{
	__super::Init_Switch_QTE_State(1.f);
}

void CRole_Qianxia::Free()
{
	__super::Free();
}
