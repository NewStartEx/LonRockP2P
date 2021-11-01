#ifndef _CLTGLOBAL_H
#define _CLTGLOBAL_H

#include "NTTProtocol.h"

typedef CArray<TNELMSG_GRAM,TNELMSG_GRAM> CTnelMsgArray;
//// Ϊȷ���򶴵ĵ�ַ�ǶԵģ��ӵ�����Ϣ�󣬱���Ѵ򶴵ĵ�ַ���ɶԷ���������
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
#define SEND_BUF_LENGTH		1024 //// ����BUF���� �������������ݱ�����
#define SENDDATA_BUFFER_COUNT	200  //// �������ݱ��Ļ������
#endif // _CLTGLOBAL_H