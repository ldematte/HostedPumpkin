using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace TestApplication {
   public class Program {
      public static void Main(string[] args) {
         Console.WriteLine("Hello world!");
      }

      public static void SomeOtherMethod() {
         Console.WriteLine("Hello world, again!");
      }

      //- try to create a file
      public static void SnippetTest1() {
         var filestream = System.IO.File.Create("Text.txt");
         filestream.Close();
      }

      //- try to read a file
      public static void SnippetTest2() {
         var fileContent = File.ReadAllText(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "win.ini"));
         Console.WriteLine(fileContent);
      }

      //- a "normal" (single-thread) snippet times out while waiting
      public static void SnippetTest3() {
         Thread.Sleep(30 * 1000);
      }

      //- a "normal" (single-thread) snippet times out while processing
      static void ConsumingOperation() {
         var j = 0;
         var rand = new Random();
         for (int i = 0; i < 1000; ) {
            j += rand.Next();
         }
      }
      public static void SnippetTest4() {
         ConsumingOperation();
      }

      //- create a thread, leave it running (alive) at the end of the snippet
      public static void SnippetTest5() {
         var t = new Thread(() => {
            Thread.Sleep(System.Threading.Timeout.Infinite);
         });
         t.Start();
      }

      //- create a thread, leave it running (busy) at the end of the snippet
      public static void SnippetTest6() {
         var t = new Thread(() => {
            ConsumingOperation();
         });
         t.Start();
      }      

      //- create lots of threads ("fork-bomb")
      static void Fork() {
         var t = new Thread(() => {
            Fork();
         });
         t.Start();
      }
      public static void SnippetTest7() {
         Fork();
      }

      //- create a thread per CPU, set the priority to realtime, and try to clog it
      public static void SnippetTest8() {
         const int cpuCount = 4;
         Thread[] workers = new Thread[cpuCount];
         for (int i = 0; i < cpuCount; ++i) {
            workers[i] = new Thread(() => {
               ConsumingOperation();
            });
            workers[i].Priority = ThreadPriority.Highest;
            workers[i].Start();
         }

         for (int i = 0; i < cpuCount; ++i)
            workers[i].Join();
      }

      //- try to spawn an external process (e.g. ftp.exe)
      public static void SnippetTest9() {
         Process process = new Process();
         // Configure the process using the StartInfo properties.
         process.StartInfo.FileName = "ftp.exe";
         process.StartInfo.Arguments = "-i";
         process.Start();
         process.WaitForExit();// Waits here for the process to exit.
      }

      //- have a finally block that runs an endless loop
      public static void SnippetTest10() {
         try {
            Thread.Sleep(1);
         }
         finally {
            ConsumingOperation();
         }
      }

      //- have a finalizer that runs an endless loop
      class C {
         public C() { }
         ~C() {
            ConsumingOperation();
         }
      }
      public static void SnippetTest11() {
         C c = new C();
      }

      //- try to allocate a very big chunk of memory
      public static void SnippetTest12() {
         byte[] mem = new byte[1024 * 1024 * 1024];
         for (int i = 0; i < mem.Length; ++i) {
            mem[i] = (byte)i;
         }
      }

      //- try to exhaust the heap (lots of small-ish allocations)
      class C2 {
         int x;
         public C2(int i) {
            x = i;
         }
      }
      public static void SnippetTest13() {
         var mem = new System.Collections.Generic.List<C2>();
         for (int i = 0; i < 1024 * 1024 * 10; ++i) {
            mem.Add(new C2(i));
         }
      }

      //- unhandled exception on the main thread
      public static void SnippetTest14() {
         throw new Exception("SnippetTest14");
      }

      //- unhandled exception on a secondary thread
      public static void SnippetTest15() {
         var t = new Thread(() => {
            throw new Exception("SnippetTest15");
         });
         t.Start();
         t.Join();
      }

      //- Thread.Abort on the main thread
      public static void SnippetTest16() {
         Thread.CurrentThread.Abort();
      }

      //- Thread.Abort on a secondary thread
      public static void SnippetTest17() {
         var t = new Thread(() => {
            Thread.CurrentThread.Abort();
         });
         t.Start();
         t.Join();
      }

      //- Stack overflow on the main thread
      static int f() {
         int x = f();
         x = x + f(); // Avoid tailcall
         return x;
      }
      public static void SnippetTest18() {
         f();
      }

      //- Stack overflow on a secondary thread
      public static void SnippetTest19() {
         var t = new Thread(() => {
            f();
         });
         t.Start();
         t.Join();
      }

      //- try to clog the thread pool (queue a lot of work items)
      static void Fork2(object ignored) {
         ThreadPool.QueueUserWorkItem(new WaitCallback(Fork2));
         Thread.Sleep(10);
      }
      public static void SnippetTest20() {
         Fork2(null);
      }

      // - try to "mask" a TAE
      public static void SnippetTest21() {
         try {
            ConsumingOperation();
         }
         catch (ThreadAbortException) {
            throw new Exception("SnippetTest20");
         }
      }

      // - try to "suppress" a TAE
      public static void SnippetTest22() {
         try {
            ConsumingOperation();
         }
         catch (ThreadAbortException) {
            Thread.ResetAbort();
         }
      }

      //- try to open a socket, connect
      //- try to open a socket, listen
      //- try to use a lot of "file" (handle-based) async operations (*)
      // TODO


      
   }
}
