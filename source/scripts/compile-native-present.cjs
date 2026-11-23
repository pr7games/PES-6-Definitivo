const fs=require('fs'),path=require('path'),cp=require('child_process');const root=path.resolve(__dirname,'..');
const extension=path.join(root,'native/extension'),out=path.join(extension,'generated');fs.mkdirSync(out,{recursive:true});
const compiler=path.join(extension,'thirdparty/dxc/bin/x64/dxc.exe');
const args=['-spirv','-HV','2018','-T','ps_6_0','-E','main','-fspv-target-env=vulkan1.2','-fvk-use-dx-layout','-fspv-extension=SPV_EXT_descriptor_indexing','-O3','-I',path.join(extension,'thirdparty/fsr1'),'-Fo',path.join(out,'native_present.spv'),path.join(extension,'shaders/native_present.hlsl')];
const result=cp.spawnSync(compiler,args,{encoding:'utf8',windowsHide:true});if(result.stdout)console.log(result.stdout);if(result.stderr)console.log(result.stderr);if(result.status!==0)process.exit(result.status||1);
const bytes=fs.readFileSync(path.join(out,'native_present.spv'));if(bytes.readUInt32LE(0)!==0x07230203)throw Error('Not SPIR-V');
const words=[];for(let p=0;p<bytes.length;p+=4)words.push(bytes.readUInt32LE(p));
fs.writeFileSync(path.join(out,'native_present.h'),'#pragma once\n#include <cstdint>\nalignas(4) inline constexpr uint32_t native_present_spirv[]={\n'+words.map(w=>'0x'+w.toString(16)+'u').join(',')+'\n};\n');
console.log('Native EASU+RCAS / FXAA shader:',bytes.length,'bytes');
