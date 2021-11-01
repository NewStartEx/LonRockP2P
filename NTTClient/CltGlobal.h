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

typedef struct _SENDDATADESC {
	int nLength;
	SOCKADDR_IN IPAddr;
	char* pBuf;
} SENDDATADESC, *PSENDDATADESC;
#define SEND_BUF_LENGTH		1024 //// 发送BUF长度 必须大于最大数据报长度
#define SENDDATA_BUFFER_COUNT	200  //// 发送数据报的缓冲个数
#endif // _CLTGLOBAL_H