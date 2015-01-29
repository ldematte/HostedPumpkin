using Newtonsoft.Json;
using Pumpkin.Data;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using System.Web;

namespace Pumpkin.Web {

   public interface IHostConnector {
      Task<SnippetResult> RunSnippetAsync(string snippetId);
   }

   public class HostSocketConnector: IHostConnector, IDisposable {

      public const int hostPort = 4321;

      private Socket socket;
      private Socket GetSocket() {
         try {
            if (socket != null) {
               if (socket.Connected) {
                  return socket;
               }
               else {
                  try {
                     socket.Dispose();
                  }
                  catch (Exception ex) {
                     System.Diagnostics.Debug.WriteLine(ex.Message);
                     throw;
                  }
               }
            }
            socket = new Socket(SocketType.Stream, ProtocolType.Tcp);
            socket.Connect("localhost", hostPort);
            return socket;
         }
         catch (Exception ex) {
            System.Diagnostics.Debug.WriteLine(ex.Message);
            throw;
         }
      }      

      public async Task<SnippetResult> RunSnippetAsync(string snippetId) {

         var socket = GetSocket();

         await socket.SendAsync(snippetId, true);
         string s = await socket.ReceiveAsync();

         // Deserialize, and return the result
         return JsonConvert.DeserializeObject<SnippetResult>(s);
      }


      public void Dispose() {
         try {
            if (socket != null) {
               socket.Dispose();
            }
            socket = null;
         }
         catch (Exception ex) {
            System.Diagnostics.Debug.WriteLine(ex.Message);            
         }
      }
   }
}