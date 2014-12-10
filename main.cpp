
#include "Common.h"
#include "Logger.h"

#include "HostCtrl.h"

#include "tclap/CmdLine.h"
#include "tclap/ValueArg.h"
#include "tclap/SwitchArg.h"
#include "tclap/ArgException.h"

#include <string>
#include <iostream>

using namespace TCLAP;
using namespace std;


BSTR toBSTR(const std::string& s) {

   int wslen = ::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
      s.data(), s.length(),
      NULL, 0);

   BSTR wsdata = ::SysAllocStringLen(NULL, wslen);
   ::MultiByteToWideChar(CP_ACP, 0 /* no flags */,
      s.data(), s.length(),
      wsdata, wslen);

   return wsdata;
}

wstring toWstring(const std::string& s) {
   std::wstring ws(s.size(), L' ');
   size_t charsConverted = 0;
   mbstowcs_s(&charsConverted, &ws[0], ws.capacity(), s.c_str(), s.size());
   ws.resize(charsConverted);
   return ws;
}


int main(int argc, char* argv [])
{

   string clrVersion;
   string assemblyFileName;
   string typeName;
   string methodName;

   try {
      CmdLine cmd("Simple CLR Host", ' ', "1.0");

      ValueArg<string> clrVersionArg("v", "clrversion", "CLR version to use", true, "v2.0.50727", "string");
      cmd.add(clrVersionArg);

      ValueArg<string> assemblyFileNameArg("a", "assembly", "The assembly file name", true, "", "string");
      cmd.add(assemblyFileNameArg);

      ValueArg<string> typeNameArg("t", "type", "The type name which contains the method to invoke", false, "", "string");
      cmd.add(typeNameArg);

      ValueArg<string> methodNameArg("m", "method", "The method to invoke", false, "", "string");
      cmd.add(methodNameArg);


      cmd.parse(argc, argv);

      // Get the argument values
      clrVersion = clrVersionArg.getValue();
      assemblyFileName = assemblyFileNameArg.getValue();
      typeName = typeNameArg.getValue();
      methodName = methodNameArg.getValue();
   }
   catch (ArgException &e) {
      cerr << "error: " << e.error() << " for arg " << e.argId() << endl;
   }

   HRESULT hr;

   ICLRMetaHost* metaHost = NULL;
   ICLRRuntimeInfo* runtimeInfo = NULL;

   // 
   // Load and start the .NET runtime.
   //    
   Logger::Info("Load and start the .NET runtime %s", clrVersion.c_str());

   hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_PPV_ARGS(&metaHost));
   if (FAILED(hr)) {
      Logger::Critical("CLRCreateInstance failed w/hr 0x%08lx\n", hr);
   }

   // Get the ICLRRuntimeInfo corresponding to a particular CLR version. It 
   // supersedes CorBindToRuntimeEx with STARTUP_LOADER_SAFEMODE.
   hr = metaHost->GetRuntime(toWstring(clrVersion).c_str(), IID_PPV_ARGS(&runtimeInfo));
   if (FAILED(hr)) {
      metaHost->Release();
      Logger::Critical("ICLRMetaHost::GetRuntime failed w/hr 0x%08lx\n", hr);
   }

   // Check if the specified runtime can be loaded into the process. This 
   // method will take into account other runtimes that may already be 
   // loaded into the process and set pbLoadable to TRUE if this runtime can 
   // be loaded in an in-process side-by-side fashion. 
   BOOL isLoadable;
   hr = runtimeInfo->IsLoadable(&isLoadable);
   if (FAILED(hr)) {
      runtimeInfo->Release();
      metaHost->Release();
      Logger::Critical("ICLRRuntimeInfo::IsLoadable failed w/hr 0x%08lx\n", hr);
   }

   if (!isLoadable) {
      runtimeInfo->Release();
      metaHost->Release(); 
      Logger::Critical(".NET runtime %s cannot be loaded\n", clrVersion);
   }

   // Load the CLR into the current process and return a runtime interface 
   // pointer. ICorRuntimeHost and ICLRRuntimeHost are the two CLR hosting  
   // interfaces supported by CLR 4.0. 
   // The ICorRuntimeHost interface that was provided in .NET v1.x, is compatible with all 
   // .NET Frameworks. 

   //ICorRuntimeHost *corRuntimeHost = NULL;
   //hr = runtimeInfo->GetInterface(CLSID_CorRuntimeHost, IID_PPV_ARGS(&corRuntimeHost));

   ICLRRuntimeHost* clr = NULL;
   hr = runtimeInfo->GetInterface(CLSID_CLRRuntimeHost,
      IID_PPV_ARGS(&clr));
   if (FAILED(hr)) {
      runtimeInfo->Release();
      metaHost->Release();
      Logger::Critical("ICLRRuntimeInfo::GetInterface failed w / hr 0x % 08lx\n", hr);
   }

   // Get the CLR control object
   ICLRControl* clrControl = NULL;
   clr->GetCLRControl(&clrControl);

   // Associate our domain manager
   clrControl->SetAppDomainManagerType(L"SimpleHostRuntime, Version=1.0.0.0, Culture=neutral, PublicKeyToken=9abf81284e6824ad",
      L"SimpleHostRuntime.SimpleHostAppDomainManager");

   // Construct our host control object.
   DHHostControl* hostControl = new DHHostControl(clr);
   clr->SetHostControl(hostControl);

   // Start the CLR.
   hr = clr->Start();
   if (FAILED(hr)) {      
      runtimeInfo->Release();
      metaHost->Release();
      clr->Release();
      Logger::Critical("CLR failed to start w/hr 0x%08lx %s", hr);
      return -1;
   }

   ISimpleHostDomainManager* domainManager = hostControl->GetDomainManagerForDefaultDomain();

   if (!domainManager) {
      Logger::Critical("Cannot obtain the Domain Manager for the Default Domain");
      runtimeInfo->Release();
      metaHost->Release();
      clr->Release();

      return -1;
   }

   long retVal = 0L;
   BSTR assemblyName = toBSTR(assemblyFileName);
   HRESULT hrExecute = domainManager->raw_Run(assemblyName, &retVal);

   if (!SUCCEEDED(hrExecute))
      Logger::Critical("Execution of method failed: 0x%x", hrExecute);

   // Stop the CLR and cleanup.
   domainManager->Release();
   hostControl->ShuttingDown();
   clr->Stop();
   clr->Release();

   return retVal;
}
