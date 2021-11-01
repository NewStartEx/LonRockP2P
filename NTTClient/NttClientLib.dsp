# Microsoft Developer Studio Project File - Name="NttClientLib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=NttClientLib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NttClientLib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NttClientLib.mak" CFG="NttClientLib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NttClientLib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "NttClientLib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""$/UDP_P2P/NTTClient", STECAAAA"
# PROP Scc_LocalPath "."
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "NttClientLib - Win32 Release"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "..\Inc" /I "..\Public" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "_LIB" /D "MEMVICOC_LIB" /D "NTTCLIENT_LIB" /Yu"stdafx.h" /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG" /d "_AFXDLL"
# ADD RSC /l 0x804 /d "NDEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\bin\NttClientLib.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copy Lib
PostBuild_Cmds=copy ..\bin\NttClientLib.lib e:\thinking\DvrGreat\Lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "NttClientLib - Win32 Debug"

# PROP BASE Use_MFC 2
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 2
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_AFXDLL" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /I "..\Inc" /I "..\Public" /D "_DEBUG" /D "NTTCLIENT_LIB" /D "WIN32" /D "_WINDOWS" /D "_AFXDLL" /D "_MBCS" /D "MEMVICOC_LIB" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG" /d "_AFXDLL"
# ADD RSC /l 0x804 /d "_DEBUG" /d "_AFXDLL"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\bindbg\NttClientLibD.lib"
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Desc=Copy Lib
PostBuild_Cmds=copy ..\bindbg\NttClientLibD.lib e:\thinking\DvrGreat\Lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "NttClientLib - Win32 Release"
# Name "NttClientLib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\CltManager.cpp
# End Source File
# Begin Source File

SOURCE=.\DVRTnel.cpp
# End Source File
# Begin Source File

SOURCE=.\FillVideo.cpp
# End Source File
# Begin Source File

SOURCE=..\Public\Global.cpp
# End Source File
# Begin Source File

SOURCE=.\NttClient.cpp
# End Source File
# Begin Source File

SOURCE=.\NttClient.rc
# End Source File
# Begin Source File

SOURCE=.\NTTClientFun.cpp
# End Source File
# Begin Source File

SOURCE=.\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\CltGlobal.h
# End Source File
# Begin Source File

SOURCE=.\CltManager.h
# End Source File
# Begin Source File

SOURCE=.\DVRTnel.h
# End Source File
# Begin Source File

SOURCE=.\FillVideo.h
# End Source File
# Begin Source File

SOURCE=..\Public\Global.h
# End Source File
# Begin Source File

SOURCE=..\inc\MemVicocDecFun.h
# End Source File
# Begin Source File

SOURCE=.\NttClient.h
# End Source File
# Begin Source File

SOURCE=.\NTTClientFun.h
# End Source File
# Begin Source File

SOURCE=..\Public\NTTProtocol.h
# End Source File
# Begin Source File

SOURCE=.\Resource.h
# End Source File
# Begin Source File

SOURCE=.\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\inc\VicoFormat.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Readme.txt
# End Source File
# End Target
# End Project
