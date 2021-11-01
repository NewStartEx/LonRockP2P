#ifndef _CLTGLOBAL_H
#define _CLTGLOBAL_H

#include "NTTProtocol.h"

typedef CArray<TNELMSG_GRAM,TNELMSG_GRAM> CTnelMsgArray;
//// 为确保打洞的地址是对的，接到打洞信息后，必须把打洞的地址换成对方发过来的
typedef struct _CNCTDESC {
	OPENTNEL OpenGram;
	SOCKADDR_IN addr;
} CNCTDESC, *PCNCTDESC;
typedef CArray<CNCTDESC,CNCTDESC> CCnctDescArray;
typedef CArray<WORK_GRAM,WORK_GRAM> CMsgGramArray;

#define WM_DVR_STATUS	(WM_USER + 1)
#define WM_DVR_CONTENT  (WM_USER + 2)
#endif // _CLTGLOBAL_H