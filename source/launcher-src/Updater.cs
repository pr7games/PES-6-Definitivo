using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Security.Cryptography;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

namespace FIFA08Launcher {
 internal sealed class UpdateAsset { public string name {get;set;} public string url {get;set;} public string sha256 {get;set;} public long size {get;set;} }
 internal sealed class UpdateManifest { public int schemaVersion {get;set;} public string version {get;set;} public string minimumLauncher {get;set;} public UpdateAsset[] packages {get;set;} }
 internal sealed class UpdateFile { public string path {get;set;} public string sha256 {get;set;} public long size {get;set;} }
 internal sealed class PatchVariant { public string sourceSha256 {get;set;} public string path {get;set;} }
 internal sealed class GamePatch { public string name {get;set;} public string sha256 {get;set;} public long size {get;set;} public PatchVariant[] variants {get;set;} }
 internal sealed class UpdateInventory { public string version {get;set;} public UpdateFile[] files {get;set;} public string[] remove {get;set;} }
 internal sealed class UpdateJob { public string Root; public string Stage; public UpdateManifest Manifest; public int Parent; public bool Restart=true; }
 internal static class PortUpdater {
#if UPDATE_PACKAGE_BUILD
  public const string CurrentVersion="0.4";
#else
  public const string CurrentVersion="0.4";
#endif
  public const string Repository="https://github.com/pr7games/PES-6-Definitivo";
  public const string Feed="https://raw.githubusercontent.com/pr7games/PES-6-Definitivo/main/updates/latest.json";
  public static bool Interactive=true;
  static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=32*1024*1024};
  public static Version ParseVersion(string value) {Version v;if(!Version.TryParse((value??"").TrimStart('v','V'),out v))throw new InvalidDataException("Versi\u00f3n inv\u00e1lida.");return new Version(v.Major,v.Minor,Math.Max(0,v.Build),Math.Max(0,v.Revision));}
  static bool HashValid(string value){return value!=null&&value.Length==64&&value.All(Uri.IsHexDigit);}
  public static string Hash(string file){using(var input=File.OpenRead(file))using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(input)).Replace("-","").ToLowerInvariant();}
  public static void Verify(string file,string digest){if(!HashValid(digest)||!Hash(file).Equals(digest,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("El SHA-256 no coincide. No se instalar\u00e1 el paquete.");}
  static HttpWebRequest Request(string url){ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;var r=(HttpWebRequest)WebRequest.Create(url);r.UserAgent="PES6-Definitivo/"+CurrentVersion;r.Timeout=30000;r.ReadWriteTimeout=30000;return r;}
  public static UpdateManifest Check(string root) {
   using(var response=Request(Feed+"?check="+DateTime.UtcNow.Ticks).GetResponse())using(var stream=response.GetResponseStream())using(var reader=new StreamReader(stream)){
    char[] buffer=new char[65537];int total=0,count;while(total<buffer.Length&&(count=reader.Read(buffer,total,buffer.Length-total))>0)total+=count;
    if(total>65536)throw new InvalidDataException("Manifiesto demasiado grande.");var m=Json.Deserialize<UpdateManifest>(new string(buffer,0,total));Validate(m);return m;
   }
  }
  public static void Validate(UpdateManifest m){
   if(m==null||m.schemaVersion!=2||m.packages==null||m.packages.Length==0||m.packages.Length>32)throw new InvalidDataException("Manifiesto de actualizaci\u00f3n inv\u00e1lido.");
   ParseVersion(m.version);ParseVersion(m.minimumLauncher);
   string prefix=Repository+"/releases/download/v"+m.version+"/";
   var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var p in m.packages)if(p==null||p.name==null||Path.GetFileName(p.name)!=p.name||!p.name.EndsWith(".zip",StringComparison.Ordinal)||p.name.Any(c=>!(Char.IsLetterOrDigit(c)||c=='.'||c=='-'||c=='_'))||!names.Add(p.name)||p.url!=prefix+p.name||!HashValid(p.sha256)||p.size<=0||p.size>1900L*1024*1024)throw new InvalidDataException("Paquete de actualizaci\u00f3n inv\u00e1lido.");
  }
  public static void DownloadPackage(UpdateAsset asset,string destination,Action<long> progress){
   using(var response=Request(asset.url).GetResponse())using(var input=response.GetResponseStream())using(var output=new FileStream(destination,FileMode.CreateNew,FileAccess.Write,FileShare.None)){
    var buffer=new byte[1024*1024];long total=0;int n;while((n=input.Read(buffer,0,buffer.Length))>0){total+=n;if(total>asset.size)throw new InvalidDataException("Descarga mayor de lo esperado.");output.Write(buffer,0,n);if(progress!=null)progress(total);}if(total!=asset.size)throw new IOException("Descarga incompleta.");
   }Verify(destination,asset.sha256);
  }
  public static bool Busy(){foreach(string name in new[]{"pes6","pes6-x","PES 6 Editor"})foreach(var p in Process.GetProcessesByName(name))using(p){try{if(!p.HasExited)return true;}catch(InvalidOperationException){}}return false;}
  static string Relative(string path){
   if(String.IsNullOrEmpty(path)||path.IndexOf('\\')>=0||path.IndexOf(':')>=0||path.StartsWith("/")||path.Split('/').Any(s=>s.Length==0||s=="."||s==".."||s.EndsWith(".")||s.EndsWith(" ")))throw new InvalidDataException("Ruta no permitida.");return path;
  }
  public static string SafePath(string root,string relative){
   Relative(relative);string full=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar))),prefix=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar;
   if(!full.StartsWith(prefix,StringComparison.OrdinalIgnoreCase))throw new IOException("Ruta fuera de la instalaci\u00f3n.");
   for(string d=Path.GetDirectoryName(full);d!=null;d=Path.GetDirectoryName(d))if(Directory.Exists(d)&&(File.GetAttributes(d)&FileAttributes.ReparsePoint)!=0)throw new IOException("No se puede actualizar sobre enlaces de carpetas.");
   if(File.Exists(full)&&(File.GetAttributes(full)&FileAttributes.ReparsePoint)!=0)throw new IOException("No se puede actualizar un enlace.");return full;
  }
  public static bool RuntimePath(string path){
   Relative(path);string r=path.ToLowerInvariant();
   if(r=="pes 6.exe"||r=="pes 6 launcher.exe"||r==".project-layout.json"||r=="port-manifest.json")return true;
   if(r=="bin/pes 6.exe"||r=="bin/pes 6 launcher.exe"||r=="bin/probar-pes6.ps1"||r=="bin/display-config.ps1"||r=="bin/ensure-layout.ps1"||r=="bin/pes6-notice.txt")return true;
   if(r=="docs/pes6-notice.txt"||r=="docs/third_party_renderer.md")return true;
   if(new[]{"bin/native/build/","bin/native-renderer/","bin/launcher-assets/","bin/neural-rendering/","bin/scripts/","data/audio/","data/balls/","data/cloth/","data/graphics/","data/loading-logo/","data/menu-carousel/","data/patches/","data/stadiums/"}.Any(p=>r.StartsWith(p,StringComparison.Ordinal)))return !r.EndsWith(".log")&&!r.Contains("/tests/")&&!r.Contains("/cache/");
   if(r=="bin/repair/dll-inventory.json"||System.Text.RegularExpressions.Regex.IsMatch(r,@"^bin/repair/pes6-dlls-[0-9]+(.[0-9]+){1,3}.zip$"))return true;
   if(r=="data/camera-wide.txt")return true;
   if(r.StartsWith("data/editor/",StringComparison.Ordinal))return !r.Contains("/defaults/")&&!r.Contains("/backups/")&&!r.Contains("/trace/")&&!r.Contains("/tests/")&&!r.EndsWith(".log")&&!r.EndsWith("trace.enabled");
   return false;
  }
  public static int RepairDlls(string root) {
   using(var installLock=new FileStream(SafePath(root,"bin/update.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
    if(Busy())throw new IOException("Cierra el juego y el editor primero. / Close the game and editor first.");
    string dllManifest=SafePath(root,"bin/repair/dll-inventory.json");
    if(!File.Exists(dllManifest))throw new IOException("Falta el inventario de esta versi\u00f3n. / This version's inventory is missing.");
    var inventory=new JavaScriptSerializer().Deserialize<UpdateInventory>(File.ReadAllText(dllManifest));
    if(inventory==null||inventory.version!=CurrentVersion||inventory.files==null||inventory.files.Length==0||inventory.files.Length>256)
     throw new InvalidDataException("Inventario de reparaci\u00f3n incompatible. / Incompatible repair inventory.");
    var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
    var damaged=new List<UpdateFile>();
    foreach(var dll in inventory.files){
     if(dll==null||!RuntimePath(dll.path)||!dll.path.EndsWith(".dll",StringComparison.OrdinalIgnoreCase)||
        !(dll.path.StartsWith("bin/native/build/",StringComparison.Ordinal)||dll.path.StartsWith("bin/native-renderer/",StringComparison.Ordinal))||
        !HashValid(dll.sha256)||dll.size<=0||dll.size>128L*1024*1024||!names.Add(dll.path))
      throw new InvalidDataException("Entrada DLL inv\u00e1lida. / Invalid DLL entry.");
     string target=SafePath(root,dll.path);
     if(!File.Exists(target)||new FileInfo(target).Length!=dll.size||!Hash(target).Equals(dll.sha256,StringComparison.OrdinalIgnoreCase))damaged.Add(dll);
    }
    if(damaged.Count==0)return 0;

    string archivePath=SafePath(root,"bin/repair/pes6-dlls-"+CurrentVersion+".zip");
    string downloaded=null;
    var staged=new List<Tuple<string,string>>();
    try{
     if(!File.Exists(archivePath)){
      downloaded=Path.Combine(Path.GetTempPath(),"PES6-DLLs-"+Guid.NewGuid().ToString("N")+".zip");
      string url=Repository+"/releases/download/v"+CurrentVersion+"/pes6-dlls-"+CurrentVersion+".zip";
      using(var response=Request(url).GetResponse())using(var input=response.GetResponseStream())
      using(var output=new FileStream(downloaded,FileMode.CreateNew,FileAccess.Write))CopyBounded(input,output,256L*1024*1024);
      archivePath=downloaded;
     }
     using(var archive=ZipFile.OpenRead(archivePath)){
      foreach(var dll in damaged){
       var entry=archive.GetEntry(dll.path);
       if(entry==null||entry.Length!=dll.size)throw new InvalidDataException("DLL ausente del paquete. / DLL missing from package: "+dll.path);
       string target=SafePath(root,dll.path);
       Directory.CreateDirectory(Path.GetDirectoryName(target));
       string temporary=target+".repair-"+Guid.NewGuid().ToString("N");
       staged.Add(Tuple.Create(target,temporary));
       using(var input=entry.Open())using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write))CopyBounded(input,output,dll.size);
       Verify(temporary,dll.sha256);
      }
     }
     if(Busy())throw new IOException("Cierra el juego y el editor primero. / Close the game and editor first.");
     foreach(var file in staged){
      if(File.Exists(file.Item1))File.Replace(file.Item2,file.Item1,null);
      else File.Move(file.Item2,file.Item1);
      WriteLog(root,"Repaired DLL: "+file.Item1);
     }
     return staged.Count;
    }finally{
     foreach(var file in staged)if(File.Exists(file.Item2))File.Delete(file.Item2);
     if(downloaded!=null&&File.Exists(downloaded))File.Delete(downloaded);
    }
   }
  }
  static void CopyBounded(Stream input,Stream output,long maximum){
   var buffer=new byte[65536];long total=0;int count;
   while((count=input.Read(buffer,0,buffer.Length))>0){
    total+=count;
    if(total>maximum)throw new InvalidDataException("Paquete demasiado grande. / Package too large.");
    output.Write(buffer,0,count);
   }
  }
  static bool PackagePath(string p){return p=="update-inventory.json"||RuntimePath(p)||(p.StartsWith("game-patches/",StringComparison.Ordinal)&&p.Split('/').Length==2&&(p.EndsWith(".delta",StringComparison.Ordinal)||p=="game-patches/manifest.json"));}
  public static void Extract(UpdateJob job,string payload){
   Validate(job.Manifest);var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long total=0;
   foreach(var p in job.Manifest.packages){string zip=SafePath(job.Stage,p.name);Verify(zip,p.sha256);
    using(var archive=ZipFile.OpenRead(zip))foreach(var e in archive.Entries){
     if(!seen.Add(e.FullName)||!PackagePath(e.FullName)||e.Length<0||e.Length>8L*1024*1024*1024||(total+=e.Length)>40L*1024*1024*1024)throw new InvalidDataException("Contenido del paquete no permitido.");
     string target=SafePath(payload,e.FullName);Directory.CreateDirectory(Path.GetDirectoryName(target));using(var input=e.Open())using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write))input.CopyTo(output);
    }
   }
  }
  static void Patch(string source,string delta,string target,GamePatch spec){
   File.Copy(source,target,false);
   using(var reader=new BinaryReader(File.OpenRead(delta)))using(var output=new FileStream(target,FileMode.Open,FileAccess.ReadWrite)){
    if(reader.ReadInt32()!=0x36534550||reader.ReadInt64()!=spec.size)throw new InvalidDataException("Cabecera de parche inv\u00e1lida.");
    long previous=-1;for(long at=reader.ReadInt64();at!=-1;at=reader.ReadInt64()){
     int n=reader.ReadInt32();if(at<0||at<=previous||n<=0||n>65536||at>spec.size-n)throw new InvalidDataException("Bloque de parche inv\u00e1lido.");byte[] bytes=reader.ReadBytes(n);if(bytes.Length!=n)throw new EndOfStreamException();output.Position=at;output.Write(bytes,0,n);previous=at;
    }if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new InvalidDataException("Parche con datos sobrantes.");output.SetLength(spec.size);
   }Verify(target,spec.sha256);
  }
  public static void Apply(UpdateJob job,string payload,Action<int> afterCopy=null){
   string root=job.Root;if(!File.Exists(SafePath(root,"bin/probar-pes6.ps1")))throw new IOException("No se reconoce esta instalaci\u00f3n de PES 6.");
   var inventory=Json.Deserialize<UpdateInventory>(File.ReadAllText(SafePath(payload,"update-inventory.json")));
   if(inventory==null||inventory.version!=job.Manifest.version||inventory.files==null||inventory.files.Length==0)throw new InvalidDataException("Inventario inv\u00e1lido.");
   var destinations=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var prepared=new List<Tuple<string,string>>();
   foreach(var f in inventory.files){if(!RuntimePath(f.path)||!destinations.Add(f.path)||f.size<0||!HashValid(f.sha256))throw new InvalidDataException("Archivo no permitido en inventario.");string source=SafePath(payload,f.path);if(new FileInfo(source).Length!=f.size)throw new InvalidDataException("Tamano incorrecto: "+f.path);Verify(source,f.sha256);prepared.Add(Tuple.Create(f.path,source));}
   foreach(string required in new[]{"PES 6 Launcher.exe","PES 6.exe","bin/PES 6 Launcher.exe","bin/PES 6.exe"})if(!destinations.Contains(required))throw new InvalidDataException("Falta launcher.");
   string gameManifest=SafePath(payload,"game-patches/manifest.json");
   if(File.Exists(gameManifest))foreach(var g in Json.Deserialize<GamePatch[]>(File.ReadAllText(gameManifest))){
    if(g.name==null||Path.GetFileName(g.name)!=g.name||!(g.name.EndsWith(".img",StringComparison.Ordinal)||g.name.EndsWith(".xex",StringComparison.Ordinal)||g.name.EndsWith(".wmv",StringComparison.Ordinal))||g.size<=0||g.size>8L*1024*1024*1024||!HashValid(g.sha256))throw new InvalidDataException("Archivo de juego no permitido.");
    string relative="game/"+g.name;if(!destinations.Add(relative))throw new InvalidDataException("Parche duplicado.");string target=SafePath(root,relative);string current=Hash(target);if(current.Equals(g.sha256,StringComparison.OrdinalIgnoreCase))continue;
    var variant=(g.variants??new PatchVariant[0]).SingleOrDefault(v=>current.Equals(v.sourceSha256,StringComparison.OrdinalIgnoreCase));
    if(variant==null)throw new IOException("La base de "+g.name+" no coincide con una versi\u00f3n soportada. No se modific\u00f3 la instalaci\u00f3n. Reinstala la versi\u00f3n base compatible.");
    if(!variant.path.StartsWith("game-patches/",StringComparison.Ordinal)||!variant.path.EndsWith(".delta",StringComparison.Ordinal))throw new InvalidDataException("Ruta de parche inv\u00e1lida.");
    string staged=SafePath(payload,"prepared-game/"+g.name);Directory.CreateDirectory(Path.GetDirectoryName(staged));Patch(target,SafePath(payload,variant.path),staged,g);prepared.Add(Tuple.Create(relative,staged));
   }
   var removals=inventory.remove??new string[0];foreach(string r in removals)if(!RuntimePath(r)||!destinations.Add(r))throw new InvalidDataException("Eliminacion no permitida.");
   string state=SafePath(payload,"prepared-state.json");File.WriteAllText(state,Json.Serialize(new{version=job.Manifest.version,files=inventory.files.Select(f=>f.path).ToArray()}));prepared.Add(Tuple.Create("data/config/update-state.json",state));
   string backup=Path.Combine(job.Stage,"rollback");var changes=new List<Tuple<string,string,bool>>();
   using(var installLock=new FileStream(SafePath(root,"bin/update.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)){
    if(Busy())throw new IOException("Cierra el juego y el editor antes de actualizar.");
    try{
     foreach(var item in prepared.Concat(removals.Select(r=>Tuple.Create(r,(string)null)))){
      string target=SafePath(root,item.Item1),saved=SafePath(backup,item.Item1);bool exists=File.Exists(target);
      if(exists){Directory.CreateDirectory(Path.GetDirectoryName(saved));File.Copy(target,saved,false);}changes.Add(Tuple.Create(target,saved,exists));
      Directory.CreateDirectory(Path.GetDirectoryName(target));if(item.Item2==null){if(exists)File.Delete(target);}else File.Copy(item.Item2,target,true);
      if(afterCopy!=null)afterCopy(changes.Count);
     }
    }catch(Exception original){var failures=new List<string>();foreach(var c in changes.AsEnumerable().Reverse())try{if(c.Item3)File.Copy(c.Item2,c.Item1,true);else if(File.Exists(c.Item1))File.Delete(c.Item1);}catch(Exception e){failures.Add(e.Message);}if(failures.Count>0)throw new IOException("Recuperacion manual necesaria en "+backup,original);throw;}
   }
  }
  public static int RunHelper(string jobFile){
   UpdateJob job=null;try{job=Json.Deserialize<UpdateJob>(File.ReadAllText(jobFile));Validate(job.Manifest);try{using(var parent=Process.GetProcessById(job.Parent))if(!parent.WaitForExit(30000))throw new IOException("Cierra el launcher.");}catch(ArgumentException){}
    if(Busy())throw new IOException("Cierra PES 6 antes de actualizar.");string payload=Path.Combine(job.Stage,"payload");Directory.CreateDirectory(payload);Extract(job,payload);Apply(job,payload);WriteLog(job.Root,"Actualizado a "+job.Manifest.version);
   }catch(Exception e){if(job!=null)WriteLog(job.Root,"ERROR: "+e.Message);MessageBox.Show(e.Message,"PES 6 Definitivo",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
   if(job.Restart)try{Process.Start(new ProcessStartInfo(Path.Combine(job.Root,"PES 6 Launcher.exe")){WorkingDirectory=job.Root,UseShellExecute=true});}catch(Exception e){WriteLog(job.Root,e.Message);}return 0;
  }
  static void WriteLog(string root,string text){try{string dir=Path.Combine(root,"data","logs");Directory.CreateDirectory(dir);File.AppendAllText(Path.Combine(dir,"update.log"),DateTime.Now.ToString("s")+" "+text+Environment.NewLine);}catch{}}
 }
 public partial class LauncherForm {
  Button repairInstallation;
  bool repairingInstallation;
  bool previewUpdate,previewRunning;string previewNotes,previewPhase;
  UpdateManifest latestUpdate;bool checkingUpdate,downloadingUpdate;string updateError;int downloadPercent;ProgressBar updateProgress;
  string InstallRoot(){return Path.GetFileName(rootDir).Equals("bin",StringComparison.OrdinalIgnoreCase)?Path.GetDirectoryName(rootDir):rootDir;}
  void InitializeUpdater(){
   try{string file=Path.Combine(InstallRoot(),"data","config","launcher-update-preview.json");if(File.Exists(file)){var data=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(file));object enabled,notes;previewUpdate=data.TryGetValue("enabled",out enabled)&&Convert.ToBoolean(enabled);if(data.TryGetValue("notes",out notes))previewNotes=Convert.ToString(notes);}}catch{previewUpdate=false;}
   SetUpdateAvailable(null);
   githubProject.Visible=true;githubProject.Enabled=true;githubProject.Click+=(s,e)=>Process.Start(PortUpdater.Repository);
   updateProgress=new ProgressBar{Height=12,Visible=false,Margin=new Padding(0,0,0,8)};aboutStack.Controls.Add(updateProgress);aboutStack.Controls.SetChildIndex(updateProgress,1);
   updateVersion.Visible=true;updateVersion.Click+=async(s,e)=>{if(latestUpdate!=null)await DownloadUpdate();else if(previewUpdate)await SimulateUpdate();else await CheckUpdate();};
   repairInstallation=new ChoiceTile{Height=38,Margin=new Padding(0,0,0,12)};
   aboutStack.Controls.Add(repairInstallation);
   aboutStack.Controls.SetChildIndex(repairInstallation,1);
   repairInstallation.Click+=async(s,e)=>await CheckInstallation();
   Shown+=async(s,e)=>{if(PortUpdater.Interactive&&!previewUpdate)await CheckUpdate();};FormClosing+=(s,e)=>{if(downloadingUpdate||repairingInstallation)e.Cancel=true;};
  }
  async Task CheckInstallation(){
   if(repairingInstallation||downloadingUpdate||runningGameProcess!=null)return;
   repairingInstallation=true;bool canPlay=btnPlay.Enabled;btnPlay.Enabled=false;RefreshAbout();
   try{
    string root=InstallRoot();
    int repaired=await Task.Run(()=>PortUpdater.RepairDlls(root));
    MessageBox.Show(this,repaired==0?T2("Todas las DLL del port est\u00e1n completas y coinciden con esta versi\u00f3n.","All port DLLs match this version."):
     T2("DLL restauradas: ","DLLs restored: ")+repaired,Text,MessageBoxButtons.OK,MessageBoxIcon.Information);
   }catch(Exception error){
    MessageBox.Show(this,T2("No se pudo completar la comprobaci\u00f3n. ","Installation check could not be completed. ")+error.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Warning);
   }finally{
    repairingInstallation=false;btnPlay.Enabled=canPlay;RefreshAbout();
   }
  }
  async Task SimulateUpdate(){
   if(downloadingUpdate||checkingUpdate||!previewUpdate)return;
   previewRunning=true;downloadingUpdate=true;downloadPercent=0;updateError=null;btnPlay.Enabled=false;ShowUpdatePage();
   try{
    for(int percent=0;percent<=100;percent++){
     if(IsDisposed||Disposing)return;
     downloadPercent=percent;
     previewPhase=percent<65?T2("Descargando actualizaci\u00f3n","Downloading update"):percent<82?T2("Verificando archivos","Verifying files"):percent<98?T2("Instalando actualizaci\u00f3n","Installing update"):T2("Finalizando","Finishing");
     RefreshAbout();await Task.Delay(percent<65?85:percent<98?125:300);
    }
    string marker=Path.Combine(InstallRoot(),"data","config","launcher-update-preview.json");
    var settings=new Dictionary<string,object>();
    if(File.Exists(marker))settings=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(marker));
    settings["enabled"]=false;File.WriteAllText(marker,new JavaScriptSerializer().Serialize(settings));
    previewPhase=T2("Actualizaci\u00f3n completada","Update complete");RefreshAbout();await Task.Delay(1400);
    previewUpdate=false;latestUpdate=null;downloadPercent=0;
   }catch(Exception error){updateError=error.Message;}
   finally{
    previewRunning=false;downloadingUpdate=false;previewPhase=null;
    if(!IsDisposed&&!Disposing){btnPlay.Enabled=true;updateNotice.Height=38;SetUpdateAvailable(null);}
   }
  }
  async Task CheckUpdate(){if(checkingUpdate||downloadingUpdate)return;checkingUpdate=true;updateError=null;latestUpdate=null;SetUpdateAvailable(null);RefreshAbout();
   try{var m=await Task.Run(()=>PortUpdater.Check(InstallRoot()));if(PortUpdater.ParseVersion(m.version)>PortUpdater.ParseVersion(PortUpdater.CurrentVersion)){latestUpdate=m;SetUpdateAvailable(m.version);}}
   catch(Exception e){updateError=e.Message;}finally{checkingUpdate=false;if(!IsDisposed)RefreshAbout();}
  }
  async Task DownloadUpdate(){
   if(downloadingUpdate||latestUpdate==null)return;if(PortUpdater.Busy()){MessageBox.Show(this,T2("Cierra el juego y el editor antes de actualizar.","Close the game and editor before updating."),Text);return;}
   if(PortUpdater.ParseVersion(latestUpdate.minimumLauncher)>PortUpdater.ParseVersion(PortUpdater.CurrentVersion)){MessageBox.Show(this,T2("Descarga el launcher actualizado desde GitHub.","Download the updated launcher from GitHub."),Text);return;}
   string stage=Path.Combine(Path.GetTempPath(),"PES6-Update-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);downloadingUpdate=true;downloadPercent=0;updateError=null;btnPlay.Enabled=false;RefreshAbout();
   try{
    long total=latestUpdate.packages.Sum(p=>p.size),done=0;
    var disk=new DriveInfo(Path.GetPathRoot(stage));if(disk.AvailableFreeSpace<total*3+2L*1024*1024*1024)throw new IOException("No hay espacio suficiente para descargar y preparar la actualizaci\u00f3n.");
    foreach(var asset in latestUpdate.packages){long completed=done;await Task.Run(()=>PortUpdater.DownloadPackage(asset,Path.Combine(stage,asset.name),bytes=>{if(!IsDisposed)BeginInvoke((Action)(()=>{downloadPercent=(int)Math.Min(100,(completed+bytes)*100/total);RefreshAbout();}));}));done+=asset.size;}
    if(PortUpdater.Busy())throw new IOException("Cierra PES 6 antes de actualizar.");SaveSettings();
    var job=new UpdateJob{Root=InstallRoot(),Stage=stage,Manifest=latestUpdate,Parent=Process.GetCurrentProcess().Id};string jobFile=Path.Combine(stage,"update.json"),helper=Path.Combine(stage,"UpdateHelper.exe");
    File.WriteAllText(jobFile,new JavaScriptSerializer().Serialize(job));File.Copy(typeof(LauncherForm).Assembly.Location,helper);
    bool elevate=false;string probe=Path.Combine(job.Root,"bin",".update-probe-"+Guid.NewGuid().ToString("N"));try{File.WriteAllText(probe,"");File.Delete(probe);}catch(UnauthorizedAccessException){elevate=true;}
    Process.Start(new ProcessStartInfo(helper,"--apply-update \""+jobFile+"\""){UseShellExecute=true,Verb=elevate?"runas":""});downloadingUpdate=false;Close();
   }catch(Exception e){updateError=e.Message;MessageBox.Show(this,e.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Information);}finally{downloadingUpdate=false;if(!IsDisposed){btnPlay.Enabled=true;RefreshAbout();}}
  }
  void ShowUpdatePage(){
   for(int i=0;i<modernPages.Length;i++){modernPages[i].Visible=i==5;((NavigationTab)tabButtons[i]).Selected=i==5;}modernPages[5].BringToFront();aboutStack.AutoScrollPosition=Point.Empty;RefreshAbout();
  }
  void SizeAboutHeader(){if(aboutHeader==null||aboutHeader.Width<40)return;int width=Math.Max(1,aboutHeader.ClientSize.Width-aboutHeader.Padding.Horizontal);int height=TextRenderer.MeasureText(aboutDetails.Text,aboutDetails.Font,new Size(width,int.MaxValue),TextFormatFlags.WordBreak|TextFormatFlags.NoPrefix).Height;aboutHeader.Height=height+aboutHeader.Padding.Vertical+(updateAvailable?aboutUpdateTitle.Height:0)+8;}
  void RefreshUpdateAccent(){
   if(aboutHeader==null)return;aboutHeader.BackColor=Color.FromArgb(36,37,41);aboutDetails.ForeColor=Color.Silver;aboutUpdateTitle.ForeColor=Color.FromArgb(74,222,128);aboutUpdateTitle.Visible=updateAvailable;aboutUpdateTitle.Text=T2("Actualizaci\u00f3n disponible","Update available")+" - "+(availableVersion??PortUpdater.CurrentVersion)+(previewUpdate&&latestUpdate==null?T2("\nVista previa visual","\nVisual preview"):"");((NavigationTab)tabButtons[5]).Attention=updateAvailable;tabButtons[5].Invalidate();if(updatePill!=null)updatePill.Text=T2("Actualizaci\u00f3n disponible - v","Update available - v")+(availableVersion??PortUpdater.CurrentVersion);SizeAboutHeader();if(modernReady)ModernLayout();
  }
  void RefreshUpdater(){githubProject.Text=T2("GitHub del proyecto","Project GitHub");updateVersion.Text=T2(latestUpdate==null?"Buscar actualizaciones":"Actualizar a "+latestUpdate.version,latestUpdate==null?"Check for updates":"Update to "+latestUpdate.version);updateVersion.Enabled=!checkingUpdate&&!downloadingUpdate&&!repairingInstallation;if(repairInstallation!=null){repairInstallation.Enabled=!repairingInstallation&&!downloadingUpdate;repairInstallation.Text=repairingInstallation?T2("Comprobando instalaci\u00f3n...","Checking installation..."):T2("Comprobar instalaci\u00f3n","Check installation");}updateVersion.BackColor=updateAvailable?PortUi.Accent:Color.FromArgb(44,45,50);updateNotice.ForeColor=Color.Silver;
   if(checkingUpdate)updateNotice.Text=T2("Buscando actualizaciones...","Checking for updates...");else if(downloadingUpdate)updateNotice.Text=downloadPercent>=100?T2("Verificando y preparando instalaci\u00f3n...","Verifying and preparing installation..."):T2("Descargando actualizaci\u00f3n: ","Downloading update: ")+downloadPercent+"%";
   else if(updateError!=null){updateNotice.Text=T2("No se pudo conectar. Pulsa para reintentar.","Connection failed. Press to retry.");updateNotice.AccessibleDescription=updateError;updateNotice.ForeColor=Color.FromArgb(240,110,115);}else if(latestUpdate!=null){updateNotice.Text=T2("Nueva versi\u00f3n disponible: ","New version available: ")+latestUpdate.version;updateNotice.ForeColor=Color.FromArgb(240,110,115);}else updateNotice.Text=T2("Tienes la versi\u00f3n mas reciente.","You have the latest version.");
   if(previewUpdate&&latestUpdate==null&&!downloadingUpdate){updateVersion.Text=T2("Actualizar a ","Update to ")+PortUpdater.CurrentVersion+T2(" (prueba visual)"," (visual preview)");updateNotice.ForeColor=Color.FromArgb(175,219,187);updateNotice.Text=T2("Vista previa visual - Misma versi\u00f3n ","Visual preview - Same version ")+PortUpdater.CurrentVersion+"\n"+(previewNotes??"");updateNotice.Height=Math.Max(70,TextRenderer.MeasureText(updateNotice.Text,updateNotice.Font,new Size(Math.Max(1,aboutStack.ClientSize.Width-32),int.MaxValue),TextFormatFlags.WordBreak).Height+12);}
   if(previewRunning){updateVersion.Text=T2("Actualizando...","Updating...");updateNotice.Text=previewPhase+" - "+downloadPercent+"%"+T2("\nSimulacion visual: no se descargan ni reemplazan archivos.","\nVisual simulation: no files are downloaded or replaced.");updateNotice.Height=52;updateNotice.ForeColor=Color.FromArgb(0,201,125);}
   if(updateProgress!=null){updateProgress.Visible=downloadingUpdate;updateProgress.Value=downloadPercent;}
  }
 }
}
