/* gCTF - CTF only with hammer - 1Hit1Kill
 * Made by pixe1 and Pata
 */
#include "hammer.h"
#include <engine/shared/config.h>
#include <game/server/entities/character.h>
#include <game/server/gamecontext.h>

// hCTF

CGameControllerHCTF::CGameControllerHCTF(class CGameContext *pGameServer, int TypeFlags)
: CGameControllerCTF(pGameServer, TypeFlags)
{
	m_Flags = TypeFlags;
	m_pGameType = "hCTF+";
}

void CGameControllerHCTF::OnCharacterSpawn(CCharacter* pChr)
{
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
}

void CGameControllerHCTF::Tick()
{
	CGameControllerCTF::Tick();
}


// hDM

CGameControllerHDM::CGameControllerHDM(class CGameContext *pGameServer, int TypeFlags)
: CGameControllerDM(pGameServer, TypeFlags)
{
	m_Flags = TypeFlags;
	m_pGameType = "hDM+";
}

void CGameControllerHDM::OnCharacterSpawn(CCharacter* pChr)
{
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
}

void CGameControllerHDM::Tick()
{
	CGameControllerDM::Tick();
}


// hTDM

CGameControllerHTDM::CGameControllerHTDM(class CGameContext *pGameServer, int TypeFlags)
: CGameControllerTDM(pGameServer, TypeFlags)
{
	m_Flags = TypeFlags;
	m_pGameType = "hTDM+";
}

void CGameControllerHTDM::OnCharacterSpawn(CCharacter* pChr)
{
	pChr->IncreaseHealth(10);
	pChr->GiveWeapon(WEAPON_HAMMER, -1);
}

void CGameControllerHTDM::Tick()
{
	CGameControllerTDM::Tick();
}
