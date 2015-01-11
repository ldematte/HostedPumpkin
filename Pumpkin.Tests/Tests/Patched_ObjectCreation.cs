using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Snippets {
    public class Patched_ObjectCreation {

        public static Pumpkin.Monitor __monitor;

        public class Foo {
            private int bar;
            private string baz;

            public Foo(int bar, string baz) {
                this.bar = bar;
                this.baz = baz;
            }

            public string Get() {
                return String.Format("{0} - {1}", bar, baz);
            }
        }

        public static void SnippetMain() {

            for (int i = 0; i < 10; ++i) {
                var args = new object[2];
                args[0] = i;
                args[1] = "Hello";
                var foo = Pumpkin.Monitor.New<Foo>(args, __monitor);
                Console.WriteLine(foo.Get());
            }

        }
    }
}