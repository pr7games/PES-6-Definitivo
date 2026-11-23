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
  public const string CurrentVersion="0.3.1";
#else
  public const string CurrentVersion="0.3.1";
#endif
  public const string Repository="https://github.com/pr7games/PES-6-Definitivo";
  public const string Feed="https://raw.githubusercontent.com/pr7games/PES-6-Definitivo/main/updates/latest.json";
  public static bool Interactive=true;
  static readonly JavaScriptSerializer Json=new JavaScriptSerializer{MaxJsonLength=32*1024*1024};
  public static Version ParseVersion(string value) {Version v;if(!Version.TryParse((value??"").TrimStart('v','V'),out v))throw new InvalidDataException("Version invalida.");return new Version(v.Major,v.Minor,Math.Max(0,v.Build),Math.Max(0,v.Revision));}
  static bool HashValid(string value){return value!=null&&value.Length==64&&value.All(Uri.IsHexDigit);}
  public static string Hash(string file){using(var input=File.OpenRead(file))using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(input)).Replace("-","").ToLowerInvariant();}
  public static void Verify(string file,string digest){if(!HashValid(digest)||!Hash(file).Equals(digest,StringComparison.OrdinalIgnoreCase))throw new InvalidDataException("El SHA-256 no coincide. No se instalara el paquete.");}
  static HttpWebRequest Request(string url){ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;var r=(HttpWebRequest)WebRequest.Create(url);r.UserAgent="PES6-Definitivo/"+CurrentVersion;r.Timeout=30000;r.ReadWriteTimeout=30000;return r;}
  public static UpdateManifest Check(string root) {
   using(var response=Request(Feed+"?check="+DateTime.UtcNow.Ticks).GetResponse())using(var stream=response.GetResponseStream())using(var reader=new StreamReader(stream)){
    char[] buffer=new char[65537];int total=0,count;while(total<buffer.Length&&(count=reader.Read(buffer,total,buffer.Length-total))>0)total+=count;
    if(total>65536)throw new InvalidDataException("Manifiesto demasiado grande.");var m=Json.Deserialize<UpdateManifest>(new string(buffer,0,total));Validate(m);return m;
   }
  }
  public static void Validate(UpdateManifest m){
   if(m==null||m.schemaVersion!=2||m.packages==null||m.packages.Length==0||m.packages.Length>32)throw new InvalidDataException("Manifiesto de actualizacion invalido.");
   ParseVersion(m.version);ParseVersion(m.minimumLauncher);
   string prefix=Repository+"/releases/download/v"+m.version+"/";
   var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
   foreach(var p in m.packages)if(p==null||p.name==null||Path.GetFileName(p.name)!=p.name||!p.name.EndsWith(".zip",StringComparison.Ordinal)||p.name.Any(c=>!(Char.IsLetterOrDigit(c)||c=='.'||c=='-'||c=='_'))||!names.Add(p.name)||p.url!=prefix+p.name||!HashValid(p.sha256)||p.size<=0||p.size>1900L*1024*1024)throw new InvalidDataException("Paquete de actualizacion invalido.");
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
   if(!full.StartsWith(prefix,StringComparison.OrdinalIgnoreCase))throw new IOException("Ruta fuera de la instalacion.");
   for(string d=Path.GetDirectoryName(full);d!=null;d=Path.GetDirectoryName(d))if(Directory.Exists(d)&&(File.GetAttributes(d)&FileAttributes.ReparsePoint)!=0)throw new IOException("No se puede actualizar sobre enlaces de carpetas.");
   if(File.Exists(full)&&(File.GetAttributes(full)&FileAttributes.ReparsePoint)!=0)throw new IOException("No se puede actualizar un enlace.");return full;
  }
  public static bool RuntimePath(string path){
   Relative(path);string r=path.ToLowerInvariant();
   if(r=="pes 6.exe"||r=="pes 6 launcher.exe"||r==".project-layout.json"||r=="port-manifest.json")return true;
   if(r=="bin/pes 6.exe"||r=="bin/pes 6 launcher.exe"||r=="bin/probar-pes6.ps1"||r=="bin/display-config.ps1"||r=="bin/ensure-layout.ps1"||r=="bin/pes6-notice.txt")return true;
   if(r=="docs/pes6-notice.txt"||r=="docs/third_party_renderer.md")return true;
   if(new[]{"bin/native/build/","bin/native-renderer/","bin/launcher-assets/","bin/neural-rendering/","bin/scripts/","data/audio/","data/balls/","data/cloth/","data/graphics/","data/loading-logo/","data/menu-carousel/","data/patches/","data/stadiums/"}.Any(p=>r.StartsWith(p,StringComparison.Ordinal)))return !r.EndsWith(".log")&&!r.Contains("/tests/")&&!r.Contains("/cache/");
   if(r=="data/camera-wide.txt")return true;
   if(r.StartsWith("data/editor/",StringComparison.Ordinal))return !r.Contains("/defaults/")&&!r.Contains("/backups/")&&!r.Contains("/trace/")&&!r.Contains("/tests/")&&!r.EndsWith(".log")&&!r.EndsWith("trace.enabled");
   return false;
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
    if(reader.ReadInt32()!=0x36534550||reader.ReadInt64()!=spec.size)throw new InvalidDataException("Cabecera de parche invalida.");
    long previous=-1;for(long at=reader.ReadInt64();at!=-1;at=reader.ReadInt64()){
     int n=reader.ReadInt32();if(at<0||at<=previous||n<=0||n>65536||at>spec.size-n)throw new InvalidDataException("Bloque de parche invalido.");byte[] bytes=reader.ReadBytes(n);if(bytes.Length!=n)throw new EndOfStreamException();output.Position=at;output.Write(bytes,0,n);previous=at;
    }if(reader.BaseStream.Position!=reader.BaseStream.Length)throw new InvalidDataException("Parche con datos sobrantes.");output.SetLength(spec.size);
   }Verify(target,spec.sha256);
  }
  public static void Apply(UpdateJob job,string payload,Action<int> afterCopy=null){
   string root=job.Root;if(!File.Exists(SafePath(root,"bin/probar-pes6.ps1")))throw new IOException("No se reconoce esta instalacion de PES 6.");
   var inventory=Json.Deserialize<UpdateInventory>(File.ReadAllText(SafePath(payload,"update-inventory.json")));
   if(inventory==null||inventory.version!=job.Manifest.version||inventory.files==null||inventory.files.Length==0)throw new InvalidDataException("Inventario invalido.");
   var destinations=new HashSet<string>(StringComparer.OrdinalIgnoreCase);var prepared=new List<Tuple<string,string>>();
   foreach(var f in inventory.files){if(!RuntimePath(f.path)||!destinations.Add(f.path)||f.size<0||!HashValid(f.sha256))throw new InvalidDataException("Archivo no permitido en inventario.");string source=SafePath(payload,f.path);if(new FileInfo(source).Length!=f.size)throw new InvalidDataException("Tamano incorrecto: "+f.path);Verify(source,f.sha256);prepared.Add(Tuple.Create(f.path,source));}
   foreach(string required in new[]{"PES 6 Launcher.exe","PES 6.exe","bin/PES 6 Launcher.exe","bin/PES 6.exe"})if(!destinations.Contains(required))throw new InvalidDataException("Falta launcher.");
   string gameManifest=SafePath(payload,"game-patches/manifest.json");
   if(File.Exists(gameManifest))foreach(var g in Json.Deserialize<GamePatch[]>(File.ReadAllText(gameManifest))){
    if(g.name==null||Path.GetFileName(g.name)!=g.name||!(g.name.EndsWith(".img",StringComparison.Ordinal)||g.name.EndsWith(".xex",StringComparison.Ordinal)||g.name.EndsWith(".wmv",StringComparison.Ordinal))||g.size<=0||g.size>8L*1024*1024*1024||!HashValid(g.sha256))throw new InvalidDataException("Archivo de juego no permitido.");
    string relative="game/"+g.name;if(!destinations.Add(relative))throw new InvalidDataException("Parche duplicado.");string target=SafePath(root,relative);string current=Hash(target);if(current.Equals(g.sha256,StringComparison.OrdinalIgnoreCase))continue;
    var variant=(g.variants??new PatchVariant[0]).SingleOrDefault(v=>current.Equals(v.sourceSha256,StringComparison.OrdinalIgnoreCase));
    if(variant==null)throw new IOException("La base de "+g.name+" no coincide con una version soportada. No se modifico la instalacion. Reinstala la version base compatible.");
    if(!variant.path.StartsWith("game-patches/",StringComparison.Ordinal)||!variant.path.EndsWith(".delta",StringComparison.Ordinal))throw new InvalidDataException("Ruta de parche invalida.");
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
  UpdateManifest latestUpdate;bool checkingUpdate,downloadingUpdate;string updateError;int downloadPercent;ProgressBar updateProgress;
  string InstallRoot(){return Path.GetFileName(rootDir).Equals("bin",StringComparison.OrdinalIgnoreCase)?Path.GetDirectoryName(rootDir):rootDir;}
  void InitializeUpdater(){
   githubProject.Visible=true;githubProject.Enabled=true;githubProject.Click+=(s,e)=>Process.Start(PortUpdater.Repository);
   updateProgress=new ProgressBar{Height=12,Visible=false,Margin=new Padding(0,0,0,8)};aboutStack.Controls.Add(updateProgress);
   updateVersion.Visible=true;updateVersion.Click+=async(s,e)=>{if(latestUpdate!=null)await DownloadUpdate();else await CheckUpdate();};
   Shown+=async(s,e)=>{if(PortUpdater.Interactive)await CheckUpdate();};FormClosing+=(s,e)=>{if(downloadingUpdate)e.Cancel=true;};
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
    var disk=new DriveInfo(Path.GetPathRoot(stage));if(disk.AvailableFreeSpace<total*3+2L*1024*1024*1024)throw new IOException("No hay espacio suficiente para descargar y preparar la actualizacion.");
    foreach(var asset in latestUpdate.packages){long completed=done;await Task.Run(()=>PortUpdater.DownloadPackage(asset,Path.Combine(stage,asset.name),bytes=>{if(!IsDisposed)BeginInvoke((Action)(()=>{downloadPercent=(int)Math.Min(100,(completed+bytes)*100/total);RefreshAbout();}));}));done+=asset.size;}
    if(PortUpdater.Busy())throw new IOException("Cierra PES 6 antes de actualizar.");SaveSettings();
    var job=new UpdateJob{Root=InstallRoot(),Stage=stage,Manifest=latestUpdate,Parent=Process.GetCurrentProcess().Id};string jobFile=Path.Combine(stage,"update.json"),helper=Path.Combine(stage,"UpdateHelper.exe");
    File.WriteAllText(jobFile,new JavaScriptSerializer().Serialize(job));File.Copy(typeof(LauncherForm).Assembly.Location,helper);
    bool elevate=false;string probe=Path.Combine(job.Root,"bin",".update-probe-"+Guid.NewGuid().ToString("N"));try{File.WriteAllText(probe,"");File.Delete(probe);}catch(UnauthorizedAccessException){elevate=true;}
    Process.Start(new ProcessStartInfo(helper,"--apply-update \""+jobFile+"\""){UseShellExecute=true,Verb=elevate?"runas":""});downloadingUpdate=false;Close();
   }catch(Exception e){updateError=e.Message;MessageBox.Show(this,e.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Information);}finally{downloadingUpdate=false;if(!IsDisposed){btnPlay.Enabled=true;RefreshAbout();}}
  }
  void RefreshUpdater(){githubProject.Text=T2("GitHub del proyecto","Project GitHub");updateVersion.Text=T2(latestUpdate==null?"Buscar actualizaciones":"Actualizar a "+latestUpdate.version,latestUpdate==null?"Check for updates":"Update to "+latestUpdate.version);updateVersion.Enabled=!checkingUpdate&&!downloadingUpdate;updateNotice.ForeColor=Color.Silver;
   if(checkingUpdate)updateNotice.Text=T2("Buscando actualizaciones...","Checking for updates...");else if(downloadingUpdate)updateNotice.Text=downloadPercent>=100?T2("Verificando y preparando instalacion...","Verifying and preparing installation..."):T2("Descargando actualizacion: ","Downloading update: ")+downloadPercent+"%";
   else if(updateError!=null){updateNotice.Text=T2("No se pudo conectar. Pulsa para reintentar.","Connection failed. Press to retry.");updateNotice.AccessibleDescription=updateError;updateNotice.ForeColor=Color.FromArgb(240,110,115);}else if(latestUpdate!=null){updateNotice.Text=T2("Nueva version disponible: ","New version available: ")+latestUpdate.version;updateNotice.ForeColor=Color.FromArgb(240,110,115);}else updateNotice.Text=T2("Tienes la version mas reciente.","You have the latest version.");
   if(updateProgress!=null){updateProgress.Visible=downloadingUpdate;updateProgress.Value=downloadPercent;}
  }
 }
}
