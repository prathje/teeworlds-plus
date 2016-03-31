/* gCTF - CTF only with hammer - 1Hit1Kill
 * Made by pixe1 and Pata
 */
#ifndef GAME_SERVER_GAMEMODES_HCTF_H
#define GAME_SERVER_GAMEMODES_HCTF_H
#include <game/server/gamecontroller.h>
#include "ctf.h"
#include "dm.h"
#include "tdm.h"


class CGameControllerHCTF : public CGameControllerCTF
{
public:
	CGameControllerHCTF(class CGameContext *pGameServer, int);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void Tick();
};

class CGameControllerHDM : public CGameControllerDM
{
public:
	CGameControllerHDM(class CGameContext *pGameServer, int);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void Tick();
};

class CGameControllerHTDM : public CGameControllerTDM
{
public:
	CGameControllerHTDM(class CGameContext *pGameServer, int);
	virtual void OnCharacterSpawn(class CCharacter *pChr);
	virtual void Tick();
};

#endif
