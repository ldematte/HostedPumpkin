using System;
using System.IO;

namespace Snippets {

    public class Patched_HelloWorld {

        public static Pumpkin.Monitor __monitor;

        public static void SnippetMain() {
            //Console.WriteLine("Hello world!");
            Pumpkin.Monitor.Console_WriteLine("Hello world!", Patched_HelloWorld.__monitor);
        }
    }
}