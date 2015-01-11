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
         if (socket == null)
            socket = new Socket(SocketType.Stream, ProtocolType.Tcp);

         if (!socket.Connected) 
            socket.Connect("localhost", hostPort);
         
         return socket;
      }

      public static async Task<SnippetResult> RunSnippetAsync(string snippetId) {

         var socket = GetSocket();
         byte[] buffer = Guid.Parse(snippetId).ToByteArray();

         await socket.SendAsync(buffer, 0, buffer.Length, SocketFlags.None);
         string s = await socket.ReceiveAsync();

         // TODO TODO Deserialize, and return the result
         return new SnippetResult();
      }

   }
}