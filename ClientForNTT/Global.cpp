#include "StdAfx.h"
#include "Global.h"
CString g_szLanguagePath;

void g_SetDialogStrings(CDialog *pDlg,UINT uDlgID){
	CString szSection = "String";
	CString szKey,szText;
	bool bSetText = 1;	//1:���ļ��������öԻ���
	//0:�ӶԻ���������浽�ļ�
	
	if(bSetText)	//1:���ļ��������öԻ���
	{
		CString szDefault = "";
		DWORD dwSize = 1000;
		char* pData = (char*)malloc(dwSize);
		
		//���Ի������ //���û�жԻ������
		// 		szKey.Format("IDD%d_Title",uDlgID);
		// 		if(GetPrivateProfileString(szSection,szKey,szDefault,
		// 			pData,dwSize,g_szLanguagePath) != 0)
		// 		{
		// 			pDlg->SetWindowText(pData);
		// 		}
		
		//д������ӿؼ��ı�������
		CWnd* pWnd = pDlg->GetWindow(GW_CHILD);
		while(NULL != pWnd)
		{
			szKey.Format("IDD%d_%d", uDlgID, pWnd->GetDlgCtrlID());
			if(GetPrivateProfileString(szSection, szKey, szDefault,
				pData, dwSize, g_szLanguagePath) != 0)
			{
				pWnd->SetWindowText(pData);
			}
			
			pWnd = pWnd->GetWindow(GW_HWNDNEXT);
		}
		
		//�ͷ��ڴ�
		free(pData);
	}
}

CString g_LoadString(CString szID){
	CString szValue;
	DWORD dwSize = 1000;
	GetPrivateProfileString("String", szID, "Not found",
		szValue.GetBuffer(dwSize), dwSize, g_szLanguagePath);
	szValue.ReleaseBuffer();
	
	szValue.Replace("\\n","\n");	//�滻�ػ��з���
	
	return szValue;
	
}

void SetSocketInfo(SOCKADDR_IN& s, short type, u_short port, void* ip){
	s.sin_family = type;
	s.sin_port = htons(port);
	memmove(&s.sin_addr.s_addr, ip, 4);
}

BOOL Encrypt(char* buff, int len){
	if(len<4 || len>16)
		return FALSE;
	memmove(buff+3, buff+2, len-2);
	buff[LENCH-1] = len;
	buff[2] = 0;
	return TRUE;
}

void InitNet(){
	WORD wVersionRequested;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(2, 2);
	WSAStartup(wVersionRequested, &wsaData);
}
