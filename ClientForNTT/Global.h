#ifndef _GLOBAL
#define _GLOBAL
#define LENCH 19
#define ONLINE 0x32
#define NOTEXIST 0x33
#define NOTONLINE 0x34
#define BUSY 0x35
#define REPEATLINE 0x36
#define NETERROR -1
#define RECVMSGTYPE 0x5354454E
#define RECVVATITLE 0x53565848
#define SENDMSGTYPE 0x454C4F48
#define VICO_BUFFER_LENGTH 0x00200000

//�ڴ���1�ֽڶ���
#pragma pack(1)
//�ͻ������ַ��ת���������͵���Ϣ��ʽ[UDPЭ��]
typedef struct UDPSendFormat{
	DWORD type;
	char content;
	char SerUser[LENCH];
	char CliID[LENCH];
	char NONO[9];
}UDPSendFormat;
//��ַ�м�������ظ����ͻ��˵���Ϣ��ʽ[UDPЭ��]
typedef struct UDPRecvFormat{
	DWORD type;
	char content;
	char SerUser[LENCH];
	DWORD SevIP;
	char NONO[2];
	char NONOPORT[2];
	char NONOKey[4];
}UDPRecvFormat;
//�ͻ��������鷢���������Ϣ��ʽ[TCPЭ��]
typedef struct TCPComFormat{
	char mainMsg[16];
	char subMsg[16];
	char shparameter[32];
	char longparameter[64];
}TCPComFormat;
//�ͻ�������վ������Ƶ��ص��������Ϣ��ʽ[TCPЭ��]
typedef struct VioFSend{
	char content[16];
	char parameter[16];
}VioFSend;
//�ͻ��˽��չ���վ�ظ�����Ƶ��ʽ����Ϣ��ʽ[TCPЭ��]
typedef struct VioFRecv{
	char title[16];
	char parameter[48];
}VioFRecv;
//VioFRecv��parameter��ʽ
typedef struct VAFormat{
	DWORD dwTitle;
	DWORD dwWidth;
	DWORD dwHight;
	DWORD dwAudio;
}VAFormat;
//�ڴ����ָ�Ĭ��
#pragma pack()

typedef struct
{
	HI_U32 u32Width;	/* ��Ƶ�� */
	HI_U32 u32Height;	/* ��Ƶ�� */
} HI_S_VideoHeader;

typedef struct
{
    HI_U32 u32Format;	/*��Ƶ��ʽ*/
} HI_S_AudioHeader;

typedef struct 
{
	HI_U32 u32SysFlag;
	HI_S_VideoHeader struVHeader;
	HI_S_AudioHeader struAHeader;
} HI_S_SysHeader;

extern CString g_szLanguagePath;

void g_SetDialogStrings(CDialog *pDlg,UINT uDlgID); //�Ի�����Ի�
CString g_LoadString(CString szID); //���ַ����Ի�
//���� Э�� �˿� IP��ַ
void SetSocketInfo(SOCKADDR_IN&, short, u_short, void*);
//�ͻ������ַ��ת���������������е�3�͵�4���ֶε����ݼ���
BOOL Encrypt(char* buff, int len);
//��ʼ������
void InitNet();

#endif