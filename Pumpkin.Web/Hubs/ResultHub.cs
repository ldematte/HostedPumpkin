using System;
using System.Collections.Generic;
using System.Linq;
using System.Web;
using Microsoft.AspNet.SignalR;

namespace Pumpkin.Web.Hubs {
   public class ResultHub : Hub {
      public void SendResult(string connectionId, string result) {
         Clients.Client(connectionId).SendResult(result);
      }
   }
}