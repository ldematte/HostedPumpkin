
#ifndef SH_ASSEMBLY_INFO_H_INCLUDED
#define SH_ASSEMBLY_INFO_H_INCLUDED

#include <string>

class AssemblyInfo {
public:
   // The assembly full name - "<assemblyName>, Version=<version>, PublicKeyToken=<token>, Culture=<culture>, ProcessorArchitecture=<architecture>"
   std::wstring FullName;
   // The (fully qualified) path to the assembly dll
   std::wstring AssemblyLoadPath;
   // The (fully qualified) path to the assembly pdb
   std::wstring AssemblyDebugInfoPath;
   
   AssemblyInfo(const std::wstring& fullName, const std::wstring& loadPath, const std::wstring& debugInfoPath);
};

#endif //SH_ASSEMBLY_INFO_H_INCLUDED