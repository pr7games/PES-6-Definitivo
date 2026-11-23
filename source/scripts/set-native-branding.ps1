param([string]$ProjectRoot=(Split-Path (Split-Path $PSScriptRoot -Parent) -Parent))
$ErrorActionPreference='Stop'
& node (Join-Path $PSScriptRoot 'prepare-shell-icon.cjs')
if($LASTEXITCODE -ne 0){throw 'Could not prepare Explorer icon'}
Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
public static class Pes6BrandResources {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern IntPtr BeginUpdateResource(string file,bool delete);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool UpdateResource(IntPtr h,IntPtr type,IntPtr name,ushort lang,byte[] data,uint size);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool EndUpdateResource(IntPtr h,bool discard);
 static byte[] Block(string key,byte[] value,ushort type,params byte[][] children){
  using(var m=new MemoryStream())using(var w=new BinaryWriter(m)){
   w.Write((ushort)0);w.Write((ushort)(type==1?value.Length/2:value.Length));w.Write(type);w.Write(Encoding.Unicode.GetBytes(key+"\0"));
   while(m.Length%4!=0)w.Write((byte)0);w.Write(value);
   foreach(var c in children){while(m.Length%4!=0)w.Write((byte)0);w.Write(c);}
   var b=m.ToArray();BitConverter.GetBytes((ushort)b.Length).CopyTo(b,0);return b;
  }
 }
 static byte[] Text(string key,string val){return Block(key,Encoding.Unicode.GetBytes(val+"\0"),1);}
 static byte[] Version(){
  var fixedInfo=new byte[52];uint[] words={0xFEEF04BD,0x10000,0x10000,0,0x10000,0,0x3f,0,0x40004,1,0,0,0};
  for(int i=0;i<words.Length;i++)BitConverter.GetBytes(words[i]).CopyTo(fixedInfo,i*4);
  var strings=Block("040904B0",new byte[0],1,
   Text("CompanyName","Golazo Virtual"),Text("FileDescription","PES 6 DEFINITIVO by Golazo Virtual"),
   Text("ProductName","PES 6 DEFINITIVO"),Text("FileVersion","1.0.0.0"),Text("ProductVersion","1.0.0.0"),
   Text("OriginalFilename","pes6.exe"),Text("InternalName","pes6"),
   Text("Comments","Golazo Virtual integration; third-party renderer. See THIRD_PARTY_RENDERER.md."));
  return Block("VS_VERSION_INFO",fixedInfo,0,Block("StringFileInfo",new byte[0],1,strings),
   Block("VarFileInfo",new byte[0],1,Block("Translation",new byte[]{9,4,176,4},0)));
 }
 static void Put(IntPtr h,int type,int id,byte[] b){if(!UpdateResource(h,(IntPtr)type,(IntPtr)id,1033,b,(uint)b.Length))throw new System.ComponentModel.Win32Exception();}
 static uint U(byte[] b,int p){return BitConverter.ToUInt32(b,p);}
 static void W(byte[] b,int p,uint v){BitConverter.GetBytes(v).CopyTo(b,p);}
 static void FixTree(byte[] b,int raw,int at,int delta){int n=BitConverter.ToUInt16(b,at+12)+BitConverter.ToUInt16(b,at+14);for(int i=0;i<n;i++){uint v=U(b,at+16+i*8+4);if((v&0x80000000)!=0)FixTree(b,raw,raw+(int)(v&0x7fffffff),delta);else {int p=raw+(int)v;W(b,p,unchecked((uint)(U(b,p)+delta)));}}}
 static void PreserveLayout(string file,byte[] before){
  byte[] edited=File.ReadAllBytes(file);int pe=(int)U(before,60),opt=pe+24,st=opt+BitConverter.ToUInt16(before,pe+20),n=BitConverter.ToUInt16(before,pe+6),rs=0;uint end=0;
  int editedSt=(int)U(edited,60)+24+BitConverter.ToUInt16(edited,(int)U(edited,60)+20);
  for(int i=0;i<BitConverter.ToUInt16(edited,(int)U(edited,60)+6);i++){int p=editedSt+i*40;if(Encoding.ASCII.GetString(edited,p,8).TrimEnd('\0')==".rsrc")rs=p;}
  if(rs==0)throw new Exception("Missing resource section");
  int baseLength=before.Length;
  if(Encoding.ASCII.GetString(before,st+(n-1)*40,8).TrimEnd('\0')==".pr7res"){baseLength=(int)U(before,st+(n-1)*40+20);n--;}
  uint minRaw=uint.MaxValue;
  for(int i=0;i<n;i++){int p=st+i*40;end=Math.Max(end,U(before,p+12)+Math.Max(U(before,p+8),U(before,p+16)));if(U(before,p+20)>0)minRaw=Math.Min(minRaw,U(before,p+20));}
  if(st+(n+1)*40>minRaw)throw new Exception("No PE header room");
  uint sa=U(before,opt+32),fa=U(before,opt+36),rva=(end+sa-1)/sa*sa,raw=((uint)baseLength+fa-1)/fa*fa,size=U(edited,rs+16);int oldRaw=(int)U(edited,rs+20);
  byte[] b=new byte[raw+size];Array.Copy(before,b,baseLength);Array.Copy(edited,oldRaw,b,raw,size);
  FixTree(b,(int)raw,(int)raw,unchecked((int)(rva-U(edited,rs+12))));
  int header=st+n*40;Array.Clear(b,header,40);Encoding.ASCII.GetBytes(".pr7res").CopyTo(b,header);W(b,header+8,U(edited,rs+8));W(b,header+12,rva);W(b,header+16,size);W(b,header+20,raw);W(b,header+36,0x40000040);
  BitConverter.GetBytes((ushort)(n+1)).CopyTo(b,pe+6);W(b,opt+128,rva);W(b,opt+132,U(edited,rs+8));W(b,opt+56,(rva+size+sa-1)/sa*sa);W(b,opt+64,0);File.WriteAllBytes(file,b);
 }
 public static void Apply(string exe,string ico){
  byte[] before=File.ReadAllBytes(exe);
  byte[] icon=File.ReadAllBytes(ico);int count=BitConverter.ToUInt16(icon,4);
  if(BitConverter.ToUInt16(icon,2)!=1||count==0)throw new Exception("Invalid ICO");
  var group=new byte[6+count*14];Array.Copy(icon,group,6);
  IntPtr h=BeginUpdateResource(exe,false);if(h==IntPtr.Zero)throw new System.ComponentModel.Win32Exception();
  bool ok=false;try{
   for(int i=0;i<count;i++){int p=6+i*16,g=6+i*14;Array.Copy(icon,p,group,g,12);BitConverter.GetBytes((ushort)(i+1)).CopyTo(group,g+12);
    int size=BitConverter.ToInt32(icon,p+8),offset=BitConverter.ToInt32(icon,p+12);var img=new byte[size];Array.Copy(icon,offset,img,0,size);Put(h,3,i+1,img);}
   Put(h,14,1,group);Put(h,16,1,Version());ok=true;
  }finally{if(!EndUpdateResource(h,!ok))throw new System.ComponentModel.Win32Exception();}
  PreserveLayout(exe,before);
 }
}
"@
foreach($relative in @('bin/native-renderer/pes6.exe','bin/native-experimental/pes6.exe','bin/native/build/pes6.exe','bin/native/build/pes6-x.exe')){
 $exe=Join-Path $ProjectRoot $relative
 if(!(Test-Path -LiteralPath $exe -PathType Leaf)){continue}
 [Pes6BrandResources]::Apply($exe,(Join-Path $ProjectRoot 'bin/launcher-assets/icon.ico'))
 Write-Output "Icon and Golazo Virtual version resources updated: $exe"
}
