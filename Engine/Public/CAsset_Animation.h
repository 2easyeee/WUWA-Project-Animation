#pragma once

#include "CAsset.h"
#include "CFunctionRegistry.h"
#include "CGameInstance.h"
#include <WFunction.h>
#include "CAsset_Animation.h"
#include <CAsset_Anim_Yaml.h>
#include <WAsset.h>

BEGIN(Engine)

class ENGINE_DLL CAsset_Animation final : public CAsset {
private:
    explicit CAsset_Animation(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CAsset_Animation() = default;

public:
    HRESULT Initialize();

    struct ANIMATION_INFO* Create_New_Animation();

    struct ANIMATION_INFO* Copy_Animation(struct ANIMATION_INFO* _pSourceAnimation);

    void Change_Animation_Name(struct ANIMATION_INFO* _pAnimationInfo, const _string& _strName);

    void Remove_Animation(struct ANIMATION_INFO* _pAnimationInfo);

    const unordered_map<_string, struct ANIMATION_INFO*>& Get_AnimationSequencies() {
        return m_umAnimationInfo;
    }

    struct ANIMATION_INFO* Get_AnimationInfo(const _string& _strName) {
        auto iter = m_umAnimationInfo.find(_strName);
        if (iter == m_umAnimationInfo.end())
            return nullptr;

        return iter->second;
    }

    _uint Get_Generation() const {
        return m_iGeneration;
    }

    void Add_Generation() {
        m_iGeneration++;
    }

private:
    unordered_map<_string, struct ANIMATION_INFO*> m_umAnimationInfo{};
    vector<WAsset<CAsset_Anim_Yaml>*> m_vecYaml{};

    WAsset<CAsset_Animation>* m_pMergeAnimationAsset{ nullptr };

    _uint m_iGeneration{ 0 };

public:
    void OnGui_Inspector_Context();

    void Merge_Asset(CAsset_Animation* _pAsset);

    void Generate_Animation_From_Yaml(const _string& _strModelName);

    static CAsset_Animation* Create(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext, const fs::directory_entry& _fsEntry);
    static HRESULT Create_File(const fs::directory_entry& _fsEntry);

    ANIMATION_INFO* OnGui_Animator_Inspector_Context(const char* _szName);

    HRESULT Save_File();
    static HRESULT Save_File(unordered_map<_string, struct ANIMATION_INFO*>& _umAnimationInfo, const fs::path& _fsPath);

    static HRESULT Save_File_Binary(const fs::path& _fsPath, const vector<_byte>& _vecData);
    static HRESULT Save_File_From_FBX(const fs::path& _fsPath, const aiScene* _pAIScene, const aiNode* _pRootNode, const _string& _strAmarture);

private:
    HRESULT Load_File(const fs::directory_entry& _fsEntry) override;

    virtual void Free() override;
};

END