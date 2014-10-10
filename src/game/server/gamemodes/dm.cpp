/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "dm.h"
#include <game/server/entities/flag.h>
#include <engine/shared/config.h>

CGameControllerDM::CGameControllerDM(class CGameContext *pGameServer)
: IGameController(pGameServer)
{
	m_pGameType = "Wanted";
	m_GameFlags = GAMEFLAG_FLAGS;
	m_Weapon = g_Config.m_SvWeapon;
	m_Weapon = g_Config.m_SvHook;
}

void CGameControllerDM::Tick()
{
	IGameController::Tick();
	
	//chance weapon
	if(m_Weapon != g_Config.m_SvWeapon)
	{
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pP = GameServer()->m_apPlayers[i];
			if(pP && pP->GetTeam() != TEAM_SPECTATORS && pP->GetCharacter())
			{
				pP->GetCharacter()->TakeWeapon(m_Weapon);
				pP->GetCharacter()->GiveWeapon(g_Config.m_SvWeapon, -1);
			}
		}
		m_Weapon = g_Config.m_SvWeapon;
	}
	
	if(m_Hook != g_Config.m_SvHook)
	{
	
		char aBuf[512];
		const char* m_aWeaponConfigurations[4] = {
				"only your weapon", "only your hook", "your weapon or your hook", "your weapon and your hook together"	
				};
				str_format(aBuf, sizeof(aBuf), "Weapon configuration changed: Use %s to catch other players!", m_aWeaponConfigurations[g_Config.m_SvHook]);
				
		GameServer()->SendChat(-1, CGameContext::CHAT_ALL, aBuf);
		m_Hook = g_Config.m_SvHook;
	}
	
	if(m_GameOverTick == -1)
	{
		int count = 0;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pP = GameServer()->m_apPlayers[i];
			if(pP && pP->m_IsWanted)
			{
				if(pP->GetTeam() != TEAM_SPECTATORS)
				{
					count++;					
					if(pP->GetCharacter() && (Server()->Tick() - pP->GetCharacter()->GetCharSpawnTick()) % Server()->TickSpeed() == 0)					
						pP->m_Score++;
				}
				else
				{
					pP->m_IsWanted = false;
				}
			}
		}
		/*
		int aPlayerID[MAX_CLIENTS];
			int PlayerCount = 0;		
			
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{				
			
				CPlayer *pP = GameServer()->m_apPlayers[i];
				if(pP)
				{
					if(pP->GetTeam() != TEAM_SPECTATORS && !pP->m_LastWinner)
						aPlayerID[PlayerCount++] = i;
					if(pP->m_LastWinner)
						pP->m_LastWinner = false;
				}
			}*/
		int PlayerCount = 0;		
			
			for(int i = 0; i < MAX_CLIENTS; ++i)
			{				
			
				CPlayer *pP = GameServer()->m_apPlayers[i];
				if(pP)
				{
					if(pP->GetTeam() != TEAM_SPECTATORS)
						++PlayerCount;				
				}
			}
			
		g_Config.m_SvNumWanted = clamp(g_Config.m_SvNumWanted, 1, PlayerCount-1);		
		if(count < g_Config.m_SvNumWanted)
		{	
			
			if(PlayerCount > 1)
			{
				
				
				srand(time_get());
				for(int a = g_Config.m_SvNumWanted-count; a > 0; --a)
				{					
					 int random = (int)(( rand() % MAX_CLIENTS));
					 int lowestScoreID = -1;
						for(int i = random+1;; ++i)							
						{
							i = i%MAX_CLIENTS;	
							CPlayer *pP = GameServer()->m_apPlayers[i];			
							if(pP && pP->GetTeam() != TEAM_SPECTATORS && !pP->m_IsWanted && !pP->m_LastWinner)
							{
								if(lowestScoreID == -1 || pP->m_Score < GameServer()->m_apPlayers[lowestScoreID]->m_Score)
									lowestScoreID = i;
							}
							if(i == random) //we ran through all players
								break;
						}
						
						if(lowestScoreID != -1)
							GameServer()->m_apPlayers[lowestScoreID]->m_IsWanted = true;
						else
						{
							bool found = false;
							for(int i = 0; i < MAX_CLIENTS; ++i)
							{
								CPlayer *pP = GameServer()->m_apPlayers[i];
								if(pP)
								{
									if(pP->m_LastWinner)
									{									
										found = true;
										pP->m_LastWinner = false;								
									}									
								}
																	
							}
							if(found == false)
								break;
							else
								++a; //try again with all players
						}
				}
			
			}
		}
		else if(count > g_Config.m_SvNumWanted)
		{
			srand(time_get());
			for(int a = count-g_Config.m_SvNumWanted; a > 0; --a)
				{					
					int highestScoreID = -1;
					int random = (int)(( rand() % MAX_CLIENTS));
					for(int i = random+1;; ++i)							
					{
						i = i%MAX_CLIENTS;	
						CPlayer *pP = GameServer()->m_apPlayers[i];			
						if(pP && pP->GetTeam() != TEAM_SPECTATORS && pP->m_IsWanted)
						{
							if(highestScoreID == -1 || pP->m_Score > GameServer()->m_apPlayers[highestScoreID]->m_Score)
								highestScoreID = i;
						}
						if(i == random) //we ran through all players
							break;
					}
					
					if(highestScoreID != -1)
						GameServer()->m_apPlayers[highestScoreID]->m_IsWanted = false;
		
			}
		}
	}else if(m_GameOverTick)
	{
		int winner = -1;
		for(int i = 0; i < MAX_CLIENTS; ++i)
		{
			CPlayer *pP = GameServer()->m_apPlayers[i];
			if(pP)
			{
				if(winner == -1)
				{
					winner = i;
				}
				else if(pP->m_Score > GameServer()->m_apPlayers[winner]->m_Score)
				{
					winner = i;
				}
			}
		}
		if(winner != -1 && GameServer()->m_apPlayers[winner])
			GameServer()->m_apPlayers[winner]->m_LastWinner = true;
	}
}
void CGameControllerDM::Snap(int SnappingClient)
{
	IGameController::Snap(SnappingClient);

	
		CNetObj_GameData *pGameDataObj = (CNetObj_GameData *)Server()->SnapNewItem(NETOBJTYPE_GAMEDATA, 0, sizeof(CNetObj_GameData));
		if(!pGameDataObj)
			return;
	if(g_Config.m_SvWeapon == WEAPON_NINJA && GameServer()->m_apPlayers[SnappingClient] && GameServer()->m_apPlayers[SnappingClient]->m_IsWanted)
	{
		pGameDataObj->m_FlagCarrierRed = SnappingClient;
	}
	else
		pGameDataObj->m_FlagCarrierRed = FLAG_TAKEN;
		
	pGameDataObj->m_FlagCarrierBlue = FLAG_TAKEN;
		
}

int CGameControllerDM::OnCharacterDeath(class CCharacter *pVictim, class CPlayer *pKiller, int WeaponID)
{
	IGameController::OnCharacterDeath(pVictim, pKiller, WeaponID);
	
	if(pVictim->GetPlayer()->m_IsWanted)
	{
		pVictim->GetPlayer()->m_IsWanted = false;
		srand(time_get());
		int lowestScoreID = -1;
		int random = (int)(( rand() % MAX_CLIENTS));
		for(int i = random+1;; ++i)							
		{
			i = i%MAX_CLIENTS;	
			CPlayer *pP = GameServer()->m_apPlayers[i];			
			if(pP && pP != pVictim->GetPlayer() && pP->GetTeam() != TEAM_SPECTATORS && !pP->m_IsWanted)
			{
				if(lowestScoreID == -1 || pP->m_Score < GameServer()->m_apPlayers[lowestScoreID]->m_Score)
					lowestScoreID = i;
			}
			if(i == random) //we ran through all players
				break;
		}
		
		if(lowestScoreID != -1)
			GameServer()->m_apPlayers[lowestScoreID]->m_IsWanted = true;
	}
	return 0;
}