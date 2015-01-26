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
using System.Diagnostics;

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

      private static async Task SubmitSnippetPostFeedback(string snippetId, string connectionId, long startTimestamp) {
         SnippetResult result;
         try {
            result = await HostConnector.RunSnippetAsync(snippetId);
         }
         catch (Exception ex) {
            result = new SnippetResult() { status = SnippetStatus.InitializationError, exception = ex.Message };
         }

         var hubContext = GlobalHost.ConnectionManager.GetHubContext<ResultHub>();
         result.totalTime = StopwatchExtensions.GetTimestampMillis() - startTimestamp;
         hubContext.Clients.Client(connectionId).SendResult(connectionId, snippetId, result, result.status.ToHealth().ToColor());
      }

      [HttpPost]
      public async Task<ActionResult> SubmitRequest(string snippetId, string connectionId) {
         long startTimestamp = StopwatchExtensions.GetTimestampMillis();
         if (String.IsNullOrEmpty(snippetId))
            return new HttpStatusCodeResult(HttpStatusCode.BadRequest);

         if (connectionId != null) {
            Task.Run(() => SubmitSnippetPostFeedback(snippetId, connectionId, startTimestamp)).FireAndForget();
            return new HttpStatusCodeResult(HttpStatusCode.Accepted);
         }
         else {
            SnippetResult result = await HostConnector.RunSnippetAsync(snippetId);
            result.totalTime = StopwatchExtensions.GetTimestampMillis() - startTimestamp;
            Response.StatusCode = (int)HttpStatusCode.OK;
            return Json(new { connectionId = connectionId, message = result, newStatus = result.status.ToHealth().ToColor() });
         }         
      }


      public static String SnippetUsing = @"
using System;
using System.Collections.Generic;
";

      private static String snippetHeader = @"
namespace Snippets {{
    public class {0} {{
        public static void {1}() {{
";
      public static String SnippetFooter = @"
        }
    }
}";

      public static String SnippetHeader() {
         return String.Format(snippetHeader, SnippetData.SnippetTypeName, SnippetData.SnippetMethodName);
      }

      [HttpPost]
      public ActionResult SubmitSnippet(String usingDirectives, String snippetSource) {

         try {
            var snippetAssembly = Pumpkin.SnippetCompiler.CompileWithCSC(
               SnippetUsing + usingDirectives + SnippetHeader() + snippetSource + SnippetFooter,
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