using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using Microsoft.AspNet.SignalR;

namespace Pumpkin.Web.Hubs {
   public class ResultHub : Hub {
      public void SendResult(string connectionId, string snippetId, string result, string newStatus) {
         Clients.Client(connectionId).SendResult(snippetId, result, newStatus);
      }
   }
}