
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





int main(int argc, char* argv [])
{

   string clrVersion;
   string assemblyFileName;
   string typeName;
   string methodName;
   bool useSandbox = false;

   CmdLine cmd("Simple CLR Host", ' ', "1.0");
   try {     

      ValueArg<string> clrVersionArg("v", "clrversion", "CLR version to use", true, "v2.0.50727", "string");
      cmd.add(clrVersionArg);

      ValueArg<string> assemblyFileNameArg("a", "assembly", "The assembly file name", true, "", "string");
      cmd.add(assemblyFileNameArg);

      ValueArg<string> typeNameArg("t", "type", "The type name which contains the method to invoke", false, "", "string");
      cmd.add(typeNameArg);

      ValueArg<string> methodNameArg("m", "method", "The method to invoke", false, "", "string");
      cmd.add(methodNameArg);

      SwitchArg useSandboxArg("s", "sandbox", "Run in a sandbox AppDomain", false);
      cmd.add(useSandboxArg);
      
      cmd.parse(argc, argv);

      if (typeNameArg.isSet() != methodNameArg.isSet()) {
         CmdLineParseException error("You should specify both the type and method name, or neither");
         try {            
            cmd.getOutput()->failure(cmd, error);
         }
         catch (ExitException &ee) {
            exit(ee.getExitStatus());
         }
      }

      // Get the argument values
      clrVersion = clrVersionArg.getValue();
      assemblyFileName = assemblyFileNameArg.getValue();
      typeName = typeNameArg.getValue();
      methodName = methodNameArg.getValue();
      useSandbox = useSandboxArg.getValue();
   }
   catch (ArgException &e) {
      cerr << "Error: " << e.error() << " for arg " << e.argId() << endl;      
      return 1;
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
      return -1;
   }

   // OPTIONAL: ask information about a specific runtime based on some policy
   //ICLRMetaHostPolicy *pMetaHostPolicy = NULL;
   //hr = CLRCreateInstance(CLSID_CLRMetaHostPolicy, IID_ICLRMetaHostPolicy, (LPVOID*) &pMetaHostPolicy);
   //pMetaHostPolicy->GerRequestedRuntime()

   // Get the ICLRRuntimeInfo corresponding to a particular CLR version. It 
   // supersedes CorBindToRuntimeEx with STARTUP_LOADER_SAFEMODE.
   // OPTIONAL: ICLRMetaHost::EnumerateInstalledRuntimes 
   hr = metaHost->GetRuntime(toWstring(clrVersion).c_str(), IID_PPV_ARGS(&runtimeInfo));
   if (FAILED(hr)) {
      metaHost->Release();
      Logger::Critical("ICLRMetaHost::GetRuntime failed w/hr 0x%08lx\n", hr);
      return -1;
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
      return -1;
   }

   if (!isLoadable) {
      runtimeInfo->Release();
      metaHost->Release(); 
      Logger::Critical(".NET runtime %s cannot be loaded\n", clrVersion);
      return -1;
   }

   // Load the CLR into the current process and return a runtime interface 
   // pointer. ICorRuntimeHost and ICLRRuntimeHost are the two CLR hosting  
   // interfaces supported by CLR 4.0. 
   // The ICorRuntimeHost interface that was provided in .NET v1.x, is compatible with all 
   // .NET Frameworks. 

   //ICorRuntimeHost *corRuntimeHost = NULL;
   //hr = runtimeInfo->GetInterface(CLSID_CorRuntimeHost, IID_PPV_ARGS(&corRuntimeHost));
   //_AppDomain* appDomain;
   //IUnknown* appDomainUnk;
   //corRuntimeHost->GetDefaultDomain(&appDomainUnk);
   //appDomainUnk->QueryInterface(&appDomain);

   ICLRRuntimeHost* clr = NULL;
   hr = runtimeInfo->GetInterface(CLSID_CLRRuntimeHost, IID_PPV_ARGS(&clr));

   if (FAILED(hr)) {
      runtimeInfo->Release();
      metaHost->Release();
      Logger::Critical("ICLRRuntimeInfo::GetInterface failed w / hr 0x % 08lx\n", hr);
      return -1;
   }

   // Get the CLR control object
   ICLRControl* clrControl = NULL;
   clr->GetCLRControl(&clrControl);

   if (useSandbox) {
      // Ask the CLR for its implementation of ICLRHostProtectionManager. 
      ICLRHostProtectionManager* clrHostProtectionManager = NULL; 
      hr = clrControl->GetCLRManager(IID_ICLRHostProtectionManager, (void **) &clrHostProtectionManager); 
      if (FAILED(hr)) {
         runtimeInfo->Release();
         metaHost->Release();
         Logger::Critical("ICLRControl::GetCLRManager failed w / hr 0x % 08lx\n", hr);
         return -1;
      }

      // Block all host protection categories.
      // TODO: only a relevant subset?
      // TODO: how does this interact with the AppDomain PermissionSet?
      hr = clrHostProtectionManager->SetProtectedCategories ( (EApiCategories)( 
         eSelfAffectingProcessMgmt | 
         eSelfAffectingThreading  |
         eSynchronization  |
         eSharedState  |
         eExternalProcessMgmt  |
         eExternalThreading  |
         eSelfAffectingProcessMgmt  |
         eSelfAffectingThreading  |
         eSecurityInfrastructure  |
         eMayLeakOnAbort  |
         eUI));

      if (FAILED(hr)) {
         Logger::Critical("ICLRHostProtectionManager::SetProtectedCategories failed w / hr 0x % 08lx\n", hr);         
      }
      clrHostProtectionManager->Release();
   }

   // Associate our domain manager
   std::list<AssemblyInfo> hostAssemblies;
   std::wstring currentDir = CurrentDirectory();
   AssemblyInfo appDomainManager(L"SimpleHostRuntime, Version=1.0.0.0, Culture=neutral, PublicKeyToken=9abf81284e6824ad, processorarchitecture=MSIL", currentDir + L"\\SimpleHostRuntime.dll", L"");
   hostAssemblies.push_back(appDomainManager);

   clrControl->SetAppDomainManagerType(appDomainManager.FullName.c_str(), L"SimpleHostRuntime.SimpleHostAppDomainManager");



   // Construct our host control object.
   DHHostControl* hostControl = new DHHostControl(clr, hostAssemblies);
   clr->SetHostControl(hostControl);

   // Start the CLR.
   hr = clr->Start();
   if (FAILED(hr)) {      
      runtimeInfo->Release();
      metaHost->Release();      
      clr->Release();
      clrControl->Release();
      Logger::Critical("CLR failed to start w/hr 0x%08lx", hr);
      return -1;
   }

   ISimpleHostDomainManager* domainManager = hostControl->GetDomainManagerForDefaultDomain();

   if (!domainManager) {
      runtimeInfo->Release();
      metaHost->Release();
      clr->Release();
      clrControl->Release();
      Logger::Critical("Cannot obtain the Domain Manager for the Default Domain");
      return -1;
   }

   int retVal = 0;
   long appDomainId = 0L;
   HRESULT hrExecute;

   bstr_t assemblyName = toBSTR(assemblyFileName);  
   
   if (methodName != "") {
      bstr_t type = toBSTR(typeName);
      bstr_t method = toBSTR(methodName);
      if (useSandbox) {
         hrExecute = domainManager->raw_RunSandboxed(assemblyName, type, method, &appDomainId);
      }
      else {
         hrExecute = domainManager->raw_Run_2(assemblyName, type, method, &appDomainId);
      }
   }
   else {
      hrExecute = domainManager->raw_Run(assemblyName, &appDomainId);
   }

   if (!SUCCEEDED(hrExecute)) {
      Logger::Critical("Execution of method failed: 0x%x", hrExecute);      
      retVal = -1;
   }
   else {
      Logger::Info("Executed code in AppDomain %ld", appDomainId);
   }

   // Stop the CLR and cleanup.
   hostControl->ShuttingDown();

   runtimeInfo->Release();
   metaHost->Release();
   clrControl->Release();
   domainManager->Release();

   clr->Stop();   
   clr->Release();

   return retVal;
}
