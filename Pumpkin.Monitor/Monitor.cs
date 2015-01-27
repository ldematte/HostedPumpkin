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
    }
}
