#include "stdafx.h"
#include "LocalSDKFun.h"

CONNECTEDFROMCLIENT g_ConnectedFromClient = NULL;
RECVFROMCLIENT g_RecvFromClient = NULL;
DISCONNECTEDFROMCLIENT g_DisconnectedFromClient = NULL;

void SetDvrCallback(CONNECTEDFROMCLIENT ConnectedFromClient,RECVFROMCLIENT RecvFromClient,DISCONNECTEDFROMCLIENT DisconnectedFromClient)
{
	g_ConnectedFromClient = ConnectedFromClient;
	g_RecvFromClient = RecvFromClient;
	g_DisconnectedFromClient = DisconnectedFromClient;
}

