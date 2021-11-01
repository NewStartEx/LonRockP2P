#ifndef _LOCALSDKFUN_H
#define _LOCALSDKFUN_H

///// provide for SDK user ///////////////
typedef int (*CONNECTEDDVR)(LPCTSTR strDVR, ULONG addr, BOOL bWork);
typedef int (*RECVFROMDVR)(LPCTSTR strDVR, PVOID pInBuffer);
typedef int (*DISCONNECTEDDVR)(LPCTSTR strDVR,UINT uReason);

void SetCltCallback(CONNECTEDDVR,RECVFROMDVR,DISCONNECTEDDVR);

#endif // _LOCALSDKFUN_H