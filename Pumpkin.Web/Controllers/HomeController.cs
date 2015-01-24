using Microsoft.AspNet.SignalR;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Net;
using System.Threading;
using System.Threading.Tasks;
using System.Web;
using System.Web.Mvc;
using Pumpkin.Data;
using Pumpkin.Web.Hubs;
using Pumpkin.Web.Models;

namespace Pumpkin.Web.Controllers {
   public class HomeController : Controller {

      SnippetDataRepository repository = new SnippetDataRepository();


      public ActionResult Index() {
         var snippets = repository.All().Select(s => new Snippet { Id = s.Id.ToString(), Source = s.SnippetSource, StatusColor = s.SnippetHealth.ToColor() });
         return View(snippets);
      }

      public ActionResult Create() {
         return View();
      }

      private static async Task SubmitSnippetPostFeedback(string snippetId, string connectionId) {
         string result = "";
         try {
            result = await HostConnector.RunSnippetAsync(snippetId);
         }
         catch (Exception ex) {
            result = ex.Message;
         }

         var hubContext = GlobalHost.ConnectionManager.GetHubContext<ResultHub>();
         hubContext.Clients.Client(connectionId).SendResult(connectionId, result);         
      }

      [HttpPost]
      public async Task<ActionResult> SubmitRequest(string snippetId, string connectionId) {
         if (String.IsNullOrEmpty(snippetId))
            return new HttpStatusCodeResult(HttpStatusCode.BadRequest);

         if (connectionId != null) {
            Task.Run(() => SubmitSnippetPostFeedback(snippetId, connectionId)).FireAndForget();
            return new HttpStatusCodeResult(HttpStatusCode.Accepted);
         }
         else {
            string result = await HostConnector.RunSnippetAsync(snippetId);
            Response.StatusCode = (int)HttpStatusCode.OK;
            return Json(new { connectionId = connectionId, message = result });
         }         
      }


      private static String snippetBody = @"
using System;
using System.Collections.Generic;
{0}
namespace Snippets {{
    public class {1} {{
        public static void {2}() {{
            {3}
        }}
    }}
}}";


      [HttpPost]
      public ActionResult SubmitSnippet(String usingDirectives, String snippetSource) {

         try {
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(
               String.Format(snippetBody, usingDirectives, SnippetData.SnippetTypeName, SnippetData.SnippetMethodName, snippetSource),
               Server.MapPath(@"~\App_Data"));

            if (snippetAssembly.success) {
               var patchedAssembly = SnippetCompiler.PatchAssembly(snippetAssembly.assemblyBytes, "Snippets." + SnippetData.SnippetTypeName);

               repository.Save(usingDirectives, snippetSource, patchedAssembly);

               return new HttpStatusCodeResult(204);
            }
            else {
               Response.StatusCode = 400;
               return Json(snippetAssembly.errors);
            }
         }
         catch (Exception ex) {
            Response.StatusCode = 500;
            return Json(new string[] { "Internal error: ", ex.Message});
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