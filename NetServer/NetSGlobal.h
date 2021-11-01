#ifndef _NETS_GLOBAL_H
#define _NETS_GLOBAL_H

#include "NTTProtocol.h"

#define MAX_DVR_COUNT	0x1000000

#define DVR_LIST_FILENAME	"\\NetSDVRList.dat"
#define BACKUP_FILENAME		"\\NetSDVRList.bak"

//// �����ļ��ṹ���ļ�ͷ��־��NETS�� ��������Ŀ
typedef struct _DVR_ITEM {
	DWORD dwType; //// NETS
	BYTE  byAct1; //// DVR_ACT_REGISTER
	char  chName[NAME_BUF_LENGTH];
	char  chPassword[NAME_BUF_LENGTH];  //// ɾ���������ʱ����Ҫ
	BYTE  byAct2;
	DWORD dwDevID[2];
	DWORD dwIPaddr;
	WORD  wReserved; //// Ϊ�Ժ�6�ֽ�IP��ַ����
	WORD  wPort;
	WORD  wLastLinkYear;
	WORD  wLastLinkMonth;
	WORD  wLastLinkDay;
	WORD  wLastLinkHour;
	WORD  wLastLinkMinute;
	WORD  wLastLinkSecond;
	DWORD dwReserved;	
} DVR_ITEM, *PDVR_ITEM; 

typedef struct _DVRDESC {
	APPLY_GRAM dvrGram;
	SOCKADDR_IN addr;
} DVRDESC;

typedef struct _CLTINFO {
	CLTREQ_GRAM cltGram;
	SOCKADDR_IN addr;
} CLTDESC;

typedef CArray<DVRDESC,DVRDESC> CDVRDescArray;
typedef CArray<CLTDESC,CLTDESC> CCltDescArray; 

typedef struct _DVRMSG {
	DWORD dwType;
	char chName[NAME_BUF_LENGTH];
	char byAct;
	DWORD dwDevID[2];
	DWORD dwIPaddr;
} DVRMSG,*PDVRMSG;

typedef struct _CLTMSG {
	BYTE byAct1;
	char chCltName[NAME_BUF_LENGTH];
	char chDvrName[NAME_BUF_LENGTH];
	BYTE byAct2;
	DWORD dwCltAddr;
} CLTMSG, *PCLTMSG;

typedef APPLY_GRAM DVR_INFO,*PDVR_INFO ;

//// Ϊ�˱�̷��㣬ʹ��SendMessage����֪���Ժ�����ʱ����PostMessage�᲻���
//// WPARAM:״̬ LPARAM:����
#define WM_DVR_CHANGED		(WM_USER+0x01)

#define WPARAM_DVR_UNKNOW	0x00
#define WPARAM_DVR_ONLINE	0x01
#define WPARAM_DVR_OFFLINE	0x02
#define WPARAM_DVR_ADDED	0x03
#define WPARAM_DVR_DELETED	0x04
	
//// WPARAM:״̬; LPARAM: �������ơ�Ϊ�˱�֤�����ȶ������Ƴ���ΪNAME_BUF_LENGTH,���һλǿ��Ϊ0
#define WM_CLIENT_REQUEST	(WM_USER+0x11)

#define WPARAM_CLT_WORKING	0x01
#define WPARAM_CLT_NODEVICE	0x02
#define WPARAM_CLT_OFFDEV	0x03

#endif // _NETS_GLOBAL_H