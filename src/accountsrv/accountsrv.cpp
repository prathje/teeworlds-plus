/* (c) Patrick Rathje based on work by Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com. */




#include <engine/config.h>
#include <engine/console.h>
#include <engine/kernel.h>
#include <engine/storage.h>

#include <engine/shared/config.h>
#include <engine/shared/netban.h>
#include <engine/shared/network.h>

#include "accountsrv.h"
#include <mastersrv/mastersrv.h>


enum {
	EXPIRE_TIME = 2,
	RELOAD_TIME = 60,
	MTU = 1400,
	MAX_SERVERS_PER_PACKET=75,
	MAX_PACKETS=16,
	MAX_SERVERS=MAX_SERVERS_PER_PACKET*MAX_PACKETS
};

class CRole {
	public:
	char *m_pName;
	
	
	array<const CRole*> m_lIncludingRoles;
	CRole(const char *pName = 0) {
		if(pName) {
			int length = str_length(pName)+1;
			m_pName = new char[length];
			str_copy(m_pName, pName, length);		
		} else {
			m_pName = 0;		
		}
		
	}
	//TODO: if we really want to delete a CRole we have to update all other roles, accounts and server entries too
	~CRole() {
		if(m_pName) {
			delete[] m_pName;
		}
	}
	
	bool Equals(const char *pRoleName) const {
		return m_pName && (str_comp_nocase(m_pName, pRoleName) == 0);
	}
	
	void AddIncludingRole(const CRole *pRole) {
		if(!IncludesRole(pRole)) {
			m_lIncludingRoles.add(pRole);
		} else {
			dbg_msg("accountsrv", "roles: skipped multiple adding from %s in role %s", pRole->m_pName, m_pName ? m_pName : "Account role");		
		}
	}

	bool IncludesRole(const CRole *pRole) const {
		return IncludesRole(pRole->m_pName);	
	}

	bool IncludesRole(const char *pRoleName) const {
		if(Equals(pRoleName)) {
			return true;		
		}
		
		for(int i = 0; i < m_lIncludingRoles.size(); ++i) {
			if(m_lIncludingRoles[i]->IncludesRole(pRoleName)) {
				return true;
			}
		}
		return false;
	}

	void Flatten() {
		//TODO: pack all  combined roles into one role
		//->faster searching
	}
};

static array<CRole*> m_lRoles;

struct CAccount {
	NETADDR m_Address;
	NETADDR m_ServerAddress;
	int64 m_LastActive;
	bool m_Banned;
	bool m_Valid;
	char m_Name[MAX_ACCOUNT_NAME_LENGTH];
	char m_Password[MAX_ACCOUNT_PASSWORD_LENGTH];
	CRole m_Role;
	
	CAccount() {
		m_LastActive = 0;
	}
	
	bool HasRole(const char*pRoleName) const {
		return m_Role.IncludesRole(pRoleName);
	}

	bool HasRole(const CRole *pRole) const {
		return m_Role.IncludesRole(pRole);
	}	
};

static array<CAccount> m_lAccounts;

struct CServerEntry
{
	//enum ServerType m_Type;
	NETADDR m_Address;
	//int64 m_Expire;
	int64 m_LastRequest;
	int m_NumRequest;
	array<const CRole*> m_lRequiredRoles;
	
	CServerEntry() {
		m_LastRequest = 0;
		m_NumRequest = 0;
	}
	
	bool HasRights(const CAccount *pAcc) const {
		
		if(m_lRequiredRoles.size() == 0) {
			return true;
		}
		
		for(int i = 0; i < m_lRequiredRoles.size(); ++i) {
			if(pAcc->HasRole(m_lRequiredRoles[i])) {
				return true;
			}		
		}		
		return false;	
	}	
};

static array<CServerEntry> m_lServers;

struct CPacketData
{
	int m_Size;
	struct {
		unsigned char m_aHeader[sizeof(SERVERBROWSE_LIST)];
		CMastersrvAddr m_aServers[MAX_SERVERS_PER_PACKET];
	} m_Data;
};

CPacketData m_aPackets[MAX_PACKETS];
static int m_NumPackets = 0;

// legacy code
struct CPacketDataLegacy
{
	int m_Size;
	struct {
		unsigned char m_aHeader[sizeof(SERVERBROWSE_LIST_LEGACY)];
		CMastersrvAddrLegacy m_aServers[MAX_SERVERS_PER_PACKET];
	} m_Data;
};

CPacketDataLegacy m_aPacketsLegacy[MAX_PACKETS];
static int m_NumPacketsLegacy = 0;


struct CCountPacketData
{
	unsigned char m_Header[sizeof(SERVERBROWSE_COUNT)];
	unsigned char m_High;
	unsigned char m_Low;
};

static CCountPacketData m_CountData;
static CCountPacketData m_CountDataLegacy;

static CNetClient m_NetOp; // main

IConsole *m_pConsole;
void BuildPacketsAll() {
	m_NumPackets = 0;
	int PacketIndex = 0;
	
	for(int i = 0; i < m_lServers.size() && (m_NumPackets + m_NumPacketsLegacy) < MAX_PACKETS; ++i) {
		const CServerEntry *pCurrent = &m_lServers[i];
		
		//if(pCurrent->m_Type == SERVERTYPE_NORMAL)
		{
			if(PacketIndex % MAX_SERVERS_PER_PACKET == 0)
			{
				PacketIndex = 0;
				m_NumPackets++;
			}

			// copy header
			mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aHeader, SERVERBROWSE_LIST, sizeof(SERVERBROWSE_LIST));

			// copy server addresses
			if(pCurrent->m_Address.type == NETTYPE_IPV6)
			{
				mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp, pCurrent->m_Address.ip,
					sizeof(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp));
			}
			else
			{
				static char IPV4Mapping[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };

				mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp, IPV4Mapping, sizeof(IPV4Mapping));
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[12] = pCurrent->m_Address.ip[0];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[13] = pCurrent->m_Address.ip[1];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[14] = pCurrent->m_Address.ip[2];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[15] = pCurrent->m_Address.ip[3];
			}

			m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aPort[0] = (pCurrent->m_Address.port>>8)&0xff;
			m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aPort[1] = pCurrent->m_Address.port&0xff;

			PacketIndex++;

			m_aPackets[m_NumPackets-1].m_Size = sizeof(SERVERBROWSE_LIST) + sizeof(CMastersrvAddr)*PacketIndex;
		}
	}
}

void BuildPackets(array<const CServerEntry*> lServers)
{
	m_NumPackets = 0;
	int PacketIndex = 0;
	
	for(int i = 0; i < lServers.size() && (m_NumPackets + m_NumPacketsLegacy) < MAX_PACKETS; ++i) {
		const CServerEntry *pCurrent = lServers[i];
		
		//if(pCurrent->m_Type == SERVERTYPE_NORMAL)
		{
			if(PacketIndex % MAX_SERVERS_PER_PACKET == 0)
			{
				PacketIndex = 0;
				m_NumPackets++;
			}

			// copy header
			mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aHeader, SERVERBROWSE_LIST, sizeof(SERVERBROWSE_LIST));

			// copy server addresses
			if(pCurrent->m_Address.type == NETTYPE_IPV6)
			{
				mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp, pCurrent->m_Address.ip,
					sizeof(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp));
			}
			else
			{
				static char IPV4Mapping[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF };

				mem_copy(m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp, IPV4Mapping, sizeof(IPV4Mapping));
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[12] = pCurrent->m_Address.ip[0];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[13] = pCurrent->m_Address.ip[1];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[14] = pCurrent->m_Address.ip[2];
				m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aIp[15] = pCurrent->m_Address.ip[3];
			}

			m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aPort[0] = (pCurrent->m_Address.port>>8)&0xff;
			m_aPackets[m_NumPackets-1].m_Data.m_aServers[PacketIndex].m_aPort[1] = pCurrent->m_Address.port&0xff;

			PacketIndex++;

			m_aPackets[m_NumPackets-1].m_Size = sizeof(SERVERBROWSE_LIST) + sizeof(CMastersrvAddr)*PacketIndex;
		}
	}
}

void SendOk(NETADDR *pAddr)
{
	CNetChunk p;
	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_DataSize = sizeof(SERVERBROWSE_FWOK);
	p.m_pData = SERVERBROWSE_FWOK;

	m_NetOp.Send(&p);
}

void SendError(NETADDR *pAddr)
{
	CNetChunk p;
	p.m_ClientID = -1;
	p.m_Address = *pAddr;
	p.m_Flags = NETSENDFLAG_CONNLESS;
	p.m_DataSize = sizeof(SERVERBROWSE_FWERROR);
	p.m_pData = SERVERBROWSE_FWERROR;
	m_NetOp.Send(&p);
}

CRole *GetRole(const char *pRoleName) {
	for(int i = 0; i < m_lRoles.size(); ++i) {
		if(m_lRoles[i]->Equals(pRoleName)) {
			return m_lRoles[i];
		}
	}
	return 0;
}

//TODO: save this?
NETADDR GetLocalNetAddr() {
	NETADDR Addr;
	Addr.type = NETTYPE_INVALID;
	net_addr_from_str(&Addr, g_Config.m_AccountserverAddress);
	return Addr;
}

bool IsLocal(const NETADDR *a) {
	if(a->ip[0] == (unsigned char)(0x7F) &&
			a->ip[1] == (unsigned char)(0x00) &&
			a->ip[2] == (unsigned char)(0x00) &&
			a->ip[3] == (unsigned char)(0x01)) {
		return true;
	}
	NETADDR local = GetLocalNetAddr();
	if(local.type == NETTYPE_INVALID) {
		return false;
	}
	for(int i = 0; i < 16; ++i) {
		if(a->ip[i] != local.ip[i]) {
			return false;
		}
	}
	return true;
}

int CompareServerAddr(const NETADDR *a, const NETADDR *b) {

	if (IsLocal(a) && IsLocal(b)) {
		return a->port - b->port;
	} else {
		return net_addr_comp(a, b);
	}
}

CServerEntry *GetServer(NETADDR *pInfo) {
	for(int i = 0; i < m_lServers.size(); i++)
	{
		if(CompareServerAddr(&m_lServers[i].m_Address, pInfo) == 0)
		{
			return &m_lServers[i];
		}
	}
	return 0;
}

void AddRole(const char* pRoleName, array<const char*> *plIncludingRoleNames = 0) {
	
	if(str_length(pRoleName) > 0) {
		CRole *pExistingRole = GetRole(pRoleName);
		if(pExistingRole == 0) {
			//role doesnt exist -> add it
			CRole *pRole = new CRole(pRoleName);
			if(plIncludingRoleNames) {
				for(int i = 0; i < plIncludingRoleNames->size(); ++i) {
					CRole *includedRole = GetRole((*plIncludingRoleNames)[i]);
					if(includedRole) {
						pRole->AddIncludingRole(includedRole);
					} else {
						dbg_msg("accountsrv", "role: %s tried to include a non existent role %s", pRoleName, (*plIncludingRoleNames)[i]);
					}
				}			
			}
			dbg_msg("accountsrv", "added role: %s", pRole->m_pName);
			m_lRoles.add(pRole);
		} else {
			//role does exist -> update the roles
			dbg_msg("accountsrv", "roles: tried to add a rolename twice!");
		}
	}
	
}

void AddServer(NETADDR *pInfo, array<const char*> *plRoleNames = 0)
{
	// see if server already exists in list
	CServerEntry *entry = GetServer(pInfo);
	char aAddrStr[NETADDR_MAXSTRSIZE];
	net_addr_str(pInfo, aAddrStr, sizeof(aAddrStr), true);
	if(entry) {
		
		dbg_msg("accountsrv", "server already in list: %s", aAddrStr);
		return;
	}
	

	// add server
	if(m_lServers.size() == MAX_SERVERS)
	{
		dbg_msg("accountsrv", "error: accountsrv is full");
		return;
	}

	
	CServerEntry server;
	server.m_Address = *pInfo;
	
	if(plRoleNames) {
		for(int i = 0; i < plRoleNames->size(); ++i) {
			CRole *role = GetRole((*plRoleNames)[i]);
			if(role) {
				server.m_lRequiredRoles.add(role);
				dbg_msg("accountsrv", "server roles: %s added %s", aAddrStr, (*plRoleNames)[i]);
			} else {
				dbg_msg("accountsrv", "server roles: %s tried to use a non existent role %s", aAddrStr, (*plRoleNames)[i]);
			}
		}
	}
	m_lServers.add(server);
	
	dbg_msg("accountsrv", "added: %s", aAddrStr);
}

CAccount *GetAccount(const char *pName) {
	for(int i = 0; i < m_lAccounts.size(); i++)
	{
		if(str_comp_nocase(m_lAccounts[i].m_Name, pName) == 0)
		{
			return &m_lAccounts[i];
		}
	}
	return 0;
}

CAccount *GetAccount(const NETADDR *pAddr) {
	for(int i = 0; i < m_lAccounts.size(); i++)
	{
		if(net_addr_comp(&m_lAccounts[i].m_Address, pAddr) == 0)
		{
			return &m_lAccounts[i];
		}
	}
	return 0;
}

void AddAccount(const char *pName, const char *pPassword, bool banned = false, array<const char*> *plRoleNames = 0) {

	CAccount *acc = GetAccount(pName);
	
	if(!acc) {
		CAccount newAcc;
		str_copy(newAcc.m_Name, pName, MAX_ACCOUNT_NAME_LENGTH);
		newAcc.m_Valid = false;
		newAcc.m_Banned = banned;
		newAcc.m_LastActive = 0;
		dbg_msg("accountsrv", "added account: name: %s", newAcc.m_Name);
		m_lAccounts.add(newAcc);
		acc = &m_lAccounts[m_lAccounts.size()-1];
	} else {
		dbg_msg("accountsrv", "updating acc: %s", pName);
	}

	str_copy(acc->m_Password, pPassword, MAX_ACCOUNT_PASSWORD_LENGTH);
	
	if(plRoleNames) {
		for(int i = 0; i < plRoleNames->size(); ++i) {
			CRole *includedRole = GetRole((*plRoleNames)[i]);
			if(includedRole) {
				acc->m_Role.AddIncludingRole(includedRole);
				dbg_msg("accountsrv", "acc roles: %s added role %s", pName, (*plRoleNames)[i]);
			} else {
				dbg_msg("accountsrv", "acc roles: %s tried to use a non existent role %s", pName, (*plRoleNames)[i]);
			}
		}
	}
}

array<const CServerEntry*> GetServersForAccount(const CAccount *pAcc) {
	array<const CServerEntry*> lPermittedServers;
	
	if(pAcc != 0 && pAcc->m_Valid && !pAcc->m_Banned) {
		for(int i = 0; i < m_lServers.size(); ++i) {		
			if(m_lServers[i].HasRights(pAcc)) {
				lPermittedServers.add(&m_lServers[i]);
			}		
		}
	}
	return lPermittedServers;
}

int AccountStatus(const CAccount *pAcc, const CServerEntry *pServer) {
	if(pAcc && pAcc->m_Valid) {
			if(pAcc->m_Banned) {
				dbg_msg("WTF", "asd3");
				return ACCOUNT_STATUS_ERROR;			
			}
					
			if(CompareServerAddr(&pAcc->m_ServerAddress, &pServer->m_Address) == 0) {
				if(pServer->HasRights(pAcc)) {
					return ACCOUNT_STATUS_OK;
				}
				dbg_msg("WTF", "rights");
				return ACCOUNT_STATUS_ERROR;
			}
			else {
				dbg_msg("WTF", "net");
			
			}
	}
	return ACCOUNT_STATUS_ERROR;
}


int Login(const char *pName, const char *pPassword) {
	//try to login with the name and password
	CAccount *pAccount = GetAccount(pName);
	
	if(pAccount && str_comp(pAccount->m_Password, pPassword) == 0) {
		//yeah the account exists and the password is correct
		if(pAccount->m_Banned) {
			return ACCOUNT_STATUS_BANNED;
		}
		return ACCOUNT_STATUS_OK;
	}
	return ACCOUNT_STATUS_ERROR;
}

void LoadServers() {

	//for each NETADDR -> AddServer(NETADDR);
	//NETADDR adr;
	//adr.m
}
void LoadAccounts() {
	//for each account (name, pw) -> AddAccount(name, pw);
}

void UpdateAccounts() {

	int64 Now = time_get();
	int64 Freq = time_freq();
	
	for(int i = 0; i < m_lAccounts.size(); ++i) {
		if(m_lAccounts[i].m_Valid && Now > m_lAccounts[i].m_LastActive+ ((int64)EXPIRE_TIME)*Freq) {
			m_lAccounts[i].m_Valid = false;
		}	
	}

}

void Con_AddRole(IConsole::IResult *pResult, void *pUserData){
	if(pResult->NumArguments() >= 1) {
		CRole role(pResult->GetString(0));
		const char *pName = pResult->GetString(0);
		array<const char*> lIncludingRoleNames;
		char * pTemp = 0;
		if(pResult->NumArguments() >= 2) {
			const char *pString = pResult->GetString(1);
			pTemp = new char[str_length(pString) +1];
			str_copy(pTemp, pString, str_length(pString) +1);
			strtok_array(pTemp, " ", lIncludingRoleNames);			
		}	
		AddRole(pName, &lIncludingRoleNames);
		if(pTemp)
			delete[] pTemp;
	}
}
void Con_AddAccount(IConsole::IResult *pResult, void *pUserData){
	if(pResult->NumArguments() >= 2) {
		const char *pName = pResult->GetString(0);
		const char *pPW = pResult->GetString(1);
		array<const char*> lRoleNames;
		char * pTemp = 0;
		if(pResult->NumArguments() >= 3) {
			const char *pString = pResult->GetString(2);
			pTemp = new char[str_length(pString) +1];
			str_copy(pTemp, pString, str_length(pString) +1);
			strtok_array(pTemp, " ", lRoleNames);			
		}		
		AddAccount(pName, pPW, false, &lRoleNames);
		if(pTemp)
			delete[] pTemp;
	}
}
void Con_AddServer(IConsole::IResult *pResult, void *pUserData){
	if(pResult->NumArguments()) {
		const char *pStringAddress = pResult->GetString(0);
		NETADDR addr;
		net_addr_from_str(&addr, pStringAddress);
		array<const char*> lRoleNames;
		char * pTemp = 0;
		if(pResult->NumArguments() >= 2) {
			const char *pString = pResult->GetString(1);
			pTemp = new char[str_length(pString) +1];
			str_copy(pTemp, pString, str_length(pString) +1);
			strtok_array(pTemp, " ", lRoleNames);			
		}		
		AddServer(&addr, &lRoleNames);
		if(pTemp)
			delete[] pTemp;
	}
}
void RegisterCommands()
{
	m_pConsole->Register("add_role", "s?r", CFGFLAG_ACCOUNT, Con_AddRole, 0, "Add a role");
	m_pConsole->Register("add_account", "ss?r", CFGFLAG_ACCOUNT, Con_AddAccount, 0, "Add a player account");
	m_pConsole->Register("add_server", "s?r", CFGFLAG_ACCOUNT, Con_AddServer, 0, "Add a server");
}

int main(int argc, const char **argv) // ignore_convention
{
	int64 LastBuild = 0, LastReload = 0;
	NETADDR BindAddr;

	dbg_logger_stdout();
	net_init();

	mem_copy(m_CountData.m_Header, SERVERBROWSE_COUNT, sizeof(SERVERBROWSE_COUNT));
	mem_copy(m_CountDataLegacy.m_Header, SERVERBROWSE_COUNT_LEGACY, sizeof(SERVERBROWSE_COUNT_LEGACY));

	IKernel *pKernel = IKernel::Create();
	IStorage *pStorage = CreateStorage("Teeworlds", IStorage::STORAGETYPE_BASIC, argc, argv);
	IConfig *pConfig = CreateConfig();
	m_pConsole = CreateConsole(CFGFLAG_ACCOUNT);
	
	bool RegisterFail = !pKernel->RegisterInterface(pStorage);
	RegisterFail |= !pKernel->RegisterInterface(m_pConsole);
	RegisterFail |= !pKernel->RegisterInterface(pConfig);

	if(RegisterFail)
		return -1;

	pConfig->Init();
	//m_NetBan.Init(m_pConsole, pStorage);
	if(argc > 1) // ignore_convention
		m_pConsole->ParseArguments(argc-1, &argv[1]); // ignore_convention
		
	RegisterCommands();
	m_pConsole->ExecuteFile("account_srv.cfg");

	if(g_Config.m_Bindaddr[0] && net_host_lookup(g_Config.m_Bindaddr, &BindAddr, NETTYPE_ALL) == 0)
	{
		// got bindaddr
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}
	else
	{
		mem_zero(&BindAddr, sizeof(BindAddr));
		BindAddr.type = NETTYPE_ALL;
		BindAddr.port = g_Config.m_SvPort;
	}

	if(!m_NetOp.Open(BindAddr, 0))
	{
		dbg_msg("accountsrv", "couldn't start network (op)");
		return -1;
	}

	// process pending commands
	m_pConsole->StoreCommands(false);
	dbg_msg("accountsrv", "started");
	
	LoadServers();
	LoadAccounts();

	while(1)
	{
		m_NetOp.Update();
		
		if(time_get()-LastBuild > time_freq()*5)
		{	
			if(time_get()-LastReload > time_freq()*RELOAD_TIME) {
				m_pConsole->ExecuteFile("roles.cfg");
				m_pConsole->ExecuteFile("accounts.cfg");
				m_pConsole->ExecuteFile("servers.cfg");
				LastReload = time_get();
			}
			
			LastBuild = time_get();
			UpdateAccounts();
			//BuildPackets();
		}
		
		// process m_aPackets
		CNetChunk Packet;
		while(m_NetOp.Recv(&Packet))
		{
			if(Packet.m_DataSize == sizeof(SERVERBROWSE_GETCOUNT) &&
				mem_comp(Packet.m_pData, SERVERBROWSE_GETCOUNT, sizeof(SERVERBROWSE_GETCOUNT)) == 0)
			{
				
				
				CAccount *pAcc = GetAccount(&Packet.m_Address);
				array<const CServerEntry*> lPermittedServers = GetServersForAccount(pAcc);
				
				dbg_msg("accountsrv", "count requested, responding with %d", lPermittedServers.size());
				
				lPermittedServers = GetServersForAccount(pAcc);
				CNetChunk p;
				p.m_ClientID = -1;
				p.m_Address = Packet.m_Address;
				p.m_Flags = NETSENDFLAG_CONNLESS;
				p.m_DataSize = sizeof(m_CountData);
				p.m_pData = &m_CountData;
				m_CountData.m_High = (m_lServers.size()>>8)&0xff;
				m_CountData.m_Low = m_lServers.size()&0xff;
				m_NetOp.Send(&p);
			}
			else if(Packet.m_DataSize == sizeof(SERVERBROWSE_GETLIST) &&
				mem_comp(Packet.m_pData, SERVERBROWSE_GETLIST, sizeof(SERVERBROWSE_GETLIST)) == 0)
			{
				CAccount *pAcc = GetAccount(&Packet.m_Address);
				array<const CServerEntry*> lPermittedServers = GetServersForAccount(pAcc);

				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(&Packet.m_Address, aAddrStr, sizeof(aAddrStr), true);
				
				if(pAcc && pAcc->m_Valid) {				
					// someone requested the list
					dbg_msg("accountsrv", "%s requested, responding with %d servers", aAddrStr, lPermittedServers.size());
				} else {				
					dbg_msg("accountsrv", "bad list request from %s: responding with %d servers", aAddrStr, lPermittedServers.size());
				}
				
				CNetChunk p;
				p.m_ClientID = -1;
				p.m_Address = Packet.m_Address;
				p.m_Flags = NETSENDFLAG_CONNLESS;
				
				BuildPackets(lPermittedServers);
				for(int i = 0; i < m_NumPackets; i++)
				{
					p.m_DataSize = m_aPackets[i].m_Size;
					p.m_pData = &m_aPackets[i].m_Data;
					m_NetOp.Send(&p);
				}
				
			}
			else if(Packet.m_DataSize >= sizeof(ACCOUNTSRV_LOGIN) &&
				mem_comp(Packet.m_pData, ACCOUNTSRV_LOGIN, sizeof(ACCOUNTSRV_LOGIN)) == 0) {
					DecodeData(((char*)Packet.m_pData)+sizeof(ACCOUNTSRV_LOGIN), Packet.m_DataSize-sizeof(ACCOUNTSRV_LOGIN));
					CUnpacker unpacker;
					unpacker.Reset(Packet.m_pData, Packet.m_DataSize);
					//we dont need the header
					unpacker.GetRaw(sizeof(ACCOUNTSRV_LOGIN));
					NETADDR address;
					mem_zero(&address, sizeof(NETADDR));
					UnpackNetAddress(&unpacker, &address);				
					
					const char *pName = unpacker.GetString(CUnpacker::SANITIZE_CC);
					const char *pPassword = unpacker.GetString(CUnpacker::SANITIZE_CC);
									
					char aAddrStr[NETADDR_MAXSTRSIZE];
					net_addr_str(&Packet.m_Address, aAddrStr, sizeof(aAddrStr), true);
					char aAddrStr2[NETADDR_MAXSTRSIZE];
					net_addr_str(&address, aAddrStr2, sizeof(aAddrStr2), true);
					unsigned char response = ACCOUNT_STATUS_ERROR;
					if(unpacker.Error() || str_length(pName) == 0 || str_length(pName) >= MAX_ACCOUNT_NAME_LENGTH || str_length(pPassword) == 0 || str_length(pPassword) >= MAX_ACCOUNT_PASSWORD_LENGTH) {
						dbg_msg("accountsrv", "received bad login from: %s, %s, %s, %s", aAddrStr, pName, pPassword, aAddrStr2);	
					}
					else {
						//check name and password
						response = Login(pName, pPassword);						
						if(response == ACCOUNT_STATUS_OK) {
							CAccount *acc = GetAccount(pName);
							acc->m_ServerAddress = address;
							acc->m_Address = Packet.m_Address;
							acc->m_Valid = true;
							acc->m_LastActive = time_get();
						}
						dbg_msg("accountsrv", "login response: to %s, name %s, response %d", aAddrStr, pName, response);
					}		
					
					static unsigned char aData[sizeof(ACCOUNTSRV_LOGIN_RESPONSE) + 1];
					mem_copy(aData, ACCOUNTSRV_LOGIN_RESPONSE, sizeof(ACCOUNTSRV_LOGIN_RESPONSE));
					
					aData[sizeof(ACCOUNTSRV_LOGIN_RESPONSE)] = response;
					
					CNetChunk p;
					p.m_ClientID = -1;
					p.m_Address = Packet.m_Address;
					p.m_Flags = NETSENDFLAG_CONNLESS;
					p.m_DataSize = sizeof(ACCOUNTSRV_LOGIN_RESPONSE) + 1;
					p.m_pData = &aData;					
					m_NetOp.Send(&p);
				}
			else if(Packet.m_DataSize >= sizeof(ACCOUNTSRV_REQUEST) &&
				mem_comp(Packet.m_pData, ACCOUNTSRV_REQUEST, sizeof(ACCOUNTSRV_REQUEST)) == 0) {
				
				CServerEntry *server = GetServer(&Packet.m_Address);
				char aAddrStr[NETADDR_MAXSTRSIZE];
				net_addr_str(&Packet.m_Address, aAddrStr, sizeof(aAddrStr), true);
				
				if(server) {
					
					CUnpacker unpacker;
					unpacker.Reset(Packet.m_pData, Packet.m_DataSize);
					unpacker.GetRaw(sizeof(ACCOUNTSRV_REQUEST));
					NETADDR clientAddress;
					UnpackNetAddress(&unpacker, &clientAddress);
					const char *pName = unpacker.GetString();
					
					if(str_length(pName) == 0 || str_length(pName) >= MAX_ACCOUNT_NAME_LENGTH || unpacker.Error()) {
						dbg_msg("accountsrv", "bad account request from: %s with name %s", aAddrStr, pName);
						continue;
					}
					CAccount *pAcc = GetAccount(pName);
					int response = AccountStatus(pAcc, server);
					dbg_msg("accountsrv", "request response: server: %s, name %s, response %d", aAddrStr, pName, response);

					CPacker packer;
					packer.Reset();
					packer.AddRaw(ACCOUNTSRV_REQUEST_RESPONSE, sizeof(ACCOUNTSRV_REQUEST_RESPONSE));
					packer.AddString(pName, MAX_ACCOUNT_NAME_LENGTH);
					packer.AddInt(response);
					if(pAcc)
						PackNetAddress(&packer, &pAcc->m_Address);
					
					CNetChunk p;
					p.m_ClientID = -1;
					p.m_Address = Packet.m_Address;
					p.m_Flags = NETSENDFLAG_CONNLESS;
					p.m_DataSize = packer.Size();
					p.m_pData = packer.Data();					
					m_NetOp.Send(&p);
				
				} else {
					//bad request
					dbg_msg("accountsrv", "bad account request from not registered server: %s", aAddrStr);
				}
			}
		}

		// be nice to the CPU
		thread_sleep(1);
	}

	return 0;
}
