using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Security.Cryptography;
using System.Threading;
using System.Web.Script.Serialization;
namespace PES6Installer {
 public static class GamePatches {
  public sealed class Entry { public string Name, OriginalSHA256, ResultSHA256, Patch; public long Length; }
  static string Digest(string file) { using(var h=SHA256.Create()) using(var f=File.OpenRead(file)) return Hex(h.ComputeHash(f)); }
  static string Hex(byte[] b) { return BitConverter.ToString(b).Replace("-",""); }
  static string Safe(string root,string name) {
   string prefix=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
   string result=Path.GetFullPath(Path.Combine(prefix,name));
   if(!result.StartsWith(prefix,StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("Ruta de parche invalida.");
   return result;
  }
  public static void Build(string original,string modified,string output) {
   Directory.CreateDirectory(output);
   IGameReader reader=null;
   bool folder=File.Exists(Path.Combine(original,"default.xex"));
   var source=new Dictionary<string,GameEntry>(StringComparer.OrdinalIgnoreCase);
   try {
    if(!folder) {
     reader=new XboxIsoReader();
     if(!reader.Open(original)){reader.Dispose();reader=new XboxGodReader();if(!reader.Open(original))throw new InvalidDataException("Origen ISO/GOD no reconocido.");}
     foreach(var e in reader.GetEntries()) if(!e.IsDirectory) source[e.Path.Replace('/','\\')]=e;
    }
    var entries=new List<Entry>();
    foreach(string file in Directory.GetFiles(modified).OrderBy(f=>f)) {
     string name=Path.GetFileName(file);
     if(!(name.EndsWith(".img",StringComparison.OrdinalIgnoreCase)||name.EndsWith(".xex",StringComparison.OrdinalIgnoreCase)||name.EndsWith(".wmv",StringComparison.OrdinalIgnoreCase)))continue;
     GameEntry entry=null;
     string sourceFile=Path.Combine(original,name);
     if(folder?!File.Exists(sourceFile):!source.TryGetValue(name,out entry)) throw new FileNotFoundException("Falta en el original: "+name);
     long originalLength=folder?new FileInfo(sourceFile).Length:entry.Size;
     string patch=name+".delta";
     Console.WriteLine("Parche: "+name);
     using(var sourceStream=folder?File.OpenRead(sourceFile):null)
     using(var target=File.OpenRead(file))
     using(var h=SHA256.Create())
     using(var writer=new BinaryWriter(File.Create(Path.Combine(output,patch)))) {
      writer.Write(0x36534550);writer.Write(target.Length);
      byte[] a=new byte[65536],b=new byte[65536];
      long max=Math.Max(originalLength,target.Length);
      for(long offset=0;offset<max;offset+=a.Length) {
       int na=(int)Math.Min(a.Length,Math.Max(0,originalLength-offset)),nb=(int)Math.Min(b.Length,Math.Max(0,target.Length-offset));
       if(na>0) {
        if(folder)ReadExact(sourceStream,a,na);else reader.ReadBytes((long)entry.Sector*2048+offset,a,na);
        h.TransformBlock(a,0,na,a,0);
       }
       if(nb>0)ReadExact(target,b,nb);
       bool same=na==nb;
       if(same) for(int i=0;i<nb;i++)if(a[i]!=b[i]){same=false;break;}
       if(!same&&nb>0){writer.Write(offset);writer.Write(nb);writer.Write(b,0,nb);}
      }
      h.TransformFinalBlock(new byte[0],0,0);writer.Write(-1L);
      entries.Add(new Entry{Name=name,OriginalSHA256=Hex(h.Hash),ResultSHA256=Digest(file),Length=target.Length,Patch=patch});
     }
    }
    File.WriteAllText(Path.Combine(output,"manifest.json"),new JavaScriptSerializer().Serialize(entries));
   } finally {if(reader!=null)reader.Dispose();}
  }
  static void ReadExact(Stream s,byte[] b,int n){int done=0;while(done<n){int r=s.Read(b,done,n-done);if(r==0)throw new EndOfStreamException();done+=r;}}
  public static void Apply(string payload,string game,CancellationToken token) {
   string root=Path.Combine(payload,"game-patches");
   var entries=new JavaScriptSerializer().Deserialize<Entry[]>(File.ReadAllText(Path.Combine(root,"manifest.json")));
   // Validate every source before altering any of them.
   foreach(var e in entries){token.ThrowIfCancellationRequested();if(!Digest(Safe(game,e.Name)).Equals(e.OriginalSHA256,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Copia original incompatible o modificada: "+e.Name);}
   foreach(var e in entries) {
    token.ThrowIfCancellationRequested();
    string file=Safe(game,e.Name);
    using(var r=new BinaryReader(File.OpenRead(Safe(root,e.Patch))))
    using(var f=new FileStream(file,FileMode.Open,FileAccess.Write)) {
     if(r.ReadInt32()!=0x36534550||r.ReadInt64()!=e.Length)throw new InvalidDataException("Cabecera de parche invalida.");
     for(long at=r.ReadInt64();at!=-1;at=r.ReadInt64()) {
      token.ThrowIfCancellationRequested();int count=r.ReadInt32();
      if(at<0||count<0||count>65536||at>e.Length-count)throw new InvalidDataException("Bloque de parche invalido.");
      byte[] bytes=r.ReadBytes(count);if(bytes.Length!=count)throw new EndOfStreamException();
      f.Position=at;f.Write(bytes,0,count);
     }
     f.SetLength(e.Length);
    }
    if(!Digest(file).Equals(e.ResultSHA256,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("Verificacion de parche fallida: "+e.Name);
   }
  }
 }
}
