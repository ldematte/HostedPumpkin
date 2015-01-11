using System;
using System.IO;

namespace Snippets {

    public class ThrowException {

        public static void SnippetMain() {
            var x =  new Exception("TEST");
            throw x;
        }
    }
}
