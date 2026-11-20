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
 internal sealed class UpdateManifest {
  public int schemaVersion {get;set;} public string channel {get;set;} public bool enabled {get;set;}
  public string version {get;set;} public string packagePath {get;set;} public string packageRef {get;set;}
  public string sha256 {get;set;} public long size {get;set;}
 }
 internal sealed class UpdateJob {
  public string Root; public string Package; public string Digest; public string Version; public int Parent;
 }
 internal sealed class UpdateLocalSettings { public string ghExe {get;set;} }
 internal sealed class GitHubContent { public string encoding {get;set;} public string content {get;set;} public long size {get;set;} }
 internal static class PortUpdater {
#if UPDATE_PACKAGE_BUILD
  public const string CurrentVersion="0.2";
#else
  public const string CurrentVersion="0.1";
#endif
  public const string Repository="https://github.com/pr7games/PES-6-Definitivo";
  const string ApiRoot="repos/pr7games/PES-6-Definitivo/contents/";
  public static bool Interactive=true;
  public static readonly string[] LauncherFiles={"PES 6 Launcher.exe","PES 6.exe","bin/PES 6 Launcher.exe","bin/PES 6.exe"};
  public static Version ParseVersion(string value) {
   Version version;
   if(!Version.TryParse((value??"").TrimStart('v','V'),out version))throw new InvalidDataException("Invalid update version.");
   return new Version(version.Major,version.Minor,Math.Max(0,version.Build),Math.Max(0,version.Revision));
  }
  static bool ValidHash(string value) { return value!=null&&value.Length==64&&value.All(Uri.IsHexDigit); }
  public static void Validate(UpdateManifest manifest) {
   if(manifest==null||manifest.schemaVersion!=1||manifest.channel!="test")throw new InvalidDataException("Invalid test manifest.");
   ParseVersion(manifest.version);if(!manifest.enabled)return;
   if(manifest.packagePath!="updates/test/packages/pes6-launcher-0.2.zip"||!ValidHash(manifest.sha256)||manifest.packageRef==null||manifest.packageRef.Length!=40||!manifest.packageRef.All(Uri.IsHexDigit)||manifest.size<=0||manifest.size>8*1024*1024)
    throw new InvalidDataException("Invalid update package, revision or SHA-256.");
  }
  public static void Verify(string path,string digest) {
   if(!ValidHash(digest))throw new InvalidDataException("Missing SHA-256.");
   using(var stream=File.OpenRead(path))using(var hash=SHA256.Create())
    if(BitConverter.ToString(hash.ComputeHash(stream)).Replace("-","").ToLowerInvariant()!=digest.ToLowerInvariant())throw new InvalidDataException("SHA-256 verification failed. No files were changed.");
  }
  static string GhPath(string root) {
   string config=Path.Combine(root,"data","config","updater-local.json");
   if(File.Exists(config)) {
    var settings=new JavaScriptSerializer().Deserialize<UpdateLocalSettings>(File.ReadAllText(config));
    if(settings!=null&&!String.IsNullOrEmpty(settings.ghExe)&&Path.IsPathRooted(settings.ghExe)&&File.Exists(settings.ghExe)&&Path.GetFileName(settings.ghExe).Equals("gh.exe",StringComparison.OrdinalIgnoreCase))return settings.ghExe;
   }
   string standard=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),"GitHub CLI","gh.exe");
   return File.Exists(standard)?standard:null;
  }
  // Private test transport. GitHub CLI retains its own credentials; no token is
  // embedded, requested or passed on the command line by the launcher.
  static void Download(string root,string endpoint,string destination,long limit) {
   string gh=GhPath(root);
   if(gh!=null) {
    var info=new ProcessStartInfo(gh,"api --hostname github.com \""+endpoint+"\"") {UseShellExecute=false,CreateNoWindow=true,RedirectStandardOutput=true,RedirectStandardError=true};
    using(var process=Process.Start(info)) {
     var errors=process.StandardError.ReadToEndAsync();
     var copy=Task.Run(()=>{
      using(var output=new MemoryStream()) {
       byte[] buffer=new byte[65536];long total=0;int count;
       while((count=process.StandardOutput.BaseStream.Read(buffer,0,buffer.Length))>0){total+=count;if(total>limit*2+65536)throw new InvalidDataException("Update download is too large.");output.Write(buffer,0,count);}
       return output.ToArray();
      }
     });
     if(!process.WaitForExit(60000)){try{process.Kill();}catch{}throw new IOException("GitHub download timed out.");}
     byte[] response=copy.GetAwaiter().GetResult();errors.GetAwaiter().GetResult();
     if(process.ExitCode!=0)throw new IOException("No se pudo acceder al repositorio privado. Revisa la sesion de GitHub CLI (gh auth status).");
     var serializer=new JavaScriptSerializer{MaxJsonLength=20*1024*1024};
     var content=serializer.Deserialize<GitHubContent>(System.Text.Encoding.UTF8.GetString(response));
     if(content==null||content.encoding!="base64"||content.size<=0||content.size>limit)throw new InvalidDataException("Invalid GitHub content response.");
     byte[] decoded=Convert.FromBase64String(content.content);
     if(decoded.LongLength!=content.size)throw new InvalidDataException("Incomplete GitHub content.");
     using(var output=new FileStream(destination,FileMode.CreateNew,FileAccess.Write,FileShare.None))output.Write(decoded,0,decoded.Length);
    }
   } else {
    ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
    var request=(HttpWebRequest)WebRequest.Create("https://api.github.com/"+endpoint);
    request.UserAgent="PES6-Definitivo/"+CurrentVersion;request.Accept="application/vnd.github.raw+json";request.Timeout=30000;request.ReadWriteTimeout=30000;
    using(var response=request.GetResponse())using(var input=response.GetResponseStream())using(var output=new FileStream(destination,FileMode.CreateNew)) {
     byte[] buffer=new byte[65536];long total=0;int count;
     while((count=input.Read(buffer,0,buffer.Length))>0){total+=count;if(total>limit)throw new InvalidDataException("Update download is too large.");output.Write(buffer,0,count);}
    }
   }
  }
  public static UpdateManifest Check(string root) {
   string temp=Path.Combine(Path.GetTempPath(),"PES6-feed-"+Guid.NewGuid().ToString("N")+".json");
   try{Download(root,ApiRoot+"updates/test/manifest.json?ref=main",temp,65536);var manifest=new JavaScriptSerializer().Deserialize<UpdateManifest>(File.ReadAllText(temp));Validate(manifest);return manifest;}
   finally{try{File.Delete(temp);}catch{}}
  }
  public static void DownloadPackage(string root,UpdateManifest manifest,string destination) {
   Validate(manifest);if(!manifest.enabled)throw new InvalidDataException("Test feed disabled.");
   Download(root,ApiRoot+manifest.packagePath+"?ref="+manifest.packageRef,destination,manifest.size);
   if(new FileInfo(destination).Length!=manifest.size)throw new InvalidDataException("Incomplete download.");Verify(destination,manifest.sha256);
  }
  public static bool Busy() {
   foreach(string name in new[]{"pes6","pes6-x","PES 6 Editor"})foreach(var process in Process.GetProcessesByName(name))using(process){try{if(!process.HasExited)return true;}catch(InvalidOperationException){}}
   return false;
  }
  static string SafePath(string root,string relative) {
   string rootPath=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
   string full=Path.GetFullPath(Path.Combine(rootPath,relative.Replace('/',Path.DirectorySeparatorChar)));
   if(!full.StartsWith(rootPath+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Unsafe update path.");
   for(string dir=Path.GetDirectoryName(full);dir!=null;dir=Path.GetDirectoryName(dir))
    if(Directory.Exists(dir)&&(File.GetAttributes(dir)&FileAttributes.ReparsePoint)!=0)throw new IOException("Update paths cannot use junctions.");
   if(File.Exists(full)&&(File.GetAttributes(full)&FileAttributes.ReparsePoint)!=0)throw new IOException("Update paths cannot use symbolic links.");return full;
  }
  public static void Apply(string package,string root,string backup,Action<int> afterCopy=null) {
   if(!File.Exists(SafePath(root,"bin/probar-pes6.ps1")))throw new IOException("This is not a PES 6 installation.");
   using(var archive=ZipFile.OpenRead(package)) {
    if(archive.Entries.Count!=LauncherFiles.Length||archive.Entries.Select(e=>e.FullName).Distinct(StringComparer.OrdinalIgnoreCase).Count()!=LauncherFiles.Length)throw new InvalidDataException("Incomplete or duplicate launcher package.");
    foreach(var entry in archive.Entries)if(!LauncherFiles.Contains(entry.FullName,StringComparer.Ordinal)||entry.Length<=0||entry.Length>4*1024*1024)throw new InvalidDataException("Unexpected update file.");
    var changes=new List<Tuple<string,string,bool>>();
    using(var installLock=new FileStream(SafePath(root,"bin/update.lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
     try {
      foreach(var entry in archive.Entries) {
       string target=SafePath(root,entry.FullName),saved=SafePath(backup,entry.FullName);bool exists=File.Exists(target);
       if(exists){Directory.CreateDirectory(Path.GetDirectoryName(saved));File.Copy(target,saved,false);}
       changes.Add(Tuple.Create(target,saved,exists));Directory.CreateDirectory(Path.GetDirectoryName(target));
       using(var input=entry.Open())using(var output=new FileStream(target,FileMode.Create,FileAccess.Write))input.CopyTo(output);
       if(afterCopy!=null)afterCopy(changes.Count);
      }
     }catch(Exception original) {
      var failures=new List<string>();foreach(var change in changes.AsEnumerable().Reverse())try{if(change.Item3)File.Copy(change.Item2,change.Item1,true);else if(File.Exists(change.Item1))File.Delete(change.Item1);}catch(Exception error){failures.Add(error.Message);}
      if(failures.Count>0)throw new IOException("Rollback needs recovery from "+backup+": "+String.Join("; ",failures.ToArray()),original);throw;
     }
    }
   }
  }
  public static int RunHelper(string jobFile) {
   UpdateJob job=null;
   try {
    job=new JavaScriptSerializer().Deserialize<UpdateJob>(File.ReadAllText(jobFile));ParseVersion(job.Version);
    try{using(var parent=Process.GetProcessById(job.Parent))if(!parent.WaitForExit(30000))throw new IOException("Close the launcher before updating.");}catch(ArgumentException){}
    if(Busy())throw new IOException("Close PES 6 and its editor before updating.");
    Verify(job.Package,job.Digest);Apply(job.Package,job.Root,Path.Combine(Path.GetDirectoryName(jobFile),"rollback"));WriteLog(job.Root,"Updated launcher to "+job.Version);
   }catch(Exception error){if(job!=null)WriteLog(job.Root,"FAILED: "+error.Message);MessageBox.Show(error.Message,"PES 6 Definitivo",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
   try{Process.Start(new ProcessStartInfo(Path.Combine(job.Root,"PES 6 Launcher.exe")){WorkingDirectory=job.Root,UseShellExecute=true});}catch(Exception error){WriteLog(job.Root,error.Message);}return 0;
  }
  static void WriteLog(string root,string text){try{string dir=Path.Combine(root,"data","logs");Directory.CreateDirectory(dir);File.AppendAllText(Path.Combine(dir,"update.log"),DateTime.Now.ToString("s")+" "+text+Environment.NewLine);}catch{}}
 }
 public partial class LauncherForm {
  UpdateManifest latestUpdate;bool checkingUpdate,downloadingUpdate;string updateError;
  string InstallRoot(){return Path.GetFileName(rootDir).Equals("bin",StringComparison.OrdinalIgnoreCase)?Path.GetDirectoryName(rootDir):rootDir;}
  void InitializeUpdater() {
   githubProject.Visible=true;githubProject.Enabled=true;githubProject.Click+=(s,e)=>Process.Start(PortUpdater.Repository);
   updateVersion.Visible=true;updateVersion.Click+=async(s,e)=>{if(latestUpdate!=null)await DownloadUpdate();else await CheckUpdate();};
   Shown+=async(s,e)=>{if(PortUpdater.Interactive)await CheckUpdate();};FormClosing+=(s,e)=>{if(downloadingUpdate)e.Cancel=true;};
  }
  async Task CheckUpdate() {
   if(checkingUpdate||downloadingUpdate)return;checkingUpdate=true;updateError=null;latestUpdate=null;SetUpdateAvailable(null);RefreshAbout();
   try{var manifest=await Task.Run(()=>PortUpdater.Check(InstallRoot()));if(manifest.enabled&&PortUpdater.ParseVersion(manifest.version)>PortUpdater.ParseVersion(PortUpdater.CurrentVersion)){latestUpdate=manifest;SetUpdateAvailable(manifest.version);}}
   catch(Exception error){updateError=error.Message;}finally{checkingUpdate=false;if(!IsDisposed)RefreshAbout();}
  }
  async Task DownloadUpdate() {
   if(downloadingUpdate||latestUpdate==null)return;
   if(PortUpdater.Busy()){MessageBox.Show(this,T2("Cierra el juego y el editor antes de actualizar.","Close the game and editor before updating."),Text);return;}
   string stage=Path.Combine(Path.GetTempPath(),"PES6-Update-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(stage);
   string package=Path.Combine(stage,"launcher.zip");downloadingUpdate=true;updateError=null;btnPlay.Enabled=false;RefreshAbout();
   try {
    await Task.Run(()=>PortUpdater.DownloadPackage(InstallRoot(),latestUpdate,package));
    if(PortUpdater.Busy())throw new IOException("Close PES 6 and its editor before updating.");
    SaveSettings();var job=new UpdateJob{Root=InstallRoot(),Package=package,Digest=latestUpdate.sha256,Version=latestUpdate.version,Parent=Process.GetCurrentProcess().Id};
    string jobFile=Path.Combine(stage,"update.json"),helper=Path.Combine(stage,"UpdateHelper.exe");File.WriteAllText(jobFile,new JavaScriptSerializer().Serialize(job));File.Copy(typeof(LauncherForm).Assembly.Location,helper);
    bool elevation=false;string probe=Path.Combine(job.Root,"bin",".update-probe-"+Guid.NewGuid().ToString("N"));
    try{File.WriteAllText(probe,"");File.Delete(probe);}catch(UnauthorizedAccessException){elevation=true;}
    Process.Start(new ProcessStartInfo(helper,"--apply-update \""+jobFile+"\""){UseShellExecute=true,Verb=elevation?"runas":""});downloadingUpdate=false;Close();
   }catch(Exception error){updateError=error.Message;MessageBox.Show(this,T2("No se pudo actualizar.\n","Update failed.\n")+error.Message,Text,MessageBoxButtons.OK,MessageBoxIcon.Information);}
   finally{downloadingUpdate=false;if(!IsDisposed){btnPlay.Enabled=true;RefreshAbout();}}
  }
  void RefreshUpdater() {
   githubProject.Text=T2("GitHub del proyecto","Project GitHub");updateVersion.Text=T2(latestUpdate==null?"Buscar actualizaciones":"Actualizar a "+latestUpdate.version,latestUpdate==null?"Check for updates":"Update to "+latestUpdate.version);
   updateVersion.Enabled=!checkingUpdate&&!downloadingUpdate;updateNotice.ForeColor=Color.Silver;
   if(checkingUpdate)updateNotice.Text=T2("Buscando actualizaciones...","Checking for updates...");
   else if(downloadingUpdate)updateNotice.Text=T2("Descargando y verificando... El launcher se reiniciara.","Downloading and verifying... The launcher will restart.");
   else if(updateError!=null){updateNotice.Text=T2("No se pudo consultar. Revisa el acceso privado a GitHub.","Check failed. Review private GitHub access.");updateNotice.ForeColor=Color.FromArgb(240,110,115);updateNotice.AccessibleDescription=updateError;}
   else if(latestUpdate!=null){updateNotice.Text=T2("Nueva version disponible: ","New version available: ")+latestUpdate.version;updateNotice.ForeColor=Color.FromArgb(240,110,115);}
   else updateNotice.Text=T2("Tienes la version mas reciente.","You have the latest version.");
  }
 }
}
