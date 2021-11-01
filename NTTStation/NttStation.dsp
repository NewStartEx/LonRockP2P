# Microsoft Developer Studio Project File - Name="NttStation" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=NttStation - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NttStation.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NttStation.mak" CFG="NttStation - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NttStation - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "NttStation - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/UDP_P2P/NTTStation", NUECAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "NttStation - Win32 Release"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\public" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D "NTTSTATION_EXPORTS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x804 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x804 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /machine:I386
# ADD LINK32 /nologo /subsystem:windows /dll /machine:I386 /out:"../bin/NttStation.dll"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=copy lib
PostBuild_Cmds=copy .\release\NttStation.lib ..\lib\NttStation.lib	copy .\release\NttStation.lib e:\thinking\dvrGreat\lib	copy .\release\NttStation.lib e:\thinking\dvrflush\lib	copy ..\bin\NttStation.dll e:\thinking\DvrGreat\bin	copy ..\bin\NttStation.dll e:\thinking\DvrFlush\bin	copy ..\bin\NttStation.dll f:\discuss
# End Special Build Tool

!ELSEIF  "$(CFG)" == "NttStation - Win32 Debug"

# PROP BASE Use_MFC 6
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 6
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\public" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" /D "_AFXDLL" /D "_MBCS" /D "_USRDLL" /D "NTTSTATION_EXPORTS" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x804 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x804 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /dll /debug /machine:I386 /out:"../bindbg/NttStation.dll" /pdbtype:sept
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=copy lib
PostBuild_Cmds=copy .\debug\NttStation.lib ..\lib\NttStation.lib	copy .\debug\NttStation.lib e:\thinking\dvrgreat\lib	copy ..\bindbg\NttStation.dll e:\thinking\dvrgreat\bindbg	copy .\NttStationFun.h e:\thinking\DvrGreat\inc	copy ..\bindbg\NttStation.dll e:\thinking\dvrflush\bindbg	copy .\NttStationFun.h e:\thinking\DvrFlush\inc
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "NttStation - Win32 Release"
# Name "NttStation - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\ChannelInfo.cpp
# End Source File
# Begin Source File

SOURCE=.\ClientTnel.cpp
# End Source File
# Begin Source File

SOURCE=.\DVRManager.cpp
# End Source File
# Begin Source File

SOURCE=..\Public\Global.cpp
# End Source File
# Begin Source File

SOURCE=.\HttpHandle.cpp
# End Source File
# Begin Source File

SOURCE=.\LinkNets.cpp
# End Source File
# Begin Source File

SOURCE=.\md5.cpp
# End Source File
# Begin Source File

SOURCE=.\NttStation.cpp
# End Source File
# Begin Source File

SOURCE=.\NttStation.rc
# End Source File
# Begin Source File

SOURCE=.\NTTStationFun.cpp
# End Source File
# Begin Source File

SOURCE=.\SendVideo.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\ChannelInfo.h
# End Source File
# Begin Source File

SOURCE=.\ClientTnel.h
# End Source File
# Begin Source File

SOURCE=.\DVRGlobal.h
# End Source File
# Begin Source File

SOURCE=.\DVRManager.h
# End Source File
# Begin Source File

SOURCE=..\Public\Global.h
# End Source File
# Begin Source File

SOURCE=.\HttpHandle.h
# End Source File
# Begin Source File

SOURCE=.\LinkNets.h
# End Source File
# Begin Source File

SOURCE=.\md5.h
# End Source File
# Begin Source File

SOURCE=..\Public\NTTProtocol.h
# End Source File
# Begin Source File

SOURCE=.\NttStation.h
# End Source File
# Begin Source File

SOURCE=.\NTTStationFun.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\SendVideo.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\res\NttStation.rc2
# End Source File
# End Group
# Begin Source File

SOURCE=.\ReadMe.txt
# End Source File
# End Target
# End Project
