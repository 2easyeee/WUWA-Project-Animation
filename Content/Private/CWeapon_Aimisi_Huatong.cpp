#include "ContentPCH.h"
#include "CWeapon_Aimisi_Huatong.h"

namespace
{
	constexpr const _char* PropAnimNames[] =
	{
		"Attack03",			// 0
		"Skill03",			// 1
	};

	constexpr _int PropAnimCount = static_cast<_int>(sizeof(PropAnimNames) / sizeof(PropAnimNames[0]));

	constexpr _uint ObjectFilter_BoneIndex_RHand = 0;			// WeaponProp02
	constexpr _uint ObjectFilter_BoneIndex_LHand = 1;			// WeaponProp01
}

CWeapon_Aimisi_Huatong::CWeapon_Aimisi_Huatong(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CWeapon(_pDevice, _pContext)
{
}

HRESULT CWeapon_Aimisi_Huatong::Awake()
{
	if (FAILED(__super::Awake())) {
		MSG_BOX("Failed To CWeapon_Aimisi_Huatong::Awake");
		return E_FAIL;
	}

	return S_OK;
}

void CWeapon_Aimisi_Huatong::Start()
{
	__super::Start();

	__super::Notify_WeaponEvent({0.f, 1.f, 0.f, 0.f}); // √÷º“ invisible
}

void CWeapon_Aimisi_Huatong::Update(_float _fTimeDelta)
{
	__super::Update(_fTimeDelta);
}

void CWeapon_Aimisi_Huatong::Late_Update(_float _fTimeDelta)
{
	__super::Late_Update(_fTimeDelta);
}

void CWeapon_Aimisi_Huatong::Play_WeaponAnim(const _int& _iAnimIndex)
{
	if (!m_pAnimationController)
		return;

	if (_iAnimIndex < 0 || _iAnimIndex >= PropAnimCount)
		return;

	Attach_ToWeaponProp(_iAnimIndex);

	m_pAnimationController->Play(PropAnimNames[_iAnimIndex], false);
}

void CWeapon_Aimisi_Huatong::Reset_WeaponAnim(const monostate&)
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
}

void CWeapon_Aimisi_Huatong::Attach_ToWeaponProp(_int _iAnimIndex)
{
	if (!m_pObjectFilter)
		return;

	_uint iFilterIndex = ObjectFilter_BoneIndex_RHand;
	_float3 vPosition = { 0.f, 0.f, 0.f };
	_float3 vRotation = { 0.f, 0.f, 0.f };

	switch (_iAnimIndex)
	{
	case 0: // Attack03
		iFilterIndex = ObjectFilter_BoneIndex_RHand;
		vPosition = { 0.f, 0.f, 0.f };
		vRotation = { 0.f, 0.f, 0.f };
		break;

	case 1: // Skill03
		iFilterIndex = ObjectFilter_BoneIndex_LHand;
		vPosition = { 0.f, 0.05f, 0.f };
		vRotation = { -180.f, 0.f, -180.f };
		break;

	default:
		return;
	}

	CObject* pBoneObject = m_pObjectFilter->Get_Object(iFilterIndex);
	if (!pBoneObject)
		return;

	CTransform* pBoneTransform = pBoneObject->GetTransform();
	if (!pBoneTransform)
		return;

	this->GetTransform()->Set_Parent(pBoneTransform);
	this->GetTransform()->Set_LocalMatrix(XMMatrixIdentity());
	this->GetTransform()->Set_Position(vPosition);
	this->GetTransform()->Set_Rotation(vRotation);
}

void CWeapon_Aimisi_Huatong::Free()
{
	__super::Free();
}
