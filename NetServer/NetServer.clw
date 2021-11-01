; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CNetServerDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "NetServer.h"

ClassCount=4
Class1=CNetServerApp
Class2=CNetServerDlg
Class3=CAboutDlg

ResourceCount=4
Resource1=IDD_NETSERVER_DIALOG
Resource2=IDR_MAINFRAME
Resource3=IDD_ABOUTBOX
Class4=CDlgUser
Resource4=IDD_DLGUSER

[CLS:CNetServerApp]
Type=0
HeaderFile=NetServer.h
ImplementationFile=NetServer.cpp
Filter=N
LastObject=CNetServerApp

[CLS:CNetServerDlg]
Type=0
HeaderFile=NetServerDlg.h
ImplementationFile=NetServerDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC
LastObject=CNetServerDlg

[CLS:CAboutDlg]
Type=0
HeaderFile=NetServerDlg.h
ImplementationFile=NetServerDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_NETSERVER_DIALOG]
Type=1
Class=CNetServerDlg
ControlCount=5
Control1=IDC_LST_ONLINEDVR,SysListView32,1350631425
Control2=IDC_TXT_DVRLST,static,1342308352
Control3=IDC_LST_WORKRECORD,listbox,1352728835
Control4=IDC_TXT_CNCTLST,static,1342308352
Control5=IDC_BTN_CLEAR,button,1342242816

[DLG:IDD_DLGUSER]
Type=1
Class=CDlgUser
ControlCount=6
Control1=IDOK,button,1342242817
Control2=IDCANCEL,button,1342242816
Control3=IDC_STATIC_ACCOUNT,static,1342308352
Control4=IDC_STATIC_PASSWORD,static,1342308352
Control5=IDC_EDT_ACCOUNT,edit,1350631552
Control6=IDC_EDT_PASSWORD,edit,1350631584

[CLS:CDlgUser]
Type=0
HeaderFile=DlgUser.h
ImplementationFile=DlgUser.cpp
BaseClass=CDialog
Filter=D
LastObject=CDlgUser
VirtualFilter=dWC

