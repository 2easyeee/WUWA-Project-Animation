#include "CommonPCH.h"
#include "CImGuiAnimation.h"
#include <CImGuiMgr.h>
#include <CFunctionRegistry.h>
#include "CComponent.h"
#include "CTransform.h"
#include "CObject.h"
#include "CSkinnedMeshRenderer.h"

CImGuiAnimation::CImGuiAnimation(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext)
	: CImGuiObj(_pDevice, _pContext) {
}

HRESULT CImGuiAnimation::Initialize() {

	return S_OK;
}

int CImGuiAnimation::Update(_float _fTimeDelta) {
	/* Selected Object */
	Update_SelectedObject();

	/* Animation */
	Sync_CurAnimation();

	return 0;
}

void CImGuiAnimation::Late_Update(_float _fTimeDelta) {

}

const char* szChooseFunction = "CFunctionRegistry::ImGui_Choose_Function";

_bool CImGuiAnimation::Is_SelectedBindingExpired() const
{
	if (m_pSelected && m_wpSelectedToken.expired())
		return true;

	if (m_pSelectedController && m_wpSelectedControllerToken.expired())
		return true;

	return false;
}

_bool CImGuiAnimation::Is_SelectedBindingAlive() const
{
	if (!m_pSelected || !m_pSelectedController)
		return false;

	if (Is_SelectedBindingExpired())
		return false;

	return true;
}

void CImGuiAnimation::Clear_SelectedBinding()
{
	m_pPrevSelected = nullptr;
	m_pSelected = nullptr;
	m_pSelectedController = nullptr;
	m_pPrevSelectedInfo = nullptr;
	m_pSelectedInfo = nullptr;
	m_wpSelectedToken.reset();
	m_wpSelectedControllerToken.reset();

	Clear_Tracks();
	Clear_CustomAnimTracks();

	m_pSelectedFunc = nullptr;
	m_tSelected = {};
	m_iCurChannel = 0;
	m_fDuration = 0.f;
	m_fCurrentTime = 0.f;
}

void CImGuiAnimation::Render() {
	Update_MouseState();

	if (!ImGui::Begin("Animation"))
	{
		ImGui::End();
		return;
	}
	ImGui::PushID(this);

	if (Is_SelectedBindingExpired())
	{
		Clear_SelectedBinding();
		ImGui::PopID();
		ImGui::End();
		return;
	}

	if (!Is_SelectedBindingAlive() || !m_pSelectedInfo)
	{
		ImGui::PopID();
		ImGui::End();
		return;
	}

	Render_BaseFrame();
	Render_TimeLine();
	Pan();

	/* Track */
	for (_int i = 0; i < m_vecTracks.size(); i++)
	{
		auto& track = m_vecTracks[i];

		Render_TrackFrame(i, track);
		Render_KeyFrames(i, track);
	}

	/* Event */
	Render_EventTrackFrame();
	Render_EventMarker();
	Handle_Event();

	/* Custom Anim */
	Render_CustomAnimFrame();
	for (_int i = 0; i < m_pSelectedInfo->vecCustomAnims.size(); i++)
	{
		auto& customAnim = m_pSelectedInfo->vecCustomAnims[i];
		_int iTrackIndex = m_vecTracks.size() + m_iEventLayerCount + customAnim.iEventLayerIndex;
		Render_CustomAnimKeyFrames(iTrackIndex, customAnim);
	}
	Delete_CustomAnimTrack();

	Render_RedLine();

	ImGui::PopID();
	ImGui::End();

	Render_SelectedEventInfo();
	Render_SelectedCustomAnimInfo();
	Render_Track();

	/* Debug */
	//Render_RootMotion_SwitchTest();
}

void CImGuiAnimation::Render_BaseFrame()
{
	m_vCanvasPos = ImGui::GetCursorScreenPos();

	_float fWidth = ImGui::GetContentRegionAvail().x;
	_float fHeight = ImGui::GetWindowHeight() * 0.9f;
	fHeight = max(fHeight, 60.f);

	m_vCanvasSize = ImVec2(fWidth, fHeight);

	m_pDrawList = ImGui::GetWindowDrawList();

	/* TimeLine Background (Black) */
	Draw_Rect(m_vCanvasPos, { m_vCanvasPos.x + m_vCanvasSize.x, m_vCanvasPos.y + m_vCanvasSize.y }, IM_COL32(20, 20, 20, 255));
}

void CImGuiAnimation::Render_TimeLine()
{
	if (!m_pDrawList)
		return;

	_float fWidth = ImGui::GetContentRegionAvail().x;
	_float fHeight = ImGui::GetWindowHeight() * 0.1f;
	fHeight = max(fHeight, 15.f);

	m_vTimelineSize = ImVec2(fWidth, fHeight);
	m_fTimelineHeight = fHeight; // For. TrackFrame

	/* TimeLine Background */
	Draw_Rect(m_vCanvasPos, ImVec2(m_vCanvasPos.x + m_vTimelineSize.x, m_vCanvasPos.y + m_vTimelineSize.y), IM_COL32(40, 40, 40, 255));

	/* Tick Line */
	_int iStartTick = m_fTrackPositionOffsetX * m_fTPS;
	_int iEndTick = (m_fTrackPositionOffsetX + m_fBaseDuration) * m_fTPS;

	for (_int i = iStartTick; i <= iEndTick; i++)
	{
		_float fTime = i / m_fTPS;
		_float fX = TimeToScreenX(fTime);

		if (fX < m_vCanvasPos.x || fX > m_vCanvasPos.x + m_vCanvasSize.x)
			continue;

		_bool bMajor = (i % (_int)m_fTPS == 0);
		_bool bMid = (i % 10 == 0);

		if (bMajor)
		{
			m_pDrawList->AddLine(
				ImVec2(fX, m_vCanvasPos.y),
				ImVec2(fX, m_vCanvasPos.y + 22.f),
				IM_COL32(225, 225, 225, 255),
				2.5f);

			_char buf[32];
			sprintf_s(buf, "%.1f", fTime);
			m_pDrawList->AddText(
				ImVec2(fX - 6.f, m_vCanvasPos.y + 24.f),
				IM_COL32(255, 255, 255, 255),
				buf);
		}
		else if (bMid)
		{
			m_pDrawList->AddLine(
				ImVec2(fX, m_vCanvasPos.y + m_vTimelineSize.y),
				ImVec2(fX, m_vCanvasPos.y + m_vTimelineSize.y - 12.f),
				IM_COL32(180, 180, 180, 255),
				1.5f);
		}
		else
		{
			m_pDrawList->AddLine(
				ImVec2(fX, m_vCanvasPos.y + m_vTimelineSize.y),
				ImVec2(fX, m_vCanvasPos.y + m_vTimelineSize.y - 6.f),
				IM_COL32(100, 100, 100, 180),
				1.f);
		}
	}

	/* Click/Drag */
	ImGui::SetCursorScreenPos(m_vCanvasPos);
	ImGui::InvisibleButton("Timeline", m_vTimelineSize);

	/* Mouse */
	_bool bHovered = ImGui::IsItemHovered();
	_bool bActive = ImGui::IsItemActive();
	_bool bInput = ImGui::IsMouseDown(0); // IsMouseDragging(0) + IsMouseDown(0)

	if (bHovered && bActive && bInput && m_tSelected.iCustomAnimLayer == -1)
	{
		_float fMouseX = ImGui::GetIO().MousePos.x;

		m_fCurrentTime = MouseToTime(fMouseX);

		/* Set Controller */
		if (Is_SelectedBindingAlive())
		{
			if (!m_pSelectedController->Is_EditorMode())
				m_pSelectedController->Set_EditorMode(true);

			m_pSelectedController->Set_Time_Editor(m_fCurrentTime);
		}
	}

	if (bHovered && ImGui::IsMouseDown(0))
	{
		m_tSelected.iCustomAnimLayer = -1;

		m_tInteractionState.bDragging = false;
		m_tInteractionState.bDraggingStarted = false;
		m_tInteractionState.bResizing = false;
	}
}

void CImGuiAnimation::Render_TrackFrame(_int _iTrackIndex, TRACK& _tTrack)
{
	if (!_tTrack.pAnimInfo)
		return;

	ImVec2 vTrackPos = Get_TrackPos(_iTrackIndex);

	/* Track Background */
	_float fTrackRatio = _tTrack.pAnimInfo->fDuration / m_fBaseDuration;
	_float fTrackWidth = m_vCanvasSize.x * fTrackRatio;
	
	ImU32 bgColor = _tTrack.bAdditive ? IM_COL32(255, 192, 203, 100) : IM_COL32(30, 30, 30, 255);
	Draw_Rect(vTrackPos, ImVec2(vTrackPos.x + fTrackWidth, vTrackPos.y + m_fTrackHeight), bgColor);

	/* Track Name */
	_string strLabel = _tTrack.pAnimInfo->strName;
	if (_tTrack.bAdditive)
		strLabel = "[Additive] " + strLabel;
	Draw_Text(ImVec2(vTrackPos.x + 5.f, vTrackPos.y + 10.f), strLabel.c_str(), IM_COL32(200, 200, 200, 255));
}

void CImGuiAnimation::Render_KeyFrames(_int _iTrackIndex, TRACK& _tTrack)
{
	if (!_tTrack.pAnimInfo)
		return;

	auto& channels = _tTrack.pAnimInfo->vecChannels;
	if (m_iCurChannel < 0 || m_iCurChannel >= channels.size())
		return;

	auto vTrackPos = Get_TrackPos(_iTrackIndex);
	_float fCenterY = vTrackPos.y + (m_fTrackHeight * 0.5f);

	for (auto& key : channels[m_iCurChannel].vecKeyFrames)
	{
		_float fX = TimeToScreenX(key.fTrackPosition);
		m_pDrawList->AddCircleFilled(
			ImVec2(fX, fCenterY),
			3.f,
			IM_COL32(255, 255, 255, 255));
	}
}

void CImGuiAnimation::Render_EventTrackFrame()
{
	_int iTrackIndex = (_int)m_vecTracks.size();
	ImVec2 vTrackPos = Get_TrackPos(iTrackIndex);

	/* bg */
	m_pDrawList->AddRectFilled(
		vTrackPos,
		ImVec2(vTrackPos.x + m_vCanvasSize.x, vTrackPos.y + m_fTrackHeight),
		IM_COL32(35, 35, 35, 255));
	m_pDrawList->AddText(
		ImVec2(vTrackPos.x + 5.f, vTrackPos.y + 10.f),
		IM_COL32(255, 255, 255, 255),
		"[Event]");
}

void CImGuiAnimation::Render_EventMarker()
{
	if (!m_pDrawList || !m_pSelectedInfo)
		return;

	_int iTrackIndex = m_vecTracks.size();
	ImVec2 vTrackPos = Get_TrackPos(iTrackIndex);
	_float fY = vTrackPos.y + m_fTrackHeight * 0.5f;

	for (_int i = 0; i < m_pSelectedInfo->vecEvents.size(); i++)
	{
		auto& event = m_pSelectedInfo->vecEvents[i];
		_float fX = TimeToScreenX(event.tKF.fTrackPosition);

		/* Color */
		_uint iHash = 0;
		for (char c : event.strEventName)
			iHash = iHash * 31 + static_cast<_uint>(c);

		if (event.strEventName.empty())
			iHash = (_uint)i + 1;

		/* Draw */
		ImVec2 p1 = { fX, fY - 6.f };
		ImVec2 p2 = { fX + 6.f, fY };
		ImVec2 p3 = { fX, fY + 6.f };
		ImVec2 p4 = { fX - 6.f, fY };

		ImU32 color = IM_COL32(
			80 + (iHash & 0x7F),
			80 + ((iHash >> 8) & 0x7F),
			80 + ((iHash >> 16) & 0x7F),
			255);

		if (i == m_tSelected.iEventKey)
			color = IM_COL32(255, 100, 150, 255);

		m_pDrawList->AddQuadFilled(p1, p2, p3, p4, color);
		
		/* ToopTip */
		_bool bHover = Is_HoverKey(m_tMouseState.fX, m_tMouseState.fY, fX, fY);
		if (bHover)
		{
			ImGui::BeginTooltip();

			ImGui::Text("Event: %s", event.strEventName.empty() ? "None" : event.strEventName.c_str());
			ImGui::Text("Time: %.3f", event.tKF.fTrackPosition);

			if (event.pFunction)
			{
				_string strChildPath = UnicodeToMultiByte(event.pFunction->Get_ChildPath());
				size_t iPos = strChildPath.rfind('/');
				_string strChildRoot = (iPos == _string::npos) ? strChildPath : strChildPath.substr(iPos + 1);

				ImGui::Text("Child Root: %s", strChildRoot.empty() ? "None" : strChildRoot.c_str());
			}

			ImGui::EndTooltip();
		}
	}
}

void CImGuiAnimation::Render_CustomAnimFrame()
{
	for (_int i = 0; i < m_vecCustomAnimLayerIndices.size(); i++)
	{
		_int iCustomAnimIndex = m_vecCustomAnimLayerIndices[i];
		_int iTrackIndex = m_vecTracks.size() + i + m_iEventLayerCount;
		ImVec2 vTrackPos = Get_TrackPos(iTrackIndex);

		/* Background */
		Draw_Rect(vTrackPos, ImVec2(vTrackPos.x + m_vCanvasSize.x, vTrackPos.y + m_fTrackHeight), IM_COL32(35, 35, 35, 255));

		if (iCustomAnimIndex < 0 || iCustomAnimIndex >= m_pSelectedInfo->vecCustomAnims.size())
			continue;

		auto& tCustomAnim = m_pSelectedInfo->vecCustomAnims[iCustomAnimIndex];

		_float fXStart = TimeToScreenX(tCustomAnim.fStartTime);
		_float fXEnd = TimeToScreenX(tCustomAnim.fEndTime);

		ImVec2 pMin = { fXStart, vTrackPos.y + 5.f };
		ImVec2 pMax = { fXEnd, vTrackPos.y + m_fTrackHeight - 5.f };

		ImU32 color = (m_tSelected.iCustomAnimIndex == iCustomAnimIndex) ? IM_COL32(255, 100, 100, 255) : IM_COL32(0, 255, 0, 255);
		Draw_Rect(pMin, pMax, color);

		/* Track Name */
		_string strLabel = "[CustomAnim] " + tCustomAnim.strCustomAnimName;
		Draw_Text(ImVec2(vTrackPos.x + 5.f, vTrackPos.y + 10.f), strLabel.c_str(), IM_COL32(200, 200, 200, 255));

		Handle_CustomAnimTrack(i, tCustomAnim, Is_HoverRect(pMin, pMax));
	}

	Add_CustomAnimLayer();

	/* Popup */
	if (Is_MouseInTrack() && ImGui::IsMouseClicked(1))
	{
		/* Layer 안에 track 이 있는지 검사 */
		_int iLayer = Get_MouseLayerIndex() - (m_vecTracks.size() + m_iEventLayerCount);
		if (iLayer >= 0 && iLayer < m_iCustomAnimLayerCount)
		{
			_int iCustomAnimIndex = m_vecCustomAnimLayerIndices[iLayer];
			if (iCustomAnimIndex == -1)
				ImGui::OpenPopup("AddCustomAnimPopup");
			else
				ImGui::OpenPopup("CustomAnimPopup");
		}	
	}

	if (ImGui::BeginPopup("AddCustomAnimPopup"))
	{
		_int iLayer = Get_MouseLayerIndex() - (m_vecTracks.size() + m_iEventLayerCount);
		if (iLayer >= 0)
		{
			if (ImGui::MenuItem("Add CustomAnim Track"))
				Create_CustomAnimTrack(iLayer);

			if (ImGui::MenuItem("Delete Layer"))
			{
				if (iLayer >= 0
					&& iLayer < m_iCustomAnimLayerCount
					&& iLayer < m_vecCustomAnimLayerIndices.size()
					&& m_vecCustomAnimLayerIndices[iLayer] == -1)
				{
					m_iCustomAnimLayerCount--;
					m_vecCustomAnimLayerIndices.erase(m_vecCustomAnimLayerIndices.begin() + iLayer);
				}
			}
		}
		ImGui::EndPopup();
	}
}

void CImGuiAnimation::Render_CustomAnimKeyFrames(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim)
{
	if (!m_pDrawList)
		return;

	ImVec2 vTrackPos = Get_TrackPos(_iLayerIndex);
	_float fY = vTrackPos.y + m_fTrackHeight * 0.5f;

	_bool bKeyHover = false;

	for (_int i = 0; i < _tCustomAnim.vecKF.size(); i++)
	{
		auto& tKF = _tCustomAnim.vecKF[i];

		_float fTime = _tCustomAnim.fStartTime + tKF.fTrackPosition;
		_float fX = TimeToScreenX(fTime);
		_bool bHover = Is_HoverKey(m_tMouseState.fX, m_tMouseState.fY, fX, fY);

		if (bKeyHover)
			bHover = false;

		if (bHover)
			bKeyHover = true;

		if (bHover && ImGui::IsMouseClicked(0))
		{
			m_tSelected.iCustomAnimKey = i;
			m_tInteractionState.bCustomAnimKeyDragging = true;
			m_tInteractionState.fDragOffset = MouseToTimeUnsnapped(m_tMouseState.fX) - fTime;
		}

		if (bHover && ImGui::IsMouseClicked(1))
		{
			m_tSelected.iCustomAnimKey = i;
			ImGui::OpenPopup("CustomAnimPopup");
		}

		ImU32 color = (i == m_tSelected.iCustomAnimKey) ? IM_COL32(255, 200, 0, 255) : (bHover ? IM_COL32(0, 100, 255, 255) : IM_COL32(0, 0, 255, 255));
		m_pDrawList->AddCircleFilled(ImVec2(fX, fY), 5.f, color);
	}
	
	/* Drag */
	if (m_tInteractionState.bCustomAnimKeyDragging && (m_tSelected.iCustomAnimIndex == Find_CustomAnimIndex(_tCustomAnim)))
	{
		if (ImGui::IsMouseDown(0))
		{
			_float fTime = MouseToTimeUnsnapped(m_tMouseState.fX);
			_float fTimeAnim = fTime - _tCustomAnim.fStartTime - m_tInteractionState.fDragOffset;
			_tCustomAnim.vecKF[m_tSelected.iCustomAnimKey].fTrackPosition = Clamp_KeyTrackPosition(_tCustomAnim, fTimeAnim);
		}
		else
		{
			m_tInteractionState.bCustomAnimKeyDragging = false;
			m_tSelected.iCustomAnimIndex = -1;
			Sort_FrameKeys(_tCustomAnim);
		}
	}
	/* Add KF */
	if (ImGui::IsMouseDoubleClicked(0) && !bKeyHover)
	{
		if (_tCustomAnim.pFunction && Is_MouseInTrack(_iLayerIndex))
		{
			_float fTime = MouseToTime(m_tMouseState.fX);
			_float fTimeInCustomAnim = fTime - _tCustomAnim.fStartTime;
			_float fLength = _tCustomAnim.fEndTime - _tCustomAnim.fStartTime;
			fTimeInCustomAnim = max(0.f, min(fTimeInCustomAnim, fLength));

			CUSTOM_KEY_FRAME tNewKey = {};
			tNewKey.fTrackPosition = fTimeInCustomAnim;
			Default_FunctionType(tNewKey, _tCustomAnim.pFunction->Get_FunctionDesc()->tType, false);
			_tCustomAnim.vecKF.push_back(tNewKey);

			Sort_FrameKeys(_tCustomAnim);
		}
	}
}

void CImGuiAnimation::Render_RedLine()
{
	/* Snap */
	_float fTick = m_fCurrentTime * m_fTPS;
	fTick = roundf(fTick);

	_float fSnappedTime = fTick / m_fTPS;

	/* Red Line */
	_float t = (m_fBaseDuration > 0.f) ? fSnappedTime / m_fBaseDuration : 0.f;
	_float fLineX = TimeToScreenX(fSnappedTime);

	m_pDrawList->AddLine(
		ImVec2(fLineX, m_vCanvasPos.y),
		ImVec2(fLineX, m_vCanvasPos.y + m_vCanvasSize.y),
		IM_COL32(255, 0, 0, 255),
		3.f);
}

void CImGuiAnimation::Pan()
{
	/* Pan */
	auto& io = ImGui::GetIO();

	if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
		return;

	if (!ImGui::IsMouseDragging(2))
		return;

	/* X */
	_float fTimePerPixel = m_fBaseDuration / m_vCanvasSize.x;
	m_fTrackPositionOffsetX -= io.MouseDelta.x * fTimePerPixel;

	/* Y */
	m_fTrackPositionOffsetY -= io.MouseDelta.y;

	/* Clamp X */
	m_fTrackPositionOffsetX = max(0.f, m_fTrackPositionOffsetX);

	/* Clamp Y */
	_float fTotalHeight = (m_vecTracks.size() + m_iEventLayerCount + m_vecCustomAnimLayerIndices.size()) * m_fTrackHeight;
	_float fVisibleHeight = m_vCanvasSize.y - m_fTimelineHeight;

	_float fMaxOffsetY = max(0.f, fTotalHeight - fVisibleHeight);
	m_fTrackPositionOffsetY = max(0.f, min(m_fTrackPositionOffsetY, fMaxOffsetY));
}

void CImGuiAnimation::Render_SelectedCustomAnimInfo()
{
	ImGui::Begin("Event Info");

	if (m_tSelected.iTrack < 0 || m_tSelected.iTrack >= m_vecTracks.size())
	{
		ImGui::Text("No Track");
		ImGui::End();
		return;
	}

	if (m_tSelected.iCustomAnimLayer < 0 ||
		m_tSelected.iCustomAnimIndex < 0 ||
		m_tSelected.iCustomAnimIndex >= m_pSelectedInfo->vecCustomAnims.size())
	{
		//ImGui::Text("No Custom Animation");
		ImGui::End();
		return;
	}

	auto& track = m_vecTracks[m_tSelected.iTrack];
	auto& customAnim = m_pSelectedInfo->vecCustomAnims[m_tSelected.iCustomAnimIndex];
	ImGui::SeparatorText("Event Function");
	if (ImGui::Button("Event Function !"))
	{
		ImGui::OpenPopup("Select Function");
	}
	if (ImGui::BeginPopup("Select Function"))
	{
		CObject* pRoot = m_pSelected;
		CObject* pOwner = m_pSelected;

		WFunction* pFunc = CFunctionRegistry::ImGui_Choose_Function(pRoot, &pOwner);
		if (pFunc)
		{
			m_pSelectedFunc = pFunc;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (m_pSelectedFunc)
	{
		Apply_FuncToCustomAnim(customAnim, m_pSelectedFunc);
		m_pSelectedFunc = nullptr;
	}

	ImGui::SeparatorText("Track Info");
	if (customAnim.pFunction)
	{
		ImGui::Text("Func: %s", customAnim.strCustomAnimName.c_str());
		ImGui::Text("Start: %.3f", customAnim.fStartTime);
		ImGui::Text("End: %.3f", customAnim.fEndTime);
		ImGui::Text("Duration Ratio (Start): %.2f", customAnim.fStartTime / track.pAnimInfo->fDuration);
	}
	else
	{
		ImGui::Text("Func: None");
	}

	ImGui::SeparatorText("KeyFrames");
	if (ImGui::BeginTable("KeyFrameTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthStretch, 0.1f);
		ImGui::TableSetupColumn("Global", ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableSetupColumn("Ratio", ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableSetupColumn("Local", ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 2.f);
		ImGui::TableHeadersRow();

		_float fStart = customAnim.fStartTime;
		_float fEnd = customAnim.fEndTime;
		_float fLength = fEnd - fStart;

		for (_int i = 0; i < customAnim.vecKF.size(); i++)
		{
			auto& kf = customAnim.vecKF[i];

			_float fGlobalTime = fStart + kf.fTrackPosition;
			_float fRatio = (fLength > 0.f) ? (kf.fTrackPosition / fLength) : 0.f;

			ImGui::TableNextRow();

			/* Index */
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%d", i);

			/* BaseTime (Global) */
			ImGui::TableSetColumnIndex(1);
			ImGui::PushID(i);
			if (ImGui::DragFloat("##Global", &fGlobalTime, 0.01f, fStart, fEnd))
			{
				kf.fTrackPosition = fGlobalTime - fStart;
				kf.fTrackPosition = Clamp_KeyTrackPosition(customAnim, kf.fTrackPosition);
			}

			/* Ratio (Local) */
			ImGui::TableSetColumnIndex(2);
			ImGui::Text("%.2f", fRatio);

			/* Time (Local) */
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%.3f", kf.fTrackPosition);

			/* Parameter */
			ImGui::TableSetColumnIndex(4);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (customAnim.pFunction)
				Input_FunctionType("##Value", customAnim.pFunction->Get_FunctionDesc()->tType, kf);

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (ImGui::Button("(+) Extra KeyFrame")) {
		CUSTOM_KEY_FRAME tNewKey = {};
		tNewKey.fTrackPosition = Clamp_KeyTrackPosition(customAnim, (m_fCurrentTime - customAnim.fStartTime));
		if (customAnim.pFunction)
			Default_FunctionType(tNewKey, customAnim.pFunction->Get_FunctionDesc()->tType, false);
		customAnim.vecKF.push_back(tNewKey);

		Sort_FrameKeys(customAnim);
		Find_LastAddedKey(customAnim, tNewKey);
	}

	ImGui::End();
}

void CImGuiAnimation::Render_SelectedEventInfo()
{
	ImGui::Begin("Event Info");

	if (m_tSelected.iTrack < 0 || m_tSelected.iTrack >= m_vecTracks.size())
	{
		ImGui::Text("No Track");
		ImGui::End();
		return;
	}

	if (m_tSelected.iEventKey < 0 || m_tSelected.iEventKey >= m_pSelectedInfo->vecEvents.size())
	{
		//ImGui::Text("No Event");
		ImGui::End();
		return;
	}

	ImGui::SeparatorText("Event Function");
	auto& event = m_pSelectedInfo->vecEvents[m_tSelected.iEventKey];
	if (ImGui::Button("Event Function !"))
	{
		ImGui::OpenPopup("Select Function");
	}
	if (ImGui::BeginPopup("Select Function"))
	{
		CObject* pRoot = m_pSelected;
		CObject* pOwner = m_pSelected;

		WFunction* pFunc = CFunctionRegistry::ImGui_Choose_Function(pRoot, &pOwner);

		if (pFunc)
		{
			event.strEventName = pFunc->Get_FunctionDesc()->strName;
			event.pFunction = pFunc;
			event.Touch_ModifiedTime();
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	{
		ImGui::SeparatorText("Event Info");
		if (event.pFunction)
		{
			ImGui::Text("Func: %s", event.strEventName.c_str());

			_string strTemp = UnicodeToMultiByte(event.pFunction->Get_ChildPath());
			ImGui::Text("Child Root: %s", strTemp.c_str());

			size_t iPos = strTemp.rfind('/');
			_string strChildRoot = (iPos == _string::npos) ? strTemp : strTemp.substr(iPos + 1);
			ImGui::Text("Child Root: % s", strChildRoot.c_str());
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();

				if (iPos != _string::npos)
				{
					_string strPrefix = strTemp.substr(0, iPos + 1);
					ImGui::TextUnformatted(strPrefix.c_str());
					ImGui::SameLine(0.f, 0.f);
					ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "%s", strChildRoot.c_str());
				}
				else
				{
					ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "%s", strChildRoot.c_str());
				}
				ImGui::EndTooltip();
			}
			
			_float fTrackPosition = event.tKF.fTrackPosition;
			ImGui::Text("Track Position(Time)");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.f);
			if (ImGui::DragFloat("##EventTrackPosition", &fTrackPosition, 0.01f, 0.f, m_fDuration))
			{
				event.tKF.fTrackPosition = max(0.f, min(fTrackPosition, m_fDuration));
				event.Touch_ModifiedTime();
			}
		}
		else
		{
			ImGui::Text("Func: None");

			_float fTrackPosition = event.tKF.fTrackPosition;
			ImGui::Text("Track Position(Time)");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.f);
			if (ImGui::DragFloat("##EventTrackPosition", &fTrackPosition, 0.01f, 0.f, m_fDuration))
			{
				event.tKF.fTrackPosition = max(0.f, min(fTrackPosition, m_fDuration));
				event.Touch_ModifiedTime();
			}
		}

		ImGui::SeparatorText("Parameter");
		if (event.pFunction)
		{
			if (event.pFunction->Get_FunctionDesc() == nullptr)
				ImGui::Text("Func: Deleted in Code");
			else
			{
				if (Input_FunctionType("Value", event.pFunction->Get_FunctionDesc()->tType, event.tKF))
					event.Touch_ModifiedTime();
			}
		}
	}
	ImGui::End();
}

void CImGuiAnimation::Render_Track()
{
	ImGui::Begin("Track");

	if (!Is_SelectedBindingAlive() ||
		m_tSelected.iTrack < 0 ||
		m_tSelected.iTrack >= (_int)m_vecTracks.size() ||
		m_vecTracks.empty())
	{
		ImGui::Text("No Track Selected");
		ImGui::End();
		return;
	}

	auto& track = m_vecTracks[m_tSelected.iTrack];
	if (!track.pAnimInfo)
	{
		ImGui::Text("No Track Selected");
		ImGui::End();
		return;
	}

	_int iMaxChannel = (_int)track.pAnimInfo->vecChannels.size() - 1;
	m_iCurChannel = max(0, min(m_iCurChannel, iMaxChannel));

	ImGui::SeparatorText("Animation Info");
	ImGui::Text("Animation Name: %s", track.pAnimInfo->strName.c_str());
	if (m_pSelectedController)
	{
		auto pAsset = m_pSelectedController->Get_Asset_Animation();
		if (pAsset && ImGui::Button("Copy##CImGuiAnimation"))
		{
			auto pCopiedAnimation = pAsset->Copy_Animation(track.pAnimInfo);
			if (pCopiedAnimation)
			{
				m_pSelectedController->Change_Animation_Editor(pCopiedAnimation);
				m_pPrevSelectedInfo = nullptr;
				Sync_CurAnimation();
				ImGui::End();
				return;
			}
		}
	}
	ImGui::Text("Animation Duration: %.3f", track.pAnimInfo->fDuration);
	ImGui::Text("Redline Time: %.3f", m_fCurrentTime);
	ImGui::Text("Redline Ratio: %.3f", track.pAnimInfo->fDuration > 0.f ? m_fCurrentTime / track.pAnimInfo->fDuration : 0.f);

	ImGui::SeparatorText("Track Info");
	if (iMaxChannel < 0)
	{
		ImGui::Text("No Channels");
	}
	else
	{
		auto& strBoneFullPath = track.pAnimInfo->vecChannels[m_iCurChannel].strBoneRoute;
		size_t pos = strBoneFullPath.rfind('/');
		_string strBone = (pos == _string::npos) ? strBoneFullPath : strBoneFullPath.substr(pos + 1);

		ImGui::Text("Bone: %s", strBone.c_str());
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			if (pos != _string::npos)
			{
				_string strPrefix = strBoneFullPath.substr(0, pos + 1);
				ImGui::TextUnformatted(strPrefix.c_str());
				ImGui::SameLine(0.f, 0.f);
				ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "%s", strBone.c_str());
			}
			else
			{
				ImGui::TextColored(ImVec4(0.3f, 1.f, 0.3f, 1.f), "%s", strBone.c_str());
			}
			ImGui::EndTooltip();
		}

		/* Channel Bone Slider */
		if (iMaxChannel >= 0)
		{
			ImGui::SetNextItemWidth(80.f);
			ImGui::SliderInt("Channel", &m_iCurChannel, 0, iMaxChannel);
		}
		ImGui::SameLine();
		if (ImGui::Button("<"))
			m_iCurChannel = max(0, m_iCurChannel - 1);
		ImGui::SameLine();
		if (ImGui::Button(">"))
			m_iCurChannel = min(iMaxChannel, m_iCurChannel + 1);
	}

	/* Editor false */
	ImGui::SeparatorText("Playback");
	_bool bEditorMode = m_pSelectedController->Is_EditorMode();
	ImVec4 btnColor = bEditorMode ? ImVec4(0.2f, 0.8f, 0.2f, 1.f) : ImVec4(0.8f, 0.2f, 0.2f, 1.f);
	ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
	if (ImGui::Button(bEditorMode ? "Play" : "Stop", ImVec2(-1, 25))
		|| ImGui::IsKeyPressed(ImGuiKey_I))
		m_pSelectedController->Set_EditorMode(!bEditorMode);
	ImGui::PopStyleColor();
	ImGui::Text("State: %s", bEditorMode ? "Editor" : "Runtime");

	ImGui::End();
}

void CImGuiAnimation::Update_MouseState()
{
	if (!ImGui::IsMouseDown(0))
	{
		 End_Drag();
	}

	auto& io = ImGui::GetIO();
	m_tMouseState.fX = io.MousePos.x;
	m_tMouseState.fY = io.MousePos.y;

	/* Zoom */
	if (io.KeyCtrl && io.MouseWheel != 0.f)
	{
		_float fZoomSpeed = 0.2f;

		m_fBaseDuration -= io.MouseWheel * fZoomSpeed * m_fBaseDuration;
		m_fBaseDuration = max(0.1f, m_fBaseDuration);
		m_fBaseDuration = min(100.f, m_fBaseDuration);
	}
}

void CImGuiAnimation::Update_SelectedObject()
{
	auto pImGuiMgr = CImGuiMgr::GetInstance();
	CObject* pCurSelected = pImGuiMgr->GetSelected();

	const _bool bSelectedExpired = m_pSelected && m_wpSelectedToken.expired();
	const _bool bControllerExpired = m_pSelectedController && m_wpSelectedControllerToken.expired();
	if (bSelectedExpired || bControllerExpired)
	{
		CObject* pExpiredSelected = m_pSelected;
		Clear_SelectedBinding();
		if (bSelectedExpired && pCurSelected == pExpiredSelected)
		{
			pImGuiMgr->Clear();
			return;
		}
	}

	if (m_pPrevSelected != pCurSelected)
	{
		if (!pCurSelected)
		{
			Clear_SelectedBinding();
			return;
		}

		OnSelectedObject();
		return;
	}

	if (!m_pSelectedController && pCurSelected)
	{
		OnSelectedObject();
	}
}

void CImGuiAnimation::Sync_CurAnimation()
{
	if (Is_SelectedBindingExpired())
	{
		Clear_SelectedBinding();
		return;
	}

	if (!m_pSelectedController)
		return;

	if (!Is_SelectedBindingAlive())
		return;

	/* Sync CurAnimation */
	auto pCurAnimation = m_pSelectedController->Get_CurAnimation();
	if (pCurAnimation && (pCurAnimation != m_pPrevSelectedInfo || m_vecTracks.empty()))
	{
		Clear_CustomAnimTracks();

		Add_Track(pCurAnimation);

		/* Build Events */
		Build_Event();
		Build_CustomAnimLayers();

		m_tSelected.iTrack = 0;
		m_tSelected.iCustomAnimLayer = -1;
		m_tSelected.iEventKey = -1;

		m_pPrevSelectedInfo = pCurAnimation;
	}

	/* Sync Runtim <-> Timeline */
	if (!m_pSelectedController->Is_EditorMode())
		m_fCurrentTime = m_pSelectedController->Get_Time();
}

void CImGuiAnimation::Build_CustomAnimLayers()
{
	m_vecCustomAnimLayerIndices.clear();
	if (!m_pSelectedInfo)
	{
		m_iCustomAnimLayerCount = 0;
		return;
	}

	auto& vecCustomAnims = m_pSelectedInfo->vecCustomAnims;
	_int iMaxLayer = -1;
	for (auto& event : vecCustomAnims)
	{
		iMaxLayer = max(iMaxLayer, event.iEventLayerIndex);
	}

	// Keep existing empty layers by taking the larger of current count and max layer.
	m_iCustomAnimLayerCount = max(m_iCustomAnimLayerCount, iMaxLayer + 1);
	m_vecCustomAnimLayerIndices.resize(m_iCustomAnimLayerCount, -1);

	for (_int i = 0; i < vecCustomAnims.size(); i++)
	{
		_int iLayer = max(0, vecCustomAnims[i].iEventLayerIndex);
		if (iLayer < m_iCustomAnimLayerCount)
			m_vecCustomAnimLayerIndices[iLayer] = i;
	}
}

void CImGuiAnimation::Create_CustomAnimTrack(_int _iLayerTrack)
{
	/* Layer 확장 */
	if (_iLayerTrack >= m_iCustomAnimLayerCount)
	{
		m_iCustomAnimLayerCount = _iLayerTrack + 1;
		m_vecCustomAnimLayerIndices.resize(m_iCustomAnimLayerCount, -1);
	}

	if (-1 != m_vecCustomAnimLayerIndices[_iLayerTrack])
		return;

	_float fTime = MouseToTime(m_tMouseState.fX);

	CUSTOM_ANIM tNewCustomAnim = {};
	tNewCustomAnim.iEventLayerIndex = _iLayerTrack;
	tNewCustomAnim.fStartTime = max(0.f, min(fTime, m_fDuration));
	tNewCustomAnim.fEndTime = min(fTime + 0.2f, m_fDuration);
	m_pSelectedInfo->vecCustomAnims.push_back(tNewCustomAnim);

	Build_CustomAnimLayers();
	m_tSelected.iCustomAnimLayer = _iLayerTrack;
	m_tSelected.iCustomAnimIndex = m_pSelectedInfo->vecCustomAnims.size() - 1;
	m_tSelected.iCustomAnimKey = -1;
}

void CImGuiAnimation::Add_CustomAnimLayer()
{
	_int iLayer = (_int)m_vecCustomAnimLayerIndices.size();
	_int iTrackIndex = (_int)m_vecTracks.size() + iLayer + m_iEventLayerCount;

	ImVec2 vTrackPos = Get_TrackPos(iTrackIndex);

	ImVec2 pMin = { vTrackPos.x, vTrackPos.y };
	ImVec2 pMax = { vTrackPos.x + m_vCanvasSize.x, vTrackPos.y + m_fTrackHeight };

	_bool bHover = Is_HoverRect(pMin, pMax);
	ImU32 color = bHover ? IM_COL32(80, 80, 80, 255) : IM_COL32(50, 50, 50, 255);
	Draw_Rect(pMin, pMax, color);
	Draw_Text(ImVec2((pMin.x + pMax.x) * 0.5f - 10.f, pMin.y + 10.f), "+", IM_COL32(200, 200, 200, 255));

	if (bHover && ImGui::IsMouseClicked(0))
	{
		m_iCustomAnimLayerCount++;
		m_vecCustomAnimLayerIndices.resize(m_iCustomAnimLayerCount, -1);
	}
}

void CImGuiAnimation::OnSelectedObject()
{
	Clear_SelectedBinding();

	/* Binding Controller */
	m_pSelected = CImGuiMgr::GetInstance()->GetSelected();
	m_pPrevSelected = m_pSelected;
	if (!m_pSelected)
		return;

	m_wpSelectedToken = m_pSelected->Get_Token();
	if (m_wpSelectedToken.expired())
	{
		Clear_SelectedBinding();
		return;
	}

	m_pSelectedController = m_pSelected->GetComponent<CAnimationController>();
	if (!m_pSelectedController)
		return;

	m_wpSelectedControllerToken = m_pSelectedController->Get_Token();
	if (m_wpSelectedControllerToken.expired())
	{
		Clear_SelectedBinding();
		return;
	}

	m_pSelectedInfo = m_pSelectedController->Get_CurAnimation();
	if (!m_pSelectedInfo)
		return;

	/* Info */
	m_fDuration = m_pSelectedInfo->fDuration;
}

_float CImGuiAnimation::MouseToTime(_float _fMouseX)
{
	_float fRatio = (_fMouseX - m_vCanvasPos.x) / m_vCanvasSize.x;
	fRatio = max(0.f, min(1.f, fRatio));

	_float fTrackPosition = (fRatio * m_fBaseDuration) + m_fTrackPositionOffsetX;

	_float fTick = fTrackPosition * m_fTPS;
	fTick = roundf(fTick);

	return fTick / m_fTPS;
}

_float CImGuiAnimation::MouseToTimeUnsnapped(_float _fMouseX)
{
	_float fRatio = (_fMouseX - m_vCanvasPos.x) / m_vCanvasSize.x;
	fRatio = max(0.f, min(1.f, fRatio));

	return (fRatio * m_fBaseDuration) + m_fTrackPositionOffsetX;
}

ImVec2 CImGuiAnimation::Get_TrackPos(_int _iIndex)
{
	return ImVec2(
		m_vCanvasPos.x,
		m_vCanvasPos.y + m_fTimelineHeight + (_iIndex * m_fTrackHeight) - m_fTrackPositionOffsetY);
}

_float CImGuiAnimation::TimeToScreenX(_float _fTime)
{
	return m_vCanvasPos.x + ((_fTime - m_fTrackPositionOffsetX) / m_fBaseDuration) * m_vCanvasSize.x;;
}

_bool CImGuiAnimation::Is_MouseInTrack()
{
	if (m_tMouseState.fX < m_vCanvasPos.x ||
		m_tMouseState.fX >(m_vCanvasPos.x + m_vCanvasSize.x))
		return false;

	_int iLayer = Get_MouseLayerIndex();
	if (iLayer < 0)
		return false;

	_int iTotalTrack = (_int)m_vecTracks.size() + (_int)m_vecCustomAnimLayerIndices.size() + m_iEventLayerCount;

	return (iLayer < iTotalTrack);
}

_bool CImGuiAnimation::Is_MouseInTrack(_int _iIndex)
{
	return (
		m_tMouseState.fX >= m_vCanvasPos.x &&
		m_tMouseState.fX <= m_vCanvasPos.x + m_vCanvasSize.x &&
		m_tMouseState.fY >= Get_TrackPos(_iIndex).y &&
		m_tMouseState.fY <= Get_TrackPos(_iIndex).y + m_fTrackHeight);
}

_bool CImGuiAnimation::Is_HoverKey(_float _fMouseX, _float _fMouseY, _float _fKeyX, _float _fKeyY)
{
	return (abs(_fMouseX - _fKeyX) < KEY_HOVER_RADIUS && abs(_fMouseY - _fKeyY) < KEY_HOVER_RADIUS);
}

_int CImGuiAnimation::Get_MouseLayerIndex()
{
	_float fY = m_tMouseState.fY - (m_vCanvasPos.y + m_fTimelineHeight) + m_fTrackPositionOffsetY;
	if (fY < 0.f)
		return -1;

	return (_int)(fY / m_fTrackHeight);
}

void CImGuiAnimation::Sort_FrameKeys(CUSTOM_ANIM& _tCustomAnim)
{
	/* sort */
	std::sort(_tCustomAnim.vecKF.begin(), _tCustomAnim.vecKF.end(),
		[](const CUSTOM_KEY_FRAME& tA, const CUSTOM_KEY_FRAME& tB)
		{
			return tA.fTrackPosition < tB.fTrackPosition;
		});
}

void CImGuiAnimation::Find_LastAddedKey(CUSTOM_ANIM& _tCustomAnim, const CUSTOM_KEY_FRAME& _tNewKey)
{
	for (_int i = 0; i < _tCustomAnim.vecKF.size(); i++)
	{
		if (abs(_tCustomAnim.vecKF[i].fTrackPosition - _tNewKey.fTrackPosition) < 0.0001f)
		{
			m_tSelected.iCustomAnimKey = i;
			return;
		}
	}

	m_tSelected.iCustomAnimKey = -1;
}

_float CImGuiAnimation::Clamp_KeyTrackPosition(const CUSTOM_ANIM& _tCustomAnim, _float _fTime)
{
	_float fLength = _tCustomAnim.fEndTime - _tCustomAnim.fStartTime;
	return max(0.f, min(_fTime, fLength));
}

void CImGuiAnimation::Create_Track(ANIMATION_INFO* _pInfo, _bool _bAdditive)
{
	if (!_pInfo)
		return;

	TRACK tNewTrack = {};
	tNewTrack.pAnimInfo = _pInfo;
	tNewTrack.bAdditive = _bAdditive;

	m_vecTracks.push_back(tNewTrack);
}

void CImGuiAnimation::Select_CustomAnim(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bResizeLeft, _bool _bResize)
{
	m_tSelected.iCustomAnimLayer = _iLayerIndex;
	m_tSelected.iCustomAnimKey = -1;
	m_tSelected.iCustomAnimIndex = Find_CustomAnimIndex(_tCustomAnim);

	/* Reset */
	m_tSelected.iEventKey = -1;

	Begin_Drag(_tCustomAnim.fStartTime);

	m_tInteractionState.bResizing = _bResize;
	m_tInteractionState.bResizeLeft = _bResizeLeft;
}

void CImGuiAnimation::Handle_CustomAnim(CUSTOM_ANIM& _tCustomAnim, _float _fMinEventLength)
{
	_bool bSnap = !ImGui::GetIO().KeyShift;
	_float fTime = bSnap ? MouseToTime(m_tMouseState.fX) : MouseToTimeUnsnapped(m_tMouseState.fX);

	if (m_tInteractionState.bResizing)
	{
		_float fOldStart = _tCustomAnim.fStartTime;
		_float fOldEnd = _tCustomAnim.fEndTime;
		_float fOldLength = max(fOldEnd - fOldStart, 0.001f);

		if (m_tInteractionState.bResizeLeft)
		{
			_tCustomAnim.fStartTime = min(fTime, _tCustomAnim.fEndTime - _fMinEventLength);
			_tCustomAnim.fStartTime = max(0.f, _tCustomAnim.fStartTime);
		}
		else
		{
			_tCustomAnim.fEndTime = max(fTime, _tCustomAnim.fStartTime + _fMinEventLength);
			_tCustomAnim.fEndTime = min(_tCustomAnim.fEndTime, m_vecTracks[m_tSelected.iTrack].pAnimInfo->fDuration);
		}
		_float fNewLength = max(_tCustomAnim.fEndTime - _tCustomAnim.fStartTime, 0.001f);
		_float fScale = fNewLength / fOldLength;

		for (auto& tKF : _tCustomAnim.vecKF)
			tKF.fTrackPosition *= fScale;
	}
	else
	{
		_float fLength = _tCustomAnim.fEndTime - _tCustomAnim.fStartTime;
		_float fNewStart = fTime - m_tInteractionState.fDragOffset;

		if (bSnap)
		{
			_float fTick = fNewStart * m_fTPS;
			fTick = roundf(fTick);
			fNewStart = fTick / m_fTPS;
		}

		_tCustomAnim.fStartTime = max(0.f, fNewStart);
		_tCustomAnim.fEndTime = _tCustomAnim.fStartTime + fLength;

		if (_tCustomAnim.fEndTime > m_vecTracks[m_tSelected.iTrack].pAnimInfo->fDuration)
		{
			_tCustomAnim.fEndTime = m_vecTracks[m_tSelected.iTrack].pAnimInfo->fDuration;
			_tCustomAnim.fStartTime = max(0.f, _tCustomAnim.fEndTime - fLength);
		}
	}
}

void CImGuiAnimation::Popup_CustomAnim(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bHover)
{
	if (!_bHover || !ImGui::IsMouseClicked(1))
		return;

	m_tSelected.iCustomAnimLayer = _iLayerIndex;
	m_tSelected.iCustomAnimIndex = Find_CustomAnimIndex(_tCustomAnim);
	ImGui::OpenPopup("POPUP_CUSTOMANIM");
}

WFunction* CImGuiAnimation::Popup_Selected(const _char* _pButtonLabel, const _char* _pPopupName)
{
	WFunction* pFunction = { nullptr };

	if (ImGui::Button(_pButtonLabel))
		ImGui::OpenPopup(_pPopupName);

	if (ImGui::BeginPopup(_pPopupName))
	{
		CObject* pRoot = { nullptr };
		CObject* pOwner = { nullptr };

		pFunction = CFunctionRegistry::ImGui_Choose_Function(pRoot, &pOwner);
		if (pFunction)
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}
	return pFunction;
}

void CImGuiAnimation::Default_FunctionType(CUSTOM_KEY_FRAME& _tKF, FUNCTION_PROPERTY_TYPE _eType, _bool _bIsEndKey)
{
	switch (_eType)
	{
	case FUNCTION_PROPERTY_TYPE::INT:
		_tKF.iParam = _bIsEndKey ? 1 : 0;
		break;

	case FUNCTION_PROPERTY_TYPE::FLOAT1:
		_tKF.fParam = _bIsEndKey ? 1.f : 0.f;
		break;

	case FUNCTION_PROPERTY_TYPE::FLOAT2:
		_tKF.v2Param = _bIsEndKey ? _float2{ 1.f, 1.f } : _float2{ 0.f, 0.f };
		break;

	case FUNCTION_PROPERTY_TYPE::FLOAT3:
		_tKF.v3Param = _bIsEndKey ? _float3{ 1.f, 1.f, 1.f } : _float3{ 0.f, 0.f, 0.f };
		break;

	case FUNCTION_PROPERTY_TYPE::FLOAT4:
		_tKF.v4Param = _bIsEndKey ? _float4{ 1.f, 1.f, 1.f, 1.f } : _float4{ 0.f, 0.f, 0.f, 0.f };
		break;

	case FUNCTION_PROPERTY_TYPE::MONOSTATE:
		break;
	}
}

_bool CImGuiAnimation::Input_FunctionType(const _char* _pLabel, FUNCTION_PROPERTY_TYPE _eType, CUSTOM_KEY_FRAME& _tKF)
{
	switch (_eType)
	{
	case FUNCTION_PROPERTY_TYPE::INT:
		return ImGui::InputInt(_pLabel, &_tKF.iParam);
		
	case FUNCTION_PROPERTY_TYPE::FLOAT1:
		return ImGui::InputFloat(_pLabel, &_tKF.fParam);
		
	case FUNCTION_PROPERTY_TYPE::FLOAT2:
		return ImGui::InputFloat2(_pLabel, (_float*)&_tKF.v2Param);
		
	case FUNCTION_PROPERTY_TYPE::FLOAT3:
		return ImGui::InputFloat3(_pLabel, (_float*)&_tKF.v3Param);
		
	case FUNCTION_PROPERTY_TYPE::FLOAT4:
		return ImGui::InputFloat4(_pLabel, (_float*)&_tKF.v4Param);

	case FUNCTION_PROPERTY_TYPE::MONOSTATE:
		ImGui::Text("No Parameter");
		return false;
	}

	return false;
}

void CImGuiAnimation::Apply_FuncToEvent(EVENT_KEY& _tEvent, WFunction* _pFunc)
{
	if (!_pFunc)
		return;

	_tEvent.strEventName = _pFunc->Get_FunctionDesc()->strName;
	_tEvent.pFunction = _pFunc;
	_tEvent.Touch_ModifiedTime();

	Default_FunctionType(_tEvent.tKF, _pFunc->Get_FunctionDesc()->tType, false);
}

void CImGuiAnimation::Apply_FuncToCustomAnim(CUSTOM_ANIM& _tCustomAnim, WFunction* _pFunc)
{
	if (!_pFunc)
		return;

	_tCustomAnim.strCustomAnimName = _pFunc->Get_FunctionDesc()->strName;
	_tCustomAnim.pFunction = _pFunc;
	_tCustomAnim.vecKF.clear();

	CUSTOM_KEY_FRAME tStart = {};
	CUSTOM_KEY_FRAME tEnd = {};

	tStart.fTrackPosition = 0.f;
	tEnd.fTrackPosition = max(_tCustomAnim.fEndTime - _tCustomAnim.fStartTime, 0.001f);

	auto eType = _pFunc->Get_FunctionDesc()->tType;
	Default_FunctionType(tStart, eType, false);
	Default_FunctionType(tEnd, eType, true);

	_tCustomAnim.vecKF.push_back(tStart);
	_tCustomAnim.vecKF.push_back(tEnd);
}

void CImGuiAnimation::Delete_CustomAnimTrack()
{
	if (-1 == m_tSelected.iCustomAnimIndex)
		return;

	if (ImGui::BeginPopup("CustomAnimPopup"))
	{
		if (ImGui::MenuItem("Delete (Track)"))
		{
			auto& vecCustomAnim = m_pSelectedInfo->vecCustomAnims;
			if (m_tSelected.iCustomAnimIndex >= 0 &&
				m_tSelected.iCustomAnimIndex < vecCustomAnim.size())
			{
				if (vecCustomAnim[m_tSelected.iCustomAnimIndex].pFunction)
				{
					Safe_Release(vecCustomAnim[m_tSelected.iCustomAnimIndex].pFunction);
					vecCustomAnim[m_tSelected.iCustomAnimIndex].pFunction = nullptr;
				}

				vecCustomAnim.erase(vecCustomAnim.begin() + m_tSelected.iCustomAnimIndex);

				Build_CustomAnimLayers();

				m_tSelected.iCustomAnimLayer = -1;
				m_tSelected.iCustomAnimIndex = -1;
				m_tSelected.iCustomAnimKey = -1;
				m_tInteractionState.bCustomAnimKeyDragging = false;
			}
		}

		if (ImGui::MenuItem("Delete (Key)"))
		{
			auto& customAnim = m_pSelectedInfo->vecCustomAnims[m_tSelected.iCustomAnimIndex];
			if (m_tSelected.iCustomAnimKey >= 0 && m_tSelected.iCustomAnimKey < customAnim.vecKF.size())
			{
				customAnim.vecKF.erase(customAnim.vecKF.begin() + m_tSelected.iCustomAnimKey);
				m_tSelected.iEventKey = -1;
				m_tInteractionState.bCustomAnimKeyDragging = false;

				Sort_FrameKeys(customAnim);
			}
		}

		if (ImGui::MenuItem("Delete (Layer)"))
		{
			_int iLayer = m_tSelected.iCustomAnimLayer;
			if (iLayer >= 0 && iLayer < m_iCustomAnimLayerCount)
			{
				auto& vecCustomAnim = m_pSelectedInfo->vecCustomAnims;
				for (_int i = (_int)vecCustomAnim.size() - 1; i >= 0; --i)
				{
					if (vecCustomAnim[i].iEventLayerIndex == iLayer)
					{
						if (vecCustomAnim[i].pFunction)
						{
							Safe_Release(vecCustomAnim[i].pFunction);
							vecCustomAnim[i].pFunction = nullptr;
						}
						vecCustomAnim.erase(vecCustomAnim.begin() + i);
					}
				}

				for (auto& customAnim : vecCustomAnim)
				{
					if (customAnim.iEventLayerIndex > iLayer)
						customAnim.iEventLayerIndex--;
				}

				m_iCustomAnimLayerCount = max(0, m_iCustomAnimLayerCount - 1);
				Build_CustomAnimLayers();

				m_tSelected.iCustomAnimLayer = -1;
				m_tSelected.iCustomAnimIndex = -1;
				m_tSelected.iCustomAnimKey = -1;
				m_tSelected.iEventKey = -1;

				Reset_Drag();
			}
		}
		ImGui::EndPopup();
	}
}

void CImGuiAnimation::Handle_CustomAnimTrack(_int _iLayerIndex, CUSTOM_ANIM& _tCustomAnim, _bool _bHover)
{
	if (ImGui::IsPopupOpen("CustomAnimPopup"))
		return;

	if (m_vecTracks.empty() || !Is_MouseInTrack())
		return;

	_int iTrackIndex = (_int)m_vecTracks.size() + _iLayerIndex + m_iEventLayerCount;
	_float fMinEventLength = 1.f / m_fTPS;

	_float fXStart = TimeToScreenX(_tCustomAnim.fStartTime);
	_float fXEnd = TimeToScreenX(_tCustomAnim.fEndTime);

	_bool bResizeLeft = (abs(m_tMouseState.fX - fXStart) < RESIZE_THRESHOLD);
	_bool bResizeRight = (abs(m_tMouseState.fX - fXEnd) < RESIZE_THRESHOLD);
	_bool bResize = bResizeLeft || bResizeRight;

	if (_bHover && bResize)
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

	/* Click (just select)*/
	if (_bHover 
		&& ImGui::IsMouseClicked(0) 
		&& Is_MouseInTrack(iTrackIndex)
		&& !m_tInteractionState.bEventKeyDragging
		&& !m_tInteractionState.bCustomAnimKeyDragging)
	{
		Select_CustomAnim(_iLayerIndex, _tCustomAnim, bResizeLeft, bResize);
	}

	/* Drag */
	if (Is_SelectedCustomAnim(_tCustomAnim) 
		&& ImGui::IsMouseDragging(0) 
		&& ImGui::IsWindowHovered() 
		&& !m_tInteractionState.bEventKeyDragging
		&& !m_tInteractionState.bCustomAnimKeyDragging)
	{
		if (!m_tInteractionState.bDraggingStarted &&
			abs(m_tMouseState.fX - m_tInteractionState.fDragStartMouseX) >= DRAG_THRESHOLD)
		{
			m_tInteractionState.bDragging = true;
			m_tInteractionState.bDraggingStarted = true;
		}
	}

	if (m_tInteractionState.bDragging 
		&& Is_SelectedCustomAnim(_tCustomAnim)
		&& !m_tInteractionState.bEventKeyDragging)
	{
		if (ImGui::IsMouseDown(0))
		{
			Handle_CustomAnim(_tCustomAnim, fMinEventLength);
		}
		else
		{
			End_Drag();
		}
	}

	/* Delete Popup */
	Popup_CustomAnim(_iLayerIndex, _tCustomAnim, _bHover);
}

void CImGuiAnimation::Clear_CustomAnimTracks()
{
	Reset_Drag();

	m_vecCustomAnimLayerIndices.clear();
	m_iCustomAnimLayerCount = 0;

	m_tSelected.iCustomAnimLayer = -1;
	m_tSelected.iCustomAnimIndex = -1;
	m_tSelected.iCustomAnimKey = -1;
}

void CImGuiAnimation::Handle_Event()
{
	_int iTrackIndex = m_vecTracks.size();

	if (!Is_MouseInTrack(iTrackIndex))
		return;

	_float fY = Get_TrackPos(iTrackIndex).y + m_fTrackHeight * 0.5f;

	/* Select */
	for (_int i = 0; i < m_pSelectedInfo->vecEvents.size(); i++)
	{
		auto& event = m_pSelectedInfo->vecEvents[i];
		_float fX = TimeToScreenX(event.tKF.fTrackPosition);

		_bool bHover = Is_HoverKey(m_tMouseState.fX, m_tMouseState.fY, fX, fY);
		if (bHover 
			&& ImGui::IsMouseClicked(0)
			&& !m_tInteractionState.bCustomAnimKeyDragging
			&& !m_tInteractionState.bDragging)
		{
			m_tSelected.iEventKey = i;
			m_tInteractionState.bEventKeyDragging = true;
			Begin_Drag(event.tKF.fTrackPosition);

			/* Reset */
			m_tSelected.iCustomAnimLayer = -1;
			m_tSelected.iCustomAnimIndex = -1;
			m_tSelected.iCustomAnimKey = -1;
		}

		if (bHover && ImGui::IsMouseClicked(1))
		{
			m_tSelected.iEventKey = i;
			ImGui::OpenPopup("EventPopup");
		}
	}

	/* Drag */
	if (m_tInteractionState.bEventKeyDragging && ImGui::IsMouseDragging(0))
	{
		if (!m_tInteractionState.bDraggingStarted && 
			abs(m_tMouseState.fX - m_tInteractionState.fDragStartMouseX) > DRAG_THRESHOLD)
		{
			m_tInteractionState.bDragging = true;
			m_tInteractionState.bDraggingStarted = true;
		}
	}

	if ((-1 != m_tSelected.iEventKey) && m_tInteractionState.bDragging && m_tInteractionState.bEventKeyDragging)
	{
		if (ImGui::IsMouseDown(0))
		{
			_float fTime = MouseToTimeUnsnapped(m_tMouseState.fX) - m_tInteractionState.fDragOffset;
			fTime = max(0.f, min(fTime, m_fDuration));
			m_pSelectedInfo->vecEvents[m_tSelected.iEventKey].tKF.fTrackPosition = fTime;
			m_pSelectedInfo->vecEvents[m_tSelected.iEventKey].Touch_ModifiedTime();
		}
		else
		{
			End_Drag();
		}
	}

	/* Add Key */
	if (ImGui::IsMouseDoubleClicked(0) 
		&& Is_MouseInTrack(iTrackIndex)
		&& Is_HoverRect(
			ImVec2(m_vCanvasPos.x, Get_TrackPos(iTrackIndex).y),
			ImVec2(m_vCanvasPos.x + m_vCanvasSize.x, Get_TrackPos(iTrackIndex).y + m_fTrackHeight))
		&& !ImGui::IsAnyItemHovered()
		&& !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
	{		
		EVENT_KEY tNewKey = {};
		tNewKey.Ensure_Meta();
		tNewKey.tKF.fTrackPosition = MouseToTimeUnsnapped(m_tMouseState.fX);
		m_pSelectedInfo->vecEvents.push_back(tNewKey);

		m_tSelected.iEventKey = m_pSelectedInfo->vecEvents.size() - 1;

		m_tSelected.iCustomAnimLayer = -1;
		m_tSelected.iCustomAnimIndex = -1;
		m_tSelected.iCustomAnimKey = -1;

		Reset_Drag();
	}

	if (ImGui::BeginPopup("EventPopup"))
	{
		Delete_Event();
		ImGui::EndPopup();
	}
}

void CImGuiAnimation::Delete_Event()
{
	if (m_tSelected.iEventKey >= 0 && m_tSelected.iEventKey < m_pSelectedInfo->vecEvents.size())
	{
		auto& event = m_pSelectedInfo->vecEvents[m_tSelected.iEventKey];
		if (ImGui::MenuItem("Delete Event"))
		{
			if (event.pFunction)
			{
				Safe_Release(event.pFunction);
				event.pFunction = nullptr;
			}
			m_pSelectedInfo->vecEvents.erase(m_pSelectedInfo->vecEvents.begin() + m_tSelected.iEventKey);
			m_tSelected.iEventKey = -1;
		}
	}
}

void CImGuiAnimation::Build_Event()
{
	if (!m_pSelectedInfo)
	{
		m_tSelected.iEventKey = -1;
		return;
	}
	
	if (m_tSelected.iEventKey >= m_pSelectedInfo->vecEvents.size())
		m_tSelected.iEventKey = -1;
}

void CImGuiAnimation::Add_Track(ANIMATION_INFO* _pInfo)
{
	if (!_pInfo || !Is_SelectedBindingAlive())
		return;

	Clear_Tracks();

	/* Base Track */
	Create_Track(_pInfo, false);

	/* Additive Track */
	for (auto& AdditiveAnim : _pInfo->vecAdditiveInfos)
	{
		auto pAdditiveAnimation = m_pSelectedController->Get_CurAnimationByName(AdditiveAnim.strAdditiveAnimName);
		Create_Track(pAdditiveAnimation, true);
	}

	/* Sync */
	m_tSelected.iTrack = 0;
	m_pSelectedInfo = _pInfo;
	m_fDuration = _pInfo->fDuration;
}

void CImGuiAnimation::Clear_Tracks()
{
	m_vecTracks.clear();
}

CImGuiAnimation* CImGuiAnimation::Create(ID3D11Device* _pDevice, ID3D11DeviceContext* _pContext) {
	auto pInstance = new CImGuiAnimation(_pDevice, _pContext);

	if (FAILED(pInstance->Initialize())) {
		Safe_Release(pInstance);
		return nullptr;
	}

	return pInstance;
}

void CImGuiAnimation::Draw_Rect(ImVec2 _Min, ImVec2 _Max, ImU32 _Color)
{
	m_pDrawList->AddRectFilled(_Min, _Max, _Color);
}

void CImGuiAnimation::Draw_Text(ImVec2 _Pos, const _char* _Text, ImU32 _Color)
{
	m_pDrawList->AddText(_Pos, _Color, _Text);
}

_bool CImGuiAnimation::Is_HoverRect(ImVec2 _Min, ImVec2 _Max)
{
	return (m_tMouseState.fX >= _Min.x && m_tMouseState.fX <= _Max.x &&
			m_tMouseState.fY >= _Min.y && m_tMouseState.fY <= _Max.y);
}

void CImGuiAnimation::Reset_Drag()
{
	m_tInteractionState.bDragging = false;
	m_tInteractionState.bDraggingStarted = false;
	m_tInteractionState.fDragOffset = 0.f;
	m_tInteractionState.fDragStartMouseX = 0.f;

	m_tInteractionState.bEventKeyDragging = false;
	m_tInteractionState.bCustomAnimKeyDragging = false;

	m_tInteractionState.bResizing = false;
	m_tInteractionState.bResizeLeft = false;
}

void CImGuiAnimation::Begin_Drag(_float _fTime)
{
	m_tInteractionState.bDragging = false;
	m_tInteractionState.bDraggingStarted = false;
	m_tInteractionState.fDragStartMouseX = m_tMouseState.fX;
	m_tInteractionState.fDragOffset = MouseToTimeUnsnapped(m_tMouseState.fX) - _fTime;
}

void CImGuiAnimation::End_Drag()
{
	m_tInteractionState.bDragging = false;
	m_tInteractionState.bDraggingStarted = false;
	m_tInteractionState.bResizing = false;
	m_tInteractionState.bResizeLeft = false;
	m_tInteractionState.fDragOffset = 0.f;
	m_tInteractionState.fDragStartMouseX = 0.f;

	m_tInteractionState.bEventKeyDragging = false;
	m_tInteractionState.bCustomAnimKeyDragging = false;
}

_bool CImGuiAnimation::Is_SelectedCustomAnim(const CUSTOM_ANIM& _tCustomAnim)
{
	if (!m_pSelectedInfo)
		return false;

	if (m_tSelected.iCustomAnimIndex < 0 ||
		m_tSelected.iCustomAnimIndex >= (_int)m_pSelectedInfo->vecCustomAnims.size())
		return false;

	return (&m_pSelectedInfo->vecCustomAnims[m_tSelected.iCustomAnimIndex] == &_tCustomAnim);
}

_int CImGuiAnimation::Find_CustomAnimIndex(const CUSTOM_ANIM& _tCustomAnim)
{
	if (!m_pSelectedInfo)
		return -1;

	for (_int i = 0; i < (_int)m_pSelectedInfo->vecCustomAnims.size(); ++i)
	{
		if (&m_pSelectedInfo->vecCustomAnims[i] == &_tCustomAnim)
			return i;
	}

	return -1;
}

void CImGuiAnimation::Free() {
	__super::Free();

	Clear_Tracks();
}
