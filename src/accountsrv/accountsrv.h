/* (c) Patrick Rathje based on work by Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com. */
#ifndef ACCOUNTSRV_ACCOUNTSRV_H
#define ACCOUNTSRV_ACCOUNTSRV_H

enum {
	ACCOUNT_STATUS_OK = 0,
	ACCOUNT_STATUS_ERROR,
	ACCOUNT_STATUS_BANNED
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

//convert netadress disgarding the endianess
inline void NetAddressToArray(const NETADDR *pAdress, unsigned char p[]) {
	for(int i = 0; i < 16; ++i) {
		p[i] = pAdress->ip[i];	
	}
	p[16] = (unsigned char)((pAdress->port >> 24)&0xFF);
	p[17] = (unsigned char)((pAdress->port >> 16)&0xFF);
	p[18] = (unsigned char)((pAdress->port >> 8)&0xFF);
	p[19] = (unsigned char)(pAdress->port&0xFF);
	p[20] = (unsigned char)((pAdress->type >> 24)&0xFF);
	p[21] = (unsigned char)((pAdress->type >> 16)&0xFF);
	p[22] = (unsigned char)((pAdress->type >> 8)&0xFF);
	p[23] = (unsigned char)(pAdress->type&0xFF);
}

inline void NetAddressFromArray(NETADDR *pAdress, const unsigned char p[]) {
	for(int i = 0; i < 16; ++i) {
		pAdress->ip[i] = p[i];
	}
	pAdress->port = (p[16]<<24) | (p[17]<<16) | (p[18]<<8) | p[19];
	pAdress->type = (p[20]<<24) | (p[21]<<16) | (p[22]<<8) | p[23];
}

#endif
