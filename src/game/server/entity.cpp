/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

#include "entity.h"
#include "gamecontext.h"
#include <engine/shared/config.h>

//////////////////////////////////////////////////
// Entity
//////////////////////////////////////////////////
CEntity::CEntity(CGameWorld *pGameWorld, int ObjType)
{
	m_pGameWorld = pGameWorld;

	m_ObjType = ObjType;
	m_Pos = vec2(0,0);
	m_ProximityRadius = 0;

	m_MarkedForDestroy = false;
	m_ID = Server()->SnapNewID();

	m_pPrevTypeEntity = 0;
	m_pNextTypeEntity = 0;

	m_AffectedCharacters = 0;
}

CEntity::~CEntity()
{
	GameWorld()->RemoveEntity(this);
	Server()->SnapFreeID(m_ID);
}

int CEntity::NetworkClipped(int SnappingClient)
{
	return NetworkClipped(SnappingClient, m_Pos);
}

int CEntity::NetworkClipped(int SnappingClient, vec2 CheckPos)
{
	if(SnappingClient == -1)
		return 0;
	else if(GameServer()->m_apPlayers[SnappingClient]->GetTeam() == TEAM_SPECTATORS && Server()->IsAuthed(SnappingClient) >= g_Config.m_SvSpectatorFullViewAuthLevel) {
		return 0;
	}

	float dx = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.x-CheckPos.x;
	float dy = GameServer()->m_apPlayers[SnappingClient]->m_ViewPos.y-CheckPos.y;
	
	if(g_Config.m_SvOwnNetworkClipping) {
	
		if( (int) absolute(dx) > g_Config.m_SvNetworkClippingX ||  (int) absolute(dy) > g_Config.m_SvNetworkClippingY)
			return 1;

		if( (int) distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > g_Config.m_SvNetworkClippingDist)
			return 1;
	
	}else {

		if(absolute(dx) > 1000.0f || absolute(dy) > 800.0f)
			return 1;

		if(distance(GameServer()->m_apPlayers[SnappingClient]->m_ViewPos, CheckPos) > 1100.0f)
			return 1;
	}
	return 0;
}

bool CEntity::GameLayerClipped(vec2 CheckPos)
{
	return round_to_int(CheckPos.x)/32 < -200 || round_to_int(CheckPos.x)/32 > GameServer()->Collision()->GetWidth()+200 ||
			round_to_int(CheckPos.y)/32 < -200 || round_to_int(CheckPos.y)/32 > GameServer()->Collision()->GetHeight()+200 ? true : false;
}

bool CEntity::IsAffected(int ClientID) {
	if (!g_Config.m_SvSprayprotection) {
		return true;
	}
	if (ClientID < 0 || ClientID > MAX_CLIENTS) {
		return true;
	} else {
		return (m_AffectedCharacters>>ClientID)&0x1;
	}
}

void CEntity::SetAffected(int ClientID, bool Affected) {
	if (ClientID >= 0 && ClientID < MAX_CLIENTS) {
		if (Affected) {
			m_AffectedCharacters |= (1<<ClientID);
		} else {
			//clear
			m_AffectedCharacters &= (1<<ClientID);
		}
	}
}

//TODO Move this to the used cases in laser and projectile
void CEntity::InitAffectedCharacters(int Owner) {
	m_AffectedCharacters = 0;

	if (Owner >= 0 && Owner <= MAX_CLIENTS) {
		for(int i = 0; i < MAX_CLIENTS; ++i) {
			if (GameServer()->m_apPlayers[i]) {
				CPlayer *pPlayer = GameServer()->m_apPlayers[i];
				if (pPlayer->GetTeam() != TEAM_SPECTATORS &&
					pPlayer->GetCharacter() &&
					pPlayer->GetCharacter()->IsAlive() &&
					!NetworkClipped(Owner, pPlayer->GetCharacter()->m_Pos)) {
					SetAffected(i);
				}
			}
		}
	}
}
