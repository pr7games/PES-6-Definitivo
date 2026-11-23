// Changes branding strings without moving code, imports, exports or guest hooks.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'../..');
for(const name of ['pes6.exe','rexruntime.dll']){
 const file=path.join(root,'bin/native-renderer',name),b=fs.readFileSync(file);
 const replacements=[['NOBK_ONLINE_TOKEN','PES6_ONLINE_TOKEN'],['NOBK_PATCH','PES6_PATCH'],['NOBK_VOICE','PES6_VOICE'],['[nobk]','[pes6]'],['[nobk-live]','[pes6-live]']];
 if(name==='pes6.exe')replacements.push(['NOBK Online','PR7 Edition']);
 let edits=0;
 for(const [old,value] of replacements){const a=Buffer.from(old),r=Buffer.from(value);if(a.length!==r.length)throw Error('Size mismatch');let p=0;while((p=b.indexOf(a,p))>=0){r.copy(b,p);p+=a.length;edits++;}}
 fs.writeFileSync(file,b);console.log(name+': '+edits+' branding/environment replacements');
}
