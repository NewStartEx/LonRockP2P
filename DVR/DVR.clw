; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CDVRDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "DVR.h"

ClassCount=3
Class1=CDVRApp
Class2=CDVRDlg
Class3=CAboutDlg

ResourceCount=3
Resource1=IDD_ABOUTBOX
Resource2=IDR_MAINFRAME
Resource3=IDD_DVR_DIALOG

[CLS:CDVRApp]
Type=0
HeaderFile=DVR.h
ImplementationFile=DVR.cpp
Filter=N

[CLS:CDVRDlg]
Type=0
HeaderFile=DVRDlg.h
ImplementationFile=DVRDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC
LastObject=CDVRDlg

[CLS:CAboutDlg]
Type=0
HeaderFile=DVRDlg.h
ImplementationFile=DVRDlg.cpp
Filter=D

[DLG:IDD_ABOUTBOX]
Type=1
Class=CAboutDlg
ControlCount=4
Control1=IDC_STATIC,static,1342177283
Control2=IDC_STATIC,static,1342308480
Control3=IDC_STATIC,static,1342308352
Control4=IDOK,button,1342373889

[DLG:IDD_DVR_DIALOG]
Type=1
Class=CDVRDlg
ControlCount=11
Control1=IDOK,button,1342242817
Control2=IDC_STATIC,static,1342308352
Control3=IDC_EDT_KEYNAME,edit,1350631552
Control4=IDC_BTN_APPLY,button,1342242816
Control5=IDC_BTN_TEST,button,1342242816
Control6=IDC_BTN_DELETE,button,1342242816
Control7=IDC_TXT_INFO,static,1342308352
Control8=IDC_EDT_STRING,edit,1350631552
Control9=IDC_STATIC,static,1342308352
Control10=IDC_STATIC,button,1342177287
Control11=IDC_EDT_PASSWORD,edit,1350631552

