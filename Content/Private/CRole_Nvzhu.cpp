#include "ContentPCH.h"
#include "CRole_Nvzhu.h"
#include "CGameInstance.h"

CRole_Nvzhu::CRole_Nvzhu(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CRole_Base(_pDevice, _pContext)
{
	m_tRoleDesc.fDamage = 1280.f;
	m_tRoleDesc.eElement = DAMAGE_FONT_ELEMENT::Light;
	m_fSwitchInTargetRadius = 1.2f;
}

HRESULT CRole_Nvzhu::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CRole_Nvzhu::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CRole_Nvzhu::Start()
{
	__super::Start();
}

void CRole_Nvzhu::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CRole_Nvzhu::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CRole_Nvzhu::Attack_Input(_float _fTimeDelta)
{
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

		if (State_Check(ATTACK_L_LOOP))
		{
			E_INPUT_EVENT tEvent = {};
			tEvent.eType = E_INPUT_TYPE::ATTACK_L;
			m_InputQueue.push(tEvent);
			return;
		}

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

	/* 점프 중 공격 금지 (Air Attack 제외) */
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
	
	if (m_bLMousePressing && m_pGameInstance->Mouse_Up(DI_MB::LBUTTON))
	{
		m_bLMousePressing = false;
		m_fLMousePressingTime = 0.f;
	}

	if (m_pGameInstance->Key_Down(DIK_F))
	{
		E_INPUT_EVENT tEvent = {};
		tEvent.eType = E_INPUT_TYPE::EXECUTE;
		m_InputQueue.push(tEvent);
	}
}

void CRole_Nvzhu::Consume_Input()
{
	if (m_InputQueue.empty())
		return;

	auto& input = m_InputQueue.front();

	switch (input.eType)
	{
	case E_INPUT_TYPE::ATTACK:
	{
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
			Get_Desc().Use_Stamina(1);

			m_iComboIndex = 0;
			m_bComboInput = true;
			m_fAttackInputBuffer = 0.f;
		}
		else if (State_Check(ATTACK_L_LOOP))
		{
			_float fNvzhuAttackLFollowTime = 1.f;
			if (m_fAttackInputBuffer <= fNvzhuAttackLFollowTime)
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
	default:
	{
		m_InputQueue.pop();
	}
		break;
	}
}

void CRole_Nvzhu::Init_Role_Desc()
{
	/* 방랑자 : 36칸, 공명률 50%(18칸) 씩 소모(E) */
	Get_Desc().fHp = 11400.f;
	Get_Desc().fMaxHp = 11400.f;
	Get_Desc().vIdentity_Energy = _float4(0.f, 36.f, 2.f, 18.f);
	Get_Desc().vHarmony_Energy = _float2(0.f, 100.f);
	Get_Desc().vUltra_Energy = _float2(0.f, 100.f);

	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_T)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_E)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_Q)] = _float2(0.f, 2.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SKILL_R)] = _float2(0.f, 4.5f);
	Get_Desc().arrCooldown[ETOI(COOLDOWN_TYPE::SWITCH)] = _float2(0.f, 1.f);
}

void CRole_Nvzhu::Init_Change_State_Attack_Combo(_float _fCombo01ExitRate, _float _fCombo02ExitRate, _float _fCombo03ExitRate)
{
	// 다음 공격으로 캔슬 가능한 최소 시간
	__super::Init_Change_State_Attack_Combo(0.65f, 0.5f, 0.7f);
}

void CRole_Nvzhu::Init_Change_State_Attack_L()
{
	auto funcIdle = [this]() { return !Has_RunInput(); };
	auto funcRun = [this]() { return Has_RunInput(); };

	/* Attack_L */
	auto funcAttack = [this]() { return m_bAttackQueued; };
	auto funcAttackL = [this]() { return !m_pGameInstance->Mouse_Pressed(DI_MB::LBUTTON); };
	
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
	
	m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Attack_L_Loop", funcCombo(ATTACK_01), true, EMORE, 0.25f));

	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Attack_L_End", funcCombo(ATTACK_L_LOOP), true));

	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Attack_01", funcAttack, true, EMORE, 0.5f));

	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_L_Loop", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));

	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
	m_pStateController->InsertChangeStatement(L"Attack_L_End", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
}

void CRole_Nvzhu::Init_Change_State_Skill()
{
	auto funcIdle = [this]() { return !Has_RunInput(); };
	auto funcRun = [this]() { return Has_RunInput(); };
	auto funcSkillOut = [this]() {
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		return m_pGameInstance->Key_Down(DIK_E);
		};
	auto funcNvzhuHalfSkillOut = [this]() {
		if (!Get_Desc().Check_Cooldown(COOLDOWN_TYPE::SKILL_E))
			return false;
		if (!Get_Desc().Check_Identity_Eneergy())
			return false;
		return m_pGameInstance->Key_Down(DIK_E);
		};

	{
		// 이 방식으로도 해볼까 ? Queue 입력 말고
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_02", funcNvzhuHalfSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_02", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}

	{
		// 이 방식으로도 해볼까 ? Queue 입력 말고
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Skill_01", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Skill_01", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Skill_01", funcSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Skill_01", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Skill_01", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Skill_01", funcSkillOut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Skill_01", funcSkillOut, true));

		m_pStateController->InsertChangeStatement(L"Skill_01", new CChangeStatement(L"Run", funcRun, true, EMORE, 1.f));
		m_pStateController->InsertChangeStatement(L"Skill_01", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
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
		// 이 방식으로도 해볼까 ? Queue 입력 말고
		m_pStateController->InsertChangeStatement(L"Idle", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Walk", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Run", new CChangeStatement(L"Burst_01", funcROut, true));

		m_pStateController->InsertChangeStatement(L"Attack_01", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_02", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_03", new CChangeStatement(L"Burst_01", funcROut, true));
		m_pStateController->InsertChangeStatement(L"Attack_04", new CChangeStatement(L"Burst_01", funcROut, true));

		m_pStateController->InsertChangeStatement(L"Burst_01", new CChangeStatement(L"Run", funcRun, true, EMORE, 0.75f));
		m_pStateController->InsertChangeStatement(L"Burst_01", new CChangeStatement(L"Idle", funcIdle, true, EMORE, 1.f));
	}
}

void CRole_Nvzhu::Init_Attack01_State(_float _fEndTime)
{
	_float fAnimFullTime = 2.633f;
	_float fAnimEndTime = fAnimFullTime * 0.15f;
	__super::Init_Attack01_State(fAnimEndTime);
}

void CRole_Nvzhu::Init_Attack02_State(_float _fEndTime)
{
	_float fAnimFullTime = 3.5f;
	_float fAnimEndTime = fAnimFullTime * 0.2f;
	__super::Init_Attack02_State(fAnimEndTime);
}

void CRole_Nvzhu::Init_Attack03_State(_float _fEndTime)
{
	_float fAnimFullTime = 2.667f;
	_float fAnimEndTime = fAnimFullTime * 0.275f;
	__super::Init_Attack03_State(fAnimEndTime);
}

void CRole_Nvzhu::Init_Attack04_State(_float _fEndTime)
{
	_float fAnimFullTime = 4.133f;
	_float fAnimEndTime = fAnimFullTime * 0.25f;
	__super::Init_Attack04_State(fAnimEndTime);
}

void CRole_Nvzhu::Init_Skill01_State(_float _fEndTime)
{
	_fEndTime = 0.8f;
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {

		Flush_Forced_Return_EffectPool();
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

void CRole_Nvzhu::Init_Skill02_State(_float _fEndTime)
{
	_fEndTime = 0.8f;
	auto pSkill = AnimTransform::Create(_fEndTime);
	pSkill->funcStarted = [this, _fEndTime]() {

		Flush_Forced_Return_EffectPool();
		Clear_AllInput();
		Reset_Locomotion();
		Reset_Input(0.f);
		Reset_Gravity();

		Add_State(CHARACTER_STATE::SKILL_02);

		Get_Desc().Spend_SkillCoolDown(COOLDOWN_TYPE::SKILL_E);
		Get_Desc().Spend_Identity_Energy();

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

void CRole_Nvzhu::Init_Burst01_State(_float _fEndTime)
{
	__super::Init_Burst01_State(2.25f);
}

void CRole_Nvzhu::Init_Switch_QTE_State(_float _fEndTime)
{
	__super::Init_Switch_QTE_State(1.f);
}

void CRole_Nvzhu::Free()
{
	__super::Free();
}
