#ifndef _DVRGlobal_H
#define _DVRGlobal_H

#include "NTTProtocol.h"

#define DVRINFO_FILE_NAME		"\\DVRInfo.dat"

//// 向NetS发送命令的返回状态
#define SENDLINK_FAILED		0
#define SENDLINK_SUCCESS	1
#define SENDLINK_EXIST		2
#define SENDLINK_NONAME		3
#define SENDLINK_NONETS		4
#define SENDLINK_ILLEGAL	5

#define INFO_VERSION		0x13290E4   //// 20091108

typedef CArray<APPLY_GRAM,APPLY_GRAM> CNetSFBArray;
typedef CArray<TNELMSG_GRAM,TNELMSG_GRAM> CTnelMsgArray;

typedef struct _CNCTDESC {
	OPENTNEL OpenGram;
	SOCKADDR_IN addr;
} CNCTDESC, *PCNCTDESC;
typedef CArray<CNCTDESC,CNCTDESC> CCnctDescArray;

typedef struct _CMDDESC {
	WORK_GRAM WorkGram;
	SOCKADDR_IN addr;
} CMDDESC, *PCMDDESC;
typedef CArray<CMDDESC,CMDDESC> CCmdDescArray;

//// 通过消息让使用者知道内部行为
//// NetS的回应
#define WM_NETS_FEEDBACK	(WM_USER+1)
//// WPARAM 是NetS返回值
//// 发出打洞的消息
#define WM_NETS_TUNNEL		(WM_USER+2)
//// WPARAM 是对方地址
#define WM_CLT_TUNNEL		(WM_USER+3)
//// WPARAM 是对方地址
//// LPARAM 是最终打洞结果，成功0还是超时1
#define WM_NETS_NORESP		(WM_USER+4)
#endif // _DVRGlobal_H