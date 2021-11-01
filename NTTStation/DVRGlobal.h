#ifndef _DVRGlobal_H
#define _DVRGlobal_H

#include "NTTProtocol.h"

#define DVRINFO_FILE_NAME		"\\DVRInfo.sys"
#define REG_NTTSTATION	"SOFTWARE\\NttStation"
#define REG_NAMEKEY		"Name"
#define REG_PASSKEY		"Password"
#define INFO_VERSION		0x13290E4   //// 20091108

typedef CArray<APPLY_GRAM,APPLY_GRAM> CNetSFBArray;
typedef CArray<TNELMSG_GRAM,TNELMSG_GRAM> CTnelMsgArray;

typedef struct _CNCTDESC {
	OPENTNEL OpenGram;
	SOCKADDR_IN addr;
} CNCTDESC, *PCNCTDESC;
typedef CArray<CNCTDESC,CNCTDESC> CCnctDescArray;

typedef struct _HSHKDESC {
	HSHK_GRAM HshkGram;
	SOCKADDR_IN addr;
} HSHKDESC, *PHSHKDESC;
typedef CArray<HSHKDESC,HSHKDESC> CHshkDescArray;

typedef struct _CMDDESC {
	WORK_GRAM WorkGram;
	SOCKADDR_IN addr;
} CMDDESC, *PCMDDESC;
typedef CArray<CMDDESC,CMDDESC> CCmdDescArray;

typedef struct _VRESDDESC {
	VRESD_GRAM VResdGram;
	SOCKADDR_IN addr;
} VRESDDESC, *PVRESDDESC;
typedef CArray<VRESDDESC,VRESDDESC> CVResdDescArray;

typedef struct _SENDDATADESC {
	int nLength;
	SOCKADDR_IN IPAddr;
	char* pBuf;
} SENDDATADESC, *PSENDDATADESC;
#define SEND_BUF_LENGTH		1024 //// 发送BUF长度 必须大于最大数据报长度
#define SENDDATA_BUFFER_COUNT	1000  //// 发送数据报的缓冲个数

#endif // _DVRGlobal_H