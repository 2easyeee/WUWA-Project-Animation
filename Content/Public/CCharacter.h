#pragma once

#include "Content_Define.h"
#include "CActor.h"
#include <CWeapon.h>

BEGIN(Engine)
class CSkinnedMeshRenderer;
END

BEGIN(Content)

class CONTENT_DLL CCharacter abstract : public CActor {
protected:
    struct CHARACTER_DESC {
        _float fHp;
        _float fMaxHp;
    };

protected:
    explicit CCharacter(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
    virtual ~CCharacter() = default;

public:
    virtual HRESULT Awake() override;
    virtual void Start() override;

    virtual void Update(_float _fTimeDelta) override;
    virtual void Late_Update(_float _fTimeDelta) override;

public:

    virtual void Register_Meta_Bone();

    void Turn(_float _fTimeDelta);
    void Auto_Turn(const _float3& _vTargetDirection, _float _fTurnSpeed, _float _fAutoTurnDuration);

protected:

    void Set_State(_ullong _iState);
    void Add_State(_ullong _iState);
    void Remove_State(_ullong _iState);

    REGISTER_FUNCTION(CCharacter, Set_Disslove, _float)
    REGISTER_FUNCTION(CCharacter, Set_Dithering, _float)
    REGISTER_FUNCTION(CCharacter, Set_Default_Renderer_Active, _int)

public:
    _bool Check_State_Turn_On(_ullong _iFlag) {
        return m_iState & _iFlag;
    }

    CObject* Get_HitCase() {
        if (auto pHitCase = m_pHitCase->Get()) {
            return pHitCase;
        }

        return this;
    }

    virtual _bool Assulted(CCharacter* _pAssulter, const ATTACK_DESC* _pAttackDesc) { return false; }

public:
    void UpdateQuestNavigationTarget();

    void OnMonsterKilled(MONSTER_TYPE type) { OnQuestNotify(QUEST_EVENT_TYPE::KILL_MONSTER, E2U(type), 1); }
    void OnNPCTalk(NPC_TYPE type)       { OnQuestNotify(QUEST_EVENT_TYPE::TALK_NPC  , E2U(type), 1); }
    void OnEnterArea(AREA_TYPE type)    { OnQuestNotify(QUEST_EVENT_TYPE::ENTER_AREA, E2U(type), 1); }
    void OnGetItem(AREA_TYPE type) { OnQuestNotify(QUEST_EVENT_TYPE::GET_ITEM, E2U(type), 1); }
    void OnPurification(PURIFICATION_TYPE type)      { OnQuestNotify(QUEST_EVENT_TYPE::PURIFICATION  , E2U(type), 1); }

private:
    void OnQuestNotify(QUEST_EVENT_TYPE eventType, _uint id, _uint count);

protected:
    _ullong m_iState{ 0 };

    _float3 m_vTargetDirection{ 0.f, 0.f, 1.f };
    _float m_fTurnSpeed{ 5.f };
    _float m_fAutoTurnDuration{ 0.f };

    WObject<CObject>* m_pHitCase{ nullptr }; // 명치에 있는 본
    WObject<CObject>* m_pMiddleCase{ nullptr };
    WObject<CObject>* m_pHandBone{ nullptr };
    WObject<CObject>* m_pBackHolder{ nullptr };

    CHARACTER_TYPE m_eCharacterType{ CHARACTER_TYPE::NONE };
protected:
    virtual void Free() override;
};

END
