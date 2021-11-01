#ifndef _DVRGlobal_H
#define _DVRGlobal_H

#include "NTTProtocol.h"

#define DVRINFO_FILE_NAME		"\\DVRInfo.dat"

//// ��NetS��������ķ���״̬
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

//// ͨ����Ϣ��ʹ����֪���ڲ���Ϊ
//// NetS�Ļ�Ӧ
#define WM_NETS_FEEDBACK	(WM_USER+1)
//// WPARAM ��NetS����ֵ
//// �����򶴵���Ϣ
#define WM_NETS_TUNNEL		(WM_USER+2)
//// WPARAM �ǶԷ���ַ
#define WM_CLT_TUNNEL		(WM_USER+3)
//// WPARAM �ǶԷ���ַ
//// LPARAM �����մ򶴽�����ɹ�0���ǳ�ʱ1
#define WM_NETS_NORESP		(WM_USER+4)
#endif // _DVRGlobal_H