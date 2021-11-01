; CLW file contains information for the MFC ClassWizard

[General Info]
Version=1
LastClass=CNETCLIDlg
LastTemplate=CDialog
NewFileInclude1=#include "stdafx.h"
NewFileInclude2=#include "NETCLI.h"

ClassCount=3
Class1=CNETCLIApp
Class2=CNETCLIDlg

ResourceCount=3
Resource2=IDD_NETCLI_DIALOG (English (U.S.))
Resource1=IDR_MAINFRAME
Class3=CVideoDlg
Resource3=IDD_VIDEO_DIALOG (English (U.S.))

[CLS:CNETCLIApp]
Type=0
HeaderFile=NETCLI.h
ImplementationFile=NETCLI.cpp
Filter=N

[CLS:CNETCLIDlg]
Type=0
HeaderFile=NETCLIDlg.h
ImplementationFile=NETCLIDlg.cpp
Filter=D
BaseClass=CDialog
VirtualFilter=dWC
LastObject=IDC_BTNSHOW



[DLG:IDD_NETCLI_DIALOG (English (U.S.))]
Type=1
Class=CNETCLIDlg
ControlCount=11
Control1=IDC_BTNCON,button,1342242816
Control2=IDC_BTNSHOW,button,1342242816
Control3=IDC_EDCS2USER,edit,1350631552
Control4=IDC_TEXTCS2USER,static,1342308352
Control5=IDC_EDCOMMANDPORT,edit,1350631552
Control6=IDC_EDWORKPORT,edit,1350631552
Control7=IDC_CMCHANNEL,combobox,1344340227
Control8=IDC_TEXTVAPORT,static,1342308352
Control9=IDC_TEXTCOMPORT,static,1342308352
Control10=IDC_BTNCUT,button,1342242816
Control11=IDC_BTNSTOPSHOW,button,1342242816

[CLS:CVideoDlg]
Type=0
HeaderFile=VideoDlg.h
ImplementationFile=VideoDlg.cpp
BaseClass=CDialog
Filter=D
LastObject=CVideoDlg
VirtualFilter=dWC

[DLG:IDD_VIDEO_DIALOG (English (U.S.))]
Type=1
Class=CVideoDlg
ControlCount=0

