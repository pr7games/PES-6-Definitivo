#pragma once
// Per-image fallback: recognize pristine WE bytes immediately before GPU conversion.
// No binding hooks, per-frame disk work, generated images or dimension-only matches.
inline bool CompetitionTextureSource(uint8_t* base,uint32_t source){
 if(!source)return false;
 const auto readable=[&](size_t size){MEMORY_BASIC_INFORMATION m{};return VirtualQuery(base+source,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+source+size<=static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize;};
 if(!readable(16)||std::memcmp(base+source,"WE00",4))return false;
 const unsigned w=REX_LOAD_U16(source+8),h=REX_LOAD_U16(source+10);
 if(!w||!h||w>512||h>512)return false;
 const unsigned bits=base[source+7];
 const bool indexed=bits==8,raw=base[source+6]==3&&(bits==24||bits==32);
 if(!indexed&&!raw)return false;
 const unsigned size=indexed?1040+w*h:16+w*h*(bits/8);
 struct Row{unsigned size;uint64_t hash;std::string file;};
 static std::filesystem::path root;
 static const auto rows=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::vector<Row> result;for(const char* table:{"data/stadiums/global-adboards/sources.txt","data/editor/competitions/textures.txt"}){std::ifstream in(root/table);Row r;while(in>>std::dec>>r.size>>std::hex>>r.hash>>r.file)if(r.size<=263184&&!r.file.empty()&&r.file.size()<40&&r.file.find_first_not_of("0123456789-.we")==std::string::npos)result.push_back(r);}std::ofstream log(root/"data/editor/competitions/runtime.log",std::ios::app);log<<"Texture-source hook active; replacements="<<result.size()<<"\n";return result;}();
 bool candidate=false;for(const auto& row:rows)if(row.size==size)candidate=true;
 if(!candidate||!readable(size))return false;
 uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<size;i++)hash=(hash^base[source+i])*1099511628211ull;
 {static std::mutex traceMutex;static std::map<uint64_t,bool> seen;std::lock_guard lock(traceMutex);if(seen.size()<64&&!seen.count(hash)){seen[hash]=true;std::ofstream log(root/"data/editor/competitions/runtime.log",std::ios::app);log<<"Source candidate "<<size<<" "<<std::hex<<hash<<std::dec<<" "<<w<<"x"<<h<<"\n";}}
 for(const auto& row:rows)if(row.size==size&&row.hash==hash){
  static std::mutex mutex;static std::map<std::string,std::vector<uint8_t>> cache;std::lock_guard lock(mutex);auto& bytes=cache[row.file];
  if(bytes.empty()){std::ifstream in(root/"data/editor/competitions/textures"/row.file,std::ios::binary|std::ios::ate);if(!in||in.tellg()!=size)return false;bytes.resize(size);in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),size);if(!in||std::memcmp(bytes.data(),"WE00",4)||bytes[6]!=base[source+6]||bytes[7]!=base[source+7]||bytes[8]!=base[source+8]||bytes[9]!=base[source+9]||bytes[10]!=base[source+10]||bytes[11]!=base[source+11]){bytes.clear();return false;}
   std::ofstream log(root/"data/editor/competitions/runtime.log",std::ios::app);log<<"GPU source replacement "<<row.file<<" "<<size<<" bytes\n";
  }
  MEMORY_BASIC_INFORMATION writable{};if(!VirtualQuery(base+source,&writable,sizeof(writable))||!(writable.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  std::memcpy(base+source,bytes.data(),size);return true;
 }
 return false;
}
