using Pumpkin.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Web;

namespace Pumpkin.Web {
   public class HostConnector {

      public const int hostPort = 4321;
      public static ProcessInfo hostProcess;

      private static Socket socket;
      private static Socket GetSocket() {
         try {
         if (socket == null)
            socket = new Socket(SocketType.Stream, ProtocolType.Tcp);

         if (!socket.Connected) 
            socket.Connect("localhost", hostPort);
         
         return socket;
      }
         catch (Exception ex) {
            System.Diagnostics.Debug.WriteLine(ex.Message);
            throw;
         }
      }

      public static async Task<string> RunSnippetAsync(string snippetId) {

         var socket = GetSocket();

         await socket.SendAsync(snippetId, true);
         string s = await socket.ReceiveAsync();

         // TODO TODO Deserialize, and return the result
         return s;
      }

   }
}