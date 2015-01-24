
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
   string snippetDataBase;
   bool useSandbox = true;
   bool testMode = false;
   int serverPort = 4321;

   CmdLine cmd("Simple CLR Host", ' ', "1.0");
   try {     

      ValueArg<string> clrVersionArg("v", "clrversion", "CLR version to use", true, "v2.0.50727", "string");
      cmd.add(clrVersionArg);

      SwitchArg testModeArg("t", "test", "Run in test mode (specify an assembly file, type/class and method)");

      ValueArg<string> assemblyFileNameArg("a", "assembly", "The assembly file name", false, "", "string");
      cmd.add(assemblyFileNameArg);

      ValueArg<string> typeNameArg("c", "type", "The type name which contains the method to invoke", false, "", "string");
      cmd.add(typeNameArg);

      ValueArg<string> methodNameArg("m", "method", "The method to invoke", false, "", "string");
      cmd.add(methodNameArg);

      ValueArg<string> snippetDataBaseArg("d", "database", "The path of the SQL CE database that holds the snippets", false, "", "string");
      cmd.add(snippetDataBaseArg);

      ValueArg<int> serverPortArg("p", "port", "The port on which this Host will listen for snippet execution requests", false, 4321, "int");
      cmd.add(serverPortArg);

      cmd.parse(argc, argv);

      testMode = testModeArg.getValue();

      if (testMode) {
         if (!typeNameArg.isSet() || !assemblyFileNameArg.isSet()) {
            CmdLineParseException error("If you run in test mode, you should specify both the type and assembly file name");
            try {
               cmd.getOutput()->failure(cmd, error);
            }
            catch (ExitException &ee) {
               exit(ee.getExitStatus());
            }
         }
      }
      else {
         if (!snippetDataBaseArg.isSet()) {
            CmdLineParseException error("You should specify a valid DB file name");
            try {
               cmd.getOutput()->failure(cmd, error);
            }
            catch (ExitException &ee) {
               exit(ee.getExitStatus());
            }            
         }
      }

      // Get the argument values
      clrVersion = clrVersionArg.getValue();
      assemblyFileName = assemblyFileNameArg.getValue();
      typeName = typeNameArg.getValue();
      methodName = methodNameArg.getValue();
      snippetDataBase = snippetDataBaseArg.getValue();
      serverPort = serverPortArg.getValue();
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
      // This "completes" the AppDomain PermissionSet
      // Both are necessary, as this mechanism does not cover all
      // aspects, but some. 
      // It is a way to have some "in-depth" defense, and to prevent some
      // functionalities that are not prevented by CAS (e.g.: Cannot call Thread.SetPriority, but
      // can call Thread.Abort.
      hr = clrHostProtectionManager->SetProtectedCategories ( (EApiCategories)( 
         //eSynchronization  |
         //eSharedState  |
         eExternalProcessMgmt  |
         eSelfAffectingProcessMgmt |
         //eExternalThreading  |
         eSelfAffectingThreading  |
         eSecurityInfrastructure  |
         eMayLeakOnAbort  //|
         //eUI
         ));

      if (FAILED(hr)) {
         Logger::Critical("ICLRHostProtectionManager::SetProtectedCategories failed w / hr 0x % 08lx\n", hr);         
      }
      clrHostProtectionManager->Release();
   }

   std::list<AssemblyInfo> hostAssemblies;
   std::wstring currentDir = CurrentDirectory();
   AssemblyInfo appDomainManager(L"SimpleHostRuntime, Version=1.0.0.0, Culture=neutral, PublicKeyToken=9abf81284e6824ad, processorarchitecture=MSIL", currentDir + L"\\SimpleHostRuntime.dll", L"");
   AssemblyInfo pumpkinData(L"Pumpkin.Data, Version=1.0.0.0, Culture=neutral, PublicKeyToken=9abf81284e6824ad, processorarchitecture=msil", currentDir + L"\\Pumpkin.Data.dll", L"");
   AssemblyInfo pumpkinMonitor(L"Pumpkin.Monitor, Version=1.0.0.0, Culture=neutral, PublicKeyToken=9abf81284e6824ad, processorarchitecture=MSIL", currentDir + L"\\Pumpkin.Monitor.dll", L"");
   hostAssemblies.push_back(appDomainManager);
   hostAssemblies.push_back(pumpkinData);
   hostAssemblies.push_back(pumpkinMonitor);

   // Construct our host control object.
   DHHostControl* hostControl = new DHHostControl(clr, hostAssemblies);

   // Associate our domain manager
   clrControl->SetAppDomainManagerType(appDomainManager.FullName.c_str(), L"SimpleHostRuntime.SimpleHostAppDomainManager");
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
   HRESULT hrExecute;

   hrExecute = domainManager->RegisterHostContext(hostControl->GetHostContext());
   
   if (testMode) {
      bstr_t assemblyName = toBSTR(assemblyFileName);
      bstr_t type = toBSTR(typeName);
      bstr_t method = toBSTR(methodName);
      if (methodName.empty()) {
         hrExecute = domainManager->raw_RunTests(assemblyName, type);
      }
      else {
         hrExecute = domainManager->raw_RunTest(assemblyName, type, method);
      }

      if (!SUCCEEDED(hrExecute)) {
         Logger::Critical("Execution of tests failed: 0x%x", hrExecute);
         retVal = -1;
      }

      getchar();
   }
   else {
      bstr_t snippetDataBaseName = toBSTR(snippetDataBase);
      hrExecute = domainManager->raw_StartListening(serverPort, snippetDataBaseName);

      if (!SUCCEEDED(hrExecute)) {
         Logger::Critical("Execution of tests failed: 0x%x", hrExecute);
         retVal = -1;
      }

      Logger::Critical("Shutting down");
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
