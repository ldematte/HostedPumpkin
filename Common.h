#ifndef __HOSTCOMMON__H__
#define __HOSTCOMMON__H__

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#ifndef _WIN32_WINNT
#define _WIN32_WINNT _WIN32_WINNT_WINXP
#endif

#define LOCK_TRACE 0xCAFEBABE

#include <stdio.h>
#include <tchar.h>

#include <metahost.h>
#pragma comment(lib, "mscoree.lib")

// Import mscorlib.tlb (Microsoft Common Language Runtime Class Library).
#import "mscorlib.tlb" raw_interfaces_only				\
    high_property_prefixes("_get","_put","_putref")		\
    rename("ReportEvent", "InteropServices_ReportEvent")
using namespace mscorlib;
//

#include <string>

std::wstring toWstring(const std::string& s);
std::string toString(const std::wstring& s);
bstr_t toBSTR(const std::string& s);

std::wstring CurrentDirectory();


#endif