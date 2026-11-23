// Build an isolated compatibility extension from our existing patched PPC functions.
// Nothing is installed into the shipping renderer by this generator.
const fs=require('fs'),path=require('path');
const root=path.resolve(__dirname,'..'),source=path.join(root,'native/generated/default'),output=path.join(root,'native/extension/generated');fs.mkdirSync(output,{recursive:true});
const map=JSON.parse(fs.readFileSync(path.join(root,'analysis/native-function-map.json'))),mappings=new Map(map.functions.map(f=>[Number(f.guest),f]));
const init=fs.readFileSync(path.join(source,'pes6_init.cpp'),'utf8'),names=new Map([...init.matchAll(/\{ (0x[0-9A-F]+), (\w+) \}/g)].map(m=>[m[2],parseInt(m[1],16)]));
const selected=[],prefix=new Set(),allBodies=[];
for(const file of fs.readdirSync(source).filter(f=>/^pes6_recomp\.\d+\.cpp$/.test(f))){
 const text=fs.readFileSync(path.join(source,file),'utf8');
 for(const line of text.slice(0,text.indexOf('DEFINE_REX_FUNC(')).split(/\r?\n/))if(line.trim()&&!line.startsWith('#include "pes6_funcs'))prefix.add(line);
 for(const match of text.matchAll(/DEFINE_REX_FUNC\((\w+)\) \{\n/g)){
  const end=text.indexOf('\n}\n',match.index);if(end<0)throw Error('Unterminated '+match[1]);
  const body=text.slice(match.index,end+3);allBodies.push({name:match[1],body});
  if(/pes6_|PES6|BootResource/.test(body)&&match[1]!=='sub_82102F28')selected.push({name:match[1],body});
 }
}
prefix.add('extern PPCFunc* Pr7Original(uint32_t);');
const nativeFlush=selected.find(f=>f.name==='sub_821AC7F8');
if(nativeFlush)nativeFlush.body='DEFINE_REX_FUNC(sub_821AC7F8) { pes6_stadium_lightmap_state(ctx,base); static auto* original=Pr7Original(0x821AC7F8u); original(ctx,base); pes6_stadium_lightmap_draw_end(ctx,base); }\n';
const selectedNames=new Set(selected.map(f=>f.name));
const sources=['runtime_compat.cpp','player_editor_runtime.cpp','native_player_editor.cpp','native_player_preview.cpp','setpiece_gameplay.cpp','game_camera.cpp','game_cloth.cpp','game_cloth_resources.cpp','game_net_sound.cpp','game_weather.cpp','game_weather_menu.cpp','game_stadium.cpp','game_ball.cpp'];
let combined=selected.map(f=>f.body).join('\n')+sources.map(f=>fs.readFileSync(path.join(root,'native/src',f),'utf8')).join('\n');
for(const file of fs.readdirSync(path.join(root,'native/src')).filter(f=>/\.(h|inl)$/.test(f)))combined+='\n'+fs.readFileSync(path.join(root,'native/src',file),'utf8');
const forwardNames=new Set([...combined.matchAll(/\b(?:__imp__)?sub_[0-9A-F]{8}\b/g)].map(m=>m[0]));
// Non-sub save/restore helper calls are present in translated bodies as well.
for(const name of names.keys())if(name.startsWith('__rest')||name.startsWith('__save'))if(combined.includes(name))forwardNames.add(name);
const forwards=[];
for(const name of forwardNames){if(selectedNames.has(name))continue;const baseName=name.replace(/^__imp__(?=sub_)/,'');const guest=names.get(baseName);if(!guest)throw Error('Unknown forwarded guest '+name);const row=mappings.get(guest);if(!row)throw Error('Missing native mapping '+name);forwards.push(`extern "C" void ${name}(PPCContext& ctx,uint8_t* base){static auto* function=Pr7Original(${guest}u);function(ctx,base);}`);}
const hooks=[];
// This guest entry is an internal block of82292010 in the native host (no
// independent address to detour). Its patched parent calls our compiled body.
const compileOnlyParents=new Map([['sub_823E2168','sub_823E2070'],['sub_82422A98','sub_82422A30'],['sub_82301670','sub_823015E8'],['sub_823CE444','sub_823CE328'],['sub_822926D4','sub_82292010'],['sub_8249E178','sub_8249E020'],['sub_826AA5C8','sub_826AA568'],['sub_826AA754','sub_826AA568'],['sub_8247AF08','sub_8247AB98'],['sub_8247ABF4','sub_8247AB98']]),compileOnly=[];
for(const fn of selected){
 // Plume owns device initialization. Mesh submission above wraps its native
 // implementation and applies only the scoped stadium depth/blend state.
 if(fn.name==='sub_82102F28')continue;
 const guest=names.get(fn.name),row=mappings.get(guest);if(!row){
 const parent=compileOnlyParents.get(fn.name),body=selected.find(f=>f.name===parent)?.body;
 if(!parent||!body?.includes(fn.name+'(ctx, base)'))throw Error('No host entry for '+fn.name);
 compileOnly.push(fn.name);continue;
}hooks.push({name:fn.name,guest,rva:row.rva,offset:row.offset});}
fs.writeFileSync(path.join(output,'patched.cpp'),'#include "../../generated/default/pes6_funcs.h"\n'+[...prefix].join('\n')+'\n'+selected.map(f=>f.body).join('\n'));
fs.writeFileSync(path.join(output,'forwards.cpp'),'#include "../../generated/default/pes6_funcs.h"\nextern PPCFunc* Pr7Original(uint32_t);\n'+forwards.join('\n'));
const samplerWrapper=`
extern PPCFunc* Pr7Original(uint32_t);
extern "C" void pes6_native_sampler_fast(PPCContext& ctx,uint8_t* base){
 const auto curves=REX_LOAD_U32(ctx.r3.u32+4),header=REX_LOAD_U32(curves+24);
 if(base[header+6]==0x50&&base[header+7]==0x36){sub_821F2DA8(ctx,base);return;}
 static PPCFunc* original=Pr7Original(0x821F2DA8u);original(ctx,base);
}
`;
fs.appendFileSync(path.join(output,'patched.cpp'),samplerWrapper);
// Original stadiums use their original host functions directly. This avoids
// entering the imported venue dispatcher or expanded crowd implementation.
const venueWrappers=new Map();
fs.appendFileSync(path.join(output,'patched.cpp'),'\nextern \"C\" bool pes6_stadium_sunset();\n');
for(const name of ['sub_8214D498','sub_8215F610','sub_8216BC78','sub_8216C710','sub_8216C3A0']){
 const guest=names.get(name),wrapper=name+'_native_venue';venueWrappers.set(name,wrapper);
 fs.appendFileSync(path.join(output,'patched.cpp'),`\nextern "C" void ${wrapper}(PPCContext& ctx,uint8_t* base){\n const auto config=REX_LOAD_U32(0x83139360u);\n if((config&&base[config+130]>=8&&base[config+130]<=10)${name==='sub_8214D498'?'||(config&&base[config+130]==5)||pes6_stadium_sunset()':''}){${name}(ctx,base);return;}\n static PPCFunc* original=Pr7Original(${guest}u);original(ctx,base);\n}\n`);
}
fs.writeFileSync(path.join(output,'hooks.inc'),hooks.map(h=>`{${h.guest}u,${h.rva}u,&${h.name==='sub_821F2DA8'?'pes6_native_sampler_fast':venueWrappers.get(h.name)??h.name},"${h.name}"},`).join('\n'));
fs.writeFileSync(path.join(output,'originals.inc'),map.functions.map(f=>`{${Number(f.guest)}u,${f.rva}u},`).join('\n'));
fs.writeFileSync(path.join(output,'inventory.json'),JSON.stringify({status:'experimental-uninstalled',hooks,compileOnly,forwardCount:forwards.length,sources},null,2));
const overlay=fs.readFileSync(path.join(root,'native/src/pc_overlay.h'),'utf8');
const drawStart=overlay.indexOf('  void OnDraw(ImGuiIO&) override {'),drawEnd=overlay.indexOf('\n  rex::ui::Window* window_',drawStart);
if(drawStart<0||drawEnd<0)throw Error('PC overlay extraction anchors changed');
const draw=overlay.slice(drawStart,drawEnd).replace('override ','').replaceAll('pes6_frame_submissions.load(std::memory_order_relaxed)','native_overlay_frames.load(std::memory_order_relaxed)');
const members=overlay.slice(drawEnd,overlay.lastIndexOf('};'));
fs.writeFileSync(path.join(output,'overlay.h'),`// Drawing logic shared verbatim with native/src/pc_overlay.h.\ninline ImFont* pes6_pc_menu_font=nullptr;\ninline std::atomic<uint64_t> native_overlay_frames{0};\nclass NativePcOverlay {\nusing Clock=std::chrono::steady_clock;\npublic:\nNativePcOverlay(bool fps,std::function<void()> exit):show_fps_(fps),exit_(std::move(exit)){}\nvoid Paint(){native_overlay_frames.fetch_add(1,std::memory_order_relaxed);if(ImGui::IsKeyPressed(ImGuiKey_F10,false))menu_.store(!menu_.load());OnDraw(ImGui::GetIO());}\nprivate:\nvoid SyncAttachment(){}\n${draw}\n${members}\n};\n`);
console.log(JSON.stringify({selected:hooks.length,forwards:forwards.length,sources},null,2));

