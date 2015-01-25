using System;
using System.Collections.Generic;
using System.Linq;
using System.Security;
using System.Security.Permissions;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Pumpkin
{
    [Serializable]
    [SecuritySafeCritical]
    public class Monitor {

        public const int MAX_ALLOCATIONS = 100;
        public const int MAX_MEMORY_USAGE = 100 * 1024; // 100 KB

        public const int MAX_THREAD_STARTS = 10;

        // TODO: a name allowed in the CLR, but disallowed in C#
        public const string MonitorFieldName = "__monitor";

        public readonly List<string> output;

        public Monitor(List<string> output) {
           this.output = output;
        }

        [SecuritySafeCritical]
        [MethodPatch(ClassName = "Console", MethodName = "WriteLine", IsStatic = true)]
        public static void Console_WriteLine(string arg, Monitor that) {
            that.output.Add(arg);
        }

        // "Patches" are static methods that accept a Monitor instance as the last argument.
        // Why? Because on the CLR arguments are pushed from left to right, and this is the first arg.
        // So to patch a call with an instance method we should "rewind" the stack by N args, and insert
        // there a ldarg for "this" (monitor instance).
        // In cecil it would be something like: 
        // var firstArgument = patchedCall;
        // 
        //                /
        //                for (int j = 0; j < key.Parameters.Count; ++j) {
        //                    firstArgument = firstArgument.Previous;
        //                }
                        
        //                il.InsertBefore(firstArgument, il.Create(OpCodes.Ldarg, monitorRef));    
        // We know how many args we have got, so walk the stack back that num
        // Problem: optimizations could skip store-ld!
        // Solution: we pass "this" as the last argument (static method + "this" (that) parameter)

        // this could be even produced automatically using cecil!
        // We could allow
        // public void Console_WriteLine(string arg) {
        //    output.Add(arg);
        // }
        // and generate
        // public static void Console_WriteLine(string arg, Monitor that) {
        //    that.Console_WriteLine(arg)
        // }
        // But this is for future consideration


        [SecuritySafeCritical]
        public static T New<T>(object[] parameters, Monitor monitor) {

            //lock (monitor.monitorLock) {

                var t = (T)Activator.CreateInstance(typeof(T), parameters);
                // This is an approximation (no padding). See http://stackoverflow.com/a/207605/863564
                monitor.numberOfObjectCreations += 1;

                if (monitor.NumberOfObjectCreations > MAX_ALLOCATIONS) {
                    // || AppDomain.CurrentDomain.MonitoringSurvivedMemorySize > MAX_MEMORY_USAGE
                    throw new TooManyAllocationsException();
                }

                return t;
            //}
        }

        private int numberOfObjectCreations = 0;
        public int NumberOfObjectCreations { get { return numberOfObjectCreations; } }
        private int numberOfThreadStarts = 0;
        public int NumberOfThreadStarts { get { return numberOfThreadStarts; } }

        private object monitorLock = new object();
  
        [MethodPatch(ClassName="Thread", MethodName="Start", IsStatic=false)]
        [SecuritySafeCritical]        
        public static void Thread_Start(System.Threading.Thread that, Monitor monitor) {
            lock (monitor.monitorLock) {
                monitor.numberOfThreadStarts += 1;

                if (monitor.NumberOfThreadStarts < Monitor.MAX_THREAD_STARTS) {
                    that.Start();
                }
                else {
                    (new PermissionSet(PermissionState.Unrestricted)).Assert();
                    System.Diagnostics.Debug.WriteLine("Refused to start Thread {0}", that.ManagedThreadId);
                    CodeAccessPermission.RevertAssert();
                    // TODO: stop and tear down everything
                }
            }
        }
    }
}
