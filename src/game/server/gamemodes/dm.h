/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_SERVER_GAMEMODES_DM_H
#define GAME_SERVER_GAMEMODES_DM_H
#include <game/server/gamecontroller.h>
#include <game/server/gamecontext.h>
class CGameControllerDM : public IGameController
{
public:
	CGameControllerDM(class CGameContext *pGameServer);
	virtual void Tick();
	virtual void Snap(int SnappingClient);
	virtual int OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID);
	int m_Weapon;
	int m_Hook;
};
#endif
