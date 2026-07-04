#pragma once
#include <memory>
#include <variant>
#include "CImGuiObj.h"
#include "CAnimationInfo.h"
#include <CAnimationController.h>

class CImGuiAnimation : public CImGuiObj {
private:
	struct MOUSE_STATE
	{
		_float fX = { 0.f };
		_float fY = { 0.f };
	};

	struct INTERACTION_STATE
	{
		_bool bDragging = { false };
		_bool bDraggingStarted = { false }; // Dragging has moved past the start threshold.
		
		_bool bEventKeyDragging = { false };
		_bool bCustomAnimKeyDragging = { false };

		_float fDragOffset = { 0.f };
		_float fDragStartMouseX = { 0.f };

		_bool bResizing = { false };
		_bool bResizeLeft = { false };
	};

	struct TRACK
	{
		ANIMATION_INFO* pAnimInfo = { nullptr };
		_bool bAdditive = { false };
	};

	struct SELECTED_STATE
	{
		_int iTrack = { -1 };
		
		_int iEventKey = { -1 };

		_int iCustomAnimLayer = { -1 }; // ImGui 레이어 선택
		_int iCustomAnimIndex = { -1 };
		_int iCustomAnimKey = { -1 };
	};

public:
	explicit CImGuiAnimation(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);
	virtual ~CImGuiAnimation() = default;

public:
	HRESULT Initialize() override;
	int Update(_float _fTimeDelta) override;
	void Late_Update(_float _fTimeDelta) override;
	void Render() override;

private: /* Binding */
	CObject* m_pPrevSelected = { nullptr };
	CObject* m_pSelected = { nullptr };
	CAnimationController* m_pSelectedController = { nullptr };
	ANIMATION_INFO* m_pPrevSelectedInfo = { nullptr };
	ANIMATION_INFO* m_pSelectedInfo = { nullptr };
	std::weak_ptr<std::monostate> m_wpSelectedToken = {};
	std::weak_ptr<std::monostate> m_wpSelectedControllerToken = {};

private: /* Data */
	_float m_fTPS = { 60.f };
	_float m_fBaseDuration = { 5.f };
	_float m_fPrevTime = { 0.f };
	_float m_fCurrentTime = { 0.f };
	_float m_fDuration = { 0.f };

	/* Track */
	vector<TRACK> m_vecTracks;
	_int m_iCurChannel = { 0 };

	/* Event */
	_int m_iEventLayerCount = { 1 };

	/* Custom Animation (Event) */
	WFunction* m_pSelectedFunc = { nullptr };
	vector<_int> m_vecCustomAnimLayerIndices;
	_int m_iCustomAnimLayerCount = { 0 };

	/* Struct */
	SELECTED_STATE m_tSelected;

private: /* ImGUI */
	ImVec2 m_vCanvasPos = {};
	ImVec2 m_vCanvasSize = {};
	ImDrawList* m_pDrawList = { nullptr };

	ImVec2 m_vTimelineSize = {};
	_float m_fTimelineHeight = { 0.f };

	_float m_fTrackPositionOffsetX = { 0.f };
	_float m_fTrackPositionOffsetY = { 0.f };

	_float m_fTrackHeight = { 40.f };

	/* Strcut */
	MOUSE_STATE m_tMouseState;
	INTERACTION_STATE m_tInteractionState;

	_float KEY_HOVER_RADIUS = { 6.f };
	_float RESIZE_THRESHOLD = { 10.f };
	_float DRAG_THRESHOLD = { 3.f };

private:
	void Render_BaseFrame();
	void Render_TimeLine();
	void Render_TrackFrame(_int _iTrackIndex, TRACK& _tTrack);
	void Render_KeyFrames(_int _iTrackIndex, TRACK& _tTrack);
	void Render_EventTrackFrame();
	void Render_EventMarker();
	void Render_CustomAnimFrame();
	void Render_CustomAnimKeyFrames(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim);
	void Render_RedLine();
	void Pan();

	/* Track */
	void Render_Track();

	/* Event */
	void Render_SelectedEventInfo();

	/* CustomAnimation */
	void Render_SelectedCustomAnimInfo();

	/* Animation */
	void Render_RootMotion_SwitchTest(); // depre

private:
	void Update_MouseState();
	void Update_SelectedObject();
	void Sync_CurAnimation();
	void OnSelectedObject();
	_bool Is_SelectedBindingExpired() const;
	_bool Is_SelectedBindingAlive() const;
	void Clear_SelectedBinding();

	void Add_Track(struct ANIMATION_INFO* _pInfo);
	void Clear_Tracks();

	/* Custom Animation */
	void Build_CustomAnimLayers();
	void Create_CustomAnimTrack(_int _iLayerTrack);
	void Add_CustomAnimLayer();
	void Delete_CustomAnimTrack();
	void Handle_CustomAnimTrack(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bHover);
	void Clear_CustomAnimTracks();

	/* Event */
	void Handle_Event();
	void Delete_Event();
	void Build_Event();

private:
	_float MouseToTime(_float _fMouseX);
	_float MouseToTimeUnsnapped(_float _fMouseX);
	ImVec2 Get_TrackPos(_int _iIndex);
	_float TimeToScreenX(_float _fTime);
	_bool Is_MouseInTrack();
	_bool Is_MouseInTrack(_int _iIndex);
	_bool Is_HoverKey(_float _fMouseX, _float _fMouseY, _float _fKeyX, _float _fKeyY);

	/* Custom Animation */
	_int Get_MouseLayerIndex();

	/* Event */
	void Sort_FrameKeys(CUSTOM_ANIM& _tCustomAnim);
	void Find_LastAddedKey(CUSTOM_ANIM& _tCustomAnim, const CUSTOM_KEY_FRAME& _tNewKey);
	_float Clamp_KeyTrackPosition(const CUSTOM_ANIM& _tCustomAnim, _float _fTime);

private: /* refactor */
	void Draw_Rect(ImVec2 _Min, ImVec2 _Max, ImU32 _Color);
	void Draw_Text(ImVec2 _Pos, const _char* _Text, ImU32 _Color);
	
	_bool Is_HoverRect(ImVec2 _Min, ImVec2 _Max);

	void Reset_Drag();
	void Begin_Drag(_float _fTime);
	void End_Drag();

	_bool Is_SelectedCustomAnim(const CUSTOM_ANIM& _tCustomAnim);
	_int Find_CustomAnimIndex(const CUSTOM_ANIM& _tCustomAnim);

	void Create_Track(struct ANIMATION_INFO* _pInfo, _bool _bAdditive);

	void Select_CustomAnim(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bResizeLeft, _bool _bResize);
	void Handle_CustomAnim(CUSTOM_ANIM& _tCustomAnim, _float _fMinEventLength);
	void Popup_CustomAnim(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bHover);

private:
	WFunction* Popup_Selected(const _char* _pButtonLabel, const _char* _pPopupName);
	void Default_FunctionType(CUSTOM_KEY_FRAME& _tKF, FUNCTION_PROPERTY_TYPE _eType, _bool _bIsEndKey);
	_bool Input_FunctionType(const _char* _pLabel, FUNCTION_PROPERTY_TYPE _eType, CUSTOM_KEY_FRAME& _tKF);
	
	void Apply_FuncToEvent(EVENT_KEY& _tEvent, WFunction* _pFunc);
	void Apply_FuncToCustomAnim(CUSTOM_ANIM& _tCustomAnim, WFunction* _pFunc);

public:
	static CImGuiAnimation* Create(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext);

private:
	virtual void Free() override;
};
