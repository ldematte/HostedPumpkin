using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security;
using System.Security.Permissions;
using System.Security.Policy;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace SimpleHostRuntime {

   public enum Status {
      Ready,
      Running,
      Done,
      Error,
      Timeout,
      Unloading,
      Zombie
   };

   [ComVisible(true), Guid("A603EC84-3449-47B9-BCF5-391C628067D6")]
   [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
   public interface ISimpleHostDomainManager {

      int GetStatus();
      int GetMainThreadManagedId();

      int Run(string assemblyFileName);
      int Run(string assemblyFileName, string mainTypeName, string methodName);
      int RunAsync(string assemblyFileName, string mainTypeName, string methodName);
      int RunSandboxed(string assemblyFileName, string mainTypeName, string methodName);
      int RunSandboxedAsync(string assemblyFileName, string mainTypeName, string methodName);
   }

   [ComVisible(true), Guid("3D4364E5-790F-4F34-A655-EFB05A40BA07"),
        ClassInterface(ClassInterfaceType.None),
        ComSourceInterfaces(typeof(ISimpleHostDomainManager))]
   public class SimpleHostAppDomainManager : System.AppDomainManager, ISimpleHostDomainManager {

      public const int ExecutionTimeout = 2000;

      private Status status;
      public int GetStatus() {
         return (int)status;
      }

      private int mainThreadManagedId = 0;
      public int GetMainThreadManagedId() {
         return mainThreadManagedId;
      }

      private Exception exception;

      private void Reset() {
         status = Status.Ready;
         exception = null;
         mainThreadManagedId = 0;
      }

      private void Error(Exception ex) {
         status = Status.Error;
         exception = ex;
      }

      public override void InitializeNewDomain(AppDomainSetup appDomainInfo) {
         Reset();

         // From MSDN "The default InitializeNewDomain implementation does nothing"
         // base.InitializeNewDomain(appDomainInfo);
         InitializationFlags = AppDomainManagerInitializationOptions.RegisterWithHost;

         // Beginning with the .NET Framework 4, you can use this method to sandbox the default 
         // application domain at application startup, or to modify the sandbox of a new application domain. 
         // To do this, adjust the DefaultGrantSet and FullTrustAssemblies properties on the ApplicationTrust 
         // object that is assigned to the AppDomainSetup.ApplicationTrust property of appDomainInfo, before 
         // you initialize the application domain.
      }

      private static AppDomain CreateSandbox(string sandboxName) {
         
         //Setting the AppDomainSetup. It is very important to set the ApplicationBase to a folder 
         //other than the one in which the sandboxer resides.
         AppDomainSetup adSetup = new AppDomainSetup();
         adSetup.ApplicationBase = "NOT_A_PATH";

         //Setting the permissions for the AppDomain. We give the permission to execute and to 
         //read/discover the location where the untrusted code is loaded.
         PermissionSet permSet = new PermissionSet(PermissionState.None);
         permSet.AddPermission(new SecurityPermission(SecurityPermissionFlag.Execution));

         //We want the sandboxer assembly's strong name, so that we can add it to the full trust list.
         StrongName fullTrustAssembly = typeof(SimpleHostAppDomainManager).Assembly.Evidence.GetHostEvidence<StrongName>();

         //Now we have everything we need to create the AppDomain, so let's create it.
         AppDomain domain = AppDomain.CreateDomain(sandboxName, null, adSetup, permSet, fullTrustAssembly);
         AppDomain.MonitoringIsEnabled = true;        

         return domain;
      }        

      public int Run(string assemblyFileName) {
         var appDomain = AppDomain.CreateDomain("Host Domain");
         int exitCode = appDomain.ExecuteAssembly(assemblyFileName);
         AppDomain.Unload(appDomain);
         return exitCode;
      }

      private int RunInDomain(AppDomain ad, string assemblyFileName, string mainTypeName, string methodName) {
         var manager = (SimpleHostAppDomainManager)ad.DomainManager;
         manager.InternalRun(assemblyFileName, mainTypeName, methodName);
         return ad.Id;
      }

      private int RunInDomainAsync(AppDomain ad, string assemblyFileName, string mainTypeName, string methodName) {
         var manager = (SimpleHostAppDomainManager)ad.DomainManager;
         manager.InternalRun(assemblyFileName, mainTypeName, methodName);
         return ad.Id;
      }

      public int Run(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = AppDomain.CreateDomain(assemblyFileName, null, null);
         return RunInDomain(ad, assemblyFileName, mainTypeName, methodName);
      }

      public int RunAsync(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = AppDomain.CreateDomain(assemblyFileName, null, null);
         return RunInDomainAsync(ad, assemblyFileName, mainTypeName, methodName);
      }

      public int RunSandboxed(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = CreateSandbox("Host Sandbox Domain");
         return RunInDomain(ad, assemblyFileName, mainTypeName, methodName);
      }

      public int RunSandboxedAsync(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = CreateSandbox("Host Sandbox Domain");
         return RunInDomainAsync(ad, assemblyFileName, mainTypeName, methodName);
      }
      
      private void InternalRun(string assemblyFileName, string mainTypeName, string methodName) {

         try {
            // Use this "trick" to go through the standard loader path.
            // This MAY be something we want, or something to avoid
            AssemblyName an = AssemblyName.GetAssemblyName(assemblyFileName);
            var clientAssembly = Assembly.Load(an);

            status = Status.Running;
            // Here we already are on the new domain
            // TODO: implement our own thread-pool here?
            // We would like to get the ThreadID, and return it..
            var thread = new Thread(() => {
               try {
                  //Load the MethodInfo for a method in the new Assembly. This might be a method you know, or 
                  //you can use Assembly.EntryPoint to get to the main function in an executable.
                  var type = clientAssembly.GetType(mainTypeName);
                  if (type == null) {
                     status = Status.Error;
                     return;
                  }
                  var target = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
                  if (target == null) {
                     status = Status.Error;
                     return;
                  }

                  //Now invoke the method.
                  target.Invoke(null, null);
                  status = Status.Done;
               }
               catch (TargetInvocationException ex) {
                  // When we print informations from a SecurityException extra information can be printed if we are 
                  //calling it with a full-trust stack.
                  (new PermissionSet(PermissionState.Unrestricted)).Assert();
                  System.Diagnostics.Debug.WriteLine("Exception caught:\n{0}", ex.InnerException.ToString());
                  CodeAccessPermission.RevertAssert();
                  status = Status.Error;
               }
               catch (Exception ex) {
                  (new PermissionSet(PermissionState.Unrestricted)).Assert();
                  System.Diagnostics.Debug.WriteLine("Exception caught:\n{0}", ex.ToString());
                  CodeAccessPermission.RevertAssert();
                  status = Status.Error;
               }
            });
                        
            thread.Start();
            try {
               if (!thread.Join(ExecutionTimeout)) {
                  status = Status.Timeout;
               }            
            }
            catch (ThreadStateException ex) {
               System.Diagnostics.Debug.WriteLine(ex.Message);
            }
         }
         catch (Exception ex) {
            (new PermissionSet(PermissionState.Unrestricted)).Assert();
            System.Diagnostics.Debug.WriteLine("Exception caught:\n{0}", ex.ToString());
            CodeAccessPermission.RevertAssert();
            status = Status.Error;
         }
      }      
   }
}
