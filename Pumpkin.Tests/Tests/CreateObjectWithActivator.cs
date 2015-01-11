using System;
using System.IO;

namespace Snippets {

    public class CreateObjectWithActivator {

        public static void SnippetMain() {
            var t = (System.Threading.Thread)Activator.CreateInstance(typeof(System.Threading.Thread));
            t.Start();           
        }
    }
}
