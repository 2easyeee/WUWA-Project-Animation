#include "ContentPCH.h"
#include "CWeapon_Levi_Yuegong.h"

namespace
{
    constexpr const _char* PropAnimNames[] =
    {
        "AirAttack_End",        // 0
        "AirAttack_Loop",       // 1
        "AirAttack_Start",      // 2

        "Attack01",             // 3
        "Attack01_Ex",          // 4
        "Attack02",             // 5
        "Attack02_Ex",          // 6
        "Attack03",             // 7
        "Attack03_Ex",          // 8
        "Attack04",             // 9
        "Attack04_Ex",          // 10

        "Burst01",              // 11

        "ExitSkill",            // 12

        "Move_B_EX",            // 13
        "Move_F_EX",            // 14

        "Skill01",              // 15
        "Skill02",              // 16
        "Skill03",              // 17
        "Skill04",              // 18
        "SkillQte",             // 19

        "Sp_Move_End",          // 20
        "Sp_Move_F",            // 21
        "Sp_Move_SkillEnd",     // 22
        "Sp_Move_Start",        // 23
        "Sp_Move_Stop",         // 24

        "Stand1_Action01",      // 25
        "Stand2",               // 26
        "Stand2_Ex",            // 27
        "StandChange",          // 28
    };

	constexpr _int PropAnimCount = static_cast<_int>(sizeof(PropAnimNames) / sizeof(PropAnimNames[0]));
   
	constexpr _uint ObjectFilter_BoneIndex_RHand = 0;			// WeaponProp02
	constexpr _uint ObjectFilter_BoneIndex_LHand = 1;			// WeaponProp01
	constexpr _uint ObjectFilter_BoneIndex_Eff = 2;				// Effect
}

CWeapon_Levi_Yuegong::CWeapon_Levi_Yuegong(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CWeapon(_pDevice, _pContext)
{
}

HRESULT CWeapon_Levi_Yuegong::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CWeapon_Levi_Yuegong::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CWeapon_Levi_Yuegong::Start()
{
	__super::Start();

	m_fRatio = 0.f;
}

void CWeapon_Levi_Yuegong::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CWeapon_Levi_Yuegong::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CWeapon_Levi_Yuegong::Notify_Weapon(const _float4& _vParam) {
	__super::Notify_Weapon(_vParam);

	if (auto pEffObj = m_pObjectFilter->Get_Object<CWeapon>(ObjectFilter_BoneIndex_Eff)) {
		pEffObj->Notify_Weapon(_vParam);
	}
}

void CWeapon_Levi_Yuegong::Play_WeaponAnim(const _int& _iAnimIndex)
{
	if (!m_pAnimationController)
		return;

	if (_iAnimIndex < 0 || _iAnimIndex >= PropAnimCount)
		return;

	m_pAnimationController->Play(PropAnimNames[_iAnimIndex], false);

	if (auto pEffObj = m_pObjectFilter->Get_Object(ObjectFilter_BoneIndex_Eff)) {
		auto pWeaponAnimController = pEffObj->GetComponent<CAnimationController>();
		pWeaponAnimController->Play(PropAnimNames[_iAnimIndex], false);
	}
}

void CWeapon_Levi_Yuegong::Pause_WeaponAnim(const monostate&)
{
	if (!m_pAnimationController)
		return;

	m_pAnimationController->Pause();

	if (auto pEffObj = m_pObjectFilter->Get_Object(ObjectFilter_BoneIndex_Eff)) {
		auto pWeaponAnimController = pEffObj->GetComponent<CAnimationController>();
		pWeaponAnimController->Pause();
	}
}

void CWeapon_Levi_Yuegong::Reset_WeaponAnim(const monostate&)
{
	if (!m_pAnimationController)
		return;

	m_pAnimationController->Pause();

	if (!m_pObjectFilter)
		return;

	CObject* pBoneObject = m_pObjectFilter->Get_Object(ObjectFilter_BoneIndex_RHand);
	if (!pBoneObject)
		return;

	CTransform* pBoneTransform = pBoneObject->GetTransform();
	if (!pBoneTransform)
		return;

	this->GetTransform()->Set_Parent(pBoneTransform);
	this->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());

	if (auto pEffObj = m_pObjectFilter->Get_Object(ObjectFilter_BoneIndex_Eff)) {
		auto pWeaponAnimController = pEffObj->GetComponent<CAnimationController>();
		pWeaponAnimController->Pause();

		pEffObj->GetTransform()->Set_Parent(pBoneTransform);
		pEffObj->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());
	}
}

void CWeapon_Levi_Yuegong::Attach_ToObjectFilterBone(const _int& _iFilterIndex)
{
	if (!m_pObjectFilter)
		return;

	CObject* pBoneObject = m_pObjectFilter->Get_Object(_iFilterIndex);
	if (!pBoneObject)
		return;

	CTransform* pBoneTransform = pBoneObject->GetTransform();
	if (!pBoneTransform)
		return;

	this->GetTransform()->Set_Parent(pBoneTransform);
	this->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());

	if (auto pEffObj = m_pObjectFilter->Get_Object(ObjectFilter_BoneIndex_Eff)) {
		auto pWeaponAnimController = pEffObj->GetComponent<CAnimationController>();
		pWeaponAnimController->Pause();

		pEffObj->GetTransform()->Set_Parent(pBoneTransform);
		pEffObj->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());
	}
}

void CWeapon_Levi_Yuegong::Free()
{
	__super::Free();
}
