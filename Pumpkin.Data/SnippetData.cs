using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Data.SqlServerCe;
using System.IO;
using System.Linq;

namespace Pumpkin.Data {

   public static class SnippetHealthExt {
      public static string ToColor(this SnippetHealth health) {
         switch (health) {
            case SnippetHealth.Unknown:
               return "white";
            case SnippetHealth.Good:
               return "green";
            case SnippetHealth.Error:
               return "gold";
            case SnippetHealth.Fatal:
               return "red";
            default:
               return "aliceblue";
         }
      }

      public static SnippetHealth ToHealth(this SnippetStatus status) {
         switch (status) {
            case SnippetStatus.Success:
               return SnippetHealth.Good;

            case SnippetStatus.Timeout:
            case SnippetStatus.ExecutionError:
            case SnippetStatus.ResourceError:
               return SnippetHealth.Error;

            case SnippetStatus.CriticalError:
               return SnippetHealth.Fatal;
         }
         // SnippetStatus.InitializationError
         return SnippetHealth.Unknown;
      }
   }

   public enum SnippetHealth: int {
      Unknown = 0,
      Good = 1,
      Error = 2,
      Fatal = 3
   }

   public class SnippetData {

      public const string SnippetTypeName = "SnippetText";
      public const string SnippetMethodName = "SnippetMain";

      public Guid Id { get; set; }
      public byte[] AssemblyBytes { get; set; }
      public string SnippetSource { get; set; }
      public string UsingDirectives { get; set; }
      public SnippetHealth SnippetHealth { get; set; }
   }

   public class SnippetDataRepository {

      private readonly string connectionString;

      public SnippetDataRepository() {
         connectionString = ConfigurationManager.ConnectionStrings["SnippetsDb"].ConnectionString;
      }

      public SnippetDataRepository(string databasePath) {
         if (!File.Exists(databasePath))
            throw new ArgumentException("Invalid database file", "databasePath");
         connectionString = "DataSource=" + databasePath;
      }

      // TODO: Cache connections and statements
      private SqlCeConnection GetOpenConnection() {
         
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
                  AssemblyBytes = assemblyBytes, 
                  SnippetSource = reader.GetString(2),
                  UsingDirectives = reader.GetString(3),
                  SnippetHealth = (SnippetHealth)reader.GetInt32(4)
               };
            }
         }
         return null;
      }

      public Guid Save(String usings, String source, byte[] assemblyBytes) {
         Guid id = Guid.NewGuid();

         // Save
         using (var connection = GetOpenConnection()) {
            SqlCeCommand cmd = connection.CreateCommand();

            cmd.CommandText = "INSERT INTO Snippets (Id, AssemblyBytes, SnippetSource, UsingDirectives, SnippetHealth) VALUES (?, ?, ?, ?, ?)";

            cmd.Parameters.Add(new SqlCeParameter("Id", SqlDbType.NChar));
            cmd.Parameters.Add(new SqlCeParameter("AssemblyBytes", SqlDbType.Image));
            cmd.Parameters.Add(new SqlCeParameter("SnippetSource", SqlDbType.NText));
            cmd.Parameters.Add(new SqlCeParameter("UsingDirectives", SqlDbType.NText));
            cmd.Parameters.Add(new SqlCeParameter("SnippetHealth", SqlDbType.Int));
           
            cmd.Prepare();

            cmd.Parameters["Id"].Value = id.ToString();
            cmd.Parameters["AssemblyBytes"].Value = assemblyBytes;
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

            cmd.Parameters.Add(new SqlCeParameter("SnippetHealth", SqlDbType.Int));
            cmd.Parameters.Add(new SqlCeParameter("Id", SqlDbType.NChar));

            cmd.Prepare();

            cmd.Parameters["SnippetHealth"].Value = (int)newStatus;
            cmd.Parameters["Id"].Value = id.ToString();
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
                  Id = Guid.Parse(reader.GetString(0)),
                  AssemblyBytes = assemblyBytes, 
                  SnippetSource = reader.GetString(2),
                  UsingDirectives = reader.GetString(3),
                  SnippetHealth = (SnippetHealth)reader.GetInt32(4)
               };
            }
         }
      }
   }
}
