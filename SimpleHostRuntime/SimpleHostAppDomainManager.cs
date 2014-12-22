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
   [SecuritySafeCritical]
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

         //if (AppDomain.CurrentDomain.IsDefaultAppDomain()) {
            new ReflectionPermission(PermissionState.Unrestricted).Assert();
            AppDomain.CurrentDomain.AssemblyResolve += domain_AssemblyResolve;
            AppDomain.CurrentDomain.AssemblyLoad += domain_AssemblyLoad;
            ReflectionPermission.RevertAssert();
         //}
      }

      private static AppDomain CreateSandbox(string sandboxName) {
         
         //Setting the AppDomainSetup. It is very important to set the ApplicationBase to a folder 
         //other than the one in which the sandboxer resides.
         AppDomainSetup adSetup = new AppDomainSetup();
         adSetup.ApplicationBase = "NOT_A_PATH";
         // Do not search the application base directory at all
         adSetup.DisallowApplicationBaseProbing = true;

         // With the restrictions we have this should never work anyway.
         // But let's set it in case future versions allow some sort of network access anyway..
         adSetup.DisallowCodeDownload = true;

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

      private int RunInDomain(AppDomain ad, string assemblyFileName, string mainTypeName, string methodName, bool runningInSandbox) {
         var manager = (SimpleHostAppDomainManager)ad.DomainManager;
         manager.InternalRun(ad, assemblyFileName, mainTypeName, methodName, runningInSandbox);
         return ad.Id;
      }

      private int RunInDomainAsync(AppDomain ad, string assemblyFileName, string mainTypeName, string methodName, bool runningInSandbox) {
         var manager = (SimpleHostAppDomainManager)ad.DomainManager;
         manager.InternalRun(ad, assemblyFileName, mainTypeName, methodName, runningInSandbox);
         return ad.Id;
      }

      public int Run(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = AppDomain.CreateDomain(assemblyFileName, null, null);
         return RunInDomain(ad, assemblyFileName, mainTypeName, methodName, false);
      }

      public int RunAsync(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = AppDomain.CreateDomain(assemblyFileName, null, null);
         return RunInDomainAsync(ad, assemblyFileName, mainTypeName, methodName, false);
      }

      public int RunSandboxed(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = CreateSandbox("Host Sandbox Domain");
         return RunInDomain(ad, assemblyFileName, mainTypeName, methodName, true);
      }

      public int RunSandboxedAsync(string assemblyFileName, string mainTypeName, string methodName) {
         var ad = CreateSandbox("Host Sandbox Domain");
         return RunInDomainAsync(ad, assemblyFileName, mainTypeName, methodName, true);
      }

      static Assembly domain_AssemblyResolve(object sender, ResolveEventArgs args) {

         // TODO: Here we have a list of the assemblies we allow in the sandbox.
         // They may be stored in a local dir, or in a DB. Locate them, deserialize (if needed)
         // and load

         var domainManagerAssembly = typeof(SimpleHostAppDomainManager).Assembly;
         if (args.Name.Equals(domainManagerAssembly.FullName)) {

            // NOTE: if you want to load it from DB or file, you will need to asser permission here
            // or the load will fail (no permissions in this sandbox). At least, you need System.Security.Permissions and 
            // System.Security.Permissions.FileIOPermission
            // Assembly.LoadFrom(monitorAssembly.Location);
            return domainManagerAssembly;
         }

         return null;
      }

      private void domain_AssemblyLoad(object sender, AssemblyLoadEventArgs args) {
         System.Diagnostics.Debug.WriteLine("Loaded: " + args.LoadedAssembly.FullName);
      }

      // This kind of customization (provide a custom HostSecurityManager, with custom DomainPolicy etc.)
      // is obsolete in .NET4. Setting the PermissionSet on the AppDomain (as we do here) is the right thing to do
      // See http://msdn.microsoft.com/en-us/library/ee191568(VS.100).aspx
      // and http://msdn.microsoft.com/en-us/library/bb763046(v=vs.100).aspx
      //public override HostSecurityManager HostSecurityManager {
      //   get { return new SimpleHostSecurityManager(); }
      //}

      //[SecuritySafeCritical]
      private void InternalRun(AppDomain appDomain, string assemblyFileName, string mainTypeName, string methodName,
                               bool runningInSandbox) {

         try {
            // Use this "trick" to go through the standard loader path.
            // This MAY be something we want, or something to avoid

            // When loading the assembly, we need at least FileIOPermission. 
            // Calling it with a full-trust stack. TODO: only what is needed
            (new PermissionSet(PermissionState.Unrestricted)).Assert();
            AssemblyName an = AssemblyName.GetAssemblyName(assemblyFileName);
            var clientAssembly = appDomain.Load(an);
            CodeAccessPermission.RevertAssert();
            
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

                  // To allow code to invoke any nonpublic member: Your code must be granted ReflectionPermission with the ReflectionPermissionFlag.MemberAccess flag
                  // To allow code to invoke any nonpublic member, as long as the grant set of the assembly that contains the invoked member is 
                  // the same as, or a subset of, the grant set of the assembly that contains the invoking code: 
                  // Your code must be granted ReflectionPermission with the ReflectionPermissionFlag.RestrictedMemberAccess flag.
                  // See http://msdn.microsoft.com/en-us/library/stfy7tfc%28v=vs.110%29.aspx
                  var bindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static;
                  if (runningInSandbox)
                     bindingFlags = BindingFlags.Public | BindingFlags.Static;

                  var target = type.GetMethod(methodName, bindingFlags);
                  if (target == null) {
                     status = Status.Error;
                     return;
                  }

                  //target.S Attributes = target.Attributes ^ MethodAttributes.Private;

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
                  exception = ex;
                  status = Status.Error;
               }
               catch (Exception ex) {
                  (new PermissionSet(PermissionState.Unrestricted)).Assert();
                  System.Diagnostics.Debug.WriteLine("Exception caught:\n{0}", ex.ToString());
                  CodeAccessPermission.RevertAssert();
                  exception = ex;
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
            exception = ex;
            status = Status.Error;
         }
      }      
   }
}
