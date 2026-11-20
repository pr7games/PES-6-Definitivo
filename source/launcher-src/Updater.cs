namespace FIFA08Launcher {
 internal static class PortUpdater {
  public const string CurrentVersion = "1.0.0";
 }
 public partial class LauncherForm {
  // PES 6 has no configured release feed. Never query or install FIFA packages.
  void InitializeUpdater() {
   githubProject.Visible = false;
   updateVersion.Visible = false;
  }
  void RefreshUpdater() {
   githubProject.Visible = false;
   updateVersion.Visible = false;
   updateNotice.Text = T2("Actualizaciones automaticas no disponibles para este port.",
                         "Automatic updates are not available for this port.");
  }
 }
}