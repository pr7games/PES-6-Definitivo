using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Windows.Forms;
namespace FIFA08Launcher {
 public partial class LauncherForm {
  Button uiLanguage;readonly Dictionary<Control,string> uiSources=new Dictionary<Control,string>();readonly Dictionary<ComboBox,string[]> choiceSources=new Dictionary<ComboBox,string[]>();string statusSource;
  void InitializeLanguage(){uiLanguage=PortUi.Toggle(()=>SetUiEnglish(!PortUi.English,true));panelControls.Controls.Add(uiLanguage);SetUiEnglish(PortUi.English,false);UpdateResponsiveLayout();}
  public void SetUiEnglish(bool english,bool persist){PortUi.English=english;PortUi.Apply(this,uiSources,btnPlay,lblStatus,uiLanguage);foreach(var box in new[]{cbDisplayMode,cbAntiAliasing,cbResolution}){string[] source;if(!choiceSources.TryGetValue(box,out source)){source=box.Items.Cast<object>().Select(Convert.ToString).ToArray();choiceSources[box]=source;}int index=box.SelectedIndex;box.BeginUpdate();box.Items.Clear();box.Items.AddRange(source.Select(PortUi.T).Cast<object>().ToArray());box.SelectedIndex=index;box.EndUpdate();}var speech=cbCommentaryLanguage.SelectedItem as Fifa08Languages.Language;if(speech!=null&&speech.Code=="original"){speech.Name=PortUi.T("No se detectaron comentarios");cbCommentaryLanguage.Refresh();}btnPlay.Text=PortUi.T(runningGameProcess==null?"JUGAR":"EN JUEGO");if(statusSource==null)statusSource=lblStatus.Text;lblStatus.Text=PortUi.T(statusSource);uiLanguage.Text=english?"EN | ES":"ES | EN";RefreshFeatureLanguage();if(modernReady&&embeddedAchievements!=null)embeddedAchievements.SetPresentationLanguage(english);if(modernReady)RefreshAbout();if(persist)PortUi.Save();}
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
