/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include <engine/shared/config.h>

#include <base/math.h>
#include <game/collision.h>
#include <game/client/gameclient.h>
#include <game/client/component.h>

#include "camera.h"
#include "controls.h"

CCamera::CCamera()
{
	m_CamType = CAMTYPE_UNDEFINED;
}

void CCamera::OnRender()
{
	//vec2 center;

	if(m_pClient->m_Snap.m_SpecInfo.m_Active) {

		if (g_Config.m_ClCameraSmoothZoom) {
			float diff = (float)g_Config.m_ClCameraZoom * 0.001f - m_Zoom;
			float factor = diff* (float)g_Config.m_ClCameraSmoothZoomFactor * 0.001f;
			m_Zoom += factor;			
		} else {
			m_Zoom = (float)g_Config.m_ClCameraZoom * 0.001f;
		}		
		m_Zoom = clamp(m_Zoom, g_Config.m_ClCameraZoomMin * 0.001f,  g_Config.m_ClCameraZoomMax * 0.001f);
	} else {
		m_Zoom = 1.0f;
	}

	// update camera center
	if(m_pClient->m_Snap.m_SpecInfo.m_Active && !m_pClient->m_Snap.m_SpecInfo.m_UsePosition)
	{
		if(m_CamType != CAMTYPE_SPEC)
		{
			m_pClient->m_pControls->m_MousePos = m_PrevCenter;
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_SPEC;
		}
		m_Center = m_pClient->m_pControls->m_MousePos;
	}
	else
	{
		if(m_CamType != CAMTYPE_PLAYER)
		{
			m_pClient->m_pControls->ClampMousePos();
			m_CamType = CAMTYPE_PLAYER;
		}

		vec2 CameraOffset(0, 0);

		float l = length(m_pClient->m_pControls->m_MousePos);
		if(l > 0.0001f) // make sure that this isn't 0
		{
			float DeadZone = g_Config.m_ClMouseDeadzone;
			float FollowFactor = g_Config.m_ClMouseFollowfactor/100.0f;
			float OffsetAmount = max(l-DeadZone, 0.0f) * FollowFactor;

			CameraOffset = normalize(m_pClient->m_pControls->m_MousePos)*OffsetAmount;
		}

		if(m_pClient->m_Snap.m_SpecInfo.m_Active)
			m_Center = m_pClient->m_Snap.m_SpecInfo.m_Position + CameraOffset;
		else
			m_Center = m_pClient->m_LocalCharacterPos + CameraOffset;
	}

	m_PrevCenter = m_Center;
}

void CCamera::ConZoomIn(IConsole::IResult *pResult, void *pUserData)
{
	CCamera *pCamera = (CCamera *)pUserData;
	g_Config.m_ClCameraZoom = clamp(g_Config.m_ClCameraZoom-g_Config.m_ClCameraZoomStep, g_Config.m_ClCameraZoomMin, g_Config.m_ClCameraZoomMax);
}

void CCamera::ConZoomOut(IConsole::IResult *pResult, void *pUserData)
{
	g_Config.m_ClCameraZoom = clamp(g_Config.m_ClCameraZoom+g_Config.m_ClCameraZoomStep, g_Config.m_ClCameraZoomMin, g_Config.m_ClCameraZoomMax);
}

void CCamera::OnConsoleInit() {
	Console()->Register("zoom_in", "", CFGFLAG_CLIENT, ConZoomIn, this, "Zoom in");
	Console()->Register("zoom_out", "", CFGFLAG_CLIENT, ConZoomOut, this, "Zoom out");
}
