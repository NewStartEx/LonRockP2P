#ifndef _NETS_GLOBAL_H
#define _NETS_GLOBAL_H

#include "NTTProtocol.h"

#define MAX_DVR_COUNT	0x1000000

#define DVR_LIST_FILENAME	"\\NetSDVRList.dat"
#define BACKUP_FILENAME		"\\NetSDVRList.bak"

//// 存盘文件结构：文件头标志：NETS， 数量，条目
typedef struct _DVR_ITEM {
	DWORD dwType; //// NETS
	BYTE  byAct1; //// DVR_ACT_REGISTER
	char  chName[NAME_BUF_LENGTH];
	char  chPassword[NAME_BUF_LENGTH];  //// 删除和申请的时候需要
	BYTE  byAct2;
	DWORD dwDevID[2];
	DWORD dwIPaddr;
	WORD  wReserved; //// 为以后6字节IP地址保留
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

//// 为了编程方便，使用SendMessage。不知道以后海量的时候，用PostMessage会不会好
//// WPARAM:状态 LPARAM:名称
#define WM_DVR_CHANGED		(WM_USER+0x01)

#define WPARAM_DVR_UNKNOW	0x00
#define WPARAM_DVR_ONLINE	0x01
#define WPARAM_DVR_OFFLINE	0x02
#define WPARAM_DVR_ADDED	0x03
#define WPARAM_DVR_DELETED	0x04
	
//// WPARAM:状态; LPARAM: 两个名称。为了保证程序稳定，名称长度为NAME_BUF_LENGTH,最后一位强制为0
#define WM_CLIENT_REQUEST	(WM_USER+0x11)

#define WPARAM_CLT_WORKING	0x01
#define WPARAM_CLT_NODEVICE	0x02
#define WPARAM_CLT_OFFDEV	0x03

#endif // _NETS_GLOBAL_H