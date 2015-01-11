using Pumpkin.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Threading.Tasks;
using System.Web;
using System.Web.Mvc;

namespace Pumpkin.Web.Controllers {
   public class HomeController : Controller {
      public ActionResult Index() {
         return View();
      }

      private static String snippetBody = @"
using System;
using System.IO;
{1}
namespace Snippets {
    public class SnippetText {
        public static void SnippetMain() {
            {1}
        }
    }
}";


      [HttpPost]
      public ActionResult SubmitSnippet(String usingDirectives, String snippetSource) {

         var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(String.Format(snippetBody, usingDirectives, snippetSource));
         var patchedAssembly = SnippetCompiler.PatchAssembly(snippetAssembly.Item1, "Snippets.SnippetText");

         SnippetDataRepository repository = new SnippetDataRepository();
         repository.Save(snippetSource, patchedAssembly);

         return new HttpStatusCodeResult(204);
      }

      [HttpPost]
      public async Task<ActionResult> RunSnippetAsync(String snippetId) {
         //SnippetDataRepository repository = new SnippetDataRepository();
         //TODO: check the existence of the snippet

         

         return Json(await HostConnector.RunSnippetAsync(snippetId));
      }
   }
}