#pragma once
// Recognize pristine WE bytes at the native reader, including direct crowd atlas loads.
// No binding hooks, per-frame disk work, generated images or dimension-only matches.
inline bool SoftCrowdSource(uint8_t* base,uint32_t source){
 static const bool enabled=[](){const char* v=std::getenv("PES6_SOFT_CROWD");return v&&v[0]=='1'&&v[1]==0;}();
 if(!enabled||!source)return false;
 // Keep the mapping only for this invocation: header, payload and write
 // checks all concern the same source address, not a new allocation.
 MEMORY_BASIC_INFORMATION mapping{};
 if(!VirtualQuery(base+source,&mapping,sizeof(mapping)))return false;
 const auto readable=[&](size_t size){return mapping.State==MEM_COMMIT&&!(mapping.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+source+size<=static_cast<uint8_t*>(mapping.BaseAddress)+mapping.RegionSize;};
 if(!readable(16)||std::memcmp(base+source,"WE00",4))return false;
 const bool staff=base[source+7]==255;
 unsigned size=0;
 if(staff){
  // Exact native sideline atlas layout; all other DDS resources are ignored.
  if(!readable(144)||std::memcmp(base+source+16,"DDS ",4)||std::memcmp(base+source+100,"DXT3",4))return false;
  const auto le32=[&](unsigned at){const auto* p=base+source+at;return unsigned(p[0])|(unsigned(p[1])<<8)|(unsigned(p[2])<<16)|(unsigned(p[3])<<24);};
  if(le32(28)!=512||le32(32)!=256||le32(44)!=2||REX_LOAD_U16(source+8)!=2||REX_LOAD_U16(source+10)!=0x8080)return false;
  size=163984;
 }else{
  if(base[source+7]!=8||REX_LOAD_U16(source+8)!=64||REX_LOAD_U16(source+10)!=128)return false;
  size=9232;
 }
 struct Row{unsigned size;uint64_t hash;std::string file;};
 static std::filesystem::path root;
 static const auto rows=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::ifstream in(root/"data/graphics/soft-crowd/sources.txt");std::vector<Row> result;Row r;while(in>>std::dec>>r.size>>std::hex>>r.hash>>r.file)if(r.size<=263184&&!r.file.empty()&&r.file.size()<40&&r.file.find_first_not_of("0123456789-.we")==std::string::npos)result.push_back(r);return result;}();
 bool candidate=false;for(const auto& row:rows)if(row.size==size){candidate=true;break;}
 if(!candidate||!readable(size))return false;
 uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<size;i++)hash=(hash^base[source+i])*1099511628211ull;
 for(const auto& row:rows)if(row.size==size&&row.hash==hash){
  static std::mutex mutex;static std::map<std::string,std::vector<uint8_t>> cache;std::lock_guard lock(mutex);auto& bytes=cache[row.file];
  if(bytes.empty()){std::ifstream in(root/"data/graphics/soft-crowd"/row.file,std::ios::binary|std::ios::ate);if(!in||in.tellg()!=size)return false;bytes.resize(size);in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),size);if(!in||std::memcmp(bytes.data(),base+source,staff?144:1040)){bytes.clear();return false;}
  }
  if(!(mapping.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  std::memcpy(base+source,bytes.data(),size);return true;
 }
 return false;
}
