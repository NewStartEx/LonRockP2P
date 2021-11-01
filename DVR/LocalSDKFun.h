#ifndef _LOCALSDKFUN_H
#define _LOCALSDKFUN_H

///// provide for SDK user ///////////////
typedef int (*CONNECTEDFROMCLIENT)(HANDLE hClient, ULONG addr);
typedef int (*RECVFROMCLIENT)(HANDLE hClient, PVOID pInBuffer,PVOID pOutBuffer);
typedef int (*DISCONNECTEDFROMCLIENT)(HANDLE hClient);

void SetDvrCallback(CONNECTEDFROMCLIENT,RECVFROMCLIENT,DISCONNECTEDFROMCLIENT);

#endif // _LOCALSDKFUN_H