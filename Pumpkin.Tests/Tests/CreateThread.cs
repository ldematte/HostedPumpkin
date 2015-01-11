using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Snippets {
    public class CreateThread {

        public static void SnippetMain() {

            var t = new Thread(() => { });
            t.Start();

        } 

    }
}
