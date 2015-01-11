using System;
using System.IO;

namespace Snippets {

    public class Patched_FileRead {

        public static Pumpkin.Monitor __monitor;

        public static void SnippetMain() {
            File.ReadAllText("C:\\Temp\\file.txt");
        }
    }
}