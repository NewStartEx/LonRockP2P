// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__AE4120D8_263D_470E_9A60_3E5FD19AA6E9__INCLUDED_)
#define AFX_STDAFX_H__AE4120D8_263D_470E_9A60_3E5FD19AA6E9__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC Automation classes
#endif // _AFX_NO_OLE_SUPPORT

#include <MMREG.H>
#include <winsock2.h>
#pragma  comment(lib,"ws2_32.lib")

#include <AFXTEMPL.H>
#pragma comment(lib,"winmm.lib")

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__AE4120D8_263D_470E_9A60_3E5FD19AA6E9__INCLUDED_)
