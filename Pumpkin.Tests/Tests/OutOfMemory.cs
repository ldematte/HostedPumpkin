using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Snippets {
    public class OutOfMemory {

        public static void SnippetMain() {

            while (true) {
                var o = new Object();
                Console.WriteLine(o.ToString());
            }

        }
    }
}