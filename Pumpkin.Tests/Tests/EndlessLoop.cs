using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Snippets {
    public class EndlessLoop {

        public static void SnippetMain() {
            int i = 0;
            while (true) {
                i = i + i;
            }

        }
    }
}