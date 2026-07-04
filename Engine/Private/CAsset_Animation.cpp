#include "EnginePCH.h"
#include "CAsset_Animation.h"
#include "CAsset_FBX_Model.h"
#include "WClassFunction.h"
#include "WShaderFunction.h"
#include "CAnimationInfo.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr _float kAnimMergeEpsilon = 0.0001f;
	constexpr _uint MAX_ANIMATION_FILE_VERSION = 8;
	constexpr _uint MAX_ANIMATION_INFO_COUNT = 1024;

	_bool IsNearlyEqual(_float _fLeft, _float _fRight) {
		return fabsf(_fLeft - _fRight) <= kAnimMergeEpsilon;
	}

	_string Make_CopiedAnimationName(const unordered_map<_string, ANIMATION_INFO*>& _umAnimationInfo, const _string& _strSourceName) {
		const _string strBaseName = _strSourceName.empty() ? "New Animation" : _strSourceName;
		_string strName = strBaseName + "_1";

		for (_uint i = 2; _umAnimationInfo.find(strName) != _umAnimationInfo.end(); ++i) {
			strName = strBaseName + "_" + to_string(i);
		}

		return strName;
	}

	WFunction* Clone_Function(WFunction* _pFunction) {
		if (_pFunction == nullptr)
			return nullptr;

		if (_pFunction->Get_FunctionType() == FUNCTION_TYPE::CLASS)
			return new WClassFunction(*static_cast<WClassFunction*>(_pFunction));

		if (_pFunction->Get_FunctionType() == FUNCTION_TYPE::SHADER)
			return new WShaderFunction(*static_cast<WShaderFunction*>(_pFunction));

		return nullptr;
	}

	EVENT_KEY Clone_EventKey(const EVENT_KEY& _tEvent) {
		EVENT_KEY tCopiedEvent = _tEvent;
		tCopiedEvent.pFunction = Clone_Function(_tEvent.pFunction);
		tCopiedEvent.strGUID.clear();
		tCopiedEvent.tModifiedTime = {};
		tCopiedEvent.Ensure_Meta();

		return tCopiedEvent;
	}

	CUSTOM_ANIM Clone_CustomAnim(const CUSTOM_ANIM& _tCustomAnim) {
		CUSTOM_ANIM tCopiedCustomAnim = _tCustomAnim;
		tCopiedCustomAnim.pFunction = Clone_Function(_tCustomAnim.pFunction);

		return tCopiedCustomAnim;
	}

	FUNCTION_PROPERTY_TYPE GetFunctionPropertyType(WFunction* _pFunction) {
		if (_pFunction == nullptr)
			return FUNCTION_PROPERTY_TYPE::MONOSTATE;

		auto pFunctionDesc = _pFunction->Get_FunctionDesc();
		if (pFunctionDesc == nullptr)
			return FUNCTION_PROPERTY_TYPE::MONOSTATE;

		return pFunctionDesc->tType;
	}

	_bool IsSameFunction(WFunction* _pLhs, WFunction* _pRhs) {
		if (_pLhs == _pRhs)
			return true;
		if (_pLhs == nullptr || _pRhs == nullptr)
			return false;
		if (_pLhs->Get_FunctionType() != _pRhs->Get_FunctionType())
			return false;
		if (_pLhs->Get_ChildPath() != _pRhs->Get_ChildPath())
			return false;

		if (_pLhs->Get_FunctionType() == FUNCTION_TYPE::CLASS) {
			auto pLhsFunctionDesc = _pLhs->Get_FunctionDesc();
			auto pRhsFunctionDesc = _pRhs->Get_FunctionDesc();
			if (pLhsFunctionDesc == nullptr || pRhsFunctionDesc == nullptr)
				return false;

			return pLhsFunctionDesc->strName == pRhsFunctionDesc->strName &&
				pLhsFunctionDesc->tType == pRhsFunctionDesc->tType;
		}

		vector<_byte> vecLhs{};
		vector<_byte> vecRhs{};
		_pLhs->Save(vecLhs);
		_pRhs->Save(vecRhs);
		return vecLhs == vecRhs;
	}

	_bool IsSameKeyFrameValue(const CUSTOM_KEY_FRAME& _tLhs, const CUSTOM_KEY_FRAME& _tRhs, FUNCTION_PROPERTY_TYPE _eType) {
		switch (_eType) {
		case FUNCTION_PROPERTY_TYPE::INT:
			return _tLhs.iParam == _tRhs.iParam;
		case FUNCTION_PROPERTY_TYPE::FLOAT1:
			return IsNearlyEqual(_tLhs.fParam, _tRhs.fParam);
		case FUNCTION_PROPERTY_TYPE::FLOAT2:
			return IsNearlyEqual(_tLhs.v2Param.x, _tRhs.v2Param.x) &&
				IsNearlyEqual(_tLhs.v2Param.y, _tRhs.v2Param.y);
		case FUNCTION_PROPERTY_TYPE::FLOAT3:
			return IsNearlyEqual(_tLhs.v3Param.x, _tRhs.v3Param.x) &&
				IsNearlyEqual(_tLhs.v3Param.y, _tRhs.v3Param.y) &&
				IsNearlyEqual(_tLhs.v3Param.z, _tRhs.v3Param.z);
		case FUNCTION_PROPERTY_TYPE::FLOAT4:
			return IsNearlyEqual(_tLhs.v4Param.x, _tRhs.v4Param.x) &&
				IsNearlyEqual(_tLhs.v4Param.y, _tRhs.v4Param.y) &&
				IsNearlyEqual(_tLhs.v4Param.z, _tRhs.v4Param.z) &&
				IsNearlyEqual(_tLhs.v4Param.w, _tRhs.v4Param.w);
		case FUNCTION_PROPERTY_TYPE::MONOSTATE:
		default:
			return true;
		}
	}

	_bool IsSameCustomKeyFrame(const CUSTOM_KEY_FRAME& _tLhs, const CUSTOM_KEY_FRAME& _tRhs, FUNCTION_PROPERTY_TYPE _eType) {
		if (!IsNearlyEqual(_tLhs.fTrackPosition, _tRhs.fTrackPosition))
			return false;

		return IsSameKeyFrameValue(_tLhs, _tRhs, _eType);
	}

	_bool IsSameEventKey(const EVENT_KEY& _tLhs, const EVENT_KEY& _tRhs) {
		if (_tLhs.iEventLayerIndex != _tRhs.iEventLayerIndex)
			return false;
		if (!IsNearlyEqual(_tLhs.tKF.fTrackPosition, _tRhs.tKF.fTrackPosition))
			return false;
		if (_tLhs.strEventName != _tRhs.strEventName)
			return false;
		if (!IsSameFunction(_tLhs.pFunction, _tRhs.pFunction))
			return false;

		auto eType = GetFunctionPropertyType(_tLhs.pFunction);
		return IsSameKeyFrameValue(_tLhs.tKF, _tRhs.tKF, eType);
	}

	_bool IsSameEventIdentity(const EVENT_KEY& _tLhs, const EVENT_KEY& _tRhs) {
		if (!_tLhs.strGUID.empty() && !_tRhs.strGUID.empty())
			return _tLhs.strGUID == _tRhs.strGUID;

		return IsSameEventKey(_tLhs, _tRhs);
	}

	_bool IsNewerEventKey(const EVENT_KEY& _tLhs, const EVENT_KEY& _tRhs) {
		return _tLhs.tModifiedTime.ullFileTimeUtc > _tRhs.tModifiedTime.ullFileTimeUtc;
	}

	void MergeEventCandidate(vector<EVENT_KEY>& _vecMerged, const EVENT_KEY& _tCandidate) {
		if (_tCandidate.pFunction == nullptr)
			return;

		EVENT_KEY tCandidate = _tCandidate;
		tCandidate.Ensure_Meta();

		auto iter = find_if(_vecMerged.begin(), _vecMerged.end(),
			[&](const EVENT_KEY& tEvent) {
				return IsSameEventIdentity(tEvent, tCandidate);
			});

		if (iter == _vecMerged.end()) {
			Safe_AddRef(tCandidate.pFunction);
			_vecMerged.push_back(tCandidate);
			return;
		}

		if (!IsNewerEventKey(tCandidate, *iter))
			return;

		WFunction* pOldFunction = iter->pFunction;
		Safe_AddRef(tCandidate.pFunction);
		*iter = tCandidate;
		Safe_Release(pOldFunction);
	}

	_bool IsSameCustomAnim(const CUSTOM_ANIM& _tLhs, const CUSTOM_ANIM& _tRhs) {
		if (_tLhs.iEventLayerIndex != _tRhs.iEventLayerIndex)
			return false;
		if (!IsNearlyEqual(_tLhs.fStartTime, _tRhs.fStartTime))
			return false;
		if (!IsNearlyEqual(_tLhs.fEndTime, _tRhs.fEndTime))
			return false;
		if (_tLhs.strCustomAnimName != _tRhs.strCustomAnimName)
			return false;
		if (!IsSameFunction(_tLhs.pFunction, _tRhs.pFunction))
			return false;
		if (_tLhs.vecKF.size() != _tRhs.vecKF.size())
			return false;

		auto eType = GetFunctionPropertyType(_tLhs.pFunction);
		for (size_t i = 0; i < _tLhs.vecKF.size(); ++i) {
			if (!IsSameCustomKeyFrame(_tLhs.vecKF[i], _tRhs.vecKF[i], eType))
				return false;
		}

		return true;
	}

	_bool IsSameAdditiveLayerInfo(const ADDITIVE_LAYER_INFO& _tLhs, const ADDITIVE_LAYER_INFO& _tRhs) {
		if (_tLhs.strAdditiveAnimName != _tRhs.strAdditiveAnimName)
			return false;
		if (_tLhs.bAdditive != _tRhs.bAdditive)
			return false;
		if (!IsNearlyEqual(_tLhs.fWeight, _tRhs.fWeight))
			return false;
		if (_tLhs.umBoneMask != _tRhs.umBoneMask)
			return false;

		return true;
	}

	void MergeAdditiveInfos(ANIMATION_INFO* _pDest, const ANIMATION_INFO* _pSrc) {
		for (const auto& tSrcInfo : _pSrc->vecAdditiveInfos) {
			auto iter = find_if(_pDest->vecAdditiveInfos.begin(), _pDest->vecAdditiveInfos.end(),
				[&](const ADDITIVE_LAYER_INFO& tDestInfo) {
					return IsSameAdditiveLayerInfo(tDestInfo, tSrcInfo);
				});

			if (iter == _pDest->vecAdditiveInfos.end()) {
				_pDest->vecAdditiveInfos.push_back(tSrcInfo);
			}
		}
	}

	void MergeEvents(ANIMATION_INFO* _pDest, const ANIMATION_INFO* _pSrc) {
		vector<EVENT_KEY> vecMerged{};
		vecMerged.reserve(_pDest->vecEvents.size() + _pSrc->vecEvents.size());

		for (const auto& tDestEvent : _pDest->vecEvents)
			MergeEventCandidate(vecMerged, tDestEvent);

		for (const auto& tSrcEvent : _pSrc->vecEvents)
			MergeEventCandidate(vecMerged, tSrcEvent);

		for (auto& tDestEvent : _pDest->vecEvents)
			Safe_Release(tDestEvent.pFunction);

		_pDest->vecEvents = std::move(vecMerged);
	}

	void MergeCustomAnims(ANIMATION_INFO* _pDest, const ANIMATION_INFO* _pSrc) {
		for (const auto& tSrcCustomAnim : _pSrc->vecCustomAnims) {
			if (tSrcCustomAnim.pFunction == nullptr)
				continue;

			auto iter = find_if(_pDest->vecCustomAnims.begin(), _pDest->vecCustomAnims.end(),
				[&](const CUSTOM_ANIM& tDestCustomAnim) {
					return IsSameCustomAnim(tDestCustomAnim, tSrcCustomAnim);
				});

			if (iter != _pDest->vecCustomAnims.end())
				continue;

			CUSTOM_ANIM tNewCustomAnim = tSrcCustomAnim;
			Safe_AddRef(tNewCustomAnim.pFunction);
			_pDest->vecCustomAnims.push_back(tNewCustomAnim);
		}
	}
}

CAsset_Animation::CAsset_Animation(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CAsset(_pDevice, _pContext) {
}

HRESULT CAsset_Animation::Initialize() {
	m_pMergeAnimationAsset = WAsset<CAsset_Animation>::Create(nullptr);

	return S_OK;
}

ANIMATION_INFO* CAsset_Animation::Create_New_Animation() {
	auto pAnimation = new ANIMATION_INFO;
	pAnimation->strName = "New Animation";
	auto [f, s] = m_umAnimationInfo.emplace("New Animation", pAnimation);
	if (s == false) {
		Safe_Release(pAnimation);
		return nullptr;
	}

	return pAnimation;
}

ANIMATION_INFO* CAsset_Animation::Copy_Animation(ANIMATION_INFO* _pSourceAnimation) {
	if (_pSourceAnimation == nullptr)
		return nullptr;

	auto pCopiedAnimation = new ANIMATION_INFO;
	pCopiedAnimation->fDuration = _pSourceAnimation->fDuration;
	pCopiedAnimation->fTickPerSecond = _pSourceAnimation->fTickPerSecond;
	pCopiedAnimation->strName = Make_CopiedAnimationName(m_umAnimationInfo, _pSourceAnimation->strName);
	pCopiedAnimation->strAmartureRoute = _pSourceAnimation->strAmartureRoute;
	pCopiedAnimation->vecChannels = _pSourceAnimation->vecChannels;
	pCopiedAnimation->iNumChannels = (_uint)pCopiedAnimation->vecChannels.size();
	pCopiedAnimation->bBlending = _pSourceAnimation->bBlending;
	pCopiedAnimation->fBlendTime = _pSourceAnimation->fBlendTime;
	pCopiedAnimation->strRootBoneXName = _pSourceAnimation->strRootBoneXName;
	pCopiedAnimation->strRootBoneZName = _pSourceAnimation->strRootBoneZName;
	pCopiedAnimation->fRootMotionMultiplierX = _pSourceAnimation->fRootMotionMultiplierX;
	pCopiedAnimation->fRootMotionMultiplierZ = _pSourceAnimation->fRootMotionMultiplierZ;
	pCopiedAnimation->fRootMotionMuteWeight = _pSourceAnimation->fRootMotionMuteWeight;
	pCopiedAnimation->vecAdditiveInfos = _pSourceAnimation->vecAdditiveInfos;

	pCopiedAnimation->vecEvents.reserve(_pSourceAnimation->vecEvents.size());
	for (const auto& tEvent : _pSourceAnimation->vecEvents) {
		pCopiedAnimation->vecEvents.push_back(Clone_EventKey(tEvent));
	}

	pCopiedAnimation->vecCustomAnims.reserve(_pSourceAnimation->vecCustomAnims.size());
	for (const auto& tCustomAnim : _pSourceAnimation->vecCustomAnims) {
		pCopiedAnimation->vecCustomAnims.push_back(Clone_CustomAnim(tCustomAnim));
	}

	pCopiedAnimation->Cache_RootBoneIndex();

	auto [iter, bInserted] = m_umAnimationInfo.emplace(pCopiedAnimation->strName, pCopiedAnimation);
	if (bInserted == false) {
		Safe_Release(pCopiedAnimation);
		return nullptr;
	}

	Add_Generation();

	return pCopiedAnimation;
}

void CAsset_Animation::Change_Animation_Name(ANIMATION_INFO* _pSeq, const _string& _strName) {
	auto f = m_umAnimationInfo.find(_pSeq->strName);
	if (f == m_umAnimationInfo.end())
		return;

	auto s = m_umAnimationInfo.find(_strName);
	if (s != m_umAnimationInfo.end())
		return;

	auto [t, d] = m_umAnimationInfo.emplace(_strName, _pSeq);
	if (d) {
		_pSeq->strName = _strName;
		m_umAnimationInfo.erase(f);
	}
}

void CAsset_Animation::Remove_Animation(ANIMATION_INFO* _pSeq) {
	if (!_pSeq) return;
	auto iter = m_umAnimationInfo.find(_pSeq->strName);
	if (iter != m_umAnimationInfo.end()) {
		Safe_Release(iter->second);
		m_umAnimationInfo.erase(iter);
	}
}

void CAsset_Animation::OnGui_Inspector_Context() {
	__super::OnGui_Inspector_Context();

	m_pMergeAnimationAsset->OnGui_Inspector_Context("Merge Animation");
	if (ImGui::Button("Merge Asset")) {
		if (auto pAnimAsset = m_pMergeAnimationAsset->Get()) {
			Merge_Asset(pAnimAsset);
		}
	}

	_int iSize = (_int)m_vecYaml.size();
	if (ImGui::InputInt("##CAsset_Animation::OnGui_Inspector_Context", &iSize, 1, 1)) {
		_int iDiff = (_int)(iSize - m_vecYaml.size());
		if (iDiff > 0) {
			m_vecYaml.push_back(WAsset<CAsset_Anim_Yaml>::Create(nullptr));
		}
		else if (iDiff < 0 && !m_vecYaml.empty()) {
			if (m_vecYaml.back()) {
				Safe_Release(m_vecYaml.back());
				m_vecYaml.pop_back();
			}
		}
	}

	_int i = 0;
	for (auto pYaml : m_vecYaml) {
		pYaml->OnGui_Inspector_Context(".anim_unity", i++);
	}

	static _string strModelName = "";
	ImGui::InputText("Model Name", &strModelName);
	if (ImGui::Button("Export##CAsset_Animation::OnGui_Inspector_Context")) {
		Generate_Animation_From_Yaml(strModelName);
	}
}

void CAsset_Animation::Merge_Asset(CAsset_Animation* _pAsset) {
	if (_pAsset == nullptr)
		return;

	for (auto iter = _pAsset->m_umAnimationInfo.begin(); iter != _pAsset->m_umAnimationInfo.end(); ++iter) {
		if ((*iter).second == nullptr)
			continue;

		auto f = m_umAnimationInfo.find((*iter).first);
		if (f == m_umAnimationInfo.end()) {
			Safe_AddRef((*iter).second);
			m_umAnimationInfo.emplace(*iter);
			continue;
		}

		auto pDestInfo = (*f).second;
		auto pSrcInfo = (*iter).second;
		if (pDestInfo == nullptr || pSrcInfo == nullptr)
			continue;

		MergeAdditiveInfos(pDestInfo, pSrcInfo);
		MergeEvents(pDestInfo, pSrcInfo);
		MergeCustomAnims(pDestInfo, pSrcInfo);

		Debug_Output("Duplicated animation name detected. Merged unique animation containers.\n");
	}

	Save_File();
}

void CAsset_Animation::Generate_Animation_From_Yaml(const _string& _strModelName) {
	for (auto& pair : m_umAnimationInfo) {
		Safe_Release(pair.second);
	}
	m_umAnimationInfo.clear();

	for (auto pYamlAsset : m_vecYaml) {
		auto pAnimationInfo = new ANIMATION_INFO;

		auto pAsset = pYamlAsset->Get();

		auto strPath = UnicodeToMultiByte(pAsset->GetMetaDesc().strPath);

		auto yAnimation = YAML::LoadFile(strPath);
		const auto& yClip = yAnimation["AnimationClip"];

		pAnimationInfo->strName = yClip["m_Name"].as<string>();

		pAnimationInfo->fTickPerSecond = 60.f;
		pAnimationInfo->fDuration = yClip["m_AnimationClipSettings"]["m_StopTime"].as<_float>();

		string strPathHeader = _strModelName + "/";

		m_umAnimationInfo.emplace(pAnimationInfo->strName, pAnimationInfo);
	}
}

CAsset_Animation* CAsset_Animation::Create(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext, const fs::directory_entry& _fsEntry) {
	auto pInstance = new CAsset_Animation(_pDevice, _pContext);
	if (FAILED(pInstance->Load_Meta(_fsEntry))) {
		Safe_Release(pInstance);
		return nullptr;
	}

	if (FAILED(pInstance->Initialize())) {
		Safe_Release(pInstance);
		return nullptr;
	}

	if (FAILED(pInstance->Load_File(_fsEntry))) {
		Safe_Release(pInstance);
		return nullptr;
	}

	return pInstance;
}

HRESULT CAsset_Animation::Create_File(const fs::directory_entry& _fsEntry) {
	if (_fsEntry.exists())
		return E_FAIL;

	_string strPath = _fsEntry.path().generic_string() + ".anim";

	fstream ofStream(strPath, ios_base::binary | ios_base::out | ios_base::trunc);
	if (!ofStream.is_open())
		return E_FAIL;

	vector<_byte> vecData{};

	_uint iVersion = 8;
	Write_To(vecData, &iVersion, sizeof(_uint));

	_uint iSize = 0;
	Write_To(vecData, &iSize, sizeof(_uint));

	ofStream.write(reinterpret_cast<const char*>(vecData.data()), vecData.size());

	ofStream.close();

	return S_OK;
}

ANIMATION_INFO* CAsset_Animation::OnGui_Animator_Inspector_Context(const char* _szName) {
	ANIMATION_INFO* pResult = nullptr;

	if (ImGui::BeginCombo("##CAsset_Animation::OnGui_Inspector_Context", _szName, ImGuiComboFlags_WidthFitPreview)) {
		for (auto& pair : m_umAnimationInfo) {
			if (ImGui::Selectable(pair.first.c_str())) {
				pResult = pair.second;
			}
		}

		ImGui::EndCombo();
	}

	return pResult;
}

HRESULT CAsset_Animation::Save_File() {
	return Save_File(m_umAnimationInfo, fs::path(m_tAssetMetaDesc.strPath));
}

HRESULT CAsset_Animation::Save_File(unordered_map<_string, ANIMATION_INFO*>& _umAnimationInfo, const fs::path& _fsPath) {
	vector<_byte> vecData{};

	_uint iAnimationSize = (_uint)_umAnimationInfo.size();
	Write_To(vecData, &iAnimationSize, sizeof(_uint));

	for (auto& pAnimation : _umAnimationInfo) {
		pAnimation.second->Save(vecData);
	}

	return Save_File_Binary(_fsPath, vecData);
}

HRESULT CAsset_Animation::Save_File_Binary(const fs::path& _fsPath, const vector<_byte>& _vecData) {
	vector<_byte> vecHeaderIncluded{};
	_uint iVersion = 8;
	Write_To(vecHeaderIncluded, &iVersion, sizeof(_uint));

	vecHeaderIncluded.insert(vecHeaderIncluded.end(), _vecData.begin(), _vecData.end());

	ofstream ofStream(_fsPath.generic_string(), ios_base::binary | ios_base::out);
	if (!ofStream.is_open())
		return E_FAIL;

	ofStream.write(reinterpret_cast<const char*>(vecHeaderIncluded.data()), vecHeaderIncluded.size());

	ofStream.close();

	return S_OK;
}

HRESULT CAsset_Animation::Save_File_From_FBX(const fs::path& _fsPath, const aiScene* _pAIScene, const aiNode* _pRootNode, const _string& _strAmartureName) {
	auto strModelPath = _fsPath.generic_string();
	_string strAnimationFilePath = { strModelPath.begin(), strModelPath.begin() + strModelPath.rfind('.') };
	strAnimationFilePath.append(".anim");

	vector<_byte> vecData{};

	unordered_map<_string, _string> umNameCache{};

	auto BuildRouteString = [](const std::vector<_string>& path) -> _string {
		_string out;
		for (size_t i = 1; i < path.size(); ++i) {
			out += path[i];
			if (i + 1 < path.size())
				out += "/";
		}
		return out;
		};

	std::function<bool(const aiNode*, const char*, std::vector<_string>&)> FindPathDFS;
	FindPathDFS = [&](const aiNode* node, const char* target, std::vector<_string>& path) -> bool {
		path.push_back(node->mName.C_Str());

		if (!strcmp(node->mName.C_Str(), target))
			return true;

		for (unsigned i = 0; i < node->mNumChildren; ++i) {
			if (FindPathDFS(node->mChildren[i], target, path))
				return true;
		}

		path.pop_back();
		return false;
		};

	unordered_map<_string, ANIMATION_INFO*> umAnimationInfo{};

	for (_uint i = 0; i < _pAIScene->mNumAnimations; i++) {
		auto pAIAnimation = _pAIScene->mAnimations[i];
		auto pAnimationInfo = new ANIMATION_INFO;

		double tps = (pAIAnimation->mTicksPerSecond != 0.0) ? pAIAnimation->mTicksPerSecond : 60.f;
		pAnimationInfo->fTickPerSecond = (float)tps;
		pAnimationInfo->fDuration = (float)(pAIAnimation->mDuration / tps);
		
		pAnimationInfo->strName = _string{ pAIAnimation->mName.C_Str() };
		pAnimationInfo->strName = { pAnimationInfo->strName.begin() + (pAnimationInfo->strName.rfind('|') + 1), pAnimationInfo->strName.end() };

		/* Channel */
		pAnimationInfo->vecChannels.reserve(pAIAnimation->mNumChannels);
		for (_uint j = 0; j < pAIAnimation->mNumChannels; j++) {
			auto pAIChannel = pAIAnimation->mChannels[j];
			if (!pAIChannel)
				continue;

			/* Bone Route */
			_string strRoute = "";
			const char* key = pAIChannel->mNodeName.C_Str();
			auto it = umNameCache.find(key);
			if (it != umNameCache.end()) {
				strRoute = it->second;
			}
			else {
				std::vector<_string> path;
				path.push_back(_pRootNode->mName.C_Str());
				if (FindPathDFS(_pRootNode, key, path)) {
					strRoute = BuildRouteString(path);
					umNameCache.emplace(key, strRoute);
				}
				else {
					Debug_Output("Missing : ", key, "\n");
					continue;
				}
			}

			/* Name */
			CHANNEL tChannel;
			tChannel.strBoneName = pAIChannel->mNodeName.C_Str();
			tChannel.strBoneRoute = strRoute;

			/* Keyframe */
			tChannel.vecScalingKeys.reserve(pAIChannel->mNumScalingKeys);
			for (_uint sk = 0; sk < pAIChannel->mNumScalingKeys; sk++) {
				KeyVec3 tKey = {};
				tKey.fTime = (_float)pAIChannel->mScalingKeys[sk].mTime;
				memcpy(&tKey.vValue, &pAIChannel->mScalingKeys[sk].mValue, sizeof(_float3));
				tChannel.vecScalingKeys.push_back(tKey);
			}

			tChannel.vecRotationKeys.reserve(pAIChannel->mNumRotationKeys);
			for (_uint rk = 0; rk < pAIChannel->mNumRotationKeys; rk++) {
				KeyVec4 tKey = {};
				tKey.fTime = (_float)pAIChannel->mRotationKeys[rk].mTime;
				auto& Quat = pAIChannel->mRotationKeys[rk].mValue;
				tKey.vValue = _float4(Quat.x, Quat.y, Quat.z, Quat.w);
				tChannel.vecRotationKeys.push_back(tKey);
			}

			tChannel.vecPositionKeys.reserve(pAIChannel->mNumPositionKeys);
			for (_uint pk = 0; pk < pAIChannel->mNumPositionKeys; pk++) {
				KeyVec3 tKey = {};
				tKey.fTime = (_float)pAIChannel->mPositionKeys[pk].mTime;
				memcpy(&tKey.vValue, &pAIChannel->mPositionKeys[pk].mValue, sizeof(_float3));
				tChannel.vecPositionKeys.push_back(tKey);
			}

			/* Keyframe Build */
			tChannel.Build_KeyFrames(pAnimationInfo->fDuration, pAnimationInfo->fTickPerSecond);

			/* Push */
			pAnimationInfo->vecChannels.push_back(std::move(tChannel));
		}
		pAnimationInfo->iNumChannels = (_int)pAnimationInfo->vecChannels.size();

		/* Amarture Route */
		auto iter = umNameCache.find(_strAmartureName);
		if (iter != umNameCache.end())
			pAnimationInfo->strAmartureRoute = iter->second;

		/* Check */
		auto [f, s] = umAnimationInfo.emplace(pAIAnimation->mName.C_Str(), pAnimationInfo);
		if (!s) {
			MSG_BOX("중복 이름 애니메이션이 있습니다.");
		}
	}

	Save_File(umAnimationInfo, strAnimationFilePath);

	for (auto& pair : umAnimationInfo) {
		Safe_Release(pair.second);
	}

	return S_OK;
}

HRESULT CAsset_Animation::Load_File(const fs::directory_entry& _fsEntry) {
	FILE* pFile = nullptr;
	if (fopen_s(&pFile, _fsEntry.path().string().c_str(), "rb") != 0 || !pFile) {
		m_eLoadState = LOADING_FAILED;
		return E_FAIL;
	}

	auto Fail_Load = [&]() -> HRESULT {
		Debug_Output("Animation load failed : ", _fsEntry.path().generic_string(), "\n");
		fclose(pFile);
		m_eLoadState = LOADING_FAILED;
		return E_FAIL;
		};

	_uint iVersion{};
	if (!READ(_uint, pFile, &iVersion))
		return Fail_Load();

	if (iVersion < 4 || iVersion > MAX_ANIMATION_FILE_VERSION)
		return Fail_Load();

	_uint iAnimationCnt{};
	if (!READ(_uint, pFile, &iAnimationCnt) || iAnimationCnt > MAX_ANIMATION_INFO_COUNT)
		return Fail_Load();

	for (_uint i = 0; i < iAnimationCnt; i++) {

		
		ANIMATION_INFO* pAnimationInfo = new ANIMATION_INFO;
		if (FAILED(pAnimationInfo->Load(iVersion, pFile))) {
			Safe_Release(pAnimationInfo);
			return Fail_Load();
		}

		auto [f, s] = m_umAnimationInfo.emplace(pAnimationInfo->strName, pAnimationInfo);
		if (s == false) {
			Safe_Release(pAnimationInfo);
		}
	}

	m_iGeneration++;

	fclose(pFile);

	m_eLoadState = LOADING_COMPLATE;

	return S_OK;
}

void CAsset_Animation::Free() {
	__super::Free();

	for (auto& pair : m_umAnimationInfo) {
		Safe_Release(pair.second);
	}

	Safe_Release(m_pMergeAnimationAsset);
}
