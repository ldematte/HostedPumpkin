using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Snippets {
    public class SemiPatched_StackOverflow {

        public static int f(int i) {
            if (i == 0)
                return i;
            else
                return f(i++);
        }

        public static void SnippetMain(Pumpkin.Monitor monitor) {

            Console.WriteLine(f(10).ToString());

        }
    }
}
