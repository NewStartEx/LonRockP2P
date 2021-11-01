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
//// DVR 的 状态： 在线，离线
typedef enum _NetSDVRActive 
{
	DVR_Online = 0,
	DVR_Offline
} NetSDVRActive;   //// 是状态字节的第0位
#define DVR_SLEEPING			(1<<1)	 ///// 休眠状态
#define DVR_PERMANENCE			(1<<8)   ///// 有这个标志的不可以从系统中删除

//// Online, Offline 是bit0
#define SETACTIVE(byStatus,bitActive) (byStatus = ((byStatus&0xFE) | bitActive))
#define GETACTIVE(byStatus) (byStatus&0x01)
#define SETSLEEPING(byStatus,bitSleeping) (byStatus = ((byStatus&0xFD) | bitSleeping))
#define GETSLEEPING(byStatus) (byStatus&0x02)
#define SETPERMANECE(byStatus,bitPermanence) (byStatus = ((byStatus&0x7F) | bitPermanence))
#define GETPERMANECE(byStatus) (byStatus&0x80)

#define MAX_DATA_LENGTH		512
#define NAME_BUF_LENGTH		19
#define MIN_NAME_LENGTH		4	//// 最少这么多字符
#define MAX_NAME_LENGTH		16  //// 最多这么多字符
#define MD5_CODE_LENGTH		16

//// 需要注意的是所有不同类别的数据报大小不可以相同

typedef struct _APPLY_GRAM {
	DWORD dwType;
	BYTE  byAct;
	char  chName[NAME_BUF_LENGTH];
	char  chPassword[NAME_BUF_LENGTH];  //// 删除和申请的时候需要
	BYTE  byStatus;  //// 判断状态，比如离线（硬盘里取出来）还是上线等
	DWORD dwDevID[4]; //// 根据序列号的规定
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
	WORD  wReserved; //// 为以后6字节IP地址保留
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
} HSHK_GRAM, *PHSHK_GRAM;  //// 注意包长不能和其他的相同
//// 默认网络状况为局域网
#define DEFAULT_NWID_PACKETS	1000 //// 默认是如此多数据包
#define DEFAULT_NETRELAY_INTERVAL	50 //// 默认如此多毫秒
enum HshkStatus {HshkStart,HshkNavi,HshkFinish};

#define MAIN_CMD_LENGTH	0x10
#define SUB_CMD_LENGTH	0x10
#define CMD_PARAM1_LENGTH	0x20
#define CMD_PARAM2_LENGTH	0x40

//// Gram for command and message
//// 心跳命令的内容：如果没有收到DVR的反馈，则持续发送最后一次发送的命令内容，如果收到，则发送普通心跳内容
//// 这个结构必须和公开的结构是一样的
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

//// 音视频传输：使用这些命令关键字。chParam2[]是要传输的通道，1为要传输，0为停止传输
//// 只有当MSG回传过来的chParam2[] 和 当前
#define CMD_VIDEO	"VIDEO"
#define MSG_VIDEO	"VIDEO"
#define CMD_AUDIO	"AUDIO"
#define MSG_AUDIO	"AUDIO"
#define MSG_EXIT	"EXIT"

#define SUBMSG_ENABLE	"enable"
#define SUBMSG_DISABLE	"disable"

#define ENABLE_VIDEO_BIT	(1<<0)  //// 这一位置1说明要传输视频了
#define ENABLE_AUDIO_BIT	(1<<0)  //// 这一位置1说明要传输音频了
//// 音视频传输包相关数据
//// 根据试验，发现 1200 的时候如果找不到路径，几秒才发送1包；
//// 1000 以下发送很快 , 但有时不知为何才发送几包。
//// 根据相关资料说明得到的548似乎比较好
#define INTERNET_MUP_LENGTH		548 

typedef struct _VPACKHEADER {
	BYTE byChanNum;
	BYTE byFrmInfo;   //// 是否I帧，是否要求回传确认
	BYTE byPackInFrm;  /// 在帧内的包序号
	BYTE byMaxPackNum;  //// 如果已经收到的帧包，这个值为0说明这一帧已经为空
	WORD wPackIndex;  //// 包序列号
	WORD wFrmIndex;
	DWORD dwFrmLength;   //// 整个帧的长度
} VPACKHEADER,*PVPACKHEADER;

#define GETFRM_CONFIRM		(1<<7)  //// byFrmInfo的最高位表示是否要求帧重传确认
#define TRAFFIC_ACK			(1<<6)  //// byFrmInfo的某位表示收到拥塞的标志
#define TRAFFIC_THRESHOLD	40	//// 如果重传的包数大于这个，要求下一个要传I帧，并且发送数据量减小1/3，此时发送端要求新发的I帧确认
#define TRAFFIC_CONTROL		0xFF  //// 发送这个表示发送数据量减小1/3，此时发送端要求新发的I帧确认
#define RESET_DECODE		0xF0  //// 发送这个表示包序列重新复位，从I帧开始发送
#define GETFRM_ACK			0x00  //// 要求重传的帧收到
typedef struct _VRESDPACKET {
	BYTE byChanNum;
	BYTE byResdNum;  
	WORD wFrmIndex; //// 重传I帧确认的时候要用这个
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

// 注册名和密码的存盘加密：用户字段的第3字节是随机数，其他同下，密码字段第3字节是随机数，其他同下
#define RANDOM_FILE_NAMEKEY		2
#define RANDOM_FILE_PASSKEY		7
// 注册名和密码的加密方法： 
// 注册的时候，用户字段的第3字节是随机数(rand)，其他数据是和随机数相加去掉高位，中间补足随机数，最后一位是实际名称长度(包括随机数)+rand
//             密码字段的第7字节是随机数(rand)，其他数据使用MD5编码再和随机数相加去掉高位。剩余两位补随机数
//             反算的时候，注意什么时候高位加1，什么时候不加
// 更新和删除的时候，用户字段第2字节和密码字段第6字节是随机数
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
#define DVR_ACT_EXIT			0x0F  //// 软件退出

#define ACK_REGISTER_SUCCESS	0x11
#define ACK_REGISTER_DUPLICATE	0x12
//// 暂时下面这个字段只回复手动更新
#define ACK_UPDATE_OK			0x13   
//// 更新的信息没有找到。本协议里没有找到的可以即时增加，因为暂时可能因为服务器系统破坏造成文件丢失。
//// 软件里面，使用更新必须登记且名称不可以修改
#define ACK_UPDATE_INVALID		0x14   	   
#define ACK_DELETE_SUCCESS		0x15
//// 没有这个注册信息
#define ACK_DEVICE_NOTEXIST		0x16  
#define ACK_PASSWORD_INVALID	0x17
#define ACK_SERVER_BUSY			0x18
#define ACK_SLEEP_SUCCESS		0x19
#define ACK_DEVICE_SLEEPING		0x1A
#define ACK_WAKEUP_SUCCESS		0x1B
#define ACK_ACCOUNT_ALIVE		0x1C  //// 激活的时候如果该帐号正在使用，就返回这个
#define ACK_OTHER_ERROR			0x20
#define ACK_ILLEGAL_INFO		0x21

//// 名称加密：所有发送名称的第3字节是随机数，其他的是随机数与之相加的和去掉高位，再补足随机数，最后一位是实际长度(包括随机数)+rand
#define RANDOM_TRANS_NAMEKEY	2	
//// 设计目标是相同的客户不会发起两次连接。但实际应用里一个客户连接使用一个唯一标识符，同时两个连接是可能的
//// 这里有一个情况未予考虑：客户端在连接过程中IP地址发生变化(如路由重启或者本身电脑IP变化)
#define TUNNEL_ASKIPADDR		0x30	//// 客户端只要一个地址
#define TUNNEL_REQUIRE			0x31
#define TUNNEL_ACK_SUCCESS		0x32
#define TUNNEL_ACK_NODEVICE		0x33    //// 没有找到目标
#define TUNNEL_ACK_OFFLINE		0x34	//// DVR没有上线
#define TUNNEL_ACK_DVRBUSY		0x35    //// DVR忙
#define TUNNEL_ACK_DUPLINK		0x36	//// 重复连接，选择这个名称。比如使用了IP和名称两种方式。先连接的优先
										//// 注意：如果一个用局域网IP,一个用广域网转发，IP和端口不同，视为不同通道

#define TUNNEL_CMD				0x41	//// 隧道里的命令
#define TUNNEL_MSG				0x42    //// 隧道里的响应
#define CONNECT_CMD				0x43    //// 直接连接的命令
#define CONNECT_MSG				0x44    //// 直接连接的响应

#define VIDEO_ACTVERSION		0x47    //// 视频传输的版本号
#define VIDEO_RESDVERSION		0x48	//// 视频重传的版本号

#define NTT_UNKNOW_ERROR		0xFF

#define HSHK_STARTING			0x81    //// 要求DVR开始建立Client资料（尤其是在直接连接的情况下）
#define HSHK_ACK_STARTING		0x82    //// DVR建立好Client资料。
#define HSHK_NAVIGATING			0x83    //// 要求DVR开始发送测试数据
#define HSHK_NETSTATUS			0x85    //// 网络状况
#define HSHK_ACK_NETSTATUS		0x86    //// DVR确认网络状况
//// DVR to NetServer INFO type and ACK type

#define NETS_NAME  "cs2.lonrock.com"
//// 客户端名称：%02日%02时%02分%02秒%03毫秒+DWORD随机数 共15个字节
//// 通过直连的主控端名称 小数点用随机数代替 
#endif // _NTTPROTOCOL_H