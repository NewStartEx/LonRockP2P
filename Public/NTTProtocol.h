// NTT means Net Tunnel Transfering.
// USING UDP P2P technology.
// 2009.10.16 version 1.0

#ifndef _NTTPROTOCOL_H
#define _NTTPROTOCOL_H

#define NETSERVER_PORT	6911
#define CLIENT_PORT		6913
#define DVR_PORT		6912

#ifndef MAKE_FOURCC
#define MAKE_FOURCC( ch0, ch1, ch2, ch3 )                       \
        ( (DWORD)(BYTE)(ch0) | ( (DWORD)(BYTE)(ch1) << 8 ) |    \
        ( (DWORD)(BYTE)(ch2) << 16 ) | ( (DWORD)(BYTE)(ch3) << 24 ) )
#endif

#define FOURCC_CARD		MAKE_FOURCC('C','A','R','D')   //// DVR CARD
#define FOURCC_MDVR		MAKE_FOURCC('M','D','V','R')   //// MOBILE DVR
#define FOURCC_EDVR		MAKE_FOURCC('E','D','V','R')   //// EMBEDDED DVR
#define FOURCC_NCAM		MAKE_FOURCC('N','C','A','M')   //// IP CAM OR VIDEO SERVER
#define FOURCC_NDVR		MAKE_FOURCC('N','D','V','R')   //// NVR

#define FOURCC_NETS		MAKE_FOURCC('N','E','T','S')   //// NetServer's Reply Gram Head.
#define FOURCC_HOLE		MAKE_FOURCC('H','O','L','E')   //// Tunnel Requirement Head. Client -> NetServer, NetServer -> DVR
#define FOURCC_OPEN		MAKE_FOURCC('O','P','E','N')   //// Tunnel Creating Head. Client <-> DVR
#define FOURCC_HSHK		MAKE_FOURCC('H','S','H','K')   //// Handshaking Head. Client <-> DVR
#define FOURCC_PUSH		MAKE_FOURCC('P','U','S','H')   //// Normal Command Head. Client <-> DVR
#define FOURCC_DATA		MAKE_FOURCC('D','A','T','A')   //// Data(video,audio,files) Sending Head. DVR -> Client
#define FOURCC_RESD		MAKE_FOURCC('R','E','S','D')   //// Resend Data Command Head. Client -> DVR
#define FOURCC_VIDE		MAKE_FOURCC('V','I','D','E')   //// Video data. DVR -> Client   
#define FOURCC_AUDI		MAKE_FOURCC('A','U','D','I')   //// Audio data. DVR -> Client
#define FOURCC_NWID		MAKE_FOURCC('N','W','I','D')   //// NetWidth Investigat Data. DVR->Client

#define FOURCC_FEND		MAKE_FOURCC('F','E','N','D')   //// File End ID. put in the end of NetSDVRList.dat
//// DVR and NetServer communicating
//// DVR �� ״̬�� ���ߣ�����
typedef enum _NetSDVRActive 
{
	DVR_Online = 0,
	DVR_Offline
} NetSDVRActive;   //// ��״̬�ֽڵĵ�0λ
#define DVR_SLEEPING			(1<<1)	 ///// ����״̬
#define DVR_PERMANENCE			(1<<8)   ///// �������־�Ĳ����Դ�ϵͳ��ɾ��

//// Online, Offline ��bit0
#define SETACTIVE(byStatus,bitActive) (byStatus = ((byStatus&0xFE) | bitActive))
#define GETACTIVE(byStatus) (byStatus&0x01)
#define SETSLEEPING(byStatus,bitSleeping) (byStatus = ((byStatus&0xFD) | bitSleeping))
#define GETSLEEPING(byStatus) (byStatus&0x02)
#define SETPERMANECE(byStatus,bitPermanence) (byStatus = ((byStatus&0x7F) | bitPermanence))
#define GETPERMANECE(byStatus) (byStatus&0x80)

#define MAX_DATA_LENGTH		512
#define NAME_BUF_LENGTH		19
#define MIN_NAME_LENGTH		4	//// ������ô���ַ�
#define MAX_NAME_LENGTH		16  //// �����ô���ַ�
#define MD5_CODE_LENGTH		16

//// ��Ҫע��������в�ͬ�������ݱ���С��������ͬ

typedef struct _APPLY_GRAM {
	DWORD dwType;
	BYTE  byAct;
	char  chName[NAME_BUF_LENGTH];
	char  chPassword[NAME_BUF_LENGTH];  //// ɾ���������ʱ����Ҫ
	BYTE  byStatus;  //// �ж�״̬���������ߣ�Ӳ����ȡ�������������ߵ�
	DWORD dwDevID[4]; //// �������кŵĹ涨
	DWORD dwReserved;
} APPLY_GRAM, *PAPPLY_GRAM;

////Gram to Client Tunnel requirment. To NetServer
typedef struct _CLTREQ_GRAM {
	DWORD  dwType;
	BYTE   byAct;    //// 0x31
	char   chDstName[NAME_BUF_LENGTH];
	char   chName[NAME_BUF_LENGTH];
	char   chReserved[9];
} CLTREQ_GRAM, *PCLTREQ_GRAM;

////Gram that NetServer send to Client and DVR to Creat Tunnel.
typedef struct _TNELMSG_GRAM {
	DWORD dwType; //// to client: NETS; to DVR: HOLE
	BYTE  byAct;  //// to client: 0x32,0x33; to DVR: 0x31
	char  chDstName[NAME_BUF_LENGTH];
	DWORD dwDstIPaddr;
	WORD  wReserved; //// Ϊ�Ժ�6�ֽ�IP��ַ����
	WORD  wDstPort;
	DWORD dwReserved;
} TNELMSG_GRAM,*PTNELMSG_GRAM;

////Gram for creating tunnel
////When Send information, FirstName is LocalName, SecondName is DestName
////For DVR, LocalName is Client asked, can be IP name or Registered name.
////If Client Send to same DVR with different Name,reply LocalName is Client asked,
////SecondName is another name has been used
typedef struct _OPENTNEL {
	DWORD dwType; //// OPEN
	BYTE  byAct;  //// Send: 0x31; ACK: 0x32
	char  chFirstName[NAME_BUF_LENGTH];
	char  chSecondName[NAME_BUF_LENGTH]; 
	BYTE  byReserverd;
	DWORD dwReserved;
} OPENTNEL, *POPENTNEL;

////Gram for handshaking when connection is creating. Vico 2010-5-2
////handshaking 1: Client sends Act creating(0x81) to DVR, DVR's ACK is ready(0x82)
////            2: When client gets creating ready(0x82), sends network navigating(0x83) to DVR, DVR sends 1000ms data
////            3: When client gets 1000ms data, sends network status(0x85) to DVR(packets number and interval),
////               DVR sends confirm(0x86)
////            4: When client gets confirm(0x86), start to send first cmd
typedef struct _HSHK_GRAM {
	DWORD dwType; //// HSHK
	BYTE  byAct; 
	char  chDestName[NAME_BUF_LENGTH]; 
	char  chLocalName[NAME_BUF_LENGTH];
	BYTE  byReserved;   //// reserved
	DWORD dwParam1;   //// When client sends network status is Packets number per second
	DWORD dwParam2;   //// When client sends network status is time interval(ms)
	DWORD dwReserved[2];
} HSHK_GRAM, *PHSHK_GRAM;  //// ע��������ܺ���������ͬ
//// Ĭ������״��Ϊ������
#define DEFAULT_NWID_PACKETS	1000 //// Ĭ������˶����ݰ�
#define DEFAULT_NETRELAY_INTERVAL	50 //// Ĭ����˶����
enum HshkStatus {HshkStart,HshkNavi,HshkFinish};

#define MAIN_CMD_LENGTH	0x10
#define SUB_CMD_LENGTH	0x10
#define CMD_PARAM1_LENGTH	0x20
#define CMD_PARAM2_LENGTH	0x40

//// Gram for command and message
//// ������������ݣ����û���յ�DVR�ķ�����������������һ�η��͵��������ݣ�����յ���������ͨ��������
//// ����ṹ����͹����Ľṹ��һ����
typedef struct _CMDINFO {
	char  chCmd[MAIN_CMD_LENGTH];
	char  chSubCmd[SUB_CMD_LENGTH];
	char  chParam1[CMD_PARAM1_LENGTH];
	char  chParam2[CMD_PARAM2_LENGTH];
} CMDINFO, *PCMDINFO;

typedef struct _WORK_GRAM {
	DWORD dwType; //// PUSH
	BYTE  byAct;  //// tunnel cmd:0x36; tunnel msg:0x37; direct cmd:0x38; direct msg:0x39
	char  chDestName[NAME_BUF_LENGTH];
	char  chLocalName[NAME_BUF_LENGTH];
	BYTE  byReserved;
	CMDINFO Info;
} WORK_GRAM,*PWORK_GRAM;

//// ����Ƶ���䣺ʹ����Щ����ؼ��֡�chParam2[]��Ҫ�����ͨ����1ΪҪ���䣬0Ϊֹͣ����
//// ֻ�е�MSG�ش�������chParam2[] �� ��ǰ
#define CMD_VIDEO	"VIDEO"
#define MSG_VIDEO	"VIDEO"
#define CMD_AUDIO	"AUDIO"
#define MSG_AUDIO	"AUDIO"
#define MSG_EXIT	"EXIT"

#define SUBMSG_ENABLE	"enable"
#define SUBMSG_DISABLE	"disable"

#define ENABLE_VIDEO_BIT	(1<<0)  //// ��һλ��1˵��Ҫ������Ƶ��
#define ENABLE_AUDIO_BIT	(1<<0)  //// ��һλ��1˵��Ҫ������Ƶ��
//// ����Ƶ������������
//// �������飬���� 1200 ��ʱ������Ҳ���·��������ŷ���1����
//// 1000 ���·��ͺܿ� , ����ʱ��֪Ϊ�βŷ��ͼ�����
//// �����������˵���õ���548�ƺ��ȽϺ�
#define INTERNET_MUP_LENGTH		548 

typedef struct _VPACKHEADER {
	BYTE byChanNum;
	BYTE byFrmInfo;   //// �Ƿ�I֡���Ƿ�Ҫ��ش�ȷ��
	BYTE byPackInFrm;  /// ��֡�ڵİ����
	BYTE byMaxPackNum;  //// ����Ѿ��յ���֡�������ֵΪ0˵����һ֡�Ѿ�Ϊ��
	WORD wPackIndex;  //// �����к�
	WORD wFrmIndex;
	DWORD dwFrmLength;   //// ����֡�ĳ���
} VPACKHEADER,*PVPACKHEADER;

#define GETFRM_CONFIRM		(1<<7)  //// byFrmInfo�����λ��ʾ�Ƿ�Ҫ��֡�ش�ȷ��
#define TRAFFIC_ACK			(1<<6)  //// byFrmInfo��ĳλ��ʾ�յ�ӵ���ı�־
#define TRAFFIC_THRESHOLD	40	//// ����ش��İ������������Ҫ����һ��Ҫ��I֡�����ҷ�����������С1/3����ʱ���Ͷ�Ҫ���·���I֡ȷ��
#define TRAFFIC_CONTROL		0xFF  //// ���������ʾ������������С1/3����ʱ���Ͷ�Ҫ���·���I֡ȷ��
#define RESET_DECODE		0xF0  //// ���������ʾ���������¸�λ����I֡��ʼ����
#define GETFRM_ACK			0x00  //// Ҫ���ش���֡�յ�
typedef struct _VRESDPACKET {
	BYTE byChanNum;
	BYTE byResdNum;  
	WORD wFrmIndex; //// �ش�I֡ȷ�ϵ�ʱ��Ҫ�����
	WORD wResdIndex[TRAFFIC_THRESHOLD];
} VRESDPACKET, *PVRESDPACKET;

typedef struct _DATA_GRAM_HEADER {
	DWORD dwType; //// VIDE,AUDI,RESD,NWID
	BYTE  byAct;  //// reserved;
	char  chDestName[NAME_BUF_LENGTH];
	char  chLocalName[NAME_BUF_LENGTH];
	BYTE  byReserved;
} DATA_GRAM_HEADER,*PDATA_GRAM_HEADER;

#define MAX_VPDATA_LENGTH		(INTERNET_MUP_LENGTH-sizeof(DATA_GRAM_HEADER)-sizeof(VPACKHEADER)) 
typedef struct _VIDEO_GRAM {
	DATA_GRAM_HEADER gramHeader;
	VPACKHEADER vHeader;
	BYTE vData[1];
} VIDEO_GRAM, *PVIDEO_GRAM;

typedef struct _VRESD_GRAM {
	DATA_GRAM_HEADER gramHeader;
	VRESDPACKET vResdPacket;
} VRESD_GRAM, *PVRESD_GRAM;

// ע����������Ĵ��̼��ܣ��û��ֶεĵ�3�ֽ��������������ͬ�£������ֶε�3�ֽ��������������ͬ��
#define RANDOM_FILE_NAMEKEY		2
#define RANDOM_FILE_PASSKEY		7
// ע����������ļ��ܷ����� 
// ע���ʱ���û��ֶεĵ�3�ֽ��������(rand)�����������Ǻ���������ȥ����λ���м䲹������������һλ��ʵ�����Ƴ���(���������)+rand
//             �����ֶεĵ�7�ֽ��������(rand)����������ʹ��MD5�����ٺ���������ȥ����λ��ʣ����λ�������
//             �����ʱ��ע��ʲôʱ���λ��1��ʲôʱ�򲻼�
// ���º�ɾ����ʱ���û��ֶε�2�ֽں������ֶε�6�ֽ��������
#define RANDOM_APPLY_NAMEKEY		1
#define RANDOM_APPLY_PASSKEY		5
#define RANDOM_FEEDBACK_NAMEKEY		3
#define RANDOM_FEEDBACK_PASSKEY		9
// Action and ACK Content
#define DVR_ACT_REGISTER		0x01
#define DVR_ACT_AUTOUPDATE		0x02
#define DVR_ACT_MANUALUPDATE	0x03
#define DVR_ACT_DELETE			0x04
#define DVR_ACT_SLEEPDOWN		0x05
#define DVR_ACT_WAKEUP			0x06
#define DVR_ACT_EXIT			0x0F  //// ����˳�

#define ACK_REGISTER_SUCCESS	0x11
#define ACK_REGISTER_DUPLICATE	0x12
//// ��ʱ��������ֶ�ֻ�ظ��ֶ�����
#define ACK_UPDATE_OK			0x13   
//// ���µ���Ϣû���ҵ�����Э����û���ҵ��Ŀ��Լ�ʱ���ӣ���Ϊ��ʱ������Ϊ������ϵͳ�ƻ�����ļ���ʧ��
//// ������棬ʹ�ø��±���Ǽ������Ʋ������޸�
#define ACK_UPDATE_INVALID		0x14   	   
#define ACK_DELETE_SUCCESS		0x15
//// û�����ע����Ϣ
#define ACK_DEVICE_NOTEXIST		0x16  
#define ACK_PASSWORD_INVALID	0x17
#define ACK_SERVER_BUSY			0x18
#define ACK_SLEEP_SUCCESS		0x19
#define ACK_DEVICE_SLEEPING		0x1A
#define ACK_WAKEUP_SUCCESS		0x1B
#define ACK_ACCOUNT_ALIVE		0x1C  //// �����ʱ��������ʺ�����ʹ�ã��ͷ������
#define ACK_OTHER_ERROR			0x20
#define ACK_ILLEGAL_INFO		0x21

//// ���Ƽ��ܣ����з������Ƶĵ�3�ֽ�������������������������֮��ӵĺ�ȥ����λ���ٲ�������������һλ��ʵ�ʳ���(���������)+rand
#define RANDOM_TRANS_NAMEKEY	2	
//// ���Ŀ������ͬ�Ŀͻ����ᷢ���������ӡ���ʵ��Ӧ����һ���ͻ�����ʹ��һ��Ψһ��ʶ����ͬʱ���������ǿ��ܵ�
//// ������һ�����δ�迼�ǣ��ͻ��������ӹ�����IP��ַ�����仯(��·���������߱������IP�仯)
#define TUNNEL_ASKIPADDR		0x30	//// �ͻ���ֻҪһ����ַ
#define TUNNEL_REQUIRE			0x31
#define TUNNEL_ACK_SUCCESS		0x32
#define TUNNEL_ACK_NODEVICE		0x33    //// û���ҵ�Ŀ��
#define TUNNEL_ACK_OFFLINE		0x34	//// DVRû������
#define TUNNEL_ACK_DVRBUSY		0x35    //// DVRæ
#define TUNNEL_ACK_DUPLINK		0x36	//// �ظ����ӣ�ѡ��������ơ�����ʹ����IP���������ַ�ʽ�������ӵ�����
										//// ע�⣺���һ���þ�����IP,һ���ù�����ת����IP�Ͷ˿ڲ�ͬ����Ϊ��ͬͨ��

#define TUNNEL_CMD				0x41	//// ����������
#define TUNNEL_MSG				0x42    //// ��������Ӧ
#define CONNECT_CMD				0x43    //// ֱ�����ӵ�����
#define CONNECT_MSG				0x44    //// ֱ�����ӵ���Ӧ

#define VIDEO_ACTVERSION		0x47    //// ��Ƶ����İ汾��
#define VIDEO_RESDVERSION		0x48	//// ��Ƶ�ش��İ汾��

#define NTT_UNKNOW_ERROR		0xFF

#define HSHK_STARTING			0x81    //// Ҫ��DVR��ʼ����Client���ϣ���������ֱ�����ӵ�����£�
#define HSHK_ACK_STARTING		0x82    //// DVR������Client���ϡ�
#define HSHK_NAVIGATING			0x83    //// Ҫ��DVR��ʼ���Ͳ�������
#define HSHK_NETSTATUS			0x85    //// ����״��
#define HSHK_ACK_NETSTATUS		0x86    //// DVRȷ������״��
//// DVR to NetServer INFO type and ACK type

#define NETS_NAME  "cs2.lonrock.com"
//// �ͻ������ƣ�%02��%02ʱ%02��%02��%03����+DWORD����� ��15���ֽ�
//// ͨ��ֱ�������ض����� С��������������� 
#endif // _NTTPROTOCOL_H