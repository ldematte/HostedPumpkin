using System;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;

namespace Snippets {

   class Utils {

      private static async Task<string> Foo() {
         await Task.Delay(100);
         return "Foo";
      }

      public static async Task Run() {
         Console.WriteLine("Hello " + await Foo());
      }
   }

   public class AsyncAwait {

      public static void SnippetMain() {
         Task.Run(() => Utils.Run()).Wait();
      }

   }
}
