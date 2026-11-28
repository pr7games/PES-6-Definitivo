using System;using System.Drawing;using System.Windows.Forms;using System.Collections.Generic;using System.IO;using System.Linq;using System.Web.Script.Serialization;
namespace FIFA08Launcher {
 public partial class LauncherForm {
  Panel modernShell,modernBody,modernTabs;Panel[] modernPages;Button[] tabButtons;Control brand;ComboBox fsrQuality,anisotropy,gameplayProfile;CheckBox windowed,keyboard;Dictionary<string,TextBox> keyFields=new Dictionary<string,TextBox>();string extraFile;bool modernReady;AchievementForm embeddedAchievements;
  string UserConfigFile(string name){
   string identity;using(var sha=System.Security.Cryptography.SHA256.Create())identity=BitConverter.ToString(sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(Path.GetFullPath(InstallRoot()).ToUpperInvariant()))).Replace("-","").Substring(0,16);
   return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Golazo Virtual","PES6",identity,name);
  }
  string ResolveConfigFile(string name){
   string user=UserConfigFile(name);return File.Exists(user)?user:Path.Combine(InstallRoot(),"data","config",name);
  }
  void WriteConfig(ref string file,string contents){
   try{Directory.CreateDirectory(Path.GetDirectoryName(file));File.WriteAllText(file,contents);}
   catch(UnauthorizedAccessException){WriteUserConfig(ref file,contents);}
   catch(IOException){WriteUserConfig(ref file,contents);}
  }
  void WriteUserConfig(ref string file,string contents){
   string user=UserConfigFile(Path.GetFileName(file));Directory.CreateDirectory(Path.GetDirectoryName(user));File.WriteAllText(user,contents);file=user;
  }
  Timer choicesTimer;
  readonly List<Action> choiceRefreshes=new List<Action>();
  void RegisterChoiceRefresh(Control group,Action refresh){
   choiceRefreshes.Add(()=>{if(group.Visible)refresh();});
   if(choicesTimer!=null)return;
   choicesTimer=new Timer{Interval=300};
   choicesTimer.Tick+=(s,e)=>{foreach(var update in choiceRefreshes)update();};
   Action sync=()=>{if(!IsDisposed&&!Disposing)choicesTimer.Enabled=Visible&&WindowState!=FormWindowState.Minimized;};
   VisibleChanged+=(s,e)=>sync();Resize+=(s,e)=>sync();
   Disposed+=(s,e)=>choicesTimer.Dispose();sync();
  }
  readonly Color accent=PortUi.Accent;
  string T2(string es,string en){return PortUi.Language=="pt"?PortUi.PT(en,es):PortUi.English?en:es;}
  Label Caption(string text){return new Label{Text=text,AutoSize=false,Height=20,ForeColor=Color.FromArgb(180,180,187),Font=new Font("Segoe UI",9f),Margin=new Padding(0,20,0,9)};}
  FlowLayoutPanel Stack(Panel parent){var flow=new DarkFlowPanel{Dock=DockStyle.Fill,FlowDirection=FlowDirection.TopDown,WrapContents=false,AutoScroll=true,Padding=new Padding(8,8,8,16),BackColor=Color.FromArgb(28,29,33)};parent.Controls.Add(new DarkScrollHost(flow){Dock=DockStyle.Fill});return flow;}
  void SizeStack(FlowLayoutPanel flow){foreach(Control c in flow.Controls){if(c is FlowLayoutPanel){c.Width=Math.Max(1,Math.Min(flow.ClientSize.Width,flow.Parent==null?flow.ClientSize.Width:flow.Parent.ClientSize.Width-14)-flow.Padding.Horizontal-8);}else c.Width=Math.Max(1,Math.Min(flow.ClientSize.Width,flow.Parent==null?flow.ClientSize.Width:flow.Parent.ClientSize.Width-14)-flow.Padding.Horizontal-8);}}
  FlowLayoutPanel Choices(FlowLayoutPanel stack,string title,string[] labels,Action<int> select,Func<int> selected,Func<int,bool> allowed=null,Func<int,string> text=null){stack.Controls.Add(Caption(title));var group=new TileStrip{Height=38,WrapContents=false,BackColor=stack.BackColor,Margin=new Padding(0,0,0,10)};var buttons=new List<Button>();for(int i=0;i<labels.Length;i++){int choice=i;var button=new ChoiceTile{Text=labels[i],Width=75,Height=30,Margin=new Padding(0,1,5,1),Font=new Font("Segoe UI",8.25f,FontStyle.Bold)};button.Click+=(s,e)=>{select(choice);foreach(var b in buttons){int index=buttons.IndexOf(b);if(allowed!=null)b.Enabled=allowed(index);if(text!=null)b.Text=text(index);bool on=index==selected();b.BackColor=on?PortUi.Accent:Color.FromArgb(36,37,41);b.ForeColor=on?Color.White:Color.Silver;}};if(allowed!=null)button.Enabled=allowed(choice);if(text!=null)button.Text=text(choice);button.BackColor=choice==selected()?PortUi.Accent:Color.FromArgb(36,37,41);buttons.Add(button);group.Controls.Add(button);}group.Resize+=(s,e)=>{int width=Math.Max(28,(group.ClientSize.Width-labels.Length*5)/labels.Length);foreach(var b in buttons)b.Width=width;group.Height=34;};RegisterChoiceRefresh(group,()=>{foreach(var b in buttons){int index=buttons.IndexOf(b);if(allowed!=null)b.Enabled=allowed(index);if(text!=null)b.Text=text(index);bool on=index==selected();b.BackColor=on?PortUi.Accent:Color.FromArgb(36,37,41);b.ForeColor=on?Color.White:Color.Silver;}});stack.Controls.Add(group);return group;}
  void RefreshNeuralRenderingStatus(){
   try {
    string project=string.Equals(Path.GetFileName(rootDir.TrimEnd(Path.DirectorySeparatorChar)),"bin",StringComparison.OrdinalIgnoreCase)?Directory.GetParent(rootDir).FullName:rootDir;
    string status=Path.Combine(project,"data","neural-rendering","status.json");
    if(!File.Exists(status)||File.GetLastWriteTimeUtc(status)<gameLaunchTime)return;
    Dictionary<string,object> data;
    using(var stream=new FileStream(status,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
    using(var reader=new StreamReader(stream))data=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(reader.ReadToEnd());
    object state;if(!data.TryGetValue("state",out state))return;
    object heartbeat;long stamp;
    if(Convert.ToString(state)=="active"&&(!data.TryGetValue("updated_unix_ms",out heartbeat)||!long.TryParse(Convert.ToString(heartbeat),out stamp)||(DateTime.UtcNow-new DateTime(1970,1,1,0,0,0,DateTimeKind.Utc)).TotalMilliseconds-stamp>3000)){
     SetLauncherStatus(T2("PES 6 activo; Neural Rendering inactivo","PES 6 running; Neural Rendering inactive"));return;
    }
    if(Convert.ToString(state)=="active"){
     string text=T2("Neural Rendering activo","Neural Rendering active")+" - NR 5%";
     object value;double fps;if(data.TryGetValue("output_fps",out value)&&value!=null&&double.TryParse(Convert.ToString(value),out fps)&&fps>0)text+=" - "+fps.ToString("0")+" FPS";
     SetLauncherStatus(text);
    }else if(Convert.ToString(state)=="unavailable")SetLauncherStatus(T2("PES 6 activo; Neural Rendering no disponible (ver registro)","PES 6 running; Neural Rendering unavailable (see log)"));
    else if(Convert.ToString(state)=="paused")SetLauncherStatus(T2("Neural Rendering inactivo - F7 para activar","Neural Rendering inactive - F7 to enable"));
    else if(Convert.ToString(state)=="recovering")SetLauncherStatus(T2("PES 6 activo; reconectando Neural Rendering","PES 6 running; reconnecting Neural Rendering"));
    else if(Convert.ToString(state)=="starting")SetLauncherStatus(T2("PES 6 activo; iniciando Neural Rendering","PES 6 running; starting Neural Rendering"));
   }catch { }
  }
  void ModernInit(){
   Font=new Font("Segoe UI",9.5f);BackColor=Color.FromArgb(20,21,25);panelControls.BackColor=BackColor;bottomPanel.BackColor=Color.FromArgb(28,29,33);btnPlay.CornerRadius=12;btnPlay.Font=new Font("Segoe UI",11f,FontStyle.Regular);
   extraFile=ResolveConfigFile("launcher-controls.json");
   Size=new Size(1200,800);MinimumSize=new Size(800,580);var area=Screen.FromControl(this).WorkingArea;Size=new Size(Math.Min(Width,area.Width-30),Math.Min(Height,area.Height-30));
   foreach(var row in settingRows){row.Label.Visible=false;row.ComboBox.Visible=false;}
   lblGpuInfo.Visible=false;achievementsButton.Visible=false;optionsButton.Visible=false;panelControls.Paint-=PanelControls_Paint;
   updatePill=new UpdateNoticePill{Visible=false,Height=46,Font=new Font("Segoe UI",9f,FontStyle.Bold),AccessibleName="Actualizacion disponible"};panelCover.Controls.Add(updatePill);updatePill.Click+=(s,e)=>ShowUpdatePage();
   brand=new BrandHeader(Path.Combine(rootDir,"launcher-assets","logo.png"));panelControls.Controls.Add(brand);
   modernTabs=new Panel{BackColor=panelControls.BackColor};panelControls.Controls.Add(modernTabs);modernBody=new BufferedPanel{BackColor=Color.FromArgb(28,29,33)};panelControls.Controls.Add(modernBody);modernPages=new Panel[6];tabButtons=new Button[6];string[] titles={"Pantalla","Mejoras","Controles","Idiomas","Logros","Acerca de"};for(int i=0;i<modernPages.Length;i++){int page=i;modernPages[i]=new BufferedPanel{Dock=DockStyle.Fill,Visible=i==0};modernBody.Controls.Add(modernPages[i]);tabButtons[i]=new NavigationTab{Text=titles[i],Height=30,Font=new Font("Segoe UI",9),Selected=i==0,BackColor=modernTabs.BackColor};tabButtons[i].Click+=(s,e)=>{for(int j=0;j<modernPages.Length;j++){modernPages[j].Visible=j==page;((NavigationTab)tabButtons[j]).Selected=j==page;}modernPages[page].BringToFront();};modernTabs.Controls.Add(tabButtons[i]);}
   var graphics=Stack(modernPages[0]);
   Choices(graphics,"API",new[]{"Nativo","Vulkan","D3D12"},i=>cbGraphicsApi.SelectedIndex=new[]{2,0,1}[i],()=>cbGraphicsApi.SelectedIndex==2?0:cbGraphicsApi.SelectedIndex+1);
   Choices(graphics,"Resolución",new[]{"720p","1080p","2K","4K"},i=>{cbResolution.SelectedIndex=i;UpdateFsrChoices();},()=>Math.Min(3,cbResolution.SelectedIndex>=4?cbResolution.SelectedIndex-3:cbResolution.SelectedIndex));
   fsrQuality=new RoundedComboBox{DropDownStyle=ComboBoxStyle.DropDownList};UpdateFsrChoices();Choices(graphics,"FSR",new[]{"Nativo","Rendimiento","Equilibrado","Calidad"},i=>fsrQuality.SelectedIndex=i,()=>fsrQuality.SelectedIndex,null,i=>new[]{T2("Nativo","Native"),T2("Rendimiento","Performance"),T2("Equilibrado","Balanced"),T2("Calidad","Quality")}[i]);

   windowed=new RedCheckBox{Text="Modo ventana",Checked=cbDisplayMode.SelectedIndex==0,ForeColor=Color.White,Height=22};windowed.CheckedChanged+=(s,e)=>cbDisplayMode.SelectedIndex=windowed.Checked?0:1;graphics.Controls.Add(windowed);chkShowFps.Visible=true;graphics.Controls.Add(chkShowFps);chkVSync.Visible=true;graphics.Controls.Add(chkVSync);foreach(var check in new CheckBox[]{windowed,chkShowFps,chkVSync}){check.Height=22;check.Margin=new Padding(0,5,0,7);}
   var effects=Stack(modernPages[1]);
   effects.Controls.Add(Caption("Gameplay"));gameplayProfile=new RoundedComboBox{DropDownStyle=ComboBoxStyle.DropDownList};gameplayProfile.Items.AddRange(new object[]{"Por defecto","Precisi\u00f3n"});gameplayProfile.SelectedIndex=1;gameplayProfile.Margin=new Padding(0,0,0,22);effects.Controls.Add(gameplayProfile);
   // Neural Rendering is reserved for a future release; implementation retained.
   foreach(var check in new RedCheckBox[]{chkImprovedRain,chkImprovedParticles,chkRealisticKits,chkPlayerDirt,chkSoftCrowd}){check.Height=26;check.Margin=new Padding(0,6,0,8);check.ForeColor=Color.White;effects.Controls.Add(check);}
   graphics.Controls.Add(Caption("Filtro de imagen"));cbAntiAliasing.Visible=true;graphics.Controls.Add(cbAntiAliasing);var fsrHint=new Label{Height=20,ForeColor=Color.Gray,Font=new Font("Segoe UI",7.8f),Text=T2("FSR activo: FXAA se omite para conservar el detalle.","FSR active: FXAA is bypassed to preserve detail."),Margin=new Padding(0,0,0,1)};graphics.Controls.Add(fsrHint);Action refreshFsrFilter=()=>{bool active=fsrQuality.Enabled&&fsrQuality.SelectedIndex>0;cbAntiAliasing.Enabled=!active;fsrHint.Visible=active;fsrHint.Text=T2("FSR activo: FXAA se omite para conservar el detalle.","FSR active: FXAA is bypassed to preserve detail.");};fsrQuality.SelectedIndexChanged+=(s,e)=>refreshFsrFilter();cbResolution.SelectedIndexChanged+=(s,e)=>refreshFsrFilter();refreshFsrFilter();
   graphics.Controls.Add(Caption("Filtrado de texturas"));anisotropy=new RoundedComboBox{DropDownStyle=ComboBoxStyle.DropDownList};anisotropy.Items.AddRange(new object[]{"Original","Off","2×","4×","8×","16×"});anisotropy.SelectedIndex=0;graphics.Controls.Add(anisotropy);

   var controls=Stack(modernPages[2]);keyboard=new RedCheckBox{Text="Activar teclado / ratón",ForeColor=Color.White,Height=26};controls.Controls.Add(keyboard);string[] keys={"lstick_up","lstick_down","lstick_left","lstick_right","a","b","x","y","left_trigger","right_trigger","left_shoulder","right_shoulder","start","back","rstick_up","rstick_down","rstick_left","rstick_right","dpad_up","dpad_down","dpad_left","dpad_right","lstick_press","rstick_press"};string[] defaults={"W","S","A","D","Space","Backspace","L","P","Q","E","1","3","Return","Tab","Up","Down","Left","Right","Shift+Up","Shift+Down","Shift+Left","Shift+Right","F","K"};controls.Controls.Add(Caption("Selecciona un campo y pulsa la tecla deseada"));var defaultsButton=new ChoiceTile{Text="Restaurar teclas",Height=32};defaultsButton.Click+=(s,e)=>{for(int n=0;n<keys.Length;n++)keyFields[keys[n]].Text=defaults[n];};controls.Controls.Add(defaultsButton);var saveKeys=new ChoiceTile{Text="Guardar controles",Height=32};saveKeys.Click+=(s,e)=>{SaveSettings();};controls.Controls.Add(saveKeys);for(int i=0;i<keys.Length;i++){controls.Controls.Add(Caption(ControlLabel(keys[i])));var input=new TextBox{Text=defaults[i],ReadOnly=true,BorderStyle=BorderStyle.None,Font=new Font("Segoe UI",10.5f),BackColor=Color.FromArgb(30,30,35),ForeColor=Color.White,Height=26};input.PreviewKeyDown+=(s,e)=>{e.IsInputKey=true;};input.KeyDown+=(s,e)=>{e.SuppressKeyPress=true;if(e.KeyCode==Keys.ControlKey||e.KeyCode==Keys.ShiftKey||e.KeyCode==Keys.Menu)return;((TextBox)s).Text=(e.Control?"Ctrl+":"")+(e.Shift?"Shift+":"")+(e.Alt?"Alt+":"")+(e.KeyCode==Keys.Enter?"Return":e.KeyCode>=Keys.D0&&e.KeyCode<=Keys.D9?((int)e.KeyCode-(int)Keys.D0).ToString():e.KeyCode.ToString());};keyFields.Add(keys[i],input);controls.Controls.Add(input);}

   var languages=Stack(modernPages[3]);languages.Controls.Add(Caption("Idioma de textos"));cbTextLanguage.Visible=true;languages.Controls.Add(cbTextLanguage);languages.Controls.Add(Caption("Comentarios"));cbCommentaryLanguage.Visible=true;languages.Controls.Add(cbCommentaryLanguage);
   Action refreshNative=()=>{
    chkShowFps.Enabled=true;
    chkRealisticKits.Text=T2("Camisetas realistas","Realistic kits");
    chkShowFps.Text=T2("Mostrar FPS","Show FPS");
    fsrHint.Visible=fsrQuality.SelectedIndex>0;
    cbAntiAliasing.Enabled=fsrQuality.SelectedIndex==0;
    cbCommentaryLanguage.Enabled=true;
    if(runningGameProcess==null)SetLauncherStatus(T2("F10 durante el partido para volver a Windows","F10 during the match to return to Windows"));
   };
   cbGraphicsApi.SelectedIndexChanged+=(s,e)=>refreshNative();
   fsrQuality.SelectedIndexChanged+=(s,e)=>refreshNative();
   cbResolution.SelectedIndexChanged+=(s,e)=>refreshNative();
   RegisterChoiceRefresh(graphics,refreshNative);refreshNative();

   var achievementsStack=Stack(modernPages[4]);
   var achHeader=new Label{Text=T2("Pro Evolution Soccer 6 — Logros y Estadísticas","Pro Evolution Soccer 6 — Achievements & Stats"),Font=new Font("Segoe UI",11,FontStyle.Bold),ForeColor=Color.White,Height=32,AutoSize=false,Margin=new Padding(0,8,0,4)};achievementsStack.Controls.Add(achHeader);
   var achCard=new Panel{Height=100,BackColor=Color.FromArgb(36,37,41),Margin=new Padding(0,4,0,8)};
   var achInfo=new Label{Text=T2("El progreso de Liga Máster, copas, ligas y datos editados se almacena localmente en la carpeta de usuario.\n\nLos logros de Xbox Live y estadísticas avanzadas estarán disponibles en la próxima actualización del port.",
                                  "Master League, cups, leagues, and edit data progress are saved locally in the user folder.\n\nXbox Live achievements and advanced stats will be available in the upcoming port update."),
                         Font=new Font("Segoe UI",9),ForeColor=Color.Silver,Dock=DockStyle.Fill,Padding=new Padding(12),TextAlign=ContentAlignment.MiddleLeft};
   achCard.Controls.Add(achInfo);achievementsStack.Controls.Add(achCard);

   lblStatus.Font=new Font("Segoe UI",7.5f);btnPlay.Font=new Font("Segoe UI",16,FontStyle.Bold);
   InitializeAbout();
   foreach(var flow in new[]{graphics,effects,controls,languages,achievementsStack})flow.Resize+=(s,e)=>SizeStack((FlowLayoutPanel)s);
   modernShell=new PremiumPanel();panelControls.Controls.Add(modernShell);modernShell.Controls.Add(modernTabs);modernShell.Controls.Add(modernBody);modernShell.Controls.Add(bottomPanel);LoadModernSettings();modernReady=true;if(!File.Exists(extraFile))SaveModernSettings();SetUiLanguage(PortUi.Language,false);ModernLayout();
  }
  string ControlLabel(string key){var es=new Dictionary<string,string>{{"lstick_up","Stick izquierdo - arriba"},{"lstick_down","Stick izquierdo - abajo"},{"lstick_left","Stick izquierdo - izquierda"},{"lstick_right","Stick izquierdo - derecha"},{"rstick_up","Stick derecho - arriba"},{"rstick_down","Stick derecho - abajo"},{"rstick_left","Stick derecho - izquierda"},{"rstick_right","Stick derecho - derecha"},{"left_trigger","Gatillo izquierdo (LT / L2)"},{"right_trigger","Gatillo derecho (RT / R2)"},{"left_shoulder","Boton superior izquierdo (LB / L1)"},{"right_shoulder","Boton superior derecho (RB / R1)"},{"lstick_press","Pulsar stick izquierdo (L3)"},{"rstick_press","Pulsar stick derecho (R3)"},{"dpad_up","Cruceta - arriba"},{"dpad_down","Cruceta - abajo"},{"dpad_left","Cruceta - izquierda"},{"dpad_right","Cruceta - derecha"},{"a","A / Cruz"},{"b","B / Circulo"},{"x","X / Cuadrado"},{"y","Y / Triangulo"},{"start","Start / Options"},{"back","Back / Share"}};return es.ContainsKey(key)?es[key]:key;}
  void UpdateFsrChoices(){if(fsrQuality==null)return;int old=fsrQuality.SelectedIndex;fsrQuality.Items.Clear();fsrQuality.Items.AddRange(new object[]{"Nativo","Rendimiento","Equilibrado","Calidad"});fsrQuality.Enabled=true;fsrQuality.SelectedIndex=Math.Max(0,Math.Min(3,old));}
  void ModernLayout(){
   if(!modernReady)return;
   int cover=Math.Max(260,(int)(ClientSize.Width*.43));panelCover.SetBounds(0,0,cover,ClientSize.Height);if(updatePill!=null){updatePill.Visible=updateAvailable;updatePill.SetBounds(24,panelCover.ClientSize.Height-70,Math.Max(1,cover-48),46);updatePill.BringToFront();}panelControls.SetBounds(cover,0,ClientSize.Width-cover,ClientSize.Height);
   int w=panelControls.Width,cw=Math.Min(880,w-Math.Max(32,(int)(w*.12))),cx=(w-cw)/2;
   brand.SetBounds(cx,22,cw,86);
   modernShell.SetBounds(cx,122,cw,Math.Max(270,ClientSize.Height-152));
   int ch=modernShell.ClientSize.Height;
   modernTabs.BackColor=Color.FromArgb(36,37,41);
   int tabColumns=cw-24<500?3:tabButtons.Length,tabRows=(tabButtons.Length+tabColumns-1)/tabColumns;
   modernTabs.SetBounds(18,18,cw-36,40*tabRows);
   for(int i=0;i<tabButtons.Length;i++){int column=i%tabColumns,left=column*modernTabs.Width/tabColumns,right=(column+1)*modernTabs.Width/tabColumns;tabButtons[i].SetBounds(left,(i/tabColumns)*40,right-left,40);}
   modernBody.SetBounds(18,26+40*tabRows,cw-36,Math.Max(120,ch-126-40*tabRows));
   bottomPanel.SetBounds(18,ch-88,cw-36,76);
   lblStatus.SetBounds(4,0,bottomPanel.Width-8,28);
   int playWidth=Math.Min(280,cw-64);btnPlay.SetBounds((bottomPanel.Width-playWidth)/2,28,playWidth,44);
   if(uiLanguage!=null){uiLanguage.SetBounds(w-LanguageSelectorWidth-20,0,LanguageSelectorWidth,24);uiLanguage.BringToFront();}
   foreach(var page in modernPages)foreach(var flow in page.Controls.OfType<FlowLayoutPanel>())if(flow.FlowDirection==FlowDirection.TopDown){SizeStack(flow);flow.PerformLayout();}
   panelCover.Invalidate();panelControls.Invalidate();
  }
  void LoadModernSettings(){try{if(!File.Exists(extraFile))return;var d=new JavaScriptSerializer().Deserialize<Dictionary<string,object>>(File.ReadAllText(extraFile));object v;if(d.TryGetValue("GameplayProfile",out v)){int profile=Array.IndexOf(new[]{"original","improved"},Convert.ToString(v));gameplayProfile.SelectedIndex=profile<0?1:profile;if(Convert.ToString(v)=="realistic"){d["GameplayProfile"]="improved";WriteConfig(ref extraFile,new JavaScriptSerializer().Serialize(d));}}if(d.TryGetValue("Keyboard",out v))keyboard.Checked=Convert.ToBoolean(v);if(d.TryGetValue("Anisotropy",out v))anisotropy.SelectedIndex=Math.Max(0,Math.Min(5,Convert.ToInt32(v)));if(d.TryGetValue("Fsr",out v)){int n=Convert.ToInt32(v);if(n<fsrQuality.Items.Count)fsrQuality.SelectedIndex=n;}if(d.TryGetValue("FsrProfile",out v)){string profile=Convert.ToString(v);int mode=Array.IndexOf(new[]{"native","performance","balanced","quality"},profile);if(fsrQuality.Items.Count==3&&mode>=2)mode=2;fsrQuality.SelectedIndex=Math.Max(0,Math.Min(fsrQuality.Items.Count-1,mode));}foreach(var key in keyFields.Keys)if(d.TryGetValue(key,out v))keyFields[key].Text=Convert.ToString(v);}catch{}}
  void SaveModernSettings(){if(!modernReady)return;var d=new Dictionary<string,object>{{"GameplayProfile",new[]{"original","improved"}[gameplayProfile.SelectedIndex]},{"Keyboard",keyboard.Checked},{"Anisotropy",anisotropy.SelectedIndex},{"Fsr",fsrQuality.SelectedIndex},{"FsrProfile",SelectedFsrProfile()}};foreach(var p in keyFields)d[p.Key]=p.Value.Text;WriteConfig(ref extraFile,new JavaScriptSerializer().Serialize(d));}
  string SelectedFsrProfile(){return fsrQuality.SelectedIndex<=0?"native":fsrQuality.SelectedIndex==1?"performance":fsrQuality.Items.Count==4&&fsrQuality.SelectedIndex==2?"balanced":"quality";}
  string ModernArgs(){if(!modernReady)return "";string profile=fsrQuality.SelectedIndex<=0?"native":fsrQuality.SelectedIndex==1?"performance":fsrQuality.Items.Count==4&&fsrQuality.SelectedIndex==2?"balanced":"quality";return " -ControlsFile '"+extraFile.Replace("'","''")+"' -GameplayProfile "+new[]{"original","improved"}[gameplayProfile.SelectedIndex]+" -FsrProfile "+profile+" -Anisotropy "+new[]{-1,0,2,3,4,5}[anisotropy.SelectedIndex]+" -KeyboardEnabled:"+(keyboard.Checked?"$true":"$false");}
  Panel aboutHeader;Label aboutUpdateTitle;Button updatePill;bool updateAvailable;string availableVersion;Label aboutDetails,updateNotice;Button updateVersion,githubProject,youtubeChannel;FlowLayoutPanel aboutStack;
  void InitializeAbout(){
   aboutStack=Stack(modernPages[5]);
   aboutHeader=new BufferedPanel{Padding=new Padding(14),Margin=new Padding(0,8,0,16)};aboutStack.Controls.Add(aboutHeader);
   aboutUpdateTitle=new Label{Dock=DockStyle.Top,Height=58,Font=new Font("Segoe UI",11,FontStyle.Bold)};aboutDetails=new Label{Dock=DockStyle.Fill,ForeColor=Color.Silver,Font=new Font("Segoe UI",9)};aboutHeader.Controls.Add(aboutDetails);aboutHeader.Controls.Add(aboutUpdateTitle);aboutHeader.Resize+=(s,e)=>SizeAboutHeader();
   githubProject=new ChoiceTile{Height=30,Enabled=false};aboutStack.Controls.Add(githubProject);
   youtubeChannel=new ChoiceTile{Height=30};youtubeChannel.Click+=(s,e)=>System.Diagnostics.Process.Start("https://www.youtube.com/@GolazoVirtual7");aboutStack.Controls.Add(youtubeChannel);
   var licenses=new ChoiceTile{Text="Licencias / Licenses",Height=30};licenses.Click+=(s,e)=>{string path=Path.Combine(Directory.GetParent(rootDir).FullName,"docs","PES6-NOTICE.txt");if(File.Exists(path))System.Diagnostics.Process.Start(path);};aboutStack.Controls.Add(licenses);
   updateNotice=new Label{Height=38,ForeColor=Color.Gray,Font=new Font("Segoe UI",8.5f),Margin=new Padding(0,12,0,3)};aboutStack.Controls.Add(updateNotice);
   var row=new Panel{Height=46,Margin=new Padding(0,4,0,12)};updateVersion=new ChoiceTile{Dock=DockStyle.Fill,Enabled=false,Size=new Size(150,38),Font=new Font("Segoe UI",9)};row.Controls.Add(updateVersion);aboutStack.Controls.Add(row);
   aboutStack.Controls.SetChildIndex(row,0);aboutStack.Controls.SetChildIndex(updateNotice,1);
   InitializeUpdater();aboutStack.Resize+=(s,e)=>SizeStack(aboutStack);RefreshAbout();
  }
  void SetUpdateAvailable(string version){availableVersion=version;updateAvailable=!string.IsNullOrEmpty(version)||previewUpdate;RefreshAbout();}
  void RefreshAbout(){if(aboutDetails==null)return;tabButtons[5].Text=T2("Acerca de","About");aboutDetails.Text=(PortUi.Language=="pt"?"PES 6 DEFINITIVO\nVersão {VERSION}\nDesenvolvido por Golazo Virtual\n\nProjeto independente, sem vínculo com Konami, Microsoft, Sony ou AMD. Requer uma cópia original compatível de Xbox 360. O instalador não inclui o jogo.\n\nUsa componentes de código aberto como ReXGlue, Xenia e FidelityFX FSR. Cada componente mantém sua licença e seus avisos.\n\nPES, Konami, marcas, imagens, músicas e recursos do jogo pertencem aos seus titulares. O software é oferecido sem garantias. Consulte os avisos e as licenças incluídos.":T2(
   @"PES 6 DEFINITIVO
Version {VERSION}
Desarrollado por Golazo Virtual

Proyecto independiente, sin afiliacion con Konami, Microsoft, Sony ni AMD. Requiere una copia original compatible de Xbox 360; el instalador no incluye el juego.

Utiliza componentes de codigo abierto: ReXGlue, Xenia y FidelityFX FSR, entre otros. Cada componente conserva su licencia y sus avisos.

PES, Konami, las marcas, imagenes, musica y recursos del juego pertenecen a sus titulares. El software se ofrece sin garantias; consulta los avisos y licencias incluidos.",
   @"PES 6 DEFINITIVO
Version {VERSION}
Developed by Golazo Virtual

Independent project, not affiliated with Konami, Microsoft, Sony or AMD. Requires a compatible original Xbox 360 copy; game files are not included in the installer.

Uses open-source components including ReXGlue, Xenia and FidelityFX FSR. Each component retains its own license and notices.

PES, Konami, trademarks, artwork, music and game assets belong to their respective owners. Software is provided without warranties; see the included notices and licenses.")).Replace("{VERSION}",PortUpdater.CurrentVersion);
   youtubeChannel.Text="YouTube - GolazoVirtual7";
   if(updateAvailable)tabButtons[5].Text=T2("Acerca de","About");
   RefreshUpdater();RefreshUpdateAccent();
  } }
}
