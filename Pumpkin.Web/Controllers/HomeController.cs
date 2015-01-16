using Pumpkin.Data;
using Pumpkin.Web.Models;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Threading.Tasks;
using System.Web;
using System.Web.Mvc;

namespace Pumpkin.Web.Controllers {
   public class HomeController : Controller {

      SnippetDataRepository repository = new SnippetDataRepository();


      public ActionResult Index() {
         var snippets = repository.All().Select(s => new Snippet { Id = s.Id.ToString(), Source = s.SnippetSource });
         return View(snippets);
      }

      public ActionResult Create() {
         return View();
      }


      private static String snippetBody = @"
using System;
using System.Collections.Generic;
{0}
namespace Snippets {{
    public class SnippetText {{
        public static void SnippetMain() {{
            {1}
        }}
    }}
}}";


      [HttpPost]
      public ActionResult SubmitSnippet(String usingDirectives, String snippetSource) {

         var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(String.Format(snippetBody, usingDirectives, snippetSource), Server.MapPath("App_Data"));

         if (snippetAssembly.success) {
            var patchedAssembly = SnippetCompiler.PatchAssembly(snippetAssembly.assemblyBytes, "Snippets.SnippetText");

            repository.Save(usingDirectives, snippetSource, patchedAssembly);

            return new HttpStatusCodeResult(204);
         }
         else {
            Response.StatusCode = 400;
            return Content(snippetAssembly.errors.First());
         }
      }

      [HttpPost]
      public async Task<ActionResult> RunSnippetAsync(String snippetId) {
         // TODO: check the existence of the snippet
         // SnippetDataRepository repository = new SnippetDataRepository();         

         return Json(await HostConnector.RunSnippetAsync(snippetId));
      }
   }
}