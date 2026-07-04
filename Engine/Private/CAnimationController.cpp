#include "EnginePCH.h"
#include "CAnimationController.h"
#include "CGameInstance.h"
#include "CTransform.h"
#include <CAnimationStateMachine.h>

CAnimationController::CAnimationController(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext, CObject* _pOwner)
	: CUpdateComponent(_pDevice, _pContext, _pOwner), m_pGameInstance(CGameInstance::GetInstance()) {
	Safe_AddRef(m_pGameInstance);
}

HRESULT CAnimationController::Initialize() {
	if (auto pASM = m_pOwner->GetComponent<CAnimationStateMachine>()) {
		SetActive(false);
		pASM->Set_AnimationController(this);
	}

	m_pAsset_Animation = WAsset<CAsset_Animation>::Create(nullptr);
	m_pCurAnimState = new ANIMATION_STATE();
	m_pCCCT = WComponent<CCCT>::Create(nullptr);

	m_pRootMotion_Target = WObject<CObject>::Create(nullptr);
	m_pRootMotion_X = WObject<CObject>::Create(nullptr);
	m_pRootMotion_Z = WObject<CObject>::Create(nullptr);

	return S_OK;
}

void CAnimationController::Update(_float _fTimeDelta) {
	_fTimeDelta *= m_fDeltaMultiply;

	if (!m_bPlayEnable)
		return;

	if (!m_pAsset_Animation || !m_pCurAnimState || !m_pCurAnimState->pAnimation)
		return;

	m_pCurAnimState->pCCCT = m_pCCCT->Get();
	m_pCurAnimState->bEnableSubRootMotion_Y = m_bEnableSubRootMotion_Y;
	Apply_RootMotionLimit(m_pCurAnimState);
	if (m_pPreAnimState)
		Apply_RootMotionLimit(m_pPreAnimState);

	if (m_bEditorMode)
	{
		Play_AnimationByEditor(_fTimeDelta);
		Update_Additive(_fTimeDelta);

		Update_EventFunction(_fTimeDelta);
		m_pCurAnimation->Update_CustomAnim(m_pCurAnimState);
		return;
	}

	/* Loop + Blend */
	Play_Animation(_fTimeDelta);

	/* Additive */
	Update_Additive(_fTimeDelta);

	/* Event */
	Update_EventFunction(_fTimeDelta);
	m_pCurAnimation->Update_CustomAnim(m_pCurAnimState);

	m_pGameInstance->ThreadPool_Push_Job([this]() {
		m_pOwner->GetTransform()->Calculate_World();
	});
}

void CAnimationController::Late_Update(_float _fTimeDelta) {

}

void CAnimationController::Play()
{
	m_bPlayEnable = true;
}

void CAnimationController::Play(_string _strAnimName, _bool _bLoop) {
	NonBlend_Animation(_strAnimName);

	m_pCurAnimState->bLoop = _bLoop;
	m_pCurAnimState->bRestInit = true;

	m_bPlayEnable = true;
}

void CAnimationController::Play(_string _strAnimName, _bool _bLoop, _float _fBlendTime) {
	Blend_Animation(_strAnimName, _fBlendTime);

	m_pCurAnimation->bBlending = true;
	m_pCurAnimation->fBlendTime = _fBlendTime;

	m_pCurAnimState->bRestInit = true;
	m_pCurAnimState->bLoop = _bLoop;

	_float fBlendTime = (m_pCurAnimation->bBlending) ? m_pCurAnimation->fBlendTime : 0.f;
	if (m_bBlending && fBlendTime > 0.f)
	{
		m_fBlendElapsed = fBlendTime;
		m_fBlendDuration = fBlendTime;
	}

	m_bPlayEnable = true;
}

void CAnimationController::Stop()
{
	m_bPlayEnable = false;

	/* Blend */
	Safe_Delete(m_pPreAnimState);
	m_pPreAnimState = nullptr;
	m_fBlendDuration = 0.f;
	m_fBlendElapsed = 0.f;

	/* Event */
	m_fPrevTime = 0.f;
	m_bEventFirstTick = false;

	/* Additive */
	this->Reset_Additive();

	/* State */
	if (m_pCurAnimState)
	{
		m_pCurAnimState->fTime = 0.01f;
		m_pCurAnimState->bFinished = false;
		m_pCurAnimState->Reset_KeyFrameIndex();

		m_pCurAnimState->vPreRootXPos = {};
		m_pCurAnimState->vPreRootZPos = {};
	}

	/* Transfrom */
	m_pOwner->GetTransform()->Set_Dirty();
}

void CAnimationController::Pause()
{
	m_bPlayEnable = false;
}

void CAnimationController::Hold(_float _fRatio)
{
	if (!m_pCurAnimState || !m_pCurAnimState->pAnimation)
		return;

	_float fDuration = m_pCurAnimState->pAnimation->fDuration;
	_float fClampTime = fDuration * _fRatio;
	if (m_pCurAnimState->fTime >= fClampTime)
	{
		m_pCurAnimState->fTime = fClampTime;
		m_pCurAnimState->bFinished = false;
	}
}

_bool CAnimationController::Is_Playing() const
{
	return m_bPlayEnable;
}

_bool CAnimationController::Is_AnimationEnded() const {
	return m_pCurAnimState->bFinished;
}

ANIMATION_INFO* CAnimationController::Get_CurAnimation() const {
	return m_pCurAnimation;
}

ANIMATION_STATE* CAnimationController::Get_CurAnimationState() {
	return m_pCurAnimState;
}

ANIMATION_INFO* CAnimationController::Get_CurAnimationByName(_string _strName) const
{
	auto pAsset = m_pAsset_Animation->Get();
	if (pAsset == nullptr)
		return nullptr;

	return pAsset->Get_AnimationInfo(_strName);
}

CAsset_Animation* CAnimationController::Get_Asset_Animation() const
{
	if (!m_pAsset_Animation)
		return nullptr;

	return m_pAsset_Animation->Get();
}

vector<_string> CAnimationController::Get_AnimationNames() const
{
	if (!m_pAsset_Animation)
		return {};

	auto pAsset = m_pAsset_Animation->Get();
	if (pAsset == nullptr)
		return {};

	vector<_string> vecAnimNames;
	for (auto& Pair : pAsset->Get_AnimationSequencies())
	{
		const _string& strAnimName = Pair.first;
		vecAnimNames.push_back(strAnimName);
	}

	return vecAnimNames;
}

void CAnimationController::Set_EditorMode(_bool _bEnable)
{
	m_bEditorMode = _bEnable;
}

_bool CAnimationController::Is_EditorMode() const
{
	return m_bEditorMode;
}

void CAnimationController::Set_Time_Editor(_float _fTime)
{
	if (!m_pCurAnimation || !m_pCurAnimState->pAnimation)
		return;

	_float fDuration = m_pCurAnimState->pAnimation->fDuration;
	m_pCurAnimState->fTime = max(0.f, min(_fTime, fDuration));
	m_fPrevTime = m_pCurAnimState->fTime;

	m_pCurAnimState->Reset_KeyFrameIndex();

	for (auto& layer : m_vecAdditiveLayers)
	{
		if (layer.pState)
			layer.pState->Reset_KeyFrameIndex();
	}

	/* 결과 반영 */
	Play_AnimationByEditor(0.f);
	Play_Additive(0.f);
	Apply_Additive();

	m_pCurAnimation->Update_CustomAnim(m_pCurAnimState);

	m_pOwner->GetTransform()->Calculate_World();
}

void CAnimationController::Change_Animation_Editor(ANIMATION_INFO* _pAnimationInfo)
{
	if (!_pAnimationInfo || !m_pCurAnimState)
		return;

	_float fEditorTime = m_pCurAnimState->fTime;

	Safe_Delete(m_pPreAnimState);
	m_pPreAnimState = nullptr;
	Reset_PreAdditive();
	m_fBlendElapsed = 0.f;
	m_fBlendDuration = 0.f;

	m_pCurAnimation = _pAnimationInfo;

	if (m_pCurAnimState->pAnimation != _pAnimationInfo)
	{
		Change_Animation(_pAnimationInfo, m_pCurAnimState);
		Build_Additive();
		Build_BoneHierarchy(0);
	}

	if (auto pObject = m_pRootMotion_Target->Get()) {
		m_pCurAnimState->pRootBoneTarget = pObject;
	}
	if (auto pObject = m_pRootMotion_X->Get()) {
		m_pCurAnimState->pRootBoneX = pObject->GetTransform();
	}
	if (auto pObject = m_pRootMotion_Z->Get()) {
		m_pCurAnimState->pRootBoneZ = pObject->GetTransform();
	}
	m_pCurAnimState->bRootMotionEnableX = m_bRootMotionEnable_X;
	m_pCurAnimState->bRootMotionEnableZ = m_bRootMotionEnable_Z;
	Apply_RootMotionLimit(m_pCurAnimState);

	Set_Time_Editor(fEditorTime);
}

_float CAnimationController::Get_Time() const
{
	if (!m_pCurAnimState)
		return 0.f;

	return m_pCurAnimState->fTime;
}

void CAnimationController::Set_ApplyRootMotion(_bool _bApply)
{
	_bool bApply = _bApply && !m_bDebugRootMotionMuteAll;

	if (m_pCurAnimState)
		m_pCurAnimState->bSwitchRootMotion = bApply;

	if (m_pPreAnimState)
		m_pPreAnimState->bSwitchRootMotion = bApply;
}

void CAnimationController::Set_ApplyRootMotionX(_bool _bApply)
{
	_bool bApply = _bApply && !m_bDebugRootMotionMuteAll;

	if (m_pCurAnimState)
	{
		m_pCurAnimState->bRootMotionEnableX = _bApply;
	}

	if (m_pPreAnimState)
	{
		m_pPreAnimState->bRootMotionEnableX = _bApply;
	}
}

void CAnimationController::Set_ApplyRootMotionZ(_bool _bApply)
{
	_bool bApply = _bApply && !m_bDebugRootMotionMuteAll;

	if (m_pCurAnimState)
	{
		m_pCurAnimState->bRootMotionEnableZ = _bApply;
	}

	if (m_pPreAnimState)
	{
		m_pPreAnimState->bRootMotionEnableZ = _bApply;
	}
}

void CAnimationController::Set_RootMotionTarget(CObject* _pTarget)
{
	m_pRootMotion_Target->Set(_pTarget);
}

void CAnimationController::Set_RootMotionMute(_float _fWeight)
{
	if (m_pCurAnimation)
		m_pCurAnimation->fRootMotionMuteWeight = _fWeight;
}

void CAnimationController::Set_RootMotionMultiplierX(_float _fMultiplier) {
	if (m_pCurAnimation)
		m_pCurAnimation->fRootMotionMultiplierX = _fMultiplier;
}

void CAnimationController::Set_RootMotionMultiplierZ(_float _fMultiplier) {
	if (m_pCurAnimation)
		m_pCurAnimation->fRootMotionMultiplierZ = _fMultiplier;
}

void CAnimationController::Set_RootMotionMultiplierZ_C(const _float& _fMultiplier) {
	if (m_pCurAnimation)
		m_pCurAnimation->fRootMotionMultiplierZ = _fMultiplier;
}

void CAnimationController::Set_SubRootMotion_Y(_bool _bApply)
{
	m_bEnableSubRootMotion_Y = _bApply;
	
	if (m_pCurAnimState)
		m_pCurAnimState->bEnableSubRootMotion_Y = _bApply;

	if (m_pPreAnimState)
		m_pPreAnimState->bEnableSubRootMotion_Y = _bApply;
}

void CAnimationController::Set_RootMotionMaxDistance(_bool _bActive, _float _fDistance)
{
	if (_bActive)
	{
		m_bRootMotionMaxDistanceEnable = true;
		m_fRootMotionMaxDistance = max(0.f, _fDistance);

		Apply_RootMotionLimit(m_pCurAnimState);
		Apply_RootMotionLimit(m_pPreAnimState);
	}
	else
	{
		m_bRootMotionMaxDistanceEnable = false;
		m_fRootMotionMaxDistance = 0.f;

		Apply_RootMotionLimit(m_pCurAnimState);
		Apply_RootMotionLimit(m_pPreAnimState);
	}
}

//Event Function
void CAnimationController::Set_MuteWeight(const _float& _fWeight)
{
	if (m_pCurAnimation)
		m_pCurAnimation->fRootMotionMuteWeight = _fWeight;
}

void CAnimationController::Set_DeltaMultiplier(const _float& _fDelta) {
	m_fDeltaMultiply = _fDelta;
}

void CAnimationController::Update_EventFunction(_float _fTimeDelta)
{
	if (!m_pCurAnimState || !m_pCurAnimState->pAnimation)
		return;

	const _float fDuration = max(m_pCurAnimState->pAnimation->fDuration, 0.0001f);
	_float fPrevTime = clamp(m_fPrevTime, 0.f, fDuration);
	_float fCurTime = clamp(m_pCurAnimState->fTime, 0.f, fDuration);

	if (m_bEventFirstTick)
		fPrevTime = 0.f;

	if (fPrevTime == fCurTime)
	{
		m_fPrevTime = fCurTime;
		m_bEventFirstTick = false;
		return;
	}

	const _bool bWrapped = m_pCurAnimState->bLoop && (fCurTime < fPrevTime);
	const _bool bForceTailCheck = (!m_pCurAnimState->bLoop && m_pCurAnimState->bFinished);
	auto isEventCrossed = [&](const _float fEventTime) -> _bool {
		_float fTrackPos = clamp(fEventTime, 0.f, fDuration);

		if (!bWrapped)
			return (fTrackPos >= fPrevTime) && (fTrackPos <= fCurTime);

		return (fTrackPos <= fCurTime);
	};

	auto& vecEvent = m_pCurAnimState->pAnimation->vecEvents;

	vector<_uint> vecEventOrder;
	vecEventOrder.reserve(vecEvent.size());

	for (_uint i = 0; i < vecEvent.size(); ++i)
		vecEventOrder.push_back(i);

	std::stable_sort(vecEventOrder.begin(), vecEventOrder.end(),
		[&](const _uint iLhs, const _uint iRhs)
		{
			const _float fLhs = clamp(vecEvent[iLhs].tKF.fTrackPosition, 0.f, fDuration);
			const _float fRhs = clamp(vecEvent[iRhs].tKF.fTrackPosition, 0.f, fDuration);

			if (fLhs == fRhs)
				return iLhs < iRhs;

			return fLhs < fRhs;
		});

	for (_uint iEventIndex : vecEventOrder)
	{
		auto& event = vecEvent[iEventIndex];
		if (!event.pFunction)
			continue;

		auto* pFuncDesc = event.pFunction->Get_FunctionDesc();
		if (pFuncDesc == nullptr)
			continue;

		auto* pContext = m_pCurAnimState->Get_EventBindContext(iEventIndex);
		if (pContext == nullptr)
			continue;

		if (FAILED(event.pFunction->Try(pContext))) {
			continue;
		}

		const _float fEventTime = clamp(event.tKF.fTrackPosition, 0.f, fDuration);
		if (!pContext->bExecuted &&
			(isEventCrossed(fEventTime) || (bForceTailCheck && fEventTime <= fCurTime)))
		{
			pContext->bExecuted = true;

			auto tType = pFuncDesc->tType;
			if (tType == FUNCTION_PROPERTY_TYPE::INT)
				event.pFunction->Invoke(pContext, ParamValue{ event.tKF.iParam });
			else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT1)
				event.pFunction->Invoke(pContext, ParamValue{ event.tKF.fParam });
			else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT2)
				event.pFunction->Invoke(pContext, ParamValue{ event.tKF.v2Param });
			else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT3)
				event.pFunction->Invoke(pContext, ParamValue{ event.tKF.v3Param });
			else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT4)
				event.pFunction->Invoke(pContext, ParamValue{ event.tKF.v4Param });
			else if (tType == FUNCTION_PROPERTY_TYPE::MONOSTATE)
				event.pFunction->Invoke(pContext, ParamValue{});
		}
	}

	m_fPrevTime = fCurTime;
	m_bEventFirstTick = false;
}

void CAnimationController::Change_Animation(ANIMATION_INFO* _pAnim, ANIMATION_STATE* _pState) {
	if (!_pState || !_pAnim)
		return;

	_float3 vPreRootX = _pState->vPreRootXPos;
	_float3 vPreRootZ = _pState->vPreRootZPos;

	/* New Animation */
	_pState->pAnimation = _pAnim;
	_pState->Reset(m_pOwner);
	
	_pState->fTime = 0.01f;
	if (_pState == m_pCurAnimState)
	{
		m_fPrevTime = _pState->fTime;
		m_bEventFirstTick = true;
	}

	_pState->vPreRootXPos = m_pCurAnimState->vPreRootXPos;
	_pState->vPreRootZPos = m_pCurAnimState->vPreRootZPos;

	/* Cache */
	_pState->Cache_BoneTransform(m_pOwner->GetTransform());

	/* Event */
	_pState->Init_BindingContext(m_pOwner);
}

void CAnimationController::Change_AnimationByName(const _string& _strName, _int _iLayerIndex)
{
	auto pAsset = m_pAsset_Animation->Get();
	if (pAsset == nullptr)
		return;
		
	/* Animation */
	ANIMATION_INFO* pAnimInfo = pAsset->Get_AnimationInfo("Stand1");

	if (!pAnimInfo)
		pAnimInfo = pAsset->Get_AnimationInfo("Stand2");

	if (!pAnimInfo && !_strName.empty())
		pAnimInfo = pAsset->Get_AnimationInfo(_strName);

	if (!pAnimInfo)
	{
		auto& AnimInfos = pAsset->Get_AnimationSequencies();
		if (!AnimInfos.empty())
			pAnimInfo = pAsset->Get_AnimationInfo(AnimInfos.begin()->first);
	}

	m_pCurAnimation = pAnimInfo;

	/* Root Motion (.json) */
	if (auto pObject = m_pRootMotion_Target->Get()) {
		m_pCurAnimState->pRootBoneTarget = pObject;
	}
	if (auto pObject = m_pRootMotion_X->Get()) {
		m_pCurAnimState->pRootBoneX = pObject->GetTransform();
	}
	if (auto pObject = m_pRootMotion_Z->Get()) {
		m_pCurAnimState->pRootBoneZ = pObject->GetTransform();
	}
	m_pCurAnimState->bRootMotionEnableX = m_bRootMotionEnable_X;
	m_pCurAnimState->bRootMotionEnableZ = m_bRootMotionEnable_Z;
	Apply_RootMotionLimit(m_pCurAnimState);

	Change_Animation(pAnimInfo, m_pCurAnimState);
	Build_Additive();
	Build_BoneHierarchy(_iLayerIndex);
}

void CAnimationController::Loop_Animation(_float _fTimeDelta, ANIMATION_STATE* _pState)
{
	if (!_pState || !_pState->pAnimation)
		return;

	_pState->fTime += _fTimeDelta;

	/* Loop */
	if (_pState->fTime >= _pState->pAnimation->fDuration)
	{
		if (_pState->bLoop) {
			_pState->fTime = fmod(_pState->fTime, _pState->pAnimation->fDuration);
			_pState->Reset_KeyFrameIndex();

			CTransform* pRootBoneX = _pState->pRootBoneX;
			CTransform* pRootBoneZ = _pState->pRootBoneZ;

			if (pRootBoneX)
			{
				_float3 vCurRootBoneX;
				XMStoreFloat3(&vCurRootBoneX, pRootBoneX->Get_Local(STATE::POSITION));
				_pState->vPreRootXPos = vCurRootBoneX;
			}
			
			if (pRootBoneZ)
			{
				_float3 vCurRootBoneZ;
				XMStoreFloat3(&vCurRootBoneZ, pRootBoneZ->Get_Local(STATE::POSITION));
				_pState->vPreRootZPos = vCurRootBoneZ;
			}
		}
		else {
			_pState->fTime = _pState->pAnimation->fDuration;
			_pState->bFinished = true;
		}
	}
}

void CAnimationController::Update_Additive(_float _fTimeDelta)
{
	if (m_vecAdditiveLayers.empty())
		return;

	_float fBlendRatio = (m_fBlendDuration > 0.f) ? (1.f - (m_fBlendElapsed / m_fBlendDuration)) : 1.f;

	for (_uint iLayer = 0; iLayer < m_vecAdditiveLayers.size(); iLayer++)
	{	
		auto& tLayer = m_vecAdditiveLayers[iLayer];
		if (!tLayer.bAdditive || !tLayer.pAnimation || !tLayer.pState || tLayer.fWeight <= 0.f)
			continue;

		auto& tCurState = m_pCurAnimState;
		auto& tAdditiveState = tLayer.pState;

		auto& vecCurChannels = tCurState->pAnimation->vecChannels;
		auto& vecAdditiveChannels = tAdditiveState->pAnimation->vecChannels;

		_float fCurRatio = tCurState->fTime / tCurState->pAnimation->fDuration;
		_float fAdditiveTime = fCurRatio * tAdditiveState->pAnimation->fDuration;

		ADDITIVE_LAYER* pPreLayer = { nullptr };
		if (iLayer < m_vecPreAdditiveLayers.size())
			pPreLayer = &m_vecPreAdditiveLayers[iLayer];

		if (m_vecChannelToBoneIndex.empty())
			continue;

		// Treat missing CurChannel/AdditiveChannel pairs as absent.
		for (_uint i = 0; i < vecCurChannels.size(); i++)
		{
			/* -1. 캐싱된 BoneMask */
			_int iBoneIndex = m_vecChannelToBoneIndex[i];
			if (iBoneIndex == -1)
				continue;

			// 각 프레임별로 Bip../Neck../Head../Hair... 이런 애들 거른다고..
			auto& BoneAdditive = tLayer.vecBoneAdditive;
			if (iBoneIndex < 0 || iBoneIndex >= BoneAdditive.size())
				continue;

			if (!BoneAdditive[iBoneIndex])
				continue;

			/* 0. Additive */
			CTransform* pCurBone = tCurState->vecBoneTransformCache[i];
			if (!pCurBone)
				continue;

			/* 1. KeyFrame */
			KEY_FRAME tAdditiveKeyFrame = vecAdditiveChannels[i].Get_CurrentKeyFrame(
				&tAdditiveState->vecKeyFrameIndices[i], fAdditiveTime);
			KEY_FRAME tBaseKeyFrame = vecCurChannels[i].Get_CurrentKeyFrame(
					&tCurState->vecKeyFrameIndices[i], tCurState->fTime);

			/* 2. Delta (Pos) */
			_vector vAdditivePos = XMLoadFloat3(&tAdditiveKeyFrame.vPosition);
			_vector vBasePos = XMLoadFloat3(&tBaseKeyFrame.vPosition);
			
			_vector vDeltaPos = (vAdditivePos - vBasePos) * tLayer.fWeight;
			
			/* 2. Delta (Rot) */
			_vector vAdditiveRot = XMLoadFloat4(&tAdditiveKeyFrame.vRotation);
			_vector vBaseRot = XMLoadFloat4(&tBaseKeyFrame.vRotation);

			_vector vDeltaRot = XMQuaternionMultiply(vAdditiveRot, XMQuaternionInverse(vBaseRot));
			vDeltaRot = XMQuaternionSlerp(XMQuaternionIdentity(), vDeltaRot, tLayer.fWeight);

			/* 3. 보간 */
			if (pPreLayer && pPreLayer->pState && pPreLayer->pAnimation && fBlendRatio < 1.f && i < pPreLayer->pAnimation->vecChannels.size())
			{
				auto& vecPreChannels = pPreLayer->pAnimation->vecChannels;
				auto& tPreState = pPreLayer->pState;

				_float fPreRatio = m_pPreAnimState ?
						(m_pPreAnimState->fTime / m_pPreAnimState->pAnimation->fDuration) : fCurRatio;
				_float fPreAdditiveTime = fPreRatio * tPreState->pAnimation->fDuration;
				KEY_FRAME tPreAdditiveKF = vecPreChannels[i].Get_CurrentKeyFrame(&tPreState->vecKeyFrameIndices[i], fPreAdditiveTime);

				_vector vPreDeltaPos = XMVectorZero();
				_vector vPreDeltaRot = XMQuaternionIdentity();

				if (i < m_pPreAnimState->pAnimation->vecChannels.size())
				{
					KEY_FRAME tPreBaseKF = m_pPreAnimState->pAnimation->vecChannels[i].Get_CurrentKeyFrame(&m_pPreAnimState->vecKeyFrameIndices[i], m_pPreAnimState->fTime);

					_vector vPreKFPos = XMLoadFloat3(&tPreAdditiveKF.vPosition);
					_vector vBaseKFPos = XMLoadFloat3(&tPreBaseKF.vPosition);
					vPreDeltaPos = (vPreKFPos - vBaseKFPos) * pPreLayer->fWeight;
					
					_vector vPreKFRot = XMLoadFloat4(&tPreAdditiveKF.vRotation);
					_vector vPreBaseRot = XMLoadFloat4(&tPreBaseKF.vRotation);
					vPreDeltaRot = XMQuaternionMultiply(vPreKFRot, XMQuaternionInverse(vPreBaseRot));
					vPreDeltaRot = XMQuaternionSlerp(XMQuaternionIdentity(), vPreDeltaRot, pPreLayer->fWeight);
				}

				vDeltaPos = XMVectorLerp(vPreDeltaPos, vDeltaPos, fBlendRatio);
				vDeltaRot = XMQuaternionSlerp(vPreDeltaRot, vDeltaRot, fBlendRatio);
			}

			/* 4. Set Final (Pos) */
			_vector vCurPos = pCurBone->Get_Local(STATE::POSITION);
			pCurBone->Set_Position_NoDirty(vCurPos + vDeltaPos);

			/* 4. Set Final (Rot) */
			_matrix matLocal = XMLoadFloat4x4(&pCurBone->GetLocalMatrix());
			_vector vRot = XMQuaternionRotationMatrix(XMLoadFloat4x4(&pCurBone->GetLocalMatrix()));
			_vector vFinalRot = XMQuaternionMultiply(vDeltaRot, vRot);
			 pCurBone->Set_Rotation_NoDirty(vFinalRot);
		}
	}
}

/* depre */
void CAnimationController::Play_Additive(_float _fTimeDelta)
{
	for (auto& tLayer : m_vecAdditiveLayers)
	{
		if (!tLayer.bAdditive || !tLayer.pState || !tLayer.pAnimation || tLayer.fWeight <= 0.f)
			continue;

		Loop_Animation(_fTimeDelta, tLayer.pState);

		tLayer.pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), tLayer.pState, tLayer.pState);
	}

	for (auto& tLayer : m_vecPreAdditiveLayers)
	{
		if (!tLayer.bAdditive || !tLayer.pState || !tLayer.pAnimation || tLayer.fWeight <= 0.f)
			continue;

		Loop_Animation(_fTimeDelta, tLayer.pState);

		tLayer.pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), tLayer.pState, tLayer.pState);
	}
}

/* depre */
void CAnimationController::Apply_Additive()
{
	if (m_vecAdditiveLayers.empty())
		return;

	_float fBlendRatio = (m_fBlendDuration > 0.f) ? (1.f - (m_fBlendElapsed / m_fBlendDuration)) : 1.f;
	fBlendRatio = clamp(fBlendRatio, 0.f, 1.f);

	_bool bIsBlending = (m_fBlendElapsed > 0.f);

	for (_uint i = 0; i < m_vecAdditiveLayers.size(); i++)
	{
		auto& tLayer = m_vecAdditiveLayers[i];
		if (!tLayer.bAdditive || !tLayer.pAnimation || !tLayer.pState || tLayer.fWeight <= 0.f)
			continue;

		ANIMATION_STATE* pCurBaseState = m_pCurAnimState;
		ANIMATION_STATE* pCurAddState = tLayer.pState;

		ADDITIVE_LAYER* pPreLayer = { nullptr };
		ANIMATION_STATE* pPreBaseState = { nullptr };
		ANIMATION_STATE* pPreAddState = { nullptr };

		if (i < m_vecPreAdditiveLayers.size())
		{
			pPreLayer = &m_vecPreAdditiveLayers[i];
			if (pPreLayer->bAdditive && pPreLayer->pAnimation && pPreLayer->pState && pPreLayer->fWeight)
			{
				pPreBaseState = m_pPreAnimState;
				pPreAddState = pPreLayer->pState;
			}
		}

		for (_uint j = 0; j < pCurBaseState->vecPose.size(); j++)
		{
			_int iBoneIndex = m_vecChannelToBoneIndex[j];
			if (iBoneIndex < 0 || iBoneIndex >= tLayer.vecBoneAdditive.size() || !tLayer.vecBoneAdditive[iBoneIndex])
				continue;

			/* Additive Delta */
			auto& tCurBasePose = pCurBaseState->vecPose[j];
			auto& tCurAddPose = pCurAddState->vecPose[j];

			CTransform* pCurBone = pCurBaseState->vecBoneTransformCache[j];
			if (!pCurBone) continue;

			_vector vCurBasePos = XMLoadFloat3(&tCurBasePose.vPosition);
			_vector vCurAddPos = XMLoadFloat3(&tCurAddPose.vPosition);
			_vector vCurDeltaPos = (vCurAddPos - vCurBasePos) * tLayer.fWeight;

			_vector vCurBaseRot = XMLoadFloat4(&tCurBasePose.vRotation);
			_vector vCurAddRot = XMLoadFloat4(&tCurAddPose.vRotation);
			_vector vCurDeltaRot = XMQuaternionMultiply(vCurAddRot, XMQuaternionInverse(vCurBaseRot));
			vCurDeltaRot = XMQuaternionSlerp(XMQuaternionIdentity(), vCurDeltaRot, tLayer.fWeight);
			vCurDeltaRot = XMQuaternionNormalize(vCurDeltaRot);

			/* Final */
			_vector vFinalPos;
			_vector vFinalRot;

			/* if Blending... */
			if (bIsBlending && pPreLayer && pPreBaseState && pPreAddState)
			{
				auto& tPreBasePose = pPreBaseState->vecPose[j];
				auto& tPreAddPose = pPreAddState->vecPose[j];

				_vector vPreBasePos = XMLoadFloat3(&tPreBasePose.vPosition);
				_vector vPreAddPos = XMLoadFloat3(&tPreAddPose.vPosition);
				_vector vPreDeltaPos = (vPreAddPos - vPreBasePos) * pPreLayer->fWeight;

				_vector vPreBaseRot = XMLoadFloat4(&tPreBasePose.vRotation);
				_vector vPreAddRot = XMLoadFloat4(&tPreAddPose.vRotation);
				_vector vPreDeltaRot = XMQuaternionMultiply(vPreAddRot, XMQuaternionInverse(vPreBaseRot));
				vPreDeltaRot = XMQuaternionSlerp(XMQuaternionIdentity(), vPreDeltaRot, pPreLayer->fWeight);
				vPreDeltaRot = XMQuaternionNormalize(vPreDeltaRot);

				_vector vBlendedBasePos = XMVectorLerp(vPreBasePos, vCurBasePos, fBlendRatio);
				_vector vBlendedBaseRot = XMQuaternionSlerp(vPreBaseRot, vCurBaseRot, fBlendRatio);
				vBlendedBaseRot = XMQuaternionNormalize(vBlendedBaseRot);

				_vector vDeltaPos = XMVectorLerp(vPreDeltaPos, vCurDeltaPos, fBlendRatio);
				_vector vDeltaRot = XMQuaternionSlerp(vPreDeltaRot, vCurDeltaRot, fBlendRatio);
				vDeltaRot = XMQuaternionNormalize(vDeltaRot);

				vFinalPos = vBlendedBasePos + vDeltaPos;
				vFinalRot = XMQuaternionMultiply(vDeltaRot, vBlendedBaseRot);
			}
			else
			{
				_vector vCurPose = pCurBone->Get_Local(STATE::POSITION);
				vFinalPos = vCurPose + vCurDeltaPos;
				
				_vector vCurRot = XMQuaternionRotationMatrix(XMLoadFloat4x4(&pCurBone->GetLocalMatrix()));
				vFinalRot = XMQuaternionMultiply(vCurDeltaRot, vCurRot);
			}

			pCurBone->Set_Position_NoDirty(vFinalPos);
			pCurBone->Set_Rotation_NoDirty(vFinalRot);
		}
	}
}

void CAnimationController::Reset_Additive()
{
	/* Reset (Cur) Additive */
	for (auto& layer : m_vecAdditiveLayers)
		Safe_Delete(layer.pState);
	m_vecAdditiveLayers.clear();
}

void CAnimationController::Reset_PreAdditive()
{
	for (auto& layer : m_vecPreAdditiveLayers)
		Safe_Delete(layer.pState);
	m_vecPreAdditiveLayers.clear();
}

void CAnimationController::Build_Additive()
{
	auto pAsset = m_pAsset_Animation->Get();
	if (pAsset == nullptr)
		return;

	/* Additive */
	Reset_Additive();

	if (m_pCurAnimation) {

	for (auto& info : m_pCurAnimation->vecAdditiveInfos)
	{
		if (info.strAdditiveAnimName.empty() || info.umBoneMask.empty())
			continue;

		auto pAdditiveAnimInfo = pAsset->Get_AnimationInfo(info.strAdditiveAnimName);
		if (!pAdditiveAnimInfo)
			continue;

		/* Create new Layer */
		ADDITIVE_LAYER tLayer = {};
		tLayer.bAdditive = info.bAdditive;
		tLayer.fWeight = info.fWeight;
		tLayer.pAnimation = pAdditiveAnimInfo;
		tLayer.pState = new ANIMATION_STATE();
		tLayer.vecBoneAdditive.resize(m_vecBoneNodes.size(), false);
		for (auto& Pair : info.umBoneMask)
		{
			if (info.umBoneMask.empty())
				continue;

			const _string& strBone = Pair.first;
			_bool bEnalbe = Pair.second;
			
			_int iBoneIndex = Find_BoneIndexByPath(strBone);
			if (-1 != iBoneIndex)
				tLayer.vecBoneAdditive[iBoneIndex] = bEnalbe;
		}

		Change_Animation(pAdditiveAnimInfo, tLayer.pState);

		m_vecAdditiveLayers.push_back(tLayer);
	}
	}

}

void CAnimationController::Set_BoneMaskInHierarchy(const _string& _strBoneFullPath, _bool _bEnable, _int _iLayerIndex)
{
	_int iBoneIndex = Find_BoneIndexByPath(_strBoneFullPath);
	if (-1 == iBoneIndex)
		return;

	Recursive_BoneMask(iBoneIndex, _bEnable, _iLayerIndex);
}

_bool CAnimationController::Is_BoneEnabled(const _string& _strBoneFullPath, _int _iLayerIndex)
{
	_int iBoneIndex = Find_BoneIndexByPath(_strBoneFullPath);
	if (-1 == iBoneIndex)
		return true;

	return m_vecAdditiveLayers[_iLayerIndex].vecBoneAdditive[iBoneIndex];
}

_int CAnimationController::Find_BoneIndexByPath(const _string& _strBoneFullPath) const
{
	for (_int i = 0; i < m_vecBoneNodes.size(); i++)
	{
		if (m_vecBoneNodes[i].strFullPath == _strBoneFullPath)
			return i;
	}
	return -1;
}

void CAnimationController::Recursive_BoneMask(_int _iBoneIndex, _bool _bEnable, _int _iLayerIndex)
{
	if (_iLayerIndex < 0 || _iLayerIndex >= m_vecAdditiveLayers.size())
		return;

	auto& layer = m_vecAdditiveLayers[_iLayerIndex];
	if (_iBoneIndex < 0 || _iBoneIndex >= layer.vecBoneAdditive.size())
		return;

	layer.vecBoneAdditive[_iBoneIndex] = _bEnable;
	
	/* Recursive */
	for (auto Child : m_vecBoneNodes[_iBoneIndex].vecChildren)
		Recursive_BoneMask(Child, _bEnable, _iLayerIndex);
}

void CAnimationController::Build_BoneHierarchy(_int _iLayerMask)
{
	m_vecBoneNodes.clear();
	m_vecChannelToBoneIndex.clear();

	if (!m_pCurAnimation || !m_pCurAnimState->pAnimation)
		return;

	auto& vecChannels = m_pCurAnimState->pAnimation->vecChannels;

	unordered_map<_string, _int> umBoneIndex;

	/* 1. Bone Tree */
	for (_uint i = 0; i < vecChannels.size(); i++)
	{
		const _string& strFullPath = vecChannels[i].strBoneRoute;
		if (strFullPath.empty())
			continue;

		vector<_string> vecParts;
		size_t iStart = { 0 }, iEnd = { 0 };

		while ((iEnd = strFullPath.find('/', iStart)) != _string::npos)
		{
			vecParts.push_back(strFullPath.substr(iStart, iEnd - iStart));
			iStart = iEnd + 1;
		}
		vecParts.push_back(strFullPath.substr(iStart));

		_int iParentIndex = -1;
		_string strCurrentPath;
		for (auto& Part : vecParts)
		{
			if (!strCurrentPath.empty())
				strCurrentPath += "/";

			strCurrentPath += Part;

			auto iter = umBoneIndex.find(strCurrentPath);

			_int iCurrentIndex = { -1 };
			if (iter == umBoneIndex.end())
			{
				BONE_NODE node;
				node.strBoneName = Part;
				node.strFullPath = strCurrentPath;
				node.iParent = iParentIndex;
				m_vecBoneNodes.push_back(node);

				iCurrentIndex = (_int)m_vecBoneNodes.size() - 1;
				umBoneIndex[strCurrentPath] = iCurrentIndex;

				if (iParentIndex != -1)
					m_vecBoneNodes[iParentIndex].vecChildren.push_back(iCurrentIndex);

			}
			else
			{
				iCurrentIndex = iter->second;
			}

			iParentIndex = iCurrentIndex;
		}
	}

	/* 2. Mask */
	for (auto& Layer : m_vecAdditiveLayers)
	{
		Layer.vecBoneAdditive.resize(m_vecBoneNodes.size(), false);
	}

	/* 3. If exist Saved Mask Data */
	for (_uint i = 0; i < m_pCurAnimation->vecAdditiveInfos.size(); i++)
	{
		auto& layer = m_pCurAnimation->vecAdditiveInfos[i];

		for (auto& Pair : layer.umBoneMask)
		{
			auto iter = umBoneIndex.find(Pair.first);
			if (iter != umBoneIndex.end())
				m_vecAdditiveLayers[i].vecBoneAdditive[iter->second] = Pair.second;
		}
		
	}

	/* 4. Channel -> BoneIndex 캐시 */
	m_vecChannelToBoneIndex.resize(vecChannels.size(), -1);
	for (_uint i = 0; i < vecChannels.size(); i++)
	{
		const _string& path = vecChannels[i].strBoneRoute;

		auto iter = umBoneIndex.find(path);
		if (iter != umBoneIndex.end())
			m_vecChannelToBoneIndex[i] = iter->second;
	}
}

void CAnimationController::Render_BoneNode(const vector<BONE_NODE>& _vecNodes, _int _iIndex, _int _iLayerIndex)
{
	const BONE_NODE& tNode = _vecNodes[_iIndex];

	_bool bEnable = m_vecAdditiveLayers[_iLayerIndex].vecBoneAdditive[_iIndex];

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
	if (tNode.vecChildren.empty())
		flags |= ImGuiTreeNodeFlags_Leaf;

	_bool bOpen = ImGui::TreeNodeEx(tNode.strBoneName.c_str(), flags);
	
	ImGui::SameLine();

	if (ImGui::Checkbox(("##" + tNode.strBoneName).c_str(), &bEnable))
	{
		Set_BoneMaskInHierarchy(tNode.strFullPath, bEnable, _iLayerIndex);
	}

	if (bOpen)
	{
		for (auto Child : tNode.vecChildren)
		{
			/* Recursive */
			Render_BoneNode(_vecNodes, Child, _iLayerIndex);
		}
		ImGui::TreePop();
	}
}

void CAnimationController::Apply_RootMotionLimit(ANIMATION_STATE* _pState)
{
	if (!_pState)
		return;

	_pState->bRootMotionMaxDistanceEnable = m_bRootMotionMaxDistanceEnable;
	_pState->fRootMotionMaxDistance = m_fRootMotionMaxDistance;
}

void CAnimationController::Play_Animation(_float _fTimeDelta)
{
	if (m_bDebugRootMotionMuteAll)
	{
		if (m_pCurAnimState)
			m_pCurAnimState->bSwitchRootMotion = false;

		if (m_pPreAnimState)
			m_pPreAnimState->bSwitchRootMotion = false;
	}

	/* Channel Update */
	Loop_Animation(_fTimeDelta, m_pCurAnimState);

	if (m_pPreAnimState)
		Loop_Animation(_fTimeDelta, m_pPreAnimState);

	if (m_pPreAnimState && m_fBlendDuration > 0.f && m_fBlendElapsed > 0.f)
	{		
		m_pPreAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pPreAnimState, m_pPreAnimState);
		m_pCurAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState);

		_float fRatio = 1.f - (m_fBlendElapsed / m_fBlendDuration);
		fRatio = clamp(fRatio, 0.f, 1.f);

		
		m_pCurAnimState->pAnimation->Apply_Blend_State(m_pOwner, m_pRootMotion_Target->Get(), m_pPreAnimState, m_pCurAnimState, (1.f - fRatio), fRatio);
	}
	else
	{
		/* Apply current animation directly */
		m_pCurAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState);
		m_pCurAnimState->pAnimation->Apply_Blend_State(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState, 1.f, 0.f);
	}

	m_fBlendElapsed = max(m_fBlendElapsed - _fTimeDelta, 0.f);
}

void CAnimationController::Play_AnimationByEditor(_float _fTimeDelta)
{
	if (!m_pCurAnimState || !m_pCurAnimState->pAnimation)
		return;

	if (m_pPreAnimState && m_fBlendDuration > 0.f && m_fBlendElapsed > 0.f)
	{
		m_pPreAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pPreAnimState, m_pPreAnimState);
		m_pCurAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState);

		_float fRatio = 1.f - (m_fBlendElapsed / m_fBlendDuration);
		fRatio = clamp(fRatio, 0.f, 1.f);


		m_pCurAnimState->pAnimation->Apply_Blend_State(m_pOwner, m_pRootMotion_Target->Get(), m_pPreAnimState, m_pCurAnimState, (1.f - fRatio), fRatio);
	}
	else
	{
		/* Apply current animation directly */
		m_pCurAnimState->pAnimation->Update_Channel(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState);
		m_pCurAnimState->pAnimation->Apply_Blend_State(m_pOwner, m_pRootMotion_Target->Get(), m_pCurAnimState, m_pCurAnimState, 1.f, 0.f);
	}
}

void CAnimationController::NonBlend_Animation(const _string& _strName) {
	Set_DeltaMultiplier(1.f);

	auto pAsset = m_pAsset_Animation->Get();
	if (!pAsset)
		return;

	auto pAnim = pAsset->Get_AnimationInfo(_strName);
	if (!pAnim)
		return;

	if (!m_pCurAnimState->pAnimation) {
		m_pCurAnimation = pAnim;
		Change_Animation(pAnim, m_pCurAnimState);
		Build_Additive();
		return;
	}

	if (m_pPreAnimState)
		Safe_Delete(m_pPreAnimState);

	m_pPreAnimState = new ANIMATION_STATE(*m_pCurAnimState);

	vector<KEY_FRAME> vecPrePose = m_pPreAnimState->vecPose;

	Reset_PreAdditive();
	for (auto& layer : m_vecAdditiveLayers) {
		ADDITIVE_LAYER tDeepCopyAdditive = {};
		tDeepCopyAdditive.bAdditive = layer.bAdditive;
		tDeepCopyAdditive.fWeight = layer.fWeight;
		tDeepCopyAdditive.pAnimation = layer.pAnimation;
		tDeepCopyAdditive.vecBoneAdditive = layer.vecBoneAdditive;

		if (layer.pState)
			tDeepCopyAdditive.pState = new ANIMATION_STATE(*layer.pState);

		m_vecPreAdditiveLayers.push_back(tDeepCopyAdditive);
	}

	m_pCurAnimation = pAnim;

	_float3 vPreRootX = m_pCurAnimState->vPreRootXPos;
	_float3 vPreRootZ = m_pCurAnimState->vPreRootZPos;

	Change_Animation(pAnim, m_pCurAnimState);

	if (!vecPrePose.empty())
		m_pCurAnimState->vecPose = vecPrePose;

	m_pCurAnimState->vPreRootXPos = vPreRootX;
	m_pCurAnimState->vPreRootZPos = vPreRootZ;

	Build_Additive();

	m_bBlending = false;
	m_fBlendElapsed = 0.f;
	m_fBlendDuration = 0.f;

	return;
}

void CAnimationController::Blend_Animation(const _string& _strName)
{
	Set_DeltaMultiplier(1.f);

	auto pAsset = m_pAsset_Animation->Get();
	if (!pAsset)
		return;

	auto pAnim = pAsset->Get_AnimationInfo(_strName);
	if (!pAnim)
		return;

	if (!m_pCurAnimState->pAnimation)
	{
		m_pCurAnimation = pAnim;
		Change_Animation(pAnim, m_pCurAnimState);
		Build_Additive();
		return;
	}

	if (m_pPreAnimState)
		Safe_Delete(m_pPreAnimState);

	m_pPreAnimState = new ANIMATION_STATE(*m_pCurAnimState);

	vector<KEY_FRAME> vecPrePose = m_pPreAnimState->vecPose;

	Reset_PreAdditive();
	for (auto& layer : m_vecAdditiveLayers)
	{
		ADDITIVE_LAYER tDeepCopyAdditive = {};
		tDeepCopyAdditive.bAdditive = layer.bAdditive;
		tDeepCopyAdditive.fWeight = layer.fWeight;
		tDeepCopyAdditive.pAnimation = layer.pAnimation;
		tDeepCopyAdditive.vecBoneAdditive = layer.vecBoneAdditive;

		if (layer.pState)
			tDeepCopyAdditive.pState = new ANIMATION_STATE(*layer.pState);

		m_vecPreAdditiveLayers.push_back(tDeepCopyAdditive);
	}

	m_pCurAnimation = pAnim;

	_float3 vPreRootX = m_pCurAnimState->vPreRootXPos;
	_float3 vPreRootZ = m_pCurAnimState->vPreRootZPos;

	Change_Animation(pAnim, m_pCurAnimState);
	
	if (!vecPrePose.empty())
		m_pCurAnimState->vecPose = vecPrePose;

	m_pCurAnimState->vPreRootXPos = vPreRootX;
	m_pCurAnimState->vPreRootZPos = vPreRootZ;
	
	Build_Additive();

	_float fBlendTime = (pAnim->bBlending) ? pAnim->fBlendTime : 0.f;
	if (m_bBlending && fBlendTime > 0.f)
	{
		m_fBlendElapsed = fBlendTime;
		m_fBlendDuration = fBlendTime;
	}

	return;
}

void CAnimationController::Blend_Animation(const _string& _strName, _float _fBlendTime) {
	Set_DeltaMultiplier(1.f);

	auto pAsset = m_pAsset_Animation->Get();
	if (!pAsset)
		return;

	auto pAnim = pAsset->Get_AnimationInfo(_strName);
	if (!pAnim)
		return;

	if (!m_pCurAnimState->pAnimation) {
		m_pCurAnimation = pAnim;
		Change_Animation(pAnim, m_pCurAnimState);
		Build_Additive();
		return;
	}

	if (m_pPreAnimState)
		Safe_Delete(m_pPreAnimState);

	m_pPreAnimState = new ANIMATION_STATE(*m_pCurAnimState);

	vector<KEY_FRAME> vecPrePose = m_pPreAnimState->vecPose;

	Reset_PreAdditive();
	for (auto& layer : m_vecAdditiveLayers) {
		ADDITIVE_LAYER tDeepCopyAdditive = {};
		tDeepCopyAdditive.bAdditive = layer.bAdditive;
		tDeepCopyAdditive.fWeight = layer.fWeight;
		tDeepCopyAdditive.pAnimation = layer.pAnimation;
		tDeepCopyAdditive.vecBoneAdditive = layer.vecBoneAdditive;

		if (layer.pState)
			tDeepCopyAdditive.pState = new ANIMATION_STATE(*layer.pState);

		m_vecPreAdditiveLayers.push_back(tDeepCopyAdditive);
	}

	m_pCurAnimation = pAnim;

	_float3 vPreRootX = m_pCurAnimState->vPreRootXPos;
	_float3 vPreRootZ = m_pCurAnimState->vPreRootZPos;

	Change_Animation(pAnim, m_pCurAnimState);

	if (!vecPrePose.empty())
		m_pCurAnimState->vecPose = vecPrePose;

	m_pCurAnimState->vPreRootXPos = vPreRootX;
	m_pCurAnimState->vPreRootZPos = vPreRootZ;

	Build_Additive();

	if (_fBlendTime > 0.f) {
		m_bBlending = true;

		m_fBlendElapsed = _fBlendTime;
		m_fBlendDuration = _fBlendTime;
	}

	return;
}

void CAnimationController::OnGui_Inspector_Context() {
	ImGui::SeparatorText("Animation");
	if (!m_pAsset_Animation)
		return;

	if (m_pAsset_Animation->OnGui_Inspector_Context("Animation")) {
		/* (+) Change Asset */
	}

	if (m_pCurAnimState)
	{
		if (ImGui::Button("Save Asset"))
		{
			Clean_Events();
			m_pAsset_Animation->Get()->Save_File();
		}
		ImGui::SameLine();
		if (m_pCurAnimation && ImGui::Button("Copy Animation"))
		{
			auto pAsset = Get_Asset_Animation();
			if (pAsset)
			{
				auto pCopiedAnimation = pAsset->Copy_Animation(m_pCurAnimation);
				if (pCopiedAnimation)
				{
					Set_EditorMode(true);
					Change_Animation_Editor(pCopiedAnimation);
					m_bBlending = m_pCurAnimation->bBlending;
					m_fBlendDuration = m_pCurAnimation->fBlendTime;
				}
			}
		}

		m_pCCCT->OnGui_Inspector_Context();
		ImGui::SameLine();
		ImGui::TextUnformatted("CCCT");

		auto pAsset = m_pAsset_Animation->Get();
		if (!pAsset)
			return;

		ImGui::SeparatorText("Animation Settings");
		ImGui::RadioButton("Set Animation", (_int*)&m_ePlayMode, (_int)ANIMATION_SET_MODE::ANIMATION);
		ImGui::SameLine();
		ImGui::RadioButton("Set Blend", (_int*)&m_ePlayMode, (_int)ANIMATION_SET_MODE::BLEND);
		ImGui::SameLine();
		ImGui::RadioButton("Set Additive", (_int*)&m_ePlayMode, (_int)ANIMATION_SET_MODE::ADDITIVE);

		switch (m_ePlayMode)
		{
		case CAnimationController::ANIMATION_SET_MODE::ANIMATION:
		{
			ImGui::SeparatorText("Loop");
			ImGui::Checkbox("Loop", &m_pCurAnimState->bLoop);

			static _char searchBuf[128] = "";
			ImGui::SeparatorText("Select Animation !");
			const _char* strCurPreview = (m_pCurAnimation) ? m_pCurAnimation->strName.c_str() : "Select Animation";
			if (ImGui::BeginCombo("##AnimationCur", strCurPreview)) {
				ImGui::InputText("Search", searchBuf, IM_ARRAYSIZE(searchBuf));
				if (ImGui::IsWindowAppearing())
					ImGui::SetKeyboardFocusHere();

				_string strInput = searchBuf;
				std::transform(strInput.begin(), strInput.end(), strInput.begin(), ::tolower);

				for (auto& Pair : pAsset->Get_AnimationSequencies()) {
					const _string& strAnimName = Pair.first;
					_string strLowerName = strAnimName;
					std::transform(strLowerName.begin(), strLowerName.end(), strLowerName.begin(), ::tolower);
					if (!strInput.empty() && strLowerName.find(strInput) == _string::npos)
						continue;

					_bool bSelected = (m_pCurAnimation == Pair.second);
					if (ImGui::Selectable(Pair.first.c_str(), bSelected)) {
						
						if (!m_pCurAnimation)
						{
							Set_EditorMode(true);
							Change_Animation_Editor(Pair.second);
							ImGui::EndCombo();
							return;
						}

						if (m_pCurAnimation == Pair.second)
							continue;

						Set_EditorMode(true);
						Change_Animation_Editor(Pair.second);

						m_bBlending = m_pCurAnimation->bBlending;
						m_fBlendDuration = m_pCurAnimation->fBlendTime;
					}

					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			
			ImGui::SeparatorText("Root Motion X / Z");
			/* Root Motion Target */
			if (m_pRootMotion_Target->OnGui_Inspector_Context("Target")) {
				if (auto pObject = m_pRootMotion_Target->Get()) {
					m_pCurAnimState->pRootBoneTarget = pObject;
				}
			}
			/* Root Motion X*/
			if (m_pRootMotion_X->OnGui_Inspector_Context("Root Motion X")) {
				if (auto pObject = m_pRootMotion_X->Get()) {
					 m_pCurAnimState->pRootBoneX = pObject->GetTransform();
				}
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("X", &m_bRootMotionEnable_X))
			{
				m_pCurAnimState->bRootMotionEnableX = m_bRootMotionEnable_X; // Optionally clamp root motion on X.
			}

			if (m_pCurAnimation != nullptr)
				ImGui::DragFloat("Root Motion X Multiplier", &m_pCurAnimation->fRootMotionMultiplierX); // Scale the root motion X offset.

			/* Root Motion Z*/
			if (m_pRootMotion_Z->OnGui_Inspector_Context("Root Motion Z")) {
				if (auto pObject = m_pRootMotion_Z->Get()) {
					m_pCurAnimState->pRootBoneZ = pObject->GetTransform();
				}
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("Z", &m_bRootMotionEnable_Z))
			{
				m_pCurAnimState->bRootMotionEnableZ = m_bRootMotionEnable_Z;
			}

			if (m_pCurAnimation != nullptr)
				ImGui::DragFloat("Root Motion Z Multiplier", &m_pCurAnimation->fRootMotionMultiplierZ);
			
			ImGui::SeparatorText("Mute");
			/* Mute */
			if (m_pCurAnimation)
			{
				if (ImGui::DragFloat("Mute Weight", &m_pCurAnimation->fRootMotionMuteWeight, 0.01f, 0.f, 1.f)) {
					Set_RootMotionMute(m_pCurAnimation->fRootMotionMuteWeight);
				}
			}
			ImGui::SameLine();
			if (ImGui::Checkbox("[Debug] Mute", &m_bDebugRootMotionMuteAll))
			{
				Set_ApplyRootMotion(!m_bDebugRootMotionMuteAll);
			}

			ImGui::SeparatorText("SubRootMotion_Y");
			/* Y */
			if (ImGui::Checkbox("SubRootMotion_Y", &m_bEnableSubRootMotion_Y))
			{
				m_pCurAnimState->bEnableSubRootMotion_Y = m_bEnableSubRootMotion_Y;
			}
		}
			break;
		case CAnimationController::ANIMATION_SET_MODE::BLEND:
		{
			ImGui::SeparatorText("Blend Settings");

			/* (Default) CurAnimation */
			const _char* strCurPreview = (m_pCurAnimation) ? m_pCurAnimation->strName.c_str() : "Select Animation";
			if (ImGui::BeginCombo("##AnimationCur", strCurPreview)) {
				for (auto& Pair : pAsset->Get_AnimationSequencies()) {
					_bool bSelected = (m_pCurAnimation == Pair.second);
					if (ImGui::Selectable(Pair.first.c_str(), bSelected)) {
						if (m_bEditorMode)
						{
							Change_Animation_Editor(Pair.second);
						}
						else
						{
							m_pCurAnimation = Pair.second;
							Blend_Animation(Pair.first);
							Build_Additive();
						}

						m_bBlending = m_pCurAnimation->bBlending;
						m_fBlendDuration = m_pCurAnimation->fBlendTime;
					}

					if (bSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			_bool bChanged = { false };
			bChanged |= ImGui::Checkbox("Blending", &m_bBlending);
			bChanged |= ImGui::DragFloat("Blending Duration", &m_fBlendDuration, 0.01f, 0.f, 10.f);
			if (bChanged && m_pCurAnimation)
			{
				m_pCurAnimation->bBlending = m_bBlending;
				m_pCurAnimation->fBlendTime = m_fBlendDuration;
			}
		}
		break;
		case CAnimationController::ANIMATION_SET_MODE::ADDITIVE:
		{
			ImGui::SeparatorText("Click (+) Additive Animation !");
			if (ImGui::Button("         +          "))
			{
				ADDITIVE_LAYER tLayer = {};
				tLayer.vecBoneAdditive.resize(m_vecBoneNodes.size(), false);
				m_vecAdditiveLayers.push_back(tLayer);

				ADDITIVE_LAYER_INFO tInfo = {};
				m_pCurAnimation->vecAdditiveInfos.push_back(tInfo);
			}

			if (ImGui::Button("Temp Send Addtivie Data Button....."))
			{
				/* erase unknown*/
				m_vecAdditiveLayers.erase(
					std::remove_if(m_vecAdditiveLayers.begin(), m_vecAdditiveLayers.end(), []
					(ADDITIVE_LAYER& layer) {
							if (!layer.pAnimation)
							{
								Safe_Delete(layer.pState);
								return true;
							}
							return false;
						}), m_vecAdditiveLayers.end());

				m_pCurAnimation->vecAdditiveInfos.resize(m_vecAdditiveLayers.size());
				for (_uint i = 0; i < m_vecAdditiveLayers.size(); i++)
				{
					auto& layer = m_vecAdditiveLayers[i];
					auto& info = m_pCurAnimation->vecAdditiveInfos[i];

					info.strAdditiveAnimName = layer.pAnimation->strName;
					info.bAdditive = layer.bAdditive;
					info.fWeight = layer.fWeight;
					info.umBoneMask.clear();
					for (_uint j = 0; j < m_vecAdditiveLayers[i].vecBoneAdditive.size(); j++)
					{
						info.umBoneMask[m_vecBoneNodes[j].strFullPath] = layer.vecBoneAdditive[j];
					}
				}
			}

			for (_uint i = 0; i < m_vecAdditiveLayers.size(); i++)
			{
				ImGui::PushID(i);
				auto& tLayer = m_vecAdditiveLayers[i];

				if (ImGui::Button("Remove Layer"))
				{
					Safe_Delete(tLayer.pState);
					m_vecAdditiveLayers.erase(m_vecAdditiveLayers.begin() + i);
					ImGui::PopID();
					break;
				}

				_bool bChanged = { false };
				bChanged |= ImGui::Checkbox("Additive", &tLayer.bAdditive);
				bChanged |= ImGui::DragFloat("Blending Weight", &tLayer.fWeight, 0.01f, 0.f, 1.f);
				if (bChanged)
				{
					m_vecAdditiveLayers[i].bAdditive = tLayer.bAdditive;
					m_vecAdditiveLayers[i].fWeight = tLayer.fWeight;
				}

				static _char searchBuf[128] = "";
				const _char* strAdditivePreview = (tLayer.pAnimation) ? tLayer.pAnimation->strName.c_str() : "Select Animation";
				if (ImGui::BeginCombo("##AnimationAdditive", strAdditivePreview)) {
					ImGui::InputText("Search", searchBuf, IM_ARRAYSIZE(searchBuf));
					if (ImGui::IsWindowAppearing())
						ImGui::SetKeyboardFocusHere();

					_string strInput = searchBuf;
					std::transform(strInput.begin(), strInput.end(), strInput.begin(), ::tolower);

					for (auto& Pair : pAsset->Get_AnimationSequencies()) {
						const _string& strAnimName = Pair.first;
						_string strLowerName = strAnimName;
						std::transform(strLowerName.begin(), strLowerName.end(), strLowerName.begin(), ::tolower);
						if (!strInput.empty() && strLowerName.find(strInput) == _string::npos)
							continue;

						_bool bSelected = (tLayer.pAnimation == Pair.second);
						if (ImGui::Selectable(Pair.first.c_str(), bSelected)) {
							tLayer.pAnimation = Pair.second;

							if (!tLayer.pState)
								tLayer.pState = new ANIMATION_STATE();

							Change_Animation(tLayer.pAnimation, tLayer.pState);
						}

						if (bSelected)
							ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::SeparatorText("Bone Mask Quick Tools");
				auto ApplyBoneMaskByKeywords = [&](const vector<_string>& _vecKeywords, _bool _bEnalbe) {
					for (_uint j = 0; j < tLayer.vecBoneAdditive.size() && j < m_vecBoneNodes.size(); j++)
					{
						const auto& strName = m_vecBoneNodes[j].strBoneName;
						for (const auto& strKeyword : _vecKeywords)
						{
							if (strKeyword.empty())
								continue;
							if (strName.find(strKeyword) != _string::npos)
							{
								tLayer.vecBoneAdditive[j] = _bEnalbe;
								break;
							}
						}
					}
					};

				auto DrawBoneMaskPreset = [&](const _char* _szLabel, const vector<_string>& _vecKeywords) {
					ImGui::TextUnformatted(_szLabel); ImGui::SameLine(140.f);
					
					_string strEnableLabel = "Enable##";
					strEnableLabel += _szLabel;

					_string strDisableLabel = "Disable##";
					strDisableLabel += _szLabel;

					if (ImGui::Button(strEnableLabel.c_str()))
						ApplyBoneMaskByKeywords(_vecKeywords, true);
					ImGui::SameLine();
					if (ImGui::Button(strDisableLabel.c_str()))
						ApplyBoneMaskByKeywords(_vecKeywords, false);
					};

				DrawBoneMaskPreset("Hair", { "Hair", "Hiar" });
				DrawBoneMaskPreset("Skirt", { "skirt", "skrit", "Skirt", "Skrit" });
				DrawBoneMaskPreset("Piao", { "Piao" });

				ImGui::Separator();
				static _char szBoneMaskKeyword[128] = "";
				ImGui::InputText("Bone Keyword", szBoneMaskKeyword, IM_ARRAYSIZE(szBoneMaskKeyword));
				_string strKeywoed = szBoneMaskKeyword;
				if (ImGui::Button("Enable Keyword"))
				{
					if (!strKeywoed.empty())
						ApplyBoneMaskByKeywords({ strKeywoed }, true);
				}
				ImGui::SameLine();
				if (ImGui::Button("Disable Keyword"))
				{
					if (!strKeywoed.empty())
						ApplyBoneMaskByKeywords({ strKeywoed }, false);
				}

				ImGui::SeparatorText("Bone Mask");

				if (ImGui::TreeNode("Mask"))
				{
					for (_int j = 0; j < m_vecBoneNodes.size(); ++j)
					{
						if (m_vecBoneNodes[j].iParent == -1)
						{
							/* Recursive */
							Render_BoneNode(m_vecBoneNodes, j, i);
						}
					}

					ImGui::TreePop();
				}

				ImGui::Separator();
				ImGui::PopID();
			}
		}
		break;
		}
	}
}

void CAnimationController::Clean_Events()
{
	if (!m_pCurAnimation)
		return;

	auto& vecEvents = m_pCurAnimation->vecEvents;
	vecEvents.erase(
		remove_if(vecEvents.begin(), vecEvents.end(), [](const EVENT_KEY& event) {
			return event.pFunction == nullptr;
			}), vecEvents.end());
}

Json::Value CAnimationController::Serialize() {
	Json::Value jsonValue;

	m_pAsset_Animation->Serialize(jsonValue["Animation"]);
	jsonValue["m_pRootMotion_Target"] = m_pRootMotion_Target->Serialize();
	jsonValue["m_pRootMotion_X"] = m_pRootMotion_X->Serialize();
	jsonValue["m_pRootMotion_Z"] = m_pRootMotion_Z->Serialize();
	jsonValue["m_bRootMotion_X"] = m_bRootMotionEnable_X;
	jsonValue["m_bRootMotion_Z"] = m_bRootMotionEnable_Z;
	jsonValue["Loop"] = m_pCurAnimState->bLoop;
	if (m_pCurAnimation)
	{
		jsonValue["strName"] = m_pCurAnimation->strName;
	}

	if (m_pCCCT) {
		jsonValue["m_pCCCT"] = m_pCCCT->Serialize();
	}
	jsonValue["m_bEnableSubRootMotion_Y"] = m_bEnableSubRootMotion_Y;

	return jsonValue;
}

void CAnimationController::Deserialize(Json::Value& _jsonValue) {
	if (_jsonValue.isMember("Animation")) {
		m_pAsset_Animation->Deserialize(_jsonValue["Animation"]);
	}

	m_pRootMotion_Target->Deserialize(_jsonValue["m_pRootMotion_Target"]);
	m_pRootMotion_X->Deserialize(_jsonValue["m_pRootMotion_X"]);
	m_pRootMotion_Z->Deserialize(_jsonValue["m_pRootMotion_Z"]);
	Get_Json(_jsonValue, "m_bRootMotion_X", m_bRootMotionEnable_X);
	Get_Json(_jsonValue, "m_bRootMotion_Z", m_bRootMotionEnable_Z);
	Get_Json(_jsonValue, "Loop", m_pCurAnimState->bLoop);
	_string strName;
	Get_Json(_jsonValue, "strName", strName);

	m_pCCCT->Deserialize(_jsonValue["m_pCCCT"]);
	Get_Json(_jsonValue, "m_bEnableSubRootMotion_Y", m_bEnableSubRootMotion_Y);

	auto pAsset = m_pAsset_Animation->Get();
	if (pAsset == nullptr)
		return;		

	Change_AnimationByName(strName, 0);
}

void CAnimationController::Free() {
	__super::Free();

	Reset_Additive();
	Reset_PreAdditive();

	if (auto pASM = m_pOwner->GetComponent<CAnimationStateMachine>()) {
		pASM->Reset_AnimationController();
	}

	Safe_Delete(m_pPreAnimState);
	Safe_Delete(m_pCurAnimState);
	Safe_Release(m_pAsset_Animation);
	Safe_Release(m_pGameInstance);
	Safe_Release(m_pRootMotion_Target);
	Safe_Release(m_pRootMotion_X);
	Safe_Release(m_pRootMotion_Z);
	Safe_Release(m_pCCCT);
}
