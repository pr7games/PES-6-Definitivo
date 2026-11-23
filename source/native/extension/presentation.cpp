// Exact-build adapter for Nativo 1.3.3. Only the final gamma/presentation shader
// is substituted; resolve/copy shaders and the game's draw shaders are untouched.
#include <windows.h>
#include <MinHook.h>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>
#include <fstream>
#include "generated/native_present.h"

namespace {
using BuildCopy=bool(*)(void*);
using CreateShader=void*(*)(void*,void**,const void*,size_t,const char*,uint32_t);
BuildCopy original_build=nullptr;
CreateShader original_shader=nullptr;
uintptr_t image_base=0;
std::filesystem::path report_path;
std::vector<uint32_t> shader;
std::once_flag install_once;
bool shader_hook_ready=false;
uint32_t output_width=1920,output_height=1080,fsr_enabled=0,fxaa_quality=0;
uint32_t EnvironmentNumber(const char* name,uint32_t fallback,uint32_t low,uint32_t high){
 const auto* value=std::getenv(name);if(!value||!*value)return fallback;
 char* end=nullptr;const auto parsed=std::strtoul(value,&end,10);
 return end&&!*end&&parsed>=low&&parsed<=high?uint32_t(parsed):fallback;
}
bool ConfigureShader(){
 output_width=EnvironmentNumber("PES6_NATIVE_OUTPUT_WIDTH",1920,320,16384);
 output_height=EnvironmentNumber("PES6_NATIVE_OUTPUT_HEIGHT",1080,240,16384);
 fsr_enabled=EnvironmentNumber("PES6_NATIVE_FSR",0,0,1);
 const auto* filter=std::getenv("PES6_NATIVE_POST_EFFECT");
 fxaa_quality=filter&&std::strcmp(filter,"fxaa_extreme")==0?2:filter&&std::strcmp(filter,"fxaa")==0?1:0;
 float reduction=fsr_enabled?0.f:.5f;uint32_t reduction_bits=0;std::memcpy(&reduction_bits,&reduction,4);
 const uint32_t values[]={output_width,output_height,fsr_enabled,fxaa_quality,reduction_bits};
 shader.assign(std::begin(native_present_spirv),std::end(native_present_spirv));
 std::map<uint32_t,uint32_t> spec_ids;
 for(size_t p=5;p<shader.size();){const auto length=shader[p]>>16,opcode=shader[p]&0xffff;if(!length||p+length>shader.size())return false;if(opcode==71&&length==4&&shader[p+2]==1)spec_ids[shader[p+1]]=shader[p+3];p+=length;}
 uint32_t patched=0;
 for(size_t p=5;p<shader.size();){const auto length=shader[p]>>16,opcode=shader[p]&0xffff;if(opcode==50&&length==4){if(const auto item=spec_ids.find(shader[p+2]);item!=spec_ids.end()&&item->second<5){shader[p+3]=values[item->second];patched|=1u<<item->second;}}p+=length;}
 return patched==31;
}
void* CreateShaderHook(void* device,void** result,const void* bytes,size_t size,const char* entry,uint32_t format){
 if(reinterpret_cast<uintptr_t>(bytes)==image_base+0x3a6f0e0u&&size==0x9acu&&format==2){
  auto* returned=original_shader(device,result,shader.data(),shader.size()*4,entry,format);
  std::ofstream log(report_path,std::ios::app);
  log<<"presentation_shader="<<(*result?"created":"failed")<<"\nfsr="<<fsr_enabled<<"\nfxaa="<<fxaa_quality<<"\noutput="<<output_width<<'x'<<output_height<<"\nshader_bytes="<<shader.size()*4<<"\n";
  return returned;
 }
 return original_shader(device,result,bytes,size,entry,format);
}
bool BuildCopyHook(void* state){
 std::call_once(install_once,[&]{
  auto* device=*reinterpret_cast<void**>(static_cast<uint8_t*>(state)+8);
  if(!device)return;
  auto* target=(*reinterpret_cast<void***>(device))[2];
  void* trampoline=nullptr;
  const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&CreateShaderHook),&trampoline);
  if(created!=MH_OK)return;
  original_shader=reinterpret_cast<CreateShader>(trampoline);
  shader_hook_ready=MH_EnableHook(target)==MH_OK;
 });
 if(!shader_hook_ready){std::ofstream log(report_path,std::ios::app);log<<"presentation shader hook failed\n";return false;}
 return original_build(state);
}
}
bool Pr7PreparePresentation(uintptr_t host,const std::filesystem::path& root,std::ofstream& log){
 image_base=host;report_path=root/"data/native-renderer/extension-presentation-status.txt";
 if(!ConfigureShader()){log<<"SPIR-V specialization validation failed\n";return false;}
 {std::ofstream report(report_path,std::ios::trunc);report<<"presentation_bridge=prepared\n";}
 void* trampoline=nullptr;
 const auto created=MH_CreateHook(reinterpret_cast<void*>(host+0x37826e0u),reinterpret_cast<void*>(&BuildCopyHook),&trampoline);
 if(created!=MH_OK){log<<"Presentation bridge failed: "<<MH_StatusToString(created)<<"\n";return false;}
 original_build=reinterpret_cast<BuildCopy>(trampoline);
 log<<"Native presentation bridge prepared: output="<<output_width<<'x'<<output_height<<", FSR="<<fsr_enabled<<", FXAA="<<fxaa_quality<<"\n";
 return true;
}
