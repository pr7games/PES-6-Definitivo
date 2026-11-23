#pragma once
#include <rex/system/kernel_state.h>
#include <rex/runtime.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>
#include <imgui.h>
#include <xinput.h>
// Shared by every text request, using UTF-8 internally and guest UTF-16 BE.
inline void PaintXboxKeyboard(pes6::ui_tools::KeyboardState& k){
 using namespace pes6::ui_tools;
 if(!k.active.load(std::memory_order_acquire))return;
 std::lock_guard lock(k.mutex);
 auto prev=[&](){if(k.cursor){--k.cursor;while(k.cursor&&(uint8_t(k.text[k.cursor])&0xc0)==0x80)--k.cursor;}};
 auto next=[&](){if(k.cursor<k.text.size()){++k.cursor;while(k.cursor<k.text.size()&&(uint8_t(k.text[k.cursor])&0xc0)==0x80)++k.cursor;}};
 auto erase=[&](){auto end=k.cursor;prev();k.text.erase(k.cursor,end-k.cursor);};
 auto insert=[&](const char* value){std::string candidate=k.text;candidate.insert(k.cursor,value);int units=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,candidate.data(),int(candidate.size()),nullptr,0);if(units>0&&unsigned(units)<=k.max_chars){k.text=std::move(candidate);k.cursor+=strlen(value);}};
 auto finish=[&](bool accept){k.accepted=accept;k.active.store(false,std::memory_order_release);};
 auto& io=ImGui::GetIO();
 ImGui::SetNextFrameWantCaptureKeyboard(true);
 ImGui::SetNextFrameWantCaptureMouse(true);
 for(int i=0;i<io.InputQueueCharacters.Size;i++){unsigned cp=io.InputQueueCharacters[i];if(cp>=32&&cp!=127){char utf8[5]{};if(cp<128)utf8[0]=char(cp);else if(cp<2048){utf8[0]=char(0xc0|(cp>>6));utf8[1]=char(0x80|(cp&63));}else{utf8[0]=char(0xe0|(cp>>12));utf8[1]=char(0x80|((cp>>6)&63));utf8[2]=char(0x80|(cp&63));}insert(utf8);}}
 io.InputQueueCharacters.resize(0);
 if(ImGui::IsKeyPressed(ImGuiKey_Backspace))erase();
 if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow))prev();
 if(ImGui::IsKeyPressed(ImGuiKey_RightArrow))next();
 if(ImGui::IsKeyPressed(ImGuiKey_Home))k.cursor=0;
 if(ImGui::IsKeyPressed(ImGuiKey_End))k.cursor=k.text.size();
 if(ImGui::IsKeyPressed(ImGuiKey_Delete)){auto start=k.cursor;next();k.text.erase(start,k.cursor-start);k.cursor=start;}
 if(ImGui::IsKeyPressed(ImGuiKey_Enter))finish(true);
 if(ImGui::IsKeyPressed(ImGuiKey_Escape))finish(false);
 const char* lower[4][10]={{"1","2","3","4","5","6","7","8","9","0"},{"q","w","e","r","t","y","u","i","o","p"},{"a","s","d","f","g","h","j","k","l","'"},{"z","x","c","v","b","n","m",",",".","?"}};
 const char* upper[4][10]={{"1","2","3","4","5","6","7","8","9","0"},{"Q","W","E","R","T","Y","U","I","O","P"},{"A","S","D","F","G","H","J","K","L","'"},{"Z","X","C","V","B","N","M",",",".","?"}};
 const char* symbols[4][10]={{"!","@","#","$","%","^","&","*","(",")"},{"[","]","{","}","<",">","+","-","=","_"},{"/","\\","|","~","`",":",";","\"","'","?"},{"1","2","3","4","5","6","7","8","9","0"}};
 const char* accents[4][10]={{"á","é","í","ó","ú","à","è","ì","ò","ù"},{"ä","ë","ï","ö","ü","â","ê","î","ô","û"},{"ñ","ç","ã","õ","å","æ","œ","ø","ß","ý"},{"Á","É","Í","Ó","Ú","Ñ","Ç","¡","¿","ÿ"}};
 auto activate=[&](){if(k.row==4){if(k.col==0)k.shift=!k.shift;else if(k.col<3)finish(false);else if(k.col<6)insert(" ");else if(k.col<8)k.accents=!k.accents;else if(k.col<10)erase();else finish(true);}else if(k.col==0){if(k.row==0)k.symbols=!k.symbols;else if(k.row<3)prev();else k.shift=!k.shift;}else if(k.col==11){if(k.row==0)k.shift=!k.shift;else if(k.row<3)next();else finish(true);}else insert((k.accents?accents:k.symbols?symbols:k.shift?upper:lower)[k.row][k.col-1]);};
 static auto input=[](){HMODULE m=LoadLibraryW(L"xinput1_4.dll");if(!m)m=LoadLibraryW(L"xinput9_1_0.dll");return m?reinterpret_cast<DWORD(WINAPI*)(DWORD,XINPUT_STATE*)>(GetProcAddress(m,"XInputGetState")):nullptr;}();
 XINPUT_STATE xs{};bool connected=false;if(input)for(DWORD i=0;i<4;i++)if(input(i,&xs)==ERROR_SUCCESS){connected=true;break;}
 if(connected){WORD b=xs.Gamepad.wButtons,pressed=b&~k.prev_buttons;k.prev_buttons=b;auto now=GetTickCount64();int dx=0,dy=0;const bool repeat=now-k.last_move>170;
 if((pressed&XINPUT_GAMEPAD_DPAD_LEFT)||(repeat&&((b&XINPUT_GAMEPAD_DPAD_LEFT)||xs.Gamepad.sThumbLX<-16000)))dx=-1;
 if((pressed&XINPUT_GAMEPAD_DPAD_RIGHT)||(repeat&&((b&XINPUT_GAMEPAD_DPAD_RIGHT)||xs.Gamepad.sThumbLX>16000)))dx=1;
 if((pressed&XINPUT_GAMEPAD_DPAD_UP)||(repeat&&((b&XINPUT_GAMEPAD_DPAD_UP)||xs.Gamepad.sThumbLY>16000)))dy=-1;
 if((pressed&XINPUT_GAMEPAD_DPAD_DOWN)||(repeat&&((b&XINPUT_GAMEPAD_DPAD_DOWN)||xs.Gamepad.sThumbLY<-16000)))dy=1;
 if(dx||dy){k.row=(k.row+dy+5)%5;if(k.row==4&&dx){const int columns[]={0,1,3,6,8,11};int best=0;for(int i=0;i<6;i++)if(std::abs(columns[i]-k.col)<std::abs(columns[best]-k.col))best=i;k.col=columns[(best+dx+6)%6];}else k.col=(k.col+dx+12)%12;k.last_move=now;}
 bool lt=xs.Gamepad.bLeftTrigger>128,rt=xs.Gamepad.bRightTrigger>128;if(lt&&!k.left_trigger)k.symbols=!k.symbols;if(rt&&!k.right_trigger)k.shift=!k.shift;k.left_trigger=lt;k.right_trigger=rt;
 if(pressed&XINPUT_GAMEPAD_A)activate();if(pressed&XINPUT_GAMEPAD_X)erase();if(pressed&XINPUT_GAMEPAD_Y)insert(" ");if(pressed&XINPUT_GAMEPAD_B)k.accents=!k.accents;
 if(pressed&XINPUT_GAMEPAD_LEFT_SHOULDER)prev();if(pressed&XINPUT_GAMEPAD_RIGHT_SHOULDER)next();if(pressed&XINPUT_GAMEPAD_LEFT_THUMB)finish(false);if(pressed&XINPUT_GAMEPAD_RIGHT_THUMB)k.shift=!k.shift;if(pressed&XINPUT_GAMEPAD_START)finish(true);
 }
 float scale=std::min(io.DisplaySize.x/780.f,io.DisplaySize.y/330.f),w=740*scale,h=238*scale;
 ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y-h-12*scale),ImGuiCond_Always,ImVec2(.5f,0));ImGui::SetNextWindowSize(ImVec2(w,h),ImGuiCond_Always);
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleColor(ImGuiCol_WindowBg,IM_COL32(17,17,17,255));
 ImGui::Begin("##XboxKeyboard",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav);
 const auto origin=ImGui::GetWindowPos();auto* draw=ImGui::GetWindowDrawList();std::string shown=k.text;shown.insert(k.cursor,"|");ImGui::PushFont(nullptr,16.f*scale);draw->AddText(ImVec2(origin.x+15*scale,origin.y+12*scale),IM_COL32_WHITE,shown.c_str());ImGui::PopFont();
 auto cell=[&](int r,int c,float x,float y,float cw,float ch,const char* label){ImVec2 a(origin.x+x*scale,origin.y+(y+45)*scale),z(a.x+cw*scale,a.y+ch*scale);bool selected=k.col==c&&(k.row==r||((c==0||c==11)&&((r==1&&k.row==2)||(r==3&&k.row==4))));draw->AddRectFilled(a,z,selected?IM_COL32(27,62,135,255):IM_COL32(c==0||c==11?31:19,c==0||c==11?31:19,c==0||c==11?31:19,255));draw->AddRect(a,z,selected?IM_COL32(78,112,178,255):IM_COL32(4,4,4,255));std::string caption=label,badge;auto split=caption.find_last_of(' ');if(split!=std::string::npos){auto token=caption.substr(split+1);if(token=="Y"||token=="B"||token=="X"||token=="LT"||token=="RT"||token=="LS"){badge=token;caption.resize(split);}}
 ImGui::PushFont(nullptr,(caption.size()==1?14.f:10.f)*scale);auto size=ImGui::CalcTextSize(caption.c_str());float extra=badge.empty()?0:25*scale;float tx=(a.x+z.x-size.x-extra)*.5f,ty=(a.y+z.y-size.y)*.5f;
 const bool cursor=caption=="Cursor left"||caption=="Cursor right",enter=caption=="Enter",caps=caption=="Caps";
 if(cursor||enter||caps){float cx=(a.x+z.x)*.5f,cy=a.y+22*scale;ty=a.y+48*scale;if(cursor){draw->AddRectFilled(ImVec2(cx-6*scale,cy-2*scale),ImVec2(cx+6*scale,cy+2*scale),IM_COL32_WHITE);draw->AddRectFilled(ImVec2(cx-2*scale,cy-6*scale),ImVec2(cx+2*scale,cy+6*scale),IM_COL32_WHITE);}else if(enter){draw->AddCircleFilled(ImVec2(cx,cy),7*scale,IM_COL32_WHITE);for(int i=-1;i<=1;i++)draw->AddLine(ImVec2(cx-4*scale,cy+i*3*scale),ImVec2(cx+4*scale,cy+i*3*scale),IM_COL32(80,80,80,255));}else {draw->AddTriangleFilled(ImVec2(cx-3*scale,cy-8*scale),ImVec2(cx+3*scale,cy-8*scale),ImVec2(cx,cy-4*scale),IM_COL32_WHITE);draw->AddRectFilled(ImVec2(cx-8*scale,cy-2*scale),ImVec2(cx+8*scale,cy+1*scale),IM_COL32_WHITE);ImGui::PushFont(nullptr,8.f*scale);auto rs=ImGui::CalcTextSize("R");draw->AddText(ImVec2(cx-rs.x*.5f,cy+2*scale),IM_COL32_WHITE,"R");ImGui::PopFont();}}
 draw->AddText(ImVec2(tx,ty),IM_COL32_WHITE,caption.c_str());
 if(!badge.empty()){ImGui::PushFont(nullptr,9.f*scale);ImVec2 center(tx+size.x+13*scale,(a.y+z.y)*.5f);ImU32 color=badge=="Y"?IM_COL32(227,162,9,255):badge=="B"?IM_COL32(207,56,12,255):IM_COL32(0,163,200,255);auto bs=ImGui::CalcTextSize(badge.c_str());if(badge.size()==1)draw->AddCircle(center,7*scale,color,20,scale);else{color=IM_COL32(30,30,30,255);draw->AddRectFilled(ImVec2(center.x-9*scale,center.y-7*scale),ImVec2(center.x+9*scale,center.y+7*scale),IM_COL32_WHITE,2*scale);}draw->AddText(ImVec2(center.x-bs.x*.5f,center.y-bs.y*.5f),color,badge.c_str());ImGui::PopFont();}
 ImGui::SetCursorScreenPos(a);ImGui::PushID(r*12+c);if(ImGui::InvisibleButton("key",ImVec2(cw*scale,ch*scale))){k.row=r;k.col=c;activate();}ImGui::PopID();ImGui::PopFont();};
 for(int r=0;r<4;r++)for(int c=1;c<=10;c++)cell(r,c,92+(c-1)*55.4f,r*36.f,55.4f,36,(k.accents?accents:k.symbols?symbols:k.shift?upper:lower)[r][c-1]);
 cell(0,0,0,0,92,36,"Symbols  LT");cell(0,11,646,0,94,36,"Shift  RT");cell(1,0,0,36,92,72,"Cursor left");cell(1,11,646,36,94,72,"Cursor right");cell(3,0,0,108,92,72,"Caps");cell(3,11,646,108,94,72,"Enter");
 cell(4,1,92,144,111,36,"Close  LS");cell(4,3,203,144,221,36,"Space  Y");cell(4,6,424,144,111,36,"Accents  B");cell(4,8,535,144,111,36,"Backspace  X");
 ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar(2);
 if(!k.active.load()&&k.guest_base){auto word=[&](uint32_t at,uint32_t v){for(unsigned i=0;i<4;i++)k.guest_base[at+i]=uint8_t(v>>(24-i*8));};if(k.accepted&&k.out_ptr){std::wstring text(k.text.size()+1,0);int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,k.text.data(),int(k.text.size()),text.data(),int(text.size()));n=std::min(n,int(k.max_chars));for(int i=0;i<=n;i++){uint16_t ch=i<n?uint16_t(text[i]):0;k.guest_base[k.out_ptr+i*2]=uint8_t(ch>>8);k.guest_base[k.out_ptr+i*2+1]=uint8_t(ch);}}if(k.ov_ptr){if(auto* kernel=rex::system::KernelState::shared())kernel->CompleteOverlappedEx(k.ov_ptr,0,k.accepted?0:1223,0);else{word(k.ov_ptr+4,0);word(k.ov_ptr+24,k.accepted?0:1223);word(k.ov_ptr,0);}}REXLOG_WARN("PES6 keyboard completed: accepted={}",k.accepted);k.guest_base=nullptr;k.finished.store(true,std::memory_order_release);}
}

// Register with the runtime UI drawer; Render detours do not create a drawable
// on Vulkan/D3D12 and cannot own a system dialog's lifecycle.
class Pes6XboxKeyboardDialog final : public rex::ui::ImGuiDialog {
 pes6::ui_tools::KeyboardState& state_;bool first_=true;
 public:
 Pes6XboxKeyboardDialog(rex::ui::ImGuiDrawer* drawer,pes6::ui_tools::KeyboardState& state):ImGuiDialog(drawer),state_(state){}
 void OnDraw(ImGuiIO&) override {
  if(first_){first_=false;REXLOG_WARN("PES6 keyboard first UI frame");}
  PaintXboxKeyboard(state_);
  if(state_.finished.load(std::memory_order_acquire))Close();
 }
};
inline bool ShowXboxKeyboard(pes6::ui_tools::KeyboardState& state){
 auto* kernel=rex::system::KernelState::shared();auto* runtime=kernel?kernel->emulator():nullptr;
 auto* drawer=runtime?runtime->imgui_drawer():nullptr;auto* app=runtime?runtime->app_context():nullptr;
 if(!drawer||!app)return false;
 auto* statePtr=&state;
 return app->CallInUIThreadSynchronous([drawer,kernel,runtime,statePtr]{
  new Pes6XboxKeyboardDialog(drawer,*statePtr);
  if(auto* window=runtime->display_window())window->SetTextInputActive(true);
  // This is an in-game bottom overlay. XN_SYS_UI makes PES hide the scene.
  REXLOG_WARN("PES6 keyboard registered with runtime drawer");
 });
}
