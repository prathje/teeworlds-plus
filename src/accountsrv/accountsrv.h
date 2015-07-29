/* (c) Patrick Rathje based on work by Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com. */
#ifndef ACCOUNTSRV_ACCOUNTSRV_H
#define ACCOUNTSRV_ACCOUNTSRV_H

#include <engine/shared/packer.h>
#include <base/system.h>

enum {
	ACCOUNT_STATUS_OK = 0,
	ACCOUNT_STATUS_ERROR,
	ACCOUNT_STATUS_BANNED,
	MAX_ACCOUNT_PASSWORD_LENGTH = 64,
	MAX_ACCOUNT_NAME_LENGTH = 64,
};
static const int ACCOUNTSRV_PORT = 8302;

//sent by and send to client

//login: name, password, serveradress
static const unsigned char ACCOUNTSRV_LOGIN[] = {255, 255, 255, 255, 'a', 'c', 'l', 'i'};
//response: int (success)
static const unsigned char ACCOUNTSRV_LOGIN_RESPONSE[] = {255, 255, 255, 255, 'a', 'c', 'l', 'r'};

//sent by and send to server

//request: name
static const unsigned char ACCOUNTSRV_REQUEST[] = {255, 255, 255, 255, 'a', 'c', 'r', '?'};
//response: name, int (success)
static const unsigned char ACCOUNTSRV_REQUEST_RESPONSE[] = {255, 255, 255, 255, 'a', 'c', 'r', '!'};

inline void UnpackNetAddress(CUnpacker *pUnpacker, NETADDR *pAddress) {
	for(int i = 0; i < 16; i+=4) {
		int combined = pUnpacker->GetInt();
		pAddress->ip[i] = (unsigned char)((combined >> 24)&0xFF);
		pAddress->ip[i+1] = (unsigned char)((combined >> 16)&0xFF);
		pAddress->ip[i+2] = (unsigned char)((combined >> 8)&0xFF);
		pAddress->ip[i+3] = (unsigned char)(combined&0xFF);		
	}
	pAddress->port = pUnpacker->GetInt();
	pAddress->type = pUnpacker->GetInt();	
}

inline void PackNetAddress(CPacker *pPacker, const NETADDR *pAddress) {
	for(int i = 0; i < 16; i+=4) {
		int combined = (pAddress->ip[i] << 24) | (pAddress->ip[i+1] << 16) | (pAddress->ip[i+2] << 8) | (pAddress->ip[i+3]);
		pPacker->AddInt(combined);
	}
	pPacker->AddInt(pAddress->port);
	pPacker->AddInt(pAddress->type);
}

#endif
