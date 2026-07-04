#include "EnginePCH.h"
#include "CAnimationInfo.h"
#include "WFunction.h"
#include "CGameInstance.h"
#include <WShaderFunction.h>
#include <WClassFunction.h>
#include "CObject.h"
#include "CTransform.h"

namespace {
    constexpr _float ROOT_MOTION_FORWARD_EPSILON = 0.0001f;
    constexpr uint32_t MAX_ANIMATION_STRING_LENGTH = 4096;
    constexpr _uint MAX_ANIMATION_CHANNEL_COUNT = 4096;
    constexpr _uint MAX_ANIMATION_KEYFRAME_COUNT = 200000;
    constexpr _uint MAX_ADDITIVE_INFO_COUNT = 512;
    constexpr _uint MAX_BONE_MASK_COUNT = 8192;
    constexpr _uint MAX_EVENT_COUNT = 8192;
    constexpr _uint MAX_CUSTOM_ANIM_COUNT = 8192;
    constexpr _uint MAX_CUSTOM_KEYFRAME_COUNT = 200000;

    template<typename T>
    _bool Read_Value(FILE* _pFile, T& _tValue) {
        return Read<T>(_pFile, &_tValue, 1);
    }

    _bool Read_String_Limited(FILE* _pFile, _string& _strValue, uint32_t _iLength) {
        if (_iLength > MAX_ANIMATION_STRING_LENGTH)
            return false;

        return Read_String(_pFile, _strValue, _iLength);
    }

    _bool Read_KeyFrames(FILE* _pFile, vector<KEY_FRAME>& _vecKeyFrames, _uint _iCount) {
        if (_iCount > MAX_ANIMATION_KEYFRAME_COUNT)
            return false;

        _vecKeyFrames.resize(_iCount);
        if (_iCount == 0)
            return true;

        return fread(_vecKeyFrames.data(), sizeof(KEY_FRAME), _iCount, _pFile) == _iCount;
    }

    _bool Read_CustomKeyFrameValue(FILE* _pFile, FUNCTION_PROPERTY_TYPE _eType, CUSTOM_KEY_FRAME& _tKeyFrame) {
        switch (_eType)
        {
        case FUNCTION_PROPERTY_TYPE::MONOSTATE:
        case FUNCTION_PROPERTY_TYPE::TEXTURE2D:
            return true;
        case FUNCTION_PROPERTY_TYPE::INT:
            return Read_Value(_pFile, _tKeyFrame.iParam);
        case FUNCTION_PROPERTY_TYPE::FLOAT1:
            return Read_Value(_pFile, _tKeyFrame.fParam);
        case FUNCTION_PROPERTY_TYPE::FLOAT2:
            return Read_Value(_pFile, _tKeyFrame.v2Param);
        case FUNCTION_PROPERTY_TYPE::FLOAT3:
            return Read_Value(_pFile, _tKeyFrame.v3Param);
        case FUNCTION_PROPERTY_TYPE::FLOAT4:
            return Read_Value(_pFile, _tKeyFrame.v4Param);
        default:
            return false;
        }
    }

    template <typename TContext>
    TContext* Ensure_BindingContext(std::unordered_map<_uint, TContext>& _umContext, _uint _iIndex, CObject* _pRoot) {
        if (_pRoot == nullptr)
            return nullptr;

        auto iter = _umContext.find(_iIndex);
        if (iter == _umContext.end()) {
            auto tResult = _umContext.emplace(_iIndex, TContext(_pRoot));
            iter = tResult.first;
        }

        auto& tContext = iter->second;
        if (tContext.pRoot != _pRoot) {
            tContext.Reset();
            tContext.pRoot = _pRoot;
        }

        return &tContext;
    }
}

/* Deprecated */
void ANIMATION_INFO::Update_Channel(CObject* _pOwner, _float _fTime, CTransform* _pRootTransform, ANIMATION_STATE* _pState) {
    for (_uint i = 0; i < vecChannels.size(); i++) {
        vecChannels[i].Apply(&_pState->vecKeyFrameIndices[i], _fTime, _pState->vecBoneTransformCache[i]);
    }
}

void ANIMATION_INFO::Update_Channel(CObject* _pOwner, CObject* _pRootMotionTarget, ANIMATION_STATE* _pCurState, ANIMATION_STATE* _pNextState, _float _fRatio)
{
    /* Blending */
    for (_uint i = 0; i < vecChannels.size(); i++)
    {
        auto& curChannel = _pCurState->pAnimation->vecChannels[i];
        auto& nextChannel = _pNextState->pAnimation->vecChannels[i];

        CTransform* pBoneTransform = _pCurState->vecBoneTransformCache[i];
        if (!pBoneTransform)
            continue;

        /* KeyFrame */
        KEY_FRAME curFrame = curChannel.Get_CurrentKeyFrame(&_pCurState->vecKeyFrameIndices[i], _pCurState->fTime);
        KEY_FRAME nextFrame = nextChannel.Get_CurrentKeyFrame(&_pNextState->vecKeyFrameIndices[i], _pNextState->fTime);

        curChannel.Apply_Blend(curFrame, nextFrame, _fRatio, pBoneTransform);
    }
}

void ANIMATION_INFO::Update_Channel(CObject* _pOwner, CObject* _pRootMotionTarget, ANIMATION_STATE* _pCurState, ANIMATION_STATE* _pNextState)
{
    if (!_pCurState || !_pCurState->pAnimation)
        return;

    if (_pCurState->vecPose.size() != _pCurState->pAnimation->vecChannels.size())
        _pCurState->vecPose.resize(_pCurState->pAnimation->vecChannels.size());

    for (_uint i = 0; i < _pCurState->pAnimation->vecChannels.size(); i++)
    {
        auto& channel = _pCurState->pAnimation->vecChannels[i];
        KEY_FRAME curFrame = channel.Get_CurrentKeyFrame(&_pCurState->vecKeyFrameIndices[i], _pCurState->fTime);

        _pCurState->vecPose[i].vScale = curFrame.vScale;
        _pCurState->vecPose[i].vRotation = curFrame.vRotation;
        _pCurState->vecPose[i].vPosition = curFrame.vPosition;
    }
}

void ANIMATION_INFO::Apply_RootMotion(CObject* _pOwner, CObject* _pTarget, _float2 _vDelta, ANIMATION_STATE* _pCurState)
{
    if (!_pCurState->bSwitchRootMotion)
        return;

    _vDelta.x *= -1.f * _pCurState->pAnimation->fRootMotionMuteWeight;
    _vDelta.y *= -1.f * _pCurState->pAnimation->fRootMotionMuteWeight;
    const _bool bForwardRootMotion = _vDelta.y > ROOT_MOTION_FORWARD_EPSILON;
    CTransform* pTargetTransform = _pTarget ? _pTarget->GetTransform() : _pOwner->GetTransform();

    _vector vDelta = XMVectorSet(_vDelta.x, 0.f, _vDelta.y, 0.f);
    vDelta = XMVector3TransformNormal(vDelta, XMLoadFloat4x4(&pTargetTransform->GetWorldMatrix()));

    /* Monster 와의 MaxDistance */
    if (_pCurState->bRootMotionMaxDistanceEnable && bForwardRootMotion)
    {
        _float fLength = XMVectorGetX(XMVector3Length(vDelta));

        if (fLength > _pCurState->fRootMotionMaxDistance)
        {
            if (_pCurState->fRootMotionMaxDistance <= 0.f)
            {
                vDelta = XMVectorZero();
            }
            else if (fLength > 0.0001f)
            {
                vDelta = XMVector3Normalize(vDelta) * _pCurState->fRootMotionMaxDistance;
            }
        }
    }

    /* CCT */
    if (_pCurState->pCCCT) {
        _pCurState->pCCCT->Add_Move(vDelta);
    }
    else {
        pTargetTransform->Translate_Vector(vDelta);
    }
}

_float2 ANIMATION_INFO::Compute_RootMotionDelta_FromKeyFrame(ANIMATION_STATE* _pCurState, ANIMATION_STATE* _pNextState, _float _fRatio)
{
    _float fDeltaBipX = 0.f; _float fDeltaBipZ = 0.f;
    _float fDeltaRootZ = 0.f; _float fDeltaRootX = 0.f;

    auto pCurAnim = _pCurState->pAnimation;
    auto pNextAnim = _pNextState->pAnimation;

    const _int iRootX = pCurAnim->iRootBoneXCacheIndex;
    const _int iRootZ = pCurAnim->iRootBoneZCacheIndex;

    if (iRootX >= 0)
    {
        auto& curChannel = pCurAnim->vecChannels[iRootX];
        auto& nextChannel = pNextAnim->vecChannels[iRootX];

        KEY_FRAME tCurFrame = curChannel.Get_CurrentKeyFrame(&_pCurState->vecKeyFrameIndices[iRootX], _pCurState->fTime);
        KEY_FRAME tNextFrame = nextChannel.Get_CurrentKeyFrame(&_pNextState->vecKeyFrameIndices[iRootX], _pNextState->fTime);

        /* Bip001 X */
        if (_pNextState->bRootMotionEnableX)
        {
            _float fCurPos = tCurFrame.vPosition.x;
            _float fNextPos = tNextFrame.vPosition.x;

            if (_pCurState->bRestInit)
                _pCurState->vPreRootXPos.x = fCurPos;

            if (_pNextState->bRestInit)
                _pNextState->vPreRootXPos.x = fNextPos;

            _float fCurDelta = fCurPos - _pCurState->vPreRootXPos.x;
            _float fNextDelta = fNextPos - _pNextState->vPreRootXPos.x;

            fDeltaBipX = fCurDelta * (1.f - _fRatio) + fNextDelta * _fRatio;

            _pCurState->vPreRootXPos.x = fCurPos;
            _pNextState->vPreRootXPos.x = fNextPos;
        }

        /* Bip001 Z */
        if (_pNextState->bRootMotionEnableZ)
        {
            _float fCurPos_Y = tCurFrame.vPosition.y;
            _float fNextPos_Y = tNextFrame.vPosition.y;

            if (_pCurState->bRestInit)
                _pCurState->vPreRootXPos.y = fCurPos_Y;

            if (_pNextState->bRestInit)
                _pNextState->vPreRootXPos.y = fNextPos_Y;

            _float fCurDelta_Y = fCurPos_Y - _pCurState->vPreRootXPos.y;
            _float fNextDelta_Y = fNextPos_Y - _pNextState->vPreRootXPos.y;

            fDeltaBipZ = fCurDelta_Y * (1.f - _fRatio) + fNextDelta_Y * _fRatio;

            _pCurState->vPreRootXPos.y = fCurPos_Y;
            _pNextState->vPreRootXPos.y = fNextPos_Y;
        }
    }

    if (iRootZ >= 0)
    {
        _uint curEnd =  { std::max<_uint>(E2U(_pCurState->vecKeyFrameIndices.size()) - 1, 0) };
        _uint nextEnd = { std::max<_uint>(E2U(_pNextState->vecKeyFrameIndices.size()) - 1, 0) };

        _uint& currIdx = (curEnd < iRootZ)  ? curEnd  : _pCurState->vecKeyFrameIndices[iRootZ];
        _uint& nextIdx = (nextEnd < iRootZ) ? nextEnd : _pNextState->vecKeyFrameIndices[iRootZ];

        CHANNEL& curChanel = (E2U(pCurAnim->vecChannels.size()) > iRootZ) ?
            pCurAnim->vecChannels[iRootZ]   : pCurAnim->vecChannels.back();

        CHANNEL& nextChanel = (E2U(pNextAnim->vecChannels.size()) > iRootZ) ?
            pNextAnim->vecChannels[iRootZ]  : pNextAnim->vecChannels.back();

        KEY_FRAME tCurFrame = curChanel.Get_CurrentKeyFrame(&currIdx, _pCurState->fTime);
        KEY_FRAME tNextFrame = nextChanel.Get_CurrentKeyFrame(&nextIdx, _pNextState->fTime);

        /* Root Z */
        if (_pNextState->bRootMotionEnableZ)
        {
            _float curPos = tCurFrame.vPosition.y;
            _float nextPos = tNextFrame.vPosition.y;

            if (_pCurState->bRestInit)
                _pCurState->vPreRootZPos.y = curPos;

            if (_pNextState->bRestInit)
                _pNextState->vPreRootZPos.y = nextPos;

            _float curDelta = curPos - _pCurState->vPreRootZPos.y;
            _float nextDelta = nextPos - _pNextState->vPreRootZPos.y;

            fDeltaRootZ = curDelta * (1.f - _fRatio) + nextDelta * _fRatio;

            _pCurState->vPreRootZPos.y = curPos;
            _pNextState->vPreRootZPos.y = nextPos;
        }

        /* Root X */
        if (_pNextState->bRootMotionEnableX)
        {
            _float curPos_X = tCurFrame.vPosition.x;
            _float nextPos_X = tNextFrame.vPosition.x;

            if (_pCurState->bRestInit)
                _pCurState->vPreRootZPos.x = curPos_X;

            if (_pNextState->bRestInit)
                _pNextState->vPreRootZPos.x = nextPos_X;

            _float curDelta_X = curPos_X - _pCurState->vPreRootZPos.x;
            _float nextDelta_X = nextPos_X - _pNextState->vPreRootZPos.x;

            fDeltaRootX = curDelta_X * (1.f - _fRatio) + nextDelta_X * _fRatio;

            _pCurState->vPreRootZPos.x = curPos_X;
            _pNextState->vPreRootZPos.x = nextPos_X;
        }
    }

    _pCurState->bRestInit = false;
    _pNextState->bRestInit = false;

    return { (fDeltaBipX + fDeltaRootX) * _pCurState->pAnimation->fRootMotionMultiplierX, (fDeltaRootZ + fDeltaBipZ) * _pCurState->pAnimation->fRootMotionMultiplierZ };
}

void ANIMATION_INFO::Update_SyncWithTarget(CObject* _pOwner, CObject* _pTarget)
{
    if (!_pTarget || _pTarget == _pOwner)
        return;

    CTransform* pOwnerTransform = _pOwner->GetTransform();
    CTransform* pTargetTransform = _pTarget->GetTransform();

    pOwnerTransform->Set_Position(pTargetTransform->GetWorldPosition());
    _matrix matWorld = XMLoadFloat4x4(&pOwnerTransform->GetWorldMatrix());
    _vector qRotation = XMQuaternionRotationMatrix(matWorld);
    pTargetTransform->Set_Rotation_Quaternion(qRotation);
}

void ANIMATION_INFO::Reset_RootBone(ANIMATION_STATE* _pState)
{
    if (!_pState || !_pState->pAnimation)
        return;

    auto pAnim = _pState->pAnimation;

    const _int iRootX = pAnim->iRootBoneXCacheIndex;
    const _int iRootZ = pAnim->iRootBoneZCacheIndex;

    if (iRootX >= 0 && _pState->bRootMotionEnableX)
    {
        CTransform* pRootBoneX = _pState->vecBoneTransformCache[iRootX];
        if (pRootBoneX)
        {
            _float3 vPosition;
            XMStoreFloat3(&vPosition, pRootBoneX->Get_Local(STATE::POSITION));
            pRootBoneX->Set_Position_NoDirty(XMVectorSet(0.f, 0.f, vPosition.z, 1.f));
        }
    }

    if (iRootZ >= 0 && _pState->bRootMotionEnableZ)
    {
        CTransform* pRootBoneZ = _pState->vecBoneTransformCache[iRootZ];
        if (pRootBoneZ)
        {
            _float3 vPosition;
            XMStoreFloat3(&vPosition, pRootBoneZ->Get_Local(STATE::POSITION));
            pRootBoneZ->Set_Position_NoDirty(XMVectorSet(0.f, 0.f, _pState->bEnableSubRootMotion_Y ? vPosition.z : 0.f, 1.f));
        }
    }

    //for (_uint i = 0; i < pAnim->vecChannels.size(); ++i)
    //{
        //if (true) {
        //    if (channel.strBoneName == pAnim->strRootBoneXName) {
        //        _float3 vPosition;
        //        XMStoreFloat3(&vPosition, pBone->Get_Local(STATE::POSITION));
        //        pBone->Set_Position_NoDirty(XMVectorSet(0.f, 0.f, vPosition.z, 1.f));
        //    }
        //
        //    if (channel.strBoneName == pAnim->strRootBoneZName) {
        //        pBone->Set_Position_NoDirty(XMVectorSet(0.f, 0.f, 0.f, 1.f));
        //    }
        //}
        //pBone->Set_Position_NoDirty(XMVectorSet(0.f, 0.f, 0.f, 1.f));
    //}
}

void ANIMATION_INFO::Update_CustomAnim(ANIMATION_STATE* _pState)
{
    if (!_pState)
        return;

    if (auto pGameInst = CGameInstance::GetInstance()) {
        if (pGameInst->Get_GameMode() == GAMEMODE::EDITOR) {
            return;
        }
    }

    _float fTime = _pState->fTime;

    for (_uint i = 0; i < vecCustomAnims.size(); ++i)
    {
        auto& anim = vecCustomAnims[i];
        if (!anim.pFunction)
            continue;

        if (fTime < anim.fStartTime || fTime > anim.fEndTime)
            continue;

        _float fTimeInCustomAnim = fTime - anim.fStartTime;

        auto* pContext = _pState->Get_CustomAnimBindContext(i);
        if (pContext == nullptr)
            continue;

        if (FAILED(anim.pFunction->Try(pContext))) {
            continue;
        }

        ParamValue value = anim.Evaluate_CustomKeyFrame(fTimeInCustomAnim);

        anim.pFunction->Invoke(pContext, value);
    }
}

void ANIMATION_INFO::Apply_Blend_State(class CObject* _pOwner, class CObject* _pRootMotionTarget, ANIMATION_STATE* _pCurState, ANIMATION_STATE* _pNextState, _float _fCurWeight, _float _fNextWeight)
{
    if (!_pCurState || !_pNextState)
        return;

    //Debug_Output(_fCurWeight, "\n");

    _uint iPoseCount = _pCurState->vecBoneTransformCache.size();
    for (_uint i = 0; i < iPoseCount; ++i)
    {
        CTransform* pBoneTransform = _pCurState->vecBoneTransformCache[i];
        if (!pBoneTransform)
            continue;

        if (i >= _pCurState->vecPose.size())
            continue;

        if (i >= _pNextState->vecPose.size())
            continue;

        auto& curPose = _pCurState->vecPose[i];
        auto& nextPose = _pNextState->vecPose[i];

        /* Scale */
        _vector vCurScale = XMLoadFloat3(&curPose.vScale);
        _vector vNextScale = XMLoadFloat3(&nextPose.vScale);
        _vector vFinalScale = XMVectorLerp(vCurScale, vNextScale, _fNextWeight);

        /* Rotation */
        _vector vCurRot = XMLoadFloat4(&curPose.vRotation);
        _vector vNextRot = XMLoadFloat4(&nextPose.vRotation);
        _vector vFinalRot = XMQuaternionSlerp(vCurRot, vNextRot, _fNextWeight);
        vFinalRot = XMQuaternionNormalize(vFinalRot);

        /* Position */
        _vector vCurPos = XMLoadFloat3(&curPose.vPosition);
        _vector vNextPos = XMLoadFloat3(&nextPose.vPosition);
        _vector vFinalPos = XMVectorLerp(vCurPos, vNextPos, _fNextWeight);

        pBoneTransform->Set_Scale_NoDirty(vFinalScale);
        pBoneTransform->Set_Rotation_NoDirty(vFinalRot);
        pBoneTransform->Set_Position_NoDirty(vFinalPos);
    }

    /* Root Motion */
    _float2 vDelta = Compute_RootMotionDelta_FromKeyFrame(_pCurState, _pNextState, _fNextWeight);
    Apply_RootMotion(_pOwner, _pRootMotionTarget, vDelta, _pCurState);
    Update_SyncWithTarget(_pOwner, _pRootMotionTarget);
    Reset_RootBone(_pCurState);
}

void ANIMATION_INFO::Cache_RootBoneIndex()
{
    iRootBoneXCacheIndex = -1;
    iRootBoneZCacheIndex = -1;

    for (_int i = 0; i < vecChannels.size(); ++i)
    {
        const auto& channel = vecChannels[i];

        if (channel.strBoneName == strRootBoneXName)
            iRootBoneXCacheIndex = i;
        if (channel.strBoneName == strRootBoneZName)
            iRootBoneZCacheIndex = i;
    }
}

void ANIMATION_INFO::Free() {
    __super::Free();

    for (_uint i = 0; i < vecEvents.size(); i++)
    {
        Safe_Release(this->vecEvents[i].pFunction);
    }

    for (_uint i = 0; i < vecCustomAnims.size(); i++)
    {
        Safe_Release(this->vecCustomAnims[i].pFunction);
    }
}

void ANIMATION_INFO::Save(vector<_byte>& _vecData) {
    Cache_RootBoneIndex();

    Write_To(_vecData, &fDuration, sizeof(fDuration));
    Write_To(_vecData, &fTickPerSecond, sizeof(fTickPerSecond));

    uint32_t iNameLength = (uint32_t)strName.length();
    Write_To(_vecData, &iNameLength, sizeof(iNameLength));
    Write_To(_vecData, strName.data(), sizeof(char) * iNameLength);

    uint32_t iAmartureRouteLength = (uint32_t)strAmartureRoute.length();
    Write_To(_vecData, &iAmartureRouteLength, sizeof(iAmartureRouteLength));
    Write_To(_vecData, strAmartureRoute.data(), sizeof(char) * iAmartureRouteLength);

    iNumChannels = (_uint)vecChannels.size();
    Write_To(_vecData, &iNumChannels, sizeof(_uint));
    for (auto& Channel : vecChannels) {
        uint32_t iBoneNameLength = (uint32_t)Channel.strBoneName.length();
        Write_To(_vecData, &iBoneNameLength, sizeof(iBoneNameLength));
        Write_To(_vecData, Channel.strBoneName.data(), iBoneNameLength);

        uint32_t iBoneRouteLength = (uint32_t)Channel.strBoneRoute.length();
        Write_To(_vecData, &iBoneRouteLength, sizeof(iBoneRouteLength));
        Write_To(_vecData, Channel.strBoneRoute.data(), sizeof(char) * iBoneRouteLength);

        _uint iKeyFrameSize = (_uint)Channel.vecKeyFrames.size();
        Write_To(_vecData, &iKeyFrameSize, sizeof(_uint));
        for (auto& key : Channel.vecKeyFrames)
            Write_To(_vecData, &key, sizeof(KEY_FRAME));
    }

    Write_To(_vecData, &bBlending, sizeof(bBlending));
    Write_To(_vecData, &fBlendTime, sizeof(fBlendTime));

    uint32_t iRootXBoneLength = (uint32_t)strRootBoneXName.length();
    Write_To(_vecData, &iRootXBoneLength, sizeof(iRootXBoneLength));
    Write_To(_vecData, strRootBoneXName.data(), sizeof(char) * iRootXBoneLength);

    uint32_t iRootZBoneLength = (uint32_t)strRootBoneZName.length();
    Write_To(_vecData, &iRootZBoneLength, sizeof(iRootZBoneLength));
    Write_To(_vecData, strRootBoneZName.data(), sizeof(char) * iRootZBoneLength);

    _uint iAdditiveSize = (_uint)vecAdditiveInfos.size();
    Write_To(_vecData, &iAdditiveSize, sizeof(_uint));
    for (auto& info : vecAdditiveInfos)
    {
        uint32_t iAdditiveAnimNameLength = (uint32_t)info.strAdditiveAnimName.length();
        Write_To(_vecData, &iAdditiveAnimNameLength, sizeof(iAdditiveAnimNameLength));
        Write_To(_vecData, info.strAdditiveAnimName.data(), sizeof(char) * iAdditiveAnimNameLength);

        Write_To(_vecData, &info.bAdditive, sizeof(_bool));
        Write_To(_vecData, &info.fWeight, sizeof(_float));

        _uint iBoneMaskSize = (_uint)info.umBoneMask.size();
        Write_To(_vecData, &iBoneMaskSize, sizeof(_uint));
        for (auto& Pair : info.umBoneMask)
        {
            uint32_t iBoneMaskLength = (uint32_t)Pair.first.length();
            Write_To(_vecData, &iBoneMaskLength, sizeof(iBoneMaskLength));
            Write_To(_vecData, Pair.first.data(), sizeof(char) * iBoneMaskLength);

            Write_To(_vecData, &Pair.second, sizeof(_bool));
        }
    }

    _uint iEventSize = 0;
    for (auto& event : vecEvents)
    {
        if ((event.pFunction != nullptr) && (event.pFunction->Get_FunctionDesc() != nullptr))
            ++iEventSize;
    }
    Write_To(_vecData, &iEventSize, sizeof(_uint));
    for (auto& event : vecEvents)
    {
        if (!event.pFunction)
            continue;

        auto* pFuncDesc = event.pFunction->Get_FunctionDesc();
        if (!pFuncDesc)
            continue;

        event.Ensure_Meta();

        uint32_t iGUIDLength = (uint32_t)event.strGUID.length();
        Write_To(_vecData, &iGUIDLength, sizeof(iGUIDLength));
        Write_To(_vecData, event.strGUID.data(), sizeof(char) * iGUIDLength);
        Write_To(_vecData, &event.tModifiedTime, sizeof(EVENT_MODIFIED_TIME));

        Write_To(_vecData, &event.iEventLayerIndex, sizeof(_int));

        uint32_t iEventNameLength = (_uint)event.strEventName.length();
        Write_To(_vecData, &iEventNameLength, sizeof(iEventNameLength));
        Write_To(_vecData, event.strEventName.data(), sizeof(char) * iEventNameLength);

        event.pFunction->Save(_vecData);

        Write_To(_vecData, &event.tKF.fTrackPosition, sizeof(_float));

        auto tType = event.pFunction->Get_FunctionDesc()->tType;
        Write_To(_vecData, &tType, sizeof(FUNCTION_PROPERTY_TYPE));
        switch (tType)
        {
        case FUNCTION_PROPERTY_TYPE::INT:
            Write_To(_vecData, &event.tKF.iParam, sizeof(_int));
            break;
        case FUNCTION_PROPERTY_TYPE::FLOAT1:
            Write_To(_vecData, &event.tKF.fParam, sizeof(_float));
            break;
        case FUNCTION_PROPERTY_TYPE::FLOAT2:
            Write_To(_vecData, &event.tKF.v2Param, sizeof(_float2));
            break;
        case FUNCTION_PROPERTY_TYPE::FLOAT3:
            Write_To(_vecData, &event.tKF.v3Param, sizeof(_float3));
            break;
        case FUNCTION_PROPERTY_TYPE::FLOAT4:
            Write_To(_vecData, &event.tKF.v4Param, sizeof(_float4));
            break;
        }
    }

    _uint iCustomAnimSize = 0;
    for (auto& customAnim : vecCustomAnims)
    {
        if ((customAnim.pFunction != nullptr) && (customAnim.pFunction->Get_FunctionDesc() != nullptr))
            ++iCustomAnimSize;
    }
    Write_To(_vecData, &iCustomAnimSize, sizeof(_uint));
    for (auto& customAnim : vecCustomAnims)
    {
        if (!customAnim.pFunction)
            continue;

        auto* pFuncDesc = customAnim.pFunction->Get_FunctionDesc();
        if (!pFuncDesc)
            continue;

        Write_To(_vecData, &customAnim.iEventLayerIndex, sizeof(_int));

        uint32_t iCustomAnimNameLength = (_uint)customAnim.strCustomAnimName.length();
        Write_To(_vecData, &iCustomAnimNameLength, sizeof(iCustomAnimNameLength));
        Write_To(_vecData, customAnim.strCustomAnimName.data(), sizeof(char) * iCustomAnimNameLength);

        Write_To(_vecData, &customAnim.fStartTime, sizeof(_float));
        Write_To(_vecData, &customAnim.fEndTime, sizeof(_float));

        customAnim.pFunction->Save(_vecData);

        _uint iKFSize = (_uint)customAnim.vecKF.size();
        Write_To(_vecData, &iKFSize, sizeof(_uint));
        for (auto& tKF : customAnim.vecKF)
        {
            Write_To(_vecData, &tKF.fTrackPosition, sizeof(_float));

            auto tType = customAnim.pFunction->Get_FunctionDesc()->tType;
            switch (tType)
            {
            case FUNCTION_PROPERTY_TYPE::INT:
                Write_To(_vecData, &tKF.iParam, sizeof(_int));
                break;
            case FUNCTION_PROPERTY_TYPE::FLOAT1:
                Write_To(_vecData, &tKF.fParam, sizeof(_float));
                break;
            case FUNCTION_PROPERTY_TYPE::FLOAT2:
                Write_To(_vecData, &tKF.v2Param, sizeof(_float2));
                break;
            case FUNCTION_PROPERTY_TYPE::FLOAT3:
                Write_To(_vecData, &tKF.v3Param, sizeof(_float3));
                break;
            case FUNCTION_PROPERTY_TYPE::FLOAT4:
                Write_To(_vecData, &tKF.v4Param, sizeof(_float4));
                break;
            }
        }

        Write_To(_vecData, &customAnim.iCurKeyFrameIndex, sizeof(_uint));
    }

    Write_To(_vecData, &fRootMotionMultiplierX, sizeof(fRootMotionMultiplierX));
    Write_To(_vecData, &fRootMotionMultiplierZ, sizeof(fRootMotionMultiplierZ));

    Write_To(_vecData, &fRootMotionMuteWeight, sizeof(fRootMotionMuteWeight));

    Write_To(_vecData, &iRootBoneXCacheIndex, sizeof(iRootBoneXCacheIndex));
    Write_To(_vecData, &iRootBoneZCacheIndex, sizeof(iRootBoneZCacheIndex));
}

HRESULT ANIMATION_INFO::Load(_uint _iVersion, FILE* _pFile) {
    if (_pFile == nullptr)
        return E_FAIL;

    if (!Read_Value(_pFile, fDuration))
        return E_FAIL;
    if (!Read_Value(_pFile, fTickPerSecond))
        return E_FAIL;

    uint32_t iNameLength = 0;
    if (!Read_Value(_pFile, iNameLength) || !Read_String_Limited(_pFile, strName, iNameLength))
        return E_FAIL;

    uint32_t iAmartureLength = 0;
    if (!Read_Value(_pFile, iAmartureLength) || !Read_String_Limited(_pFile, strAmartureRoute, iAmartureLength))
        return E_FAIL;

    _uint iLoadedChannelCount = 0;
    if (!Read_Value(_pFile, iLoadedChannelCount) || iLoadedChannelCount > MAX_ANIMATION_CHANNEL_COUNT)
        return E_FAIL;

    iNumChannels = iLoadedChannelCount;
    vecChannels.resize(iNumChannels);

    for (_uint i = 0; i < iNumChannels; ++i) {
        auto& channel = vecChannels[i];

        uint32_t iBoneNameLength = 0;
        if (!Read_Value(_pFile, iBoneNameLength) || !Read_String_Limited(_pFile, channel.strBoneName, iBoneNameLength))
            return E_FAIL;

        uint32_t iBoneRouteLength = 0;
        if (!Read_Value(_pFile, iBoneRouteLength) || !Read_String_Limited(_pFile, channel.strBoneRoute, iBoneRouteLength))
            return E_FAIL;

        _uint iKeySize = 0;
        if (!Read_Value(_pFile, iKeySize) || !Read_KeyFrames(_pFile, channel.vecKeyFrames, iKeySize))
            return E_FAIL;
    }

    if (!Read_Value(_pFile, bBlending))
        return E_FAIL;
    if (!Read_Value(_pFile, fBlendTime))
        return E_FAIL;

    uint32_t iRootXBoneLength = 0;
    if (!Read_Value(_pFile, iRootXBoneLength) || !Read_String_Limited(_pFile, strRootBoneXName, iRootXBoneLength))
        return E_FAIL;

    uint32_t iRootZBoneLength = 0;
    if (!Read_Value(_pFile, iRootZBoneLength) || !Read_String_Limited(_pFile, strRootBoneZName, iRootZBoneLength))
        return E_FAIL;

    _uint iAdditiveSize = 0;
    if (!Read_Value(_pFile, iAdditiveSize) || iAdditiveSize > MAX_ADDITIVE_INFO_COUNT)
        return E_FAIL;

    vecAdditiveInfos.resize(iAdditiveSize);

    for (_uint i = 0; i < iAdditiveSize; ++i)
    {
        auto& info = vecAdditiveInfos[i];

        uint32_t iAdditiveAnimNameLength = 0;
        if (!Read_Value(_pFile, iAdditiveAnimNameLength) || !Read_String_Limited(_pFile, info.strAdditiveAnimName, iAdditiveAnimNameLength))
            return E_FAIL;

        if (!Read_Value(_pFile, info.bAdditive))
            return E_FAIL;
        if (!Read_Value(_pFile, info.fWeight))
            return E_FAIL;

        _uint iBoneMaskSize = 0;
        if (!Read_Value(_pFile, iBoneMaskSize) || iBoneMaskSize > MAX_BONE_MASK_COUNT)
            return E_FAIL;

        info.umBoneMask.clear();
        for (_uint j = 0; j < iBoneMaskSize; ++j)
        {
            _string strBoneMask;
            uint32_t iBoneMaskLength = 0;
            if (!Read_Value(_pFile, iBoneMaskLength) || !Read_String_Limited(_pFile, strBoneMask, iBoneMaskLength))
                return E_FAIL;

            _bool bEnable = true;
            if (!Read_Value(_pFile, bEnable))
                return E_FAIL;

            info.umBoneMask[strBoneMask] = bEnable;
        }
    }

    _uint iEventSize = 0;
    if (!Read_Value(_pFile, iEventSize) || iEventSize > MAX_EVENT_COUNT)
        return E_FAIL;

    vecEvents.resize(iEventSize);

    for (_uint i = 0; i < iEventSize; ++i)
    {
        auto& event = vecEvents[i];

        if (_iVersion >= 8)
        {
            uint32_t iGUIDLength = 0;
            if (!Read_Value(_pFile, iGUIDLength) || !Read_String_Limited(_pFile, event.strGUID, iGUIDLength))
                return E_FAIL;

            if (!Read_Value(_pFile, event.tModifiedTime))
                return E_FAIL;
        }
        event.Ensure_Meta();

        if (!Read_Value(_pFile, event.iEventLayerIndex))
            return E_FAIL;

        uint32_t iEventNameLength = 0;
        if (!Read_Value(_pFile, iEventNameLength) || !Read_String_Limited(_pFile, event.strEventName, iEventNameLength))
            return E_FAIL;

        event.pFunction = WFunction::Create(_pFile);
        if (event.pFunction == nullptr)
            return E_FAIL;

        if (!Read_Value(_pFile, event.tKF.fTrackPosition))
            return E_FAIL;

        FUNCTION_PROPERTY_TYPE eType = FUNCTION_PROPERTY_TYPE::MONOSTATE;
        if (!Read_Value(_pFile, eType) || !Read_CustomKeyFrameValue(_pFile, eType, event.tKF))
            return E_FAIL;
    }

    _uint iCustomAnimSize = 0;
    if (!Read_Value(_pFile, iCustomAnimSize) || iCustomAnimSize > MAX_CUSTOM_ANIM_COUNT)
        return E_FAIL;

    vecCustomAnims.resize(iCustomAnimSize);

    for (_uint i = 0; i < iCustomAnimSize; i++)
    {
        auto& customAnim = vecCustomAnims[i];

        if (!Read_Value(_pFile, customAnim.iEventLayerIndex))
            return E_FAIL;

        uint32_t iCustomNameLength = 0;
        if (!Read_Value(_pFile, iCustomNameLength) || !Read_String_Limited(_pFile, customAnim.strCustomAnimName, iCustomNameLength))
            return E_FAIL;

        if (!Read_Value(_pFile, customAnim.fStartTime))
            return E_FAIL;
        if (!Read_Value(_pFile, customAnim.fEndTime))
            return E_FAIL;

        customAnim.pFunction = WFunction::Create(_pFile);
        if (customAnim.pFunction == nullptr || customAnim.pFunction->Get_FunctionDesc() == nullptr)
            return E_FAIL;

        _uint iKFSize = 0;
        if (!Read_Value(_pFile, iKFSize) || iKFSize > MAX_CUSTOM_KEYFRAME_COUNT)
            return E_FAIL;

        customAnim.vecKF.resize(iKFSize);

        auto tType = customAnim.pFunction->Get_FunctionDesc()->tType;
        for (_uint j = 0; j < iKFSize; j++)
        {
            auto& tKF = customAnim.vecKF[j];
            if (!Read_Value(_pFile, tKF.fTrackPosition))
                return E_FAIL;

            if (!Read_CustomKeyFrameValue(_pFile, tType, tKF))
                return E_FAIL;
        }

        if (!Read_Value(_pFile, customAnim.iCurKeyFrameIndex))
            return E_FAIL;
    }

    if (_iVersion >= 5) {
        if (!Read_Value(_pFile, fRootMotionMultiplierX))
            return E_FAIL;
        if (!Read_Value(_pFile, fRootMotionMultiplierZ))
            return E_FAIL;
    }

    if (_iVersion >= 6) {
        if (!Read_Value(_pFile, fRootMotionMuteWeight))
            return E_FAIL;
    }

    if (_iVersion >= 7) {
        if (!Read_Value(_pFile, iRootBoneXCacheIndex))
            return E_FAIL;
        if (!Read_Value(_pFile, iRootBoneZCacheIndex))
            return E_FAIL;
    }

    Cache_RootBoneIndex();
    return S_OK;
}

void tagChannel::Build_KeyFrames(_float _fDuration, _float _fTickPerSecond) {
    vecKeyFrames.clear();

    _uint iNumKeyFrames = (_uint)max({ vecScalingKeys.size(), vecRotationKeys.size(), vecPositionKeys.size() });
    vecKeyFrames.reserve(iNumKeyFrames);

    _float3 vScale = { 1.f, 1.f, 1.f };
    _float4 vRotation = { 0.f, 0.f, 0.f, 1.f };
    _float3 vPosition = { 0.f, 0.f, 0.f };

    vector<_float> vecTimes;

    for (auto& key : vecScalingKeys)
        vecTimes.push_back(key.fTime);
    for (auto& key : vecRotationKeys)
        vecTimes.push_back(key.fTime);
    for (auto& key : vecPositionKeys)
        vecTimes.push_back(key.fTime);

    sort(vecTimes.begin(), vecTimes.end());
    vecTimes.erase(unique(vecTimes.begin(), vecTimes.end()), vecTimes.end());

    _uint iS = 0, iR = 0, iP = 0;
    for (auto t : vecTimes) {
        KEY_FRAME tKeyFrame = {};
        tKeyFrame.fTrackPosition = t / _fTickPerSecond;

        /* Scale */
        if (iS < vecScalingKeys.size() && vecScalingKeys[iS].fTime == t) {
            vScale = vecScalingKeys[iS].vValue;
            ++iS;
        }
        /* Rotation */
        if (iR < vecRotationKeys.size() && vecRotationKeys[iR].fTime == t) {
            vRotation = vecRotationKeys[iR].vValue;
            ++iR;
        }
        /* Position */
        if (iP < vecPositionKeys.size() && vecPositionKeys[iP].fTime == t) {
            vPosition = vecPositionKeys[iP].vValue;
            ++iP;
        }

        tKeyFrame.vScale = vScale;
        tKeyFrame.vRotation = vRotation;
        tKeyFrame.vPosition = vPosition;

        vecKeyFrames.push_back(move(tKeyFrame));
    }

    iNumKeyFrames = (_uint)vecKeyFrames.size();
}

KEY_FRAME tagChannel::Get_CurrentKeyFrame(_uint* _pKeyFrameIndex, _float _fTrackPosition)
{
    if (*_pKeyFrameIndex >= vecKeyFrames.size())
        *_pKeyFrameIndex = 0;

    if (_fTrackPosition < vecKeyFrames[*_pKeyFrameIndex].fTrackPosition)
        *_pKeyFrameIndex = 0;

    if (_fTrackPosition >= vecKeyFrames.back().fTrackPosition)
        return vecKeyFrames.back();

    while ((*_pKeyFrameIndex) + 1 < vecKeyFrames.size() &&
        _fTrackPosition >= vecKeyFrames[(*_pKeyFrameIndex) + 1].fTrackPosition)
    {
        ++(*_pKeyFrameIndex);
    }

    _float fRatio = {
        (_fTrackPosition - vecKeyFrames[(*_pKeyFrameIndex)].fTrackPosition) /
        (vecKeyFrames[(*_pKeyFrameIndex) + 1].fTrackPosition - vecKeyFrames[(*_pKeyFrameIndex)].fTrackPosition)
    };

    _vector vScale, vRotation, vPosition;

    /* 선형 보간 */
    _vector vSourScale = XMLoadFloat3(&vecKeyFrames[(*_pKeyFrameIndex)].vScale);
    _vector vDestScale = XMLoadFloat3(&vecKeyFrames[(*_pKeyFrameIndex) + 1].vScale);
    vScale = XMVectorLerp(vSourScale, vDestScale, fRatio);

    _vector	vSourRotate = XMLoadFloat4(&vecKeyFrames[(*_pKeyFrameIndex)].vRotation);
    _vector	vDestRotate = XMLoadFloat4(&vecKeyFrames[(*_pKeyFrameIndex) + 1].vRotation);
    vRotation = XMQuaternionSlerp(vSourRotate, vDestRotate, fRatio);
    vRotation = XMQuaternionNormalize(vRotation);

    _vector vSourPosition = XMVectorSetW(XMLoadFloat3(&vecKeyFrames[(*_pKeyFrameIndex)].vPosition), 1.f);
    _vector vDestPosition = XMVectorSetW(XMLoadFloat3(&vecKeyFrames[(*_pKeyFrameIndex) + 1].vPosition), 1.f);
    vPosition = XMVectorLerp(vSourPosition, vDestPosition, fRatio);

    KEY_FRAME tKeyFrame = {};
    XMStoreFloat3(&tKeyFrame.vScale, vScale);
    XMStoreFloat4(&tKeyFrame.vRotation, vRotation);
    XMStoreFloat3(&tKeyFrame.vPosition, vPosition);

    return tKeyFrame;
}

void tagChannel::Apply(_uint* _pCurrentKeyFrameIndex, _float _fCurrentTrackPosition, CTransform* _pBoneTransform) {
    if (vecKeyFrames.empty())
        return;

    /* Bone Transform */
    // 채널에서 캐쉬를 가지고 있으면 안됨, 채널은 에셋의 소유, 가지고 있으려면 m_pAnimState이 소유하는게 맞다
    if (!_pBoneTransform)
        return;

    /* SRT (S)Lerp */
    if (0.f == _fCurrentTrackPosition)
        (*_pCurrentKeyFrameIndex) = 0;

    KEY_FRAME& tLastKeyFrame = vecKeyFrames.back();
    _vector vScale, vRotation, vPosition;

    if (_fCurrentTrackPosition >= tLastKeyFrame.fTrackPosition) {
        vScale = XMLoadFloat3(&tLastKeyFrame.vScale);
        vRotation = XMLoadFloat4(&tLastKeyFrame.vRotation);
        vPosition = XMVectorSetW(XMLoadFloat3(&tLastKeyFrame.vPosition), 1.f);
    }
    else {
        while ((*_pCurrentKeyFrameIndex) + 1 < vecKeyFrames.size() &&
            _fCurrentTrackPosition >= vecKeyFrames[(*_pCurrentKeyFrameIndex) + 1].fTrackPosition)
        {
            ++(*_pCurrentKeyFrameIndex);
        }

        _float fRatio = {
            (_fCurrentTrackPosition - vecKeyFrames[(*_pCurrentKeyFrameIndex)].fTrackPosition) /
            (vecKeyFrames[(*_pCurrentKeyFrameIndex) + 1].fTrackPosition - vecKeyFrames[(*_pCurrentKeyFrameIndex)].fTrackPosition)
        };

        /* 선형 보간 */
        _vector vSourScale = XMLoadFloat3(&vecKeyFrames[(*_pCurrentKeyFrameIndex)].vScale);
        _vector vDestScale = XMLoadFloat3(&vecKeyFrames[(*_pCurrentKeyFrameIndex) + 1].vScale);
        vScale = XMVectorLerp(vSourScale, vDestScale, fRatio);

        _vector	vSourRotate = XMLoadFloat4(&vecKeyFrames[(*_pCurrentKeyFrameIndex)].vRotation);
        _vector	vDestRotate = XMLoadFloat4(&vecKeyFrames[(*_pCurrentKeyFrameIndex) + 1].vRotation);
        vRotation = XMQuaternionSlerp(vSourRotate, vDestRotate, fRatio);
        vRotation = XMQuaternionNormalize(vRotation);

        _vector vSourPosition = XMVectorSetW(XMLoadFloat3(&vecKeyFrames[(*_pCurrentKeyFrameIndex)].vPosition), 1.f);
        _vector vDestPosition = XMVectorSetW(XMLoadFloat3(&vecKeyFrames[(*_pCurrentKeyFrameIndex) + 1].vPosition), 1.f);
        vPosition = XMVectorLerp(vSourPosition, vDestPosition, fRatio);
    }

    /* Apply Bone Transform */
    _pBoneTransform->Set_Scale_NoDirty(vScale);
    _pBoneTransform->Set_Rotation_NoDirty(vRotation);
    _pBoneTransform->Set_Position_NoDirty(vPosition);
}

void tagChannel::Apply_Blend(KEY_FRAME& _curKeyFrame, KEY_FRAME& _nextKeyFrame, _float _fRatio, CTransform* _pBoneTransform)
{
    _vector vScale, vRotation, vPosition;
    /* Scale */
    _vector vSourScale = XMLoadFloat3(&_curKeyFrame.vScale);
    _vector vDestScale = XMLoadFloat3(&_nextKeyFrame.vScale);
    vScale = XMVectorLerp(vSourScale, vDestScale, _fRatio);

    /* Rotation */
    _vector vSourRotate = XMLoadFloat4(&_curKeyFrame.vRotation);
    _vector vDestRotate = XMLoadFloat4(&_nextKeyFrame.vRotation);
    vRotation = XMQuaternionSlerp(vSourRotate, vDestRotate, _fRatio);
    vRotation = XMQuaternionNormalize(vRotation);

    /* Position */
    _vector vSourPosition = XMLoadFloat3(&_curKeyFrame.vPosition);
    _vector vDestPosition = XMLoadFloat3(&_nextKeyFrame.vPosition);
    vPosition = XMVectorLerp(vSourPosition, vDestPosition, _fRatio);

    /* Apply Bone Transform */
    _pBoneTransform->Set_Scale_NoDirty(vScale);
    _pBoneTransform->Set_Rotation_NoDirty(vRotation);
    _pBoneTransform->Set_Position_NoDirty(vPosition);
}

void ANIMATION_STATE::Reset(CObject* _pOwner) {
    fTime = 0.f;
    bFinished = false;
    pBindingRoot = _pOwner;

    if (!pAnimation)
        return;

    vecKeyFrameIndices.assign(pAnimation->vecChannels.size(), 0); // 초기화 후 채우기
    vecBoneTransformCache.resize(pAnimation->vecChannels.size());
    vecPose.resize(pAnimation->vecChannels.size());
    Init_BindingContext(_pOwner);

    /* Root Motion */
    vPreRootXPos = { 0.f, 0.f, 0.f };
    vPreRootZPos = { 0.f, 0.f, 0.f };
}

void ANIMATION_STATE::Reset_KeyFrameIndex()
{
    if (vecKeyFrameIndices.empty())
        return;

    if (pAnimation) {
        auto itAnim = m_umBindingContextByAnim.find(pAnimation);
        if (itAnim != m_umBindingContextByAnim.end()) {
            for (auto& pair : itAnim->second.umEventBindingContext) {
                pair.second.Reset_Executed();
            }
        }
    }

    std::fill(vecKeyFrameIndices.begin(), vecKeyFrameIndices.end(), 0);
}

void ANIMATION_STATE::Cache_BoneTransform(CTransform* _pRootTransform)
{
    if (!_pRootTransform)
        return;

    vecBoneTransformCache.resize(pAnimation->vecChannels.size());
    for (_int i = 0; i < pAnimation->vecChannels.size(); ++i)
    {
        if (pAnimation->vecChannels[i].strBoneRoute.empty())
        {
            vecBoneTransformCache[i] = _pRootTransform;
        }
        else
        {
            vecBoneTransformCache[i] = _pRootTransform->Get_ChildToPath(MultiByteToUnicode(pAnimation->vecChannels[i].strBoneRoute));
        }
    }
}

void ANIMATION_STATE::Init_BindingContext(CObject* _pOwner)
{
    pBindingRoot = _pOwner;

    if (!_pOwner || !pAnimation)
        return;

    auto& tCache = m_umBindingContextByAnim[pAnimation];

    if (tCache.umEventBindingContext.size() != pAnimation->vecEvents.size()) {
        tCache.umEventBindingContext.clear();
        tCache.umEventBindingContext.reserve(pAnimation->vecEvents.size());
        for (_uint i = 0; i < pAnimation->vecEvents.size(); ++i) {
            tCache.umEventBindingContext.emplace(i, EventBindContext(_pOwner));
        }
    }
    else {
        for (auto& pair : tCache.umEventBindingContext) {
            auto& ctx = pair.second;
            if (ctx.pRoot != _pOwner) {
                ctx.Reset();
                ctx.pRoot = _pOwner;
            }
            ctx.Reset_Executed();
        }
    }

    if (tCache.umCustomAnimBindingContext.size() != pAnimation->vecCustomAnims.size()) {
        tCache.umCustomAnimBindingContext.clear();
        tCache.umCustomAnimBindingContext.reserve(pAnimation->vecCustomAnims.size());
        for (_uint i = 0; i < pAnimation->vecCustomAnims.size(); ++i) {
            tCache.umCustomAnimBindingContext.emplace(i, BindingContext(_pOwner));
        }
    }
    else {
        for (auto& pair : tCache.umCustomAnimBindingContext) {
            auto& ctx = pair.second;
            if (ctx.pRoot != _pOwner) {
                ctx.Reset();
                ctx.pRoot = _pOwner;
            }
        }
    }
}

EventBindContext* ANIMATION_STATE::Get_EventBindContext(_uint _iEventIndex)
{
    if (!pAnimation)
        return nullptr;

    auto itAnim = m_umBindingContextByAnim.find(pAnimation);
    if (itAnim == m_umBindingContextByAnim.end() ||
        itAnim->second.umEventBindingContext.size() != pAnimation->vecEvents.size()) {
        Init_BindingContext(pBindingRoot);
        itAnim = m_umBindingContextByAnim.find(pAnimation);
    }

    if (itAnim == m_umBindingContextByAnim.end())
        return nullptr;

    return Ensure_BindingContext(itAnim->second.umEventBindingContext, _iEventIndex, pBindingRoot);
}

BindingContext* ANIMATION_STATE::Get_CustomAnimBindContext(_uint _iCustomAnimIndex)
{
    if (!pAnimation)
        return nullptr;

    auto itAnim = m_umBindingContextByAnim.find(pAnimation);
    if (itAnim == m_umBindingContextByAnim.end() ||
        itAnim->second.umCustomAnimBindingContext.size() != pAnimation->vecCustomAnims.size()) {
        Init_BindingContext(pBindingRoot);
        itAnim = m_umBindingContextByAnim.find(pAnimation);
    }

    if (itAnim == m_umBindingContextByAnim.end())
        return nullptr;

    return Ensure_BindingContext(itAnim->second.umCustomAnimBindingContext, _iCustomAnimIndex, pBindingRoot);
}

ParamValue CUSTOM_ANIM::Evaluate_CustomKeyFrame(_float _fTimeInCustomAnim)
{
    if (!pFunction || vecKF.size() < 2)
        return ParamValue{};

    if (iCurKeyFrameIndex >= vecKF.size())
        iCurKeyFrameIndex = 0;

    if (_fTimeInCustomAnim < vecKF[iCurKeyFrameIndex].fTrackPosition)
        iCurKeyFrameIndex = 0;

    auto* pFuncDesc = pFunction->Get_FunctionDesc();
    if (!pFuncDesc)
        return ParamValue{};

    auto tType = pFuncDesc->tType;

    /* Last Key */
    if (_fTimeInCustomAnim >= vecKF.back().fTrackPosition)
    {
        auto& kf = vecKF.back();
        switch (tType)
        {
        case FUNCTION_PROPERTY_TYPE::INT: return ParamValue{ kf.iParam };
        case FUNCTION_PROPERTY_TYPE::FLOAT1: return ParamValue{ kf.fParam };
        case FUNCTION_PROPERTY_TYPE::FLOAT2: return ParamValue{ kf.v2Param };
        case FUNCTION_PROPERTY_TYPE::FLOAT3: return ParamValue{ kf.v3Param };
        case FUNCTION_PROPERTY_TYPE::FLOAT4: return ParamValue{ kf.v4Param };
        }
    }

    /* Find KeyFrame */
    while ((iCurKeyFrameIndex + 1) < vecKF.size() &&
        _fTimeInCustomAnim >= vecKF[iCurKeyFrameIndex + 1].fTrackPosition)
    {
        ++iCurKeyFrameIndex;
    }

    /* Evaluate */
    auto& tA = vecKF[iCurKeyFrameIndex];
    auto& tB = vecKF[iCurKeyFrameIndex + 1];

    _float fDelta = tB.fTrackPosition - tA.fTrackPosition;
    _float fRatio = (fDelta > 0.f) ? (_fTimeInCustomAnim - tA.fTrackPosition) / fDelta : 0.f;
    fRatio = max(0.f, min(1.f, fRatio));

    if (tType == FUNCTION_PROPERTY_TYPE::INT)
    {
        _int iValue = (_int)((1.f - fRatio) * tA.iParam + fRatio * tB.iParam);
        return ParamValue{ iValue };
    }
    else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT1)
    {
        _float fValue = (1.f - fRatio) * tA.fParam + fRatio * tB.fParam;
        return ParamValue{ fValue };
    }
    else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT2)
    {
        _vector vA = XMLoadFloat2(&tA.v2Param);
        _vector vB = XMLoadFloat2(&tB.v2Param);
        _vector vLerp = XMVectorLerp(vA, vB, fRatio);

        _float2 fValue;
        XMStoreFloat2(&fValue, vLerp);
        return ParamValue{ fValue };
    }
    else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT3)
    {
        _vector vA = XMLoadFloat3(&tA.v3Param);
        _vector vB = XMLoadFloat3(&tB.v3Param);
        _vector vLerp = XMVectorLerp(vA, vB, fRatio);

        _float3 fValue;
        XMStoreFloat3(&fValue, vLerp);
        return ParamValue{ fValue };
    }
    else if (tType == FUNCTION_PROPERTY_TYPE::FLOAT4)
    {
        _vector vA = XMLoadFloat4(&tA.v4Param);
        _vector vB = XMLoadFloat4(&tB.v4Param);
        _vector vLerp = XMQuaternionSlerp(vA, vB, fRatio);

        _float4 fValue;
        XMStoreFloat4(&fValue, vLerp);
        return ParamValue{ fValue };
    }

    return ParamValue();
}

void EVENT_MODIFIED_TIME::Update_Now()
{
    FILETIME tFileTime{};
    GetSystemTimeAsFileTime(&tFileTime);

    ULARGE_INTEGER tValue{};
    tValue.LowPart = tFileTime.dwLowDateTime;
    tValue.HighPart = tFileTime.dwHighDateTime;

    ullFileTimeUtc = tValue.QuadPart;
}

_bool EVENT_MODIFIED_TIME::Is_Empty() const
{
    return ullFileTimeUtc == 0;
}

HRESULT EVENT_KEY::Ensure_Meta()
{
    if (strGUID.empty())
    {
        if (FAILED(Create_GUID(strGUID)))
            return E_FAIL;
    }

    if (tModifiedTime.Is_Empty())
        tModifiedTime.Update_Now();

    return S_OK;
}

void EVENT_KEY::Touch_ModifiedTime()
{
    tModifiedTime.Update_Now();
}
