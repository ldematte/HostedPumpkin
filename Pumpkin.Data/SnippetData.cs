using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlServerCe;
using System.Linq;

namespace Pumpkin.Data {

   public enum SnippetHealth: int {
      Unknown = 0,
      Good = 1,
      Timeout = 2,
      Fatal = 3
   }

   public class SnippetData {
      public Guid Id { get; set; }
      public byte[] AssembyBytes { get; set; }
      public string SnippetSource { get; set; }
      public string UsingDirectives { get; set; }
      public int SnippetHealth { get; set; }
   }

   public class SnippetDataRepository {
      // TODO: Cache connections and statements
      private static SqlCeConnection GetOpenConnection() {
         var connectionString = ConfigurationManager.ConnectionStrings["SnippetsDb"].ConnectionString;
         var connection = new SqlCeConnection(connectionString);
         connection.Open();
         return connection;
      }

      public SnippetData Get(Guid id) {
         using (var connection = GetOpenConnection()) {
            SqlCeCommand cmd = connection.CreateCommand();
            cmd.CommandText = "SELECT * FROM Snippets WHERE Id = ?";

            cmd.Parameters.Add(new SqlCeParameter("Id", SqlDbType.NVarChar));
            cmd.Parameters["Id"].Value = id.ToString();

            SqlCeDataReader reader = cmd.ExecuteReader();

            if (reader.Read()) {
               long length = reader.GetBytes(1, 0, null, 0, 0);
               byte[] assemblyBytes = new byte[length];
               reader.GetBytes(1, 0, assemblyBytes, 0, assemblyBytes.Length);

               return new SnippetData() { 
                  Id = id, 
                  AssembyBytes = assemblyBytes, 
                  SnippetSource = reader.GetString(2),
                  SnippetHealth = reader.GetInt32(3) };
            }
         }
         return null;
      }

      public Guid Save(String usings, String source, byte[] assemblyBytes) {
         Guid id = Guid.NewGuid();

         // Save
         using (var connection = GetOpenConnection()) {
            SqlCeCommand cmd = connection.CreateCommand();

            cmd.CommandText = "INSERT INTO Snippets (Id, AssembyBytes, SnippetHealth) VALUES (?, ?, ?)";

            cmd.Parameters.Add(new SqlCeParameter("Id", SqlDbType.UniqueIdentifier));
            cmd.Parameters.Add(new SqlCeParameter("AssembyBytes", SqlDbType.Image));
            cmd.Parameters.Add(new SqlCeParameter("SnippetSource", SqlDbType.NText));
            cmd.Parameters.Add(new SqlCeParameter("UsingDirectives", SqlDbType.NVarChar));
            cmd.Parameters.Add(new SqlCeParameter("SnippetHealth", SqlDbType.Int));
           
            cmd.Prepare();

            cmd.Parameters["Id"].Value = id.ToString();
            cmd.Parameters["AssembyBytes"].Value = assemblyBytes;
            cmd.Parameters["SnippetSource"].Value = source;
            cmd.Parameters["UsingDirectives"].Value = usings;
            cmd.Parameters["SnippetHealth"].Value = (int)SnippetHealth.Unknown;
            cmd.ExecuteNonQuery();
         }

         return id;
      }

      public void UpdateStatus(Guid id, SnippetHealth newStatus) {
         using (var connection = GetOpenConnection()) {
            SqlCeCommand cmd = connection.CreateCommand();

            cmd.CommandText = "UPDATE Snippets SET SnippetHealth = ? WHERE Id = ?";

            cmd.Parameters.Add(new SqlCeParameter("Id", SqlDbType.UniqueIdentifier));
            cmd.Parameters.Add(new SqlCeParameter("SnippetHealth", SqlDbType.Int));

            cmd.Prepare();

            cmd.Parameters["Id"].Value = id.ToString();
            cmd.Parameters["SnippetHealth"].Value = (int)newStatus;
            cmd.ExecuteNonQuery();
         }
      }

      public IEnumerable<SnippetData> All() {
         using (var connection = GetOpenConnection()) {
            SqlCeCommand cmd = connection.CreateCommand();
            cmd.CommandText = "SELECT * FROM Snippets";
            
            SqlCeDataReader reader = cmd.ExecuteReader();

            while (reader.Read()) {
               long length = reader.GetBytes(1, 0, null, 0, 0);
               byte[] assemblyBytes = new byte[length];
               reader.GetBytes(1, 0, assemblyBytes, 0, assemblyBytes.Length);

               yield return new SnippetData() {
                  Id = reader.GetGuid(0),
                  AssembyBytes = assemblyBytes,
                  SnippetSource = reader.GetString(2),
                  SnippetHealth = reader.GetInt32(3)
               };
            }
         }
      }
   }
}
