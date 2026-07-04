#include "ContentPCH.h"
#include "CCharacterCamera.h"
#include <CAsset_ActionFSM.h>
#include <CActionStateMachine.h>
#include <CAnimator.h>
#include <CStateController.h>
#include <CCollider_Sphere.h>
#include "CRole_Base.h"
#include "CCamera.h"
#include "CTransform.h"
#include "CVirtualCamera.h"
#include "CCollisionManager.h"

constexpr _float MAX_LOOK_ARM_LEN = 12.f;

namespace {
	constexpr _float CAMERA_BASE_EPSILON = 1e-6f;

	_vector NormalizeOr(_fvector _vValue, _fvector _vFallback) {
		if (XMVectorGetX(XMVector3LengthSq(_vValue)) <= CAMERA_BASE_EPSILON)
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
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > CAMERA_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		vProjected = RemoveAxisComponent(_vFallback, _vBaseUp);
		if (XMVectorGetX(XMVector3LengthSq(vProjected)) > CAMERA_BASE_EPSILON)
			return XMVector3Normalize(vProjected);

		return XMVectorSet(0.f, 0.f, 1.f, 0.f);
	}

	void BuildBaseFrame(_fvector _vBaseUp, _vector& _vRight, _vector& _vForward) {
		_vector vBaseUp = NormalizeOr(_vBaseUp, XMVectorSet(0.f, 1.f, 0.f, 0.f));
		_vForward = RemoveAxisComponent(XMVectorSet(0.f, 0.f, 1.f, 0.f), vBaseUp);

		if (XMVectorGetX(XMVector3LengthSq(_vForward)) <= CAMERA_BASE_EPSILON)
			_vForward = RemoveAxisComponent(XMVectorSet(1.f, 0.f, 0.f, 0.f), vBaseUp);

		_vForward = NormalizeOr(_vForward, XMVectorSet(0.f, 0.f, 1.f, 0.f));
		_vRight = NormalizeOr(XMVector3Cross(vBaseUp, _vForward), XMVectorSet(1.f, 0.f, 0.f, 0.f));
		_vForward = NormalizeOr(XMVector3Cross(_vRight, vBaseUp), _vForward);
	}

	_matrix BuildBaseMatrix(CTransform* _pTransform) {
		_vector vBaseUp = GetTransformBaseUp(_pTransform);
		_vector vRight{}, vForward{};
		BuildBaseFrame(vBaseUp, vRight, vForward);

		return _matrix(
			XMVectorSetW(vRight, 0.f),
			XMVectorSetW(vBaseUp, 0.f),
			XMVectorSetW(vForward, 0.f),
			XMVectorSet(0.f, 0.f, 0.f, 1.f)
		);
	}

	_float CalculateYawOnBasePlane(CTransform* _pBaseTransform, _fvector _vDirection) {
		_vector vBaseUp = GetTransformBaseUp(_pBaseTransform);
		_vector vRight{}, vForward{};
		BuildBaseFrame(vBaseUp, vRight, vForward);

		_vector vDirection = ProjectOnBasePlane(_vDirection, vBaseUp, vForward);
		_float fRight = XMVectorGetX(XMVector3Dot(vDirection, vRight));
		_float fForward = XMVectorGetX(XMVector3Dot(vDirection, vForward));

		return XMConvertToDegrees(atan2f(fRight, fForward));
	}

	void SyncBaseUpFromTarget(CTransform* _pCameraTransform, CTransform* _pTargetTransform) {
		if (_pCameraTransform == nullptr || _pTargetTransform == nullptr)
			return;

		_pCameraTransform->Set_BaseUp(_pTargetTransform->Get_BaseUp());
	}

	_float ExpDampAlpha(_float _fSpeed, _float _fTimeDelta) {
		if (_fSpeed <= 0.f)
			return 1.f;

		return 1.f - expf(-_fSpeed * _fTimeDelta);
	}

	void SetWorldPosition(CTransform* _pTransform, _fvector _vWorldPos) {
		if (_pTransform == nullptr)
			return;

		_vector vLocalPos = _vWorldPos;
		if (auto pParent = _pTransform->GetParent()) {
			_matrix matParentInv = XMMatrixInverse(nullptr, XMLoadFloat4x4(&pParent->GetWorldMatrix()));
			vLocalPos = XMVector3TransformCoord(_vWorldPos, matParentInv);
		}

		_pTransform->Set_State(STATE::POSITION, XMVectorSetW(vLocalPos, 1.f));
	}

	_bool ResolveCameraSweepDistance(
		_fvector _vStart,
		_fvector _vDir,
		_float _fDistance,
		_float _fRadius,
		_float _fSkin,
		_float& _fResolvedDistance) {
		_fResolvedDistance = _fDistance;

		if (_fDistance <= CAMERA_BASE_EPSILON)
			return false;

		_float3 vStart{};
		_float3 vDir{};
		XMStoreFloat3(&vStart, _vStart);
		XMStoreFloat3(&vDir, NormalizeOr(_vDir, XMVectorSet(0.f, 0.f, -1.f, 0.f)));

		physx::PxSweepBuffer tSweepHit{};
		if (!CCollider::Sweep(
			_fRadius,
			vStart,
			vDir,
			_fDistance,
			tSweepHit,
			physx::PxHitFlag::ePOSITION | physx::PxHitFlag::eNORMAL,
			ETOU(Engine::COLLIDER_TAG::PLAYER),
			Engine::ColliderTag_Bit(Engine::COLLIDER_TAG::STATIC),
			CCollider::SEARCH_STATIC
		) || !tSweepHit.hasBlock || tSweepHit.block.hadInitialOverlap()) {
			return false;
		}

		_fResolvedDistance = std::min(std::max(tSweepHit.block.distance - _fSkin, 0.f), _fDistance);
		return _fResolvedDistance < _fDistance;
	}
}
CCharacterCamera::CCharacterCamera(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CGameObject(_pDevice, _pContext) {
	m_eLayer = OBJECT_LAYER::CAMERA;
}

HRESULT CCharacterCamera::Awake() {
	m_pTargetObject = WObject<CCharacter>::Create(nullptr);
	m_pLookTransform = WComponent<CTransform>::Create(nullptr);

	m_pAnimator = CreateComponent<CAnimator>();
	m_pStateController = CreateComponent<CStateController>();
	m_pVirtualCamera = CreateComponent<CVirtualCamera>();
	if (m_pVirtualCamera)
		m_pVirtualCamera->Set_Fov(k_fDefaultFov);

	return S_OK;
}

void CCharacterCamera::Start() {
	auto pIdle = AnimTransform::Create(0.f);
	pIdle->funcStarted = [this]() {
		m_fDestRoll = 0.f;
		};
	pIdle->funcUpdate = bind(&CCharacterCamera::Mouse_Input, this, placeholders::_1);
	m_pAnimator->PushAnimInfo(L"Idle", pIdle);

	m_pLookAnimation = AnimTransform::Create(0.25f);
	m_pLookAnimation->AddProperty(new AnimAddProperty<_float>(
		[this](const _float& _f) {
			m_fYaw += _f;
		},
		[this](_float& _s, _float& _e) {
			if (auto pTargetObject = m_pTargetObject->Get()) {
				auto pTargetTransform = pTargetObject->GetTransform();
				SyncBaseUpFromTarget(m_pTransform, pTargetTransform);
				m_fDestYaw = CalculateYawOnBasePlane(m_pTransform, pTargetTransform->Get_World(STATE::LOOK));
			}

			_s = 0.f;
			_float fDelta = m_fDestYaw - m_fYaw;

			if (fDelta < -180.f) fDelta += 360.f;
			else if (fDelta > 180.f) fDelta -= 360.f;
			_e = fDelta;
		}
	));
	m_pAnimator->PushAnimInfo(L"TurnPlayerLook", m_pLookAnimation);

	m_pBackAnimation = AnimTransform::Create(0.25f);
	m_pBackAnimation->AddProperty(new AnimAddProperty<_float>(
		[this](const _float& _f) {
			m_fYaw += _f;
		},
		[this](_float& _s, _float& _e) {
			if (auto pTargetObject = m_pTargetObject->Get()) {
				auto pTargetTransform = pTargetObject->GetTransform();
				SyncBaseUpFromTarget(m_pTransform, pTargetTransform);
				m_fDestYaw = CalculateYawOnBasePlane(m_pTransform, -pTargetTransform->Get_World(STATE::LOOK));
			}

			_s = 0.f;
			_float fDelta = m_fDestYaw - m_fYaw;

			if (fDelta < -180.f) fDelta += 360.f;
			else if (fDelta > 180.f) fDelta -= 360.f;
			_e = fDelta;
		}
	));
	m_pAnimator->PushAnimInfo(L"TurnPlayerBack", m_pBackAnimation);

	auto pParring = AnimTransform::Create(0.5f);
	pParring->funcGraph = &WEasing::InOutCirc;
	pParring->AddProperty(new AnimSetProperty<_float>(
		[this](const _float& _f) {
			m_fYaw = _f;
		},
		[this](_float& _s, _float& _e) {
			_s = m_fYaw;
			_e = m_fDestYaw;

			_float fDelta = m_fDestYaw - m_fYaw;

			if (fDelta < -180.f) fDelta += 360.f;
			else if (fDelta > 180.f) fDelta -= 360.f;
			_e = _s + fDelta;
		}
	));
	pParring->AddProperty(new AnimSetProperty<_float>(
		[this](const _float& _f) {
			m_fPitch = _f;
		},
		[this](_float& _s, _float& _e) {
			_s = m_fPitch;
			_e = m_fDestPitch;

			_float fDelta = m_fDestPitch - m_fPitch;

			if (fDelta < -180.f) fDelta += 360.f;
			else if (fDelta > 180.f) fDelta -= 360.f;
			_e = _s + fDelta;
		}
	));
	//pParring->AddProperty(new AnimSetProperty<_float>(
	//	[this](const _float& _f) {
	//		m_fRoll = _f;
	//	},
	//	[this](_float& _s, _float& _e) {
	//		_s = m_fRoll;
	//		_e = m_fDestRoll;
	//
	//		_float fDelta = m_fDestRoll - m_fRoll;
	//
	//		if (fDelta < -180.f) fDelta += 360.f;
	//		else if (fDelta > 180.f) fDelta -= 360.f;
	//		_e = _s + fDelta;
	//	}
	//));
	pParring->funcGraph = &WEasing::InOutCirc;
	pParring->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vCameraOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vCameraOffset;
			_e = m_vCameraOriginOffset;
		}
	));
	pParring->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vObjectOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vObjectOffset;
			_e = m_vObjectOriginOffset;
		}
	));
	m_pAnimator->PushAnimInfo(L"Parring", pParring);

	auto pQTEStart = AnimTransform::Create(0.25f);
	pQTEStart->funcStarted = [this]() {
		m_vCameraDestOffset = { 0.f, 1.f, -2.f };
		};
	pQTEStart->funcGraph = &WEasing::InOutCirc;
	pQTEStart->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vCameraOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vCameraOffset;
			_e = m_vCameraDestOffset;
		}
	));
	pQTEStart->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vObjectOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vObjectOffset;
			_e = m_vObjectDestOffset;
		}
	));
	m_pAnimator->PushAnimInfo(L"QTE", pQTEStart);

	auto pQTE = AnimTransform::Create(2.5f);
	m_pAnimator->PushAnimInfo(L"QTE", pQTE);

	auto pQTEEnd = AnimTransform::Create(0.25f);
	pQTEEnd->funcGraph = &WEasing::InOutCirc;
	pQTEEnd->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vCameraOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vCameraOffset;
			_e = m_vCameraOriginOffset;
		}
	));
	pQTEEnd->AddProperty(new AnimAddProperty<_float>(
		[this](const _float& _f) {
			m_fYaw += _f;
		},
		[this](_float& _s, _float& _e) {
			if (auto pTargetObject = m_pTargetObject->Get()) {
				auto pTargetTransform = pTargetObject->GetTransform();
				SyncBaseUpFromTarget(m_pTransform, pTargetTransform);
				m_fDestYaw = CalculateYawOnBasePlane(m_pTransform, pTargetTransform->Get_World(STATE::LOOK));
			}

			_s = 0.f;
			_float fDelta = m_fDestYaw - m_fYaw;

			if (fDelta < -180.f) fDelta += 360.f;
			else if (fDelta > 180.f) fDelta -= 360.f;
			_e = fDelta;
		}
	));
	pQTEEnd->AddProperty(new AnimSetProperty<_float3>(
		[this](const _float3& _f) {
			m_vObjectOffset = _f;
		},
		[this](_float3& _s, _float3& _e) {
			_s = m_vObjectOffset;
			_e = m_vObjectOriginOffset;
		}
	));
	m_pAnimator->PushAnimInfo(L"QTEEnd", pQTEEnd);

	/* Execute */
	{
		auto funcGetExecuteCameraDesc = [this]() -> CRole_Base::EXECUTE_CAMERA_DESC {
			if (m_pTargetObject)
			{
				auto pTargetObject = m_pTargetObject->Get();
				
				if (auto pRole = pTargetObject->As<CRole_Base>())
					return pRole->Get_ExecuteCameraDesc();
			}
			return CRole_Base::EXECUTE_CAMERA_DESC{};
			};

		// 1. 시작하자마자
		auto pExecuteStart = AnimTransform::Create(0.05f);
		pExecuteStart->funcStarted = [this, funcGetExecuteCameraDesc]() {
			CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

			m_fPitch = 0.f;
			m_fRoll = 0.f;

			m_vCameraOffset = tDesc.vStartCameraOffset;
			m_vCameraDestOffset = tDesc.vStartCameraOffset;

			m_vObjectOffset = tDesc.vStartObjectOffset;
			m_vObjectDestOffset = tDesc.vStartObjectOffset;
			m_fDestCameraArmLength = tDesc.fStartArmLength;
			
			m_fDestPitch = m_fPitch;
			m_fDestYaw = m_fYaw;
			m_fDestRoll = m_fRoll;

			m_fCameraArmLength = tDesc.fStartArmLength;

			m_fObjectOffsetSpeed = 10.f;
			m_fCameraArmSpeed = 10.f;
			};
		m_pAnimator->PushAnimInfo(L"Execute", pExecuteStart);

		auto pExecute = AnimTransform::Create(0.164f);
		pExecute->funcStarted = [this]() {
			m_fObjectOffsetSpeed = 5.f;
			m_fCameraArmSpeed = 5.f;
			};
		pExecute->funcGraph = &WEasing::InOutCirc;
		pExecute->AddProperty(new AnimSetProperty<_float3>(
			[this](const _float3& _f) {
				m_vCameraOffset = _f;
				m_vCameraDestOffset = _f;
			},
			[this, funcGetExecuteCameraDesc](_float3& _s, _float3& _e) {
				CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

				_s = m_vCameraOffset;
				_e = tDesc.vMiddleCameraOffset;
			}
			));
		pExecute->AddProperty(new AnimSetProperty<_float3>(
			[this](const _float3& _f) {
				m_vObjectOffset = _f;
				m_vObjectDestOffset = _f;
			},
			[this, funcGetExecuteCameraDesc](_float3& _s, _float3& _e) {
				CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

				_s = m_vObjectOffset;
				_e = tDesc.vMiddleObjectOffset;
			}
		));
		pExecute->AddProperty(new AnimSetProperty<_float>(
			[this](const _float& _f) {
				m_fCameraArmLength = _f;
				m_fDestCameraArmLength = _f;
			},
			[this, funcGetExecuteCameraDesc](_float& _s, _float& _e) {
				CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

				_s = m_fCameraArmLength;
				_e = tDesc.fMiddleArmLength;
			}
		));
		m_pAnimator->PushAnimInfo(L"Execute", pExecute);

		auto pExecuteEnd = AnimTransform::Create(1.f);
		pExecuteEnd->funcStarted = [this, funcGetExecuteCameraDesc]() {
			CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

			Set_ObjectOffset(tDesc.vEndObjectOffset);
			Set_ObjectOffset_Speed(5.f);
			Set_CameraArmLength(tDesc.fEndArmLength);
			Set_CameraArm_Speed(5.f);
			};
		pExecuteEnd->funcGraph = &WEasing::InOutCirc;
		pExecuteEnd->AddProperty(new AnimSetProperty<_float>(
			[this](const _float& _f) {
				m_fPitch = _f;
			},
			[this, funcGetExecuteCameraDesc](_float& _s, _float& _e) {
				CRole_Base::EXECUTE_CAMERA_DESC tDesc = funcGetExecuteCameraDesc();

				_s = m_fPitch;
				_e = 35.f;
			}
		));
		pExecuteEnd->funcEnded = [this]() {
			m_vCameraDestOffset = m_vCameraOriginOffset;
			m_vObjectDestOffset = m_vObjectOriginOffset;
			m_fDestCameraArmLength = 3.4f;
			m_vAdditionalCameraOffset = _float3{ 0.f, 0.f, 0.f };
			};
		m_pAnimator->PushAnimInfo(L"ExecuteEnd", pExecuteEnd);
	}

	auto WipeOutWait = AnimTransform::Create(1.5f);
	m_pAnimator->PushAnimInfo(L"WipeOut", WipeOutWait);

	auto WipeOut_01 = AnimTransform::Create(1.5f);
	WipeOut_01->funcStarted = [this]() {
		m_fYaw = -15.f;
		m_fPitch = 12.f;
		};
	m_pAnimator->PushAnimInfo(L"WipeOut", WipeOut_01);

	auto WipeOut_02 = AnimTransform::Create(1.5f);
	WipeOut_02->funcStarted = [this]() {
		m_fYaw = 20.f;
		m_fPitch = 18.f;
		};
	m_pAnimator->PushAnimInfo(L"WipeOut", WipeOut_02);

	auto WipeOut_03 = AnimTransform::Create(2.f);
	WipeOut_03->funcStarted = [this]() {
		m_fYaw = -35.f;
		m_fPitch = 8.f;
		};
	m_pAnimator->PushAnimInfo(L"WipeOut", WipeOut_03);


	m_pStateController->InsertChangeStatement(L"Parring", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"QTE", new CChangeStatement(L"QTEEnd", 1.f));
	m_pStateController->InsertChangeStatement(L"QTEEnd", new CChangeStatement(L"Idle", 1.f));

	m_pStateController->InsertChangeStatement(L"Execute", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"ExecuteEnd", new CChangeStatement(L"Idle", 1.f));

	m_pStateController->InsertChangeStatement(L"TurnPlayerLook", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->InsertChangeStatement(L"TurnPlayerBack", new CChangeStatement(L"Idle", 1.f));
	m_pStateController->ChangeAnimState(L"Idle");

	//Turn_To_Player_Look(0.1f, 0.f);
}

void CCharacterCamera::SetUp_CameraDestOffset(const _float3& _vCameraDestOffset, _float _fFov) {
	m_vCameraDestOffset = _vCameraDestOffset;
	m_fDestFov = _fFov;
}

void CCharacterCamera::Reset_CameraDestOffset() {
	m_vCameraDestOffset = m_vCameraOriginOffset;
}

void CCharacterCamera::Mouse_Input(_float _fTimeDelta) {
	if (m_bMouseInputLocked)
		return;

	m_fYaw += m_pGameInstance->Get_MouseDelta(DI_MM::X) * m_fMouseSensor;
	m_fPitch += m_pGameInstance->Get_MouseDelta(DI_MM::Y) * m_fMouseSensor;

	_vector vCur = XMLoadFloat3(&m_vCameraOffset);
	_vector vDest = XMLoadFloat3(&m_vCameraDestOffset);
	float a = 1.f - expf(-3.f * _fTimeDelta);
	XMStoreFloat3(&m_vCameraOffset, XMVectorLerp(vCur, vDest, a));

	_float fFov = m_pVirtualCamera->Get_Fov();
	_float t = 1.f - expf(-3.f * _fTimeDelta);
	t *= m_fFovDampMultiplier;
	m_pVirtualCamera->Set_Fov(lerp(fFov, m_fDestFov, t));

	m_fPitch = clamp(m_fPitch, m_fPitchLimit.x, m_fPitchLimit.y);
}

static inline _vector QuatFromBasis(_vector right, _vector up, _vector forward) {
	_matrix m(
		XMVectorSetW(right, 0.f),
		XMVectorSetW(up, 0.f),
		XMVectorSetW(forward, 0.f),
		XMVectorSet(0.f, 0.f, 0.f, 1.f)
	);
	return XMQuaternionRotationMatrix(m);
}

static inline float DampAlpha(float halfLife, float dt) {
	if (halfLife <= 0.f) return 1.f;
	const float k = 0.69314718056f / halfLife;
	return 1.f - expf(-k * dt);
}

static inline _vector GetWorldQuat_FromWorldMatrix(const DirectX::XMFLOAT4X4& world) {
	_matrix M = XMLoadFloat4x4(&world);

	_vector S, R, T;

	XMMatrixDecompose(&S, &R, &T, M);

	R = XMQuaternionNormalize(R);

	return R;
}

void CCharacterCamera::Priority_Update(_float _fTimeDelta)
{
	auto eGameMode = m_pGameInstance->Get_GameMode();
	if (eGameMode == GAMEMODE::EDITOR_PAUSE || eGameMode == GAMEMODE::EDITOR) {
		Late_Update(_fTimeDelta);
	}
}

void CCharacterCamera::Late_Update(_float _fTimeDelta) {
	const _float fDestCameraArmLength = max(0.f, m_fDestCameraArmLength + m_fCameraArmLengthBias);
	const _float fArmAlpha = m_fCameraArmSpeed <= 0.f ? 0.f : 1.f - expf(-m_fCameraArmSpeed * _fTimeDelta);
	m_fCameraArmLength = lerp(m_fCameraArmLength, fDestCameraArmLength, fArmAlpha);

	_vector vObjectOffset = XMLoadFloat3(&m_vObjectOffset);
	_vector vDestObjectOffset = XMLoadFloat3(&m_vObjectDestOffset);

	vObjectOffset += (vDestObjectOffset - vObjectOffset) * m_fObjectOffsetSpeed * _fTimeDelta;

	XMStoreFloat3(&m_vObjectOffset, vObjectOffset);

	m_fRoll -= m_fRoll * 2.f * _fTimeDelta;

	if (fabs(m_fRoll) < 0.01f)
		m_fRoll = 0.f;

	if (auto pTarget = m_pTargetObject->Get()) {
		auto pTargetTransform = pTarget->GetTransform();
		SyncBaseUpFromTarget(m_pTransform, pTargetTransform);

		{
			_matrix matBase = BuildBaseMatrix(m_pTransform);
			_matrix matRot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_fPitch), XMConvertToRadians(m_fYaw), 0.f) * matBase;

			_vector vTargetWorldPos = pTargetTransform->Get_World(STATE::POSITION) + XMVector3TransformNormal(XMLoadFloat3(&m_vObjectOffset), matBase);
			_vector vCameraWorldPos = m_pTransform->Get_World(STATE::POSITION);
			_vector vDesiredArm = XMVector3TransformNormal(XMLoadFloat3(&m_vCameraOffset) * m_fCameraArmLength, matRot);
			_float fDesiredArmLength = XMVectorGetX(XMVector3Length(vDesiredArm));
			_vector vArmDir = NormalizeOr(vDesiredArm, -matRot.r[2]);

			constexpr _float fCameraSweepRadius{ 0.25f };
			constexpr _float fCameraCollisionSkin{ 0.03f };
			constexpr _float fCameraCollisionRecoverSpeed{ 8.f };

			_float fAllowedArmLength = fDesiredArmLength;
			ResolveCameraSweepDistance(
				vTargetWorldPos,
				vArmDir,
				fDesiredArmLength,
				fCameraSweepRadius,
				fCameraCollisionSkin,
				fAllowedArmLength
			);

			if (!m_bCameraCollisionInit || fDesiredArmLength <= CAMERA_BASE_EPSILON) {
				m_fCameraCollisionArmLength = fAllowedArmLength;
				m_bCameraCollisionInit = true;
			}
			else if (fAllowedArmLength < m_fCameraCollisionArmLength) {
				m_fCameraCollisionArmLength = fAllowedArmLength;
			}
			else {
				const _float fRecoverAlpha = ExpDampAlpha(fCameraCollisionRecoverSpeed, _fTimeDelta);
				m_fCameraCollisionArmLength = lerp(m_fCameraCollisionArmLength, fAllowedArmLength, fRecoverAlpha);
			}

			m_fCameraCollisionArmLength = std::min(std::max(m_fCameraCollisionArmLength, 0.f), fDesiredArmLength);

			_vector vDestPos = vTargetWorldPos + vArmDir * m_fCameraCollisionArmLength;
			vDestPos += XMLoadFloat3(&m_vAdditionalCameraOffset);
			 
			const float a = ExpDampAlpha(m_fPosDamping, _fTimeDelta);

			_vector vNewPos = XMVectorLerp(vCameraWorldPos, vDestPos, a);

			_vector vCandidateArm = vNewPos - vTargetWorldPos;
			_float fCandidateArmLength = XMVectorGetX(XMVector3Length(vCandidateArm));
			if (fCandidateArmLength > CAMERA_BASE_EPSILON) {
				_vector vCandidateDir = vCandidateArm / fCandidateArmLength;
				_float fCandidateAllowedLength = fCandidateArmLength;
				if (ResolveCameraSweepDistance(
					vTargetWorldPos,
					vCandidateDir,
					fCandidateArmLength,
					fCameraSweepRadius,
					fCameraCollisionSkin,
					fCandidateAllowedLength
				)) {
					vNewPos = vTargetWorldPos + vCandidateDir * fCandidateAllowedLength;
					m_fCameraCollisionArmLength = std::min(m_fCameraCollisionArmLength, fCandidateAllowedLength);
				}
			}

			SetWorldPosition(m_pTransform, vNewPos);

			_vector dir = NormalizeOr(vTargetWorldPos - vNewPos, -vArmDir);

			_vector upRef = GetTransformBaseUp(m_pTransform);
			_vector baseRight{}, baseForward{};
			BuildBaseFrame(upRef, baseRight, baseForward);

			_vector right = XMVector3Cross(upRef, dir);
			if (XMVectorGetX(XMVector3LengthSq(right)) <= CAMERA_BASE_EPSILON)
				right = baseRight;
			right = XMVector3Normalize(right);
			_vector up = XMVector3Normalize(XMVector3Cross(dir, right));

			_vector qTarget = QuatFromBasis(right, up, dir);
			_vector qRoll = XMQuaternionRotationAxis(dir, XMConvertToRadians(m_fRoll));
			qTarget = XMQuaternionNormalize(XMQuaternionMultiply(qTarget, qRoll));

			float halfLifeRot = 0.01f;
			float a2 = DampAlpha(halfLifeRot, _fTimeDelta);

			_vector qCur = GetWorldQuat_FromWorldMatrix(m_pTransform->GetWorldMatrix());

			if (XMVectorGetX(XMVector4Dot(qCur, qTarget)) < 0.f)
				qTarget = XMVectorNegate(qTarget);

			_vector qNew = XMQuaternionSlerp(qCur, qTarget, a2);
			qNew = XMQuaternionNormalize(qTarget);
			_matrix rot = XMMatrixRotationQuaternion(qNew);

			_vector newRight = rot.r[0];
			_vector newUp = rot.r[1];
			_vector newLook = rot.r[2];

			m_pTransform->Set_State_NoDirty(STATE::RIGHT, newRight);
			m_pTransform->Set_State_NoDirty(STATE::UP, newUp);
			m_pTransform->Set_State(STATE::LOOK, newLook);
		}

	}
	else {
		m_bCameraCollisionInit = false;
	}

	
}

void CCharacterCamera::Enter(_float3 _vPrevCameraPos) {
	m_vPrevCameraPos = _vPrevCameraPos;
}

void CCharacterCamera::Leave() {

}

void CCharacterCamera::Character_Changed(CCharacterCamera* _pPrevPivot) {
	m_pStateController->ChangeAnimState(L"QTEEnd", true);

	if (auto pTargetObject = m_pTargetObject->Get()) {
		// 이전 카메라의 각도를 가지고 시작한다.
		m_fYaw = _pPrevPivot->m_fYaw;
		m_fPitch = _pPrevPivot->m_fPitch;
		m_fRoll = _pPrevPivot->m_fRoll;
	}
}


void CCharacterCamera::Character_Parried(CCharacterCamera* _pPrevPivot) {
	if (auto pTargetObject = m_pTargetObject->Get()) {
		// 이전 카메라의 각도를 가지고 시작한다.
		m_fYaw = _pPrevPivot->m_fYaw;
		m_fPitch = _pPrevPivot->m_fPitch;
		m_fRoll = _pPrevPivot->m_fRoll;

		m_fDestPitch = -20.f;

		// Decide the parry direction.
		// Use the character's local Y axis as the base so the two side offsets stay consistent.
		auto pTargetTransform = pTargetObject->GetTransform();
		SyncBaseUpFromTarget(m_pTransform, pTargetTransform);

		_vector vBaseUp = GetTransformBaseUp(m_pTransform);
		_vector vCharacterLook = ProjectOnBasePlane(pTargetTransform->Get_World(STATE::LOOK), vBaseUp, XMVectorSet(0.f, 0.f, 1.f, 0.f));
		_vector vCameraLook = ProjectOnBasePlane(_pPrevPivot->GetTransform()->Get_World(STATE::LOOK), vBaseUp, vCharacterLook);

		_vector vCross = XMVector3Cross(vCharacterLook, vCameraLook);

		// Decide the parry side on the BaseUp plane.
		_float fUpDot = XMVectorGetX(XMVector3Dot(vBaseUp, vCross));

		// Resolve yaw in the BaseUp frame.

		m_fDestYaw = CalculateYawOnBasePlane(m_pTransform, vCharacterLook);
		if (fUpDot >= 0.f) { // 왼쪽에 있다
			m_fDestYaw += 40.f;
			m_fRoll = -22.5f;
		}
		else { // 오른쪽에 있다
			m_fDestYaw -= 40.f;
			m_fRoll = 22.5f;
		}

		m_fDestYaw = fmodf(m_fDestYaw, 360.f);
		if (m_fDestYaw < 0.f) m_fDestYaw += 360.f;

		m_pStateController->ChangeAnimState(L"Parring", true);
	}

}

void CCharacterCamera::Begin_QTE() {
	m_pStateController->ChangeAnimState(L"QTE", true);
}

void CCharacterCamera::End_QTE() {
	m_pStateController->ChangeAnimState(L"QTEEnd", true);
}

void CCharacterCamera::Turn_To_Player_Look_Instant(_float _fPitch) {
	if (auto pTargetObject = m_pTargetObject->Get()) {
		m_fPitch = _fPitch;

		auto pTargetTransform = pTargetObject->GetTransform();
		SyncBaseUpFromTarget(m_pTransform, pTargetTransform);
		m_fYaw = CalculateYawOnBasePlane(m_pTransform, pTargetTransform->Get_World(STATE::LOOK));
	}
}

void CCharacterCamera::Init_Camera_Pos() {
	if (m_pTargetObject == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	auto pTargetObject = m_pTargetObject->Get();
	if (pTargetObject == nullptr || m_pTransform == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	auto pTargetTransform = pTargetObject->GetTransform();
	if (pTargetTransform == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	SyncBaseUpFromTarget(m_pTransform, pTargetTransform);

	Calc_LocalRotToWorld(m_vStartLocalRot);
	m_fPitch = clamp(m_fPitch, m_fPitchLimit.x, m_fPitchLimit.y);
	m_fDestPitch = m_fPitch;
	m_fDestYaw = m_fYaw;
	m_fDestRoll = m_fRoll;

	const _float fDestCameraArmLength = max(0.f, m_fDestCameraArmLength + m_fCameraArmLengthBias);
	m_fCameraArmLength = fDestCameraArmLength;

	_matrix matBase = BuildBaseMatrix(m_pTransform);
	_matrix matRot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_fPitch), XMConvertToRadians(m_fYaw), 0.f) * matBase;

	_vector vTargetWorldPos = pTargetTransform->Get_World(STATE::POSITION) + XMVector3TransformNormal(XMLoadFloat3(&m_vObjectOffset), matBase);
	_vector vDesiredArm = XMVector3TransformNormal(XMLoadFloat3(&m_vCameraOffset) * m_fCameraArmLength, matRot);
	_float fDesiredArmLength = XMVectorGetX(XMVector3Length(vDesiredArm));
	_vector vArmDir = NormalizeOr(vDesiredArm, -matRot.r[2]);

	constexpr _float fCameraSweepRadius{ 0.25f };
	constexpr _float fCameraCollisionSkin{ 0.03f };

	_float fAllowedArmLength = fDesiredArmLength;
	ResolveCameraSweepDistance(
		vTargetWorldPos,
		vArmDir,
		fDesiredArmLength,
		fCameraSweepRadius,
		fCameraCollisionSkin,
		fAllowedArmLength
	);

	m_fCameraCollisionArmLength = std::min(std::max(fAllowedArmLength, 0.f), fDesiredArmLength);
	m_bCameraCollisionInit = true;

	_vector vDestPos = vTargetWorldPos + vArmDir * m_fCameraCollisionArmLength;
	vDestPos += XMLoadFloat3(&m_vAdditionalCameraOffset);

	SetWorldPosition(m_pTransform, vDestPos);

	_vector vDir = NormalizeOr(vTargetWorldPos - vDestPos, -vArmDir);
	_vector vUpRef = GetTransformBaseUp(m_pTransform);
	_vector vBaseRight{}, vBaseForward{};
	BuildBaseFrame(vUpRef, vBaseRight, vBaseForward);

	_vector vRight = XMVector3Cross(vUpRef, vDir);
	if (XMVectorGetX(XMVector3LengthSq(vRight)) <= CAMERA_BASE_EPSILON)
		vRight = vBaseRight;
	vRight = XMVector3Normalize(vRight);

	_vector vUp = XMVector3Normalize(XMVector3Cross(vDir, vRight));
	_vector qTarget = QuatFromBasis(vRight, vUp, vDir);
	_vector qRoll = XMQuaternionRotationAxis(vDir, XMConvertToRadians(m_fRoll));
	qTarget = XMQuaternionNormalize(XMQuaternionMultiply(qTarget, qRoll));

	_matrix matCameraRot = XMMatrixRotationQuaternion(qTarget);
	m_pTransform->Set_State_NoDirty(STATE::RIGHT, matCameraRot.r[0]);
	m_pTransform->Set_State_NoDirty(STATE::UP, matCameraRot.r[1]);
	m_pTransform->Set_State(STATE::LOOK, matCameraRot.r[2]);

}

void CCharacterCamera::Snap_ToTargetKeepingRotation() {
	if (m_pTargetObject == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	auto pTargetObject = m_pTargetObject->Get();
	if (pTargetObject == nullptr || m_pTransform == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	auto pTargetTransform = pTargetObject->GetTransform();
	if (pTargetTransform == nullptr) {
		m_bCameraCollisionInit = false;
		return;
	}

	SyncBaseUpFromTarget(m_pTransform, pTargetTransform);

	m_fPitch = clamp(m_fPitch, m_fPitchLimit.x, m_fPitchLimit.y);
	m_fDestYaw = m_fYaw;
	m_fDestPitch = m_fPitch;
	m_fDestRoll = m_fRoll;

	m_vObjectOffset = m_vObjectDestOffset;

	const _float fDestCameraArmLength = max(0.f, m_fDestCameraArmLength + m_fCameraArmLengthBias);
	m_fCameraArmLength = fDestCameraArmLength;

	_matrix matBase = BuildBaseMatrix(m_pTransform);
	_matrix matRot = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_fPitch), XMConvertToRadians(m_fYaw), 0.f) * matBase;

	_vector vTargetWorldPos = pTargetTransform->Get_World(STATE::POSITION) + XMVector3TransformNormal(XMLoadFloat3(&m_vObjectOffset), matBase);
	_vector vDesiredArm = XMVector3TransformNormal(XMLoadFloat3(&m_vCameraOffset) * m_fCameraArmLength, matRot);
	_float fDesiredArmLength = XMVectorGetX(XMVector3Length(vDesiredArm));
	_vector vArmDir = NormalizeOr(vDesiredArm, -matRot.r[2]);

	constexpr _float fCameraSweepRadius{ 0.25f };
	constexpr _float fCameraCollisionSkin{ 0.03f };

	_float fAllowedArmLength = fDesiredArmLength;
	ResolveCameraSweepDistance(
		vTargetWorldPos,
		vArmDir,
		fDesiredArmLength,
		fCameraSweepRadius,
		fCameraCollisionSkin,
		fAllowedArmLength
	);

	m_fCameraCollisionArmLength = std::min(std::max(fAllowedArmLength, 0.f), fDesiredArmLength);
	m_bCameraCollisionInit = true;

	_vector vDestPos = vTargetWorldPos + vArmDir * m_fCameraCollisionArmLength;
	vDestPos += XMLoadFloat3(&m_vAdditionalCameraOffset);

	SetWorldPosition(m_pTransform, vDestPos);

	_vector vDir = NormalizeOr(vTargetWorldPos - vDestPos, -vArmDir);
	_vector vUpRef = GetTransformBaseUp(m_pTransform);
	_vector vBaseRight{}, vBaseForward{};
	BuildBaseFrame(vUpRef, vBaseRight, vBaseForward);

	_vector vRight = XMVector3Cross(vUpRef, vDir);
	if (XMVectorGetX(XMVector3LengthSq(vRight)) <= CAMERA_BASE_EPSILON)
		vRight = vBaseRight;
	vRight = XMVector3Normalize(vRight);

	_vector vUp = XMVector3Normalize(XMVector3Cross(vDir, vRight));
	_vector qTarget = QuatFromBasis(vRight, vUp, vDir);
	_vector qRoll = XMQuaternionRotationAxis(vDir, XMConvertToRadians(m_fRoll));
	qTarget = XMQuaternionNormalize(XMQuaternionMultiply(qTarget, qRoll));

	_matrix matCameraRot = XMMatrixRotationQuaternion(qTarget);
	m_pTransform->Set_State_NoDirty(STATE::RIGHT, matCameraRot.r[0]);
	m_pTransform->Set_State_NoDirty(STATE::UP, matCameraRot.r[1]);
	m_pTransform->Set_State(STATE::LOOK, matCameraRot.r[2]);
}

void CCharacterCamera::Turn_To_Player_Look(_float _fAnimTime, _float _fPitch) {
	//m_fDestPitch = _fPitch;
	m_pLookAnimation->Change_EndTime(_fAnimTime);
	m_pStateController->ChangeAnimState(L"TurnPlayerLook", true);
}

void CCharacterCamera::Turn_To_Player_Back(_float _fAnimTime, _float _fPitch) {
	m_pLookAnimation->Change_EndTime(_fAnimTime);
	m_pStateController->ChangeAnimState(L"TurnPlayerBack", true);
}

void CCharacterCamera::WipeOut() {
	m_pStateController->ChangeAnimState(L"WipeOut", true);
}

void CCharacterCamera::Begin_Execute()
{
	m_bMouseInputLocked = true;
	m_pStateController->ChangeAnimState(L"Execute", true);
}

void CCharacterCamera::End_Execute()
{
	m_pStateController->ChangeAnimState(L"ExecuteEnd", true);
}

_bool CCharacterCamera::Calc_LocalRot(_float3& _vOutLocalRot)
{
	if (nullptr == m_pTargetObject)
		return false;

	auto pTargetObject = m_pTargetObject->Get();
	if (nullptr == pTargetObject)
		return false;

	auto pTargetTransform = pTargetObject->GetTransform();
	if (nullptr == pTargetTransform || nullptr == m_pTransform)
		return false;

	_matrix matCamWorld = XMLoadFloat4x4(&m_pTransform->GetWorldMatrix());
	_matrix matTargetWorld = XMLoadFloat4x4(&pTargetTransform->GetWorldMatrix());

	_vector vCamScale, qCamRot, vCamTrans;
	_vector vTargetScale, qTargetRot, vTargetTrans;

	XMMatrixDecompose(&vCamScale, &qCamRot, &vCamTrans, matCamWorld);
	XMMatrixDecompose(&vTargetScale, &qTargetRot, &vTargetTrans, matTargetWorld);

	_matrix matCamRot = XMMatrixRotationQuaternion(qCamRot);
	_matrix matTargetRot = XMMatrixRotationQuaternion(qTargetRot);

	_matrix matLocalRot = matCamRot * XMMatrixInverse(nullptr, matTargetRot);

	_float localPitch = asinf(-XMVectorGetZ(matLocalRot.r[1]));
	_float localYaw = atan2f(XMVectorGetZ(matLocalRot.r[0]), XMVectorGetZ(matLocalRot.r[2]));
	_float localRoll = atan2f(XMVectorGetX(matLocalRot.r[1]), XMVectorGetY(matLocalRot.r[1]));

	_vOutLocalRot = _float3(XMConvertToDegrees(localPitch), XMConvertToDegrees(localYaw), XMConvertToDegrees(localRoll));

	return true;
}

void CCharacterCamera::Calc_LocalRotToWorld(const _float3& _vLocalRot)
{
	if (m_pTargetObject == nullptr)
		return;

	auto pTargetObject = m_pTargetObject->Get();
	if (pTargetObject == nullptr)
		return;

	auto pTargetTransform = pTargetObject->GetTransform();
	if (pTargetTransform == nullptr || m_pTransform == nullptr)
		return;

	_matrix matTargetWorld = XMLoadFloat4x4(&pTargetTransform->GetWorldMatrix());

	_vector vTargetScale, qTargetRot, vTargetTrans;
	XMMatrixDecompose(&vTargetScale, &qTargetRot, &vTargetTrans, matTargetWorld);

	_matrix matTargetRot = XMMatrixRotationQuaternion(qTargetRot);

	_matrix matLocalRot = XMMatrixRotationRollPitchYaw(
		XMConvertToRadians(_vLocalRot.x),
		XMConvertToRadians(_vLocalRot.y),
		XMConvertToRadians(_vLocalRot.z)
	);

	_matrix matWorldRot = matLocalRot * matTargetRot;

	_vector vWorldLook = matWorldRot.r[2];

	SyncBaseUpFromTarget(m_pTransform, pTargetTransform);

	_vector vBaseUp = GetTransformBaseUp(m_pTransform);
	_vector vLook = XMVector3Normalize(vWorldLook);

	_float fUpDot = XMVectorGetX(XMVector3Dot(vLook, vBaseUp));
	fUpDot = clamp(fUpDot, -1.f, 1.f);

	m_fPitch = XMConvertToDegrees(asinf(fUpDot));
	m_fYaw = CalculateYawOnBasePlane(m_pTransform, vLook);
	m_fRoll = _vLocalRot.z;

	m_fDestPitch = m_fPitch;
	m_fDestYaw = m_fYaw;
	m_fDestRoll = m_fRoll;
}

void CCharacterCamera::OnGui_Inspector_Context() {
	__super::OnGui_Inspector_Context();

	ImGui::DragFloat3("Dest Object Offset", &m_vObjectDestOffset.x, 0.01f);
	ImGui::DragFloat("Object Offset Speed", &m_fObjectOffsetSpeed);

	ImGui::Separator();

	ImGui::DragFloat("Camera Arm Length", &m_fDestCameraArmLength);
	ImGui::DragFloat("Camera Arm Speed", &m_fCameraArmSpeed);

	ImGui::Separator();

	ImGui::DragFloat("Mouse Sensor", &m_fMouseSensor);

	_float fRot[3] = {
		m_fPitch,
		m_fYaw,
		m_fRoll
	};

	if (ImGui::DragFloat3("Rot", fRot)) {
		m_fPitch = fRot[0];
		m_fYaw = fRot[1];
		m_fRoll = fRot[2];
	}

	_float fLocalRot[3] = {
		m_fLocalPitch,
		m_fLocalYaw,
		m_fLocalRoll
	};

	_bool bChanged = ImGui::DragFloat3("Local Rot", fLocalRot, 0.01f);

	if (bChanged) {
		// DestYaw, Yaw Pitch Roll 모두 fLocalRot을 로컬 -> 월드로 옮긴 상태의 값으로 적용
		m_fLocalPitch = fLocalRot[0];
		m_fLocalYaw = fLocalRot[1];
		m_fLocalRoll = fLocalRot[2];

		Calc_LocalRotToWorld(_float3(m_fLocalPitch, m_fLocalYaw, m_fLocalRoll));
	}
	else if (!ImGui::IsItemActive()) {
		_float3 vLocalRot{};
		if (Calc_LocalRot(vLocalRot)) {
			m_fLocalPitch = vLocalRot.x;
			m_fLocalYaw = vLocalRot.y;
			m_fLocalRoll = vLocalRot.z;
		}
	}

	ImGui::DragFloat2("Pitch Limit", &m_fPitchLimit.x, 0.01f);
	ImGui::DragFloat3("Start Local Rot", &m_vStartLocalRot.x, 0.01f);

	ImGui::SeparatorText("[Debug] Camera Offsets");
	ImGui::DragFloat3("[Debug] CameraOffset", &m_vCameraOffset.x, 0.01f);
	ImGui::DragFloat3("[Debug] CameraDestOffset", &m_vCameraDestOffset.x, 0.01f);
	ImGui::DragFloat3("[Debug] ObjectOffset", &m_vObjectOffset.x, 0.01f);
	ImGui::DragFloat3("[Debug] ObjectDestOffset", &m_vObjectDestOffset.x, 0.01f);
}

Json::Value CCharacterCamera::Serialize() {
	Json::Value jsonResult = __super::Serialize();

	jsonResult["ObjectOffset"] = Serialize_Vector(m_vObjectOffset);
	jsonResult["MouseSensor"] = m_fMouseSensor;
	jsonResult["StartLocalRot"] = Serialize_Vector(m_vStartLocalRot);

	return jsonResult;
}

void CCharacterCamera::Deserialize(Json::Value& _jsonValue) {
	__super::Deserialize(_jsonValue);

	Get_Json(_jsonValue, "ObjectOffset", m_vObjectOffset);
	Get_Json(_jsonValue, "MouseSensor", m_fMouseSensor);
	Get_Json(_jsonValue, "StartLocalRot", m_vStartLocalRot);
}


void CCharacterCamera::Free() {
	__super::Free();

	Safe_Release(m_pLookTransform);
	Safe_Release(m_pTargetObject);
}
