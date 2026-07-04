#include "ContentPCH.h"
#include "CWeapon_Qianxia_Jiandao.h"

namespace
{
	constexpr const _char* WeaponAnimNames[] =
	{
		"AirAttack_End_Jiandao",			// 0
		"AirAttack_Loop_Jiandao",			// 1
		"AirAttack_Start_Jiandao",			// 2

		"Attack01_Jiandao",					// 3
		"Attack02_Jiandao",					// 4
		"Attack03_End_Jiandao",				// 5
		"Attack03_Loop_Jiandao",			// 6
		"Attack03_Start_Jiandao",			// 7

		"Attack_Ex2_Air",					// 8
		"Attack_Ex2_Air_Jiandao",			// 9
		"Attack_Ex2_Dash_F_Jiandao",		// 10
		"Attack_Ex2_Dash_Jiandao",			// 11
		"Attack_Ex2_Lv1_Jiandao",			// 12
		"Attack_Ex2_Lv2_Jiandao",			// 13
		"Attack_Ex2_Lv3_Jiandao",			// 14
		"Attack_Ex3_Air_N_Jiandao",			// 15
		"Attack_Ex3_Jiandao",				// 16
		"Attack_Ex3_Jiandao_Air",			// 17

		"Burst01",							// 18

		"H_Attack01_Jiandao",				// 19

		"Skill02_Ex_Jiandao",				// 20
		"Skill02_Jiandao",					// 21

		"Sp_Attack01_E_Saw",				// 22
		"Sp_Attack01_Saw",					// 23
		"Sp_Attack03_1_Saw",				// 24
		"Sp_Attack03_End_Saw",				// 25
		"Sp_Attack03_Ex_Saw",				// 26
		"Sp_Attack03_Saw",					// 27
		"Sp_Attack05_Ex_Saw",				// 28
		"Sp_Attack05_Saw",					// 29

		"Sp_Dodge_B_A",						// 30
		"Sp_Dodge_B_Ex_A",					// 31
		"Sp_Dodge_F_A",						// 32
		"Sp_Dodge_F_Ex_A",					// 33

		"Stand1_Action02",					// 34
		"Stand1_Action03",					// 35
		"Stand2_Jiandao",					// 36
		"Standchange_Jiandao",				// 37
	};

	constexpr _int WeaponAnimCount = static_cast<_int>(sizeof(WeaponAnimNames) / sizeof(WeaponAnimNames[0]));

	constexpr _uint ObjectFilter_BoneIndex_Jiandao = 0;			// WeaponProp05
	constexpr _uint ObjectFilter_BoneIndex_RHand = 1;			// WeaponProp02
	constexpr _uint ObjectFilter_BoneIndex_LHand = 2;			// WeaponProp01

	_bool Is_SawAnim(const _char* _pAnimName)
	{
		if (!_pAnimName)
			return false;

		return strstr(_pAnimName, "Saw") != nullptr;
	}
}

CWeapon_Qianxia_Jiandao::CWeapon_Qianxia_Jiandao(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CWeapon(_pDevice, _pContext)
{
}

HRESULT CWeapon_Qianxia_Jiandao::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CWeapon_Qianxia_Jiandao::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CWeapon_Qianxia_Jiandao::Start()
{
	__super::Start();
}

void CWeapon_Qianxia_Jiandao::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CWeapon_Qianxia_Jiandao::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CWeapon_Qianxia_Jiandao::Attach_ToWeaponProp(_int _iAnimIndex)
{
	if (_iAnimIndex < 0 || _iAnimIndex >= WeaponAnimCount)
		return;

	if (!m_pObjectFilter)
		return;

	const _char* pAnimName = WeaponAnimNames[_iAnimIndex];
	const _uint iBoneIndex = Is_SawAnim(pAnimName) ? ObjectFilter_BoneIndex_RHand : ObjectFilter_BoneIndex_Jiandao;

	CObject* pBoneObject = m_pObjectFilter->Get_Object(iBoneIndex);
	if (!pBoneObject)
		return;

	CTransform* pBoneTransform = pBoneObject->GetTransform();
	if (!pBoneTransform)
		return;

	this->GetTransform()->Set_Parent(pBoneTransform);
	this->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());
	this->GetTransform()->Set_Rotation({ -90.f, 0.f, 0.f }); // Weapon rotation offset
}

void CWeapon_Qianxia_Jiandao::Play_WeaponAnim(const _int& _iAnimIndex)
{
	if (!m_pAnimationController)
		return;

	if (_iAnimIndex < 0 || _iAnimIndex >= WeaponAnimCount)
		return;

	Attach_ToWeaponProp(_iAnimIndex);

	m_pAnimationController->Play(WeaponAnimNames[_iAnimIndex], false);
}

void CWeapon_Qianxia_Jiandao::Pause_WeaponAnim(const monostate&)
{
	if (!m_pAnimationController)
		return;

	m_pAnimationController->Pause();
}

void CWeapon_Qianxia_Jiandao::Attach_ToObjectFilterBone(const _int& _iFilterIndex)
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
	this->GetTransform()->Set_Rotation({ -90.f, 0.f, 0.f }); // Weapon rotation offset
}

void CWeapon_Qianxia_Jiandao::Free()
{
	__super::Free();
}
