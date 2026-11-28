using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows.Forms;
namespace FIFA08Launcher {
 public partial class LauncherForm {
  const int LanguageSelectorWidth=126;
  Panel uiLanguage;
  readonly Dictionary<string,Button> languageButtons=new Dictionary<string,Button>();readonly Dictionary<Control,string> uiSources=new Dictionary<Control,string>();readonly Dictionary<ComboBox,string[]> choiceSources=new Dictionary<ComboBox,string[]>();string statusSource;
  void InitializeLanguage(){
   uiLanguage=new Panel{Width=LanguageSelectorWidth,Height=24};
   AddLanguageButton("es","ES","Espa\u00f1ol",0);
   AddLanguageButton("en","ENG","English",42);
   AddLanguageButton("pt","PT","Portugu\u00eas",84);
   panelControls.Controls.Add(uiLanguage);
   SetUiLanguage(PortUi.Language,false);
   UpdateResponsiveLayout();
  }
  void AddLanguageButton(string code,string label,string name,int left){
   var button=PortUi.Toggle(()=>SetUiLanguage(code,true));
   button.Text=label;
   button.AccessibleName=name;
   button.SetBounds(left,0,40,24);
   languageButtons.Add(code,button);
   uiLanguage.Controls.Add(button);
  }
  void RefreshLanguageButtons(){
   foreach(var entry in languageButtons){
    bool active=entry.Key==PortUi.Language;
    entry.Value.BackColor=active?PortUi.Accent:System.Drawing.Color.FromArgb(22,22,25);
    entry.Value.ForeColor=active?System.Drawing.Color.Black:System.Drawing.Color.Gainsboro;
   }
  }
  public void SetUiEnglish(bool english,bool persist){SetUiLanguage(english?"en":"es",persist);}
  void SetUiLanguage(string language,bool persist){PortUi.Language=language;bool english=PortUi.English;PortUi.Apply(this,uiSources,btnPlay,lblStatus,uiLanguage);foreach(var box in new[]{cbDisplayMode,cbAntiAliasing,cbResolution,gameplayProfile}){if(box==null)continue;string[] source;if(!choiceSources.TryGetValue(box,out source)){source=box.Items.Cast<object>().Select(Convert.ToString).ToArray();choiceSources[box]=source;}int index=box.SelectedIndex;box.BeginUpdate();box.Items.Clear();box.Items.AddRange(source.Select(PortUi.T).Cast<object>().ToArray());box.SelectedIndex=index;box.EndUpdate();}var speech=cbCommentaryLanguage.SelectedItem as Fifa08Languages.Language;if(speech!=null&&speech.Code=="original"){speech.Name=PortUi.T("No se detectaron comentarios");cbCommentaryLanguage.Refresh();}btnPlay.Text=PortUi.T(runningGameProcess==null?"JUGAR":"EN JUEGO");if(statusSource==null)statusSource=lblStatus.Text;lblStatus.Text=PortUi.T(statusSource);RefreshLanguageButtons();RefreshFeatureLanguage();if(modernReady&&embeddedAchievements!=null)embeddedAchievements.SetPresentationLanguage(english);if(modernReady)RefreshAbout();if(persist)PortUi.Save();}
  void SetLauncherStatus(string text){statusSource=text;lblStatus.Text=PortUi.T(text);}
  string GameDataRoot(){
   string direct=Path.Combine(rootDir,"game");
   if(File.Exists(Path.Combine(direct,"default.xex")))return Path.GetFullPath(direct);
   string clean=Path.Combine(rootDir,"..","game");
   if(File.Exists(Path.Combine(clean,"default.xex")))return Path.GetFullPath(clean);
   string installed=Path.Combine(rootDir,"GameData");
   return File.Exists(Path.Combine(installed,"default.xex"))?installed:direct;
  }
  void PopulateLanguages(){
   var catalog = Fifa08Languages.Detect(GameDataRoot());
   cbTextLanguage.Items.Clear();
   foreach (var lang in catalog.Text) cbTextLanguage.Items.Add(lang);
   SelectLanguage(cbTextLanguage, catalog.DefaultText);
   cbCommentaryLanguage.Items.Clear();
   foreach (var lang in catalog.Commentary) cbCommentaryLanguage.Items.Add(lang);
   SelectLanguage(cbCommentaryLanguage, catalog.DefaultCommentary);
  }
  static void SelectLanguage(ComboBox box,string code){var selected=Fifa08Languages.Resolve(box.Items.Cast<Fifa08Languages.Language>(),code);box.SelectedItem=selected??box.Items.Cast<object>().FirstOrDefault();}
  static string LanguageCode(ComboBox box){var selected=box.SelectedItem as Fifa08Languages.Language;if(selected==null)throw new InvalidOperationException(PortUi.T("No se detectaron idiomas de juego"));return selected.Code;}
 }
}
