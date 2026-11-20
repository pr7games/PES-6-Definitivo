#pragma once
#include <array>
#include <string>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <windows.h>
namespace pes6::editor {
inline constexpr unsigned boot_capacity=128;
inline unsigned boot_count=49;
inline constexpr unsigned custom_boot_count=19;
inline constexpr uint32_t custom_boot_first_entry=6704;
inline std::array<std::string,boot_capacity> boot_names{"Winning Black","Adidas Predator Absolute White/Red","Adidas Predator Absolute Black/Yellow","Adidas Predator Absolute Blue","Adidas Copa Mundial","Adidas +F50 Black/Red","Adidas +F50 White/Red","Adidas +F50 White/Yellow","Adidas Copa Mundial 09","Adidas Copa Mundial 10","Adidas Copa Mundial 11","Adidas 7406","Adidas Copa Mundial 13","Adidas Copa Mundial 14","Adidas Copa Mundial 15","Reebok Pro Rage White/Red","Reebok Pro Rage Orange","Reebok Sprintfit White/Red","Reebok Sprintfit Red/Black","Reebok Integrity Black/White","Adidas Copa Mundial 21","Reebok Sprintfit White/Red 22","Reebok Sprintfit White/Blue","Adidas Copa Mundial 24","Adidas Copa Mundial 25","Winning Black 26","Winning White","Winning Blue","Winning Silver","Winning Red","Nike Mercurial Vapor Iii Royal Blue","Adidas Predator Rojo/Negro","Nike Mercurial Vapor Iii Navy Platinum","Nike Mercurial Vapor Iii Verde","Diadora Italia Oro","Nike Mercurial Vapor Iii Rojo","Adidas Predator Oro","Lotto Zhero Gravity Oro","Nike Tiempo Legend Blanco/Azul","Puma V1 Rojo/Blanco","Nike Mercurial Vapor Iv Orange Blue","Nike Mercurial Vapor Iii Blanco/Rojo","Adidas Predator Blanco/Negro","Lotto Zhero Gravity Blanco/Azul","Puma V1 Azul/Rojo","Puma V1 Blanco/Rojo","Puma V1 Negro/Blanco","Nike Total 90 Laser Azul/Blanco","Nike Tiempo Legend R10 Oro/Blanco"};
inline std::array<uint32_t,boot_capacity> boot_resources{2716,2717,2718,2719,2720,2721,2722,2723,2724,2725,2726,2727,2728,2729,2730,2731,2732,2733,2734,2735,2736,2737,2738,2739,2740,2741,2742,2743,2744,2745,6704,6705,6706,6707,6708,6709,6710,6711,6712,6713,6714,6715,6716,6717,6718,6719,6720,6721,6722};
inline void ReadBootCatalog(const std::filesystem::path& root){
 std::ifstream in(root/"data/editor/asset-runtime.txt");std::string line;
 while(std::getline(in,line)){std::istringstream row(line);std::string kind,name;unsigned id,entry;if(row>>kind>>id>>entry>>std::quoted(name)&&kind=="BOOT"&&id<boot_capacity&&entry<10000&&!name.empty()){boot_names[id]=name;boot_resources[id]=entry;boot_count=std::max(boot_count,id+1);}}
}
inline void EnsureBootCatalog(){
 static bool loaded=false;if(loaded)return;loaded=true;
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();
 for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();ReadBootCatalog(root);
}
inline int BootIndexForResource(uint32_t entry){EnsureBootCatalog();for(unsigned i=0;i<boot_count;i++)if(boot_resources[i]==entry)return int(i);return -1;}
inline uint32_t BootResource(unsigned index){EnsureBootCatalog();return index<boot_count?boot_resources[index]:boot_resources[0];}
inline std::string BootName(unsigned index){return index<boot_count?boot_names[index]:"Sin asignar";}
}
