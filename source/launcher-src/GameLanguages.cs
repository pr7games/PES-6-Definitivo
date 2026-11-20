using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

public static class Fifa08Languages {
 public sealed class Language {
  public string Code, Tag, Name;
  public int UserLanguage;
  public int PesIndex;
  public override string ToString() { return Name; }
 }

 public sealed class Catalog {
  public List<Language> Text = new List<Language>(), Commentary = new List<Language>();
  public string DefaultText, DefaultCommentary;
 }

 private static readonly Language[] AllPes6Languages = new[] {
  new Language { Code = "es", Tag = "s", Name = "Espa\u00f1ol", UserLanguage = 5, PesIndex = 4 },
  new Language { Code = "en", Tag = "e", Name = "English", UserLanguage = 1, PesIndex = 0 },
  new Language { Code = "fr", Tag = "f", Name = "Fran\u00e7ais", UserLanguage = 4, PesIndex = 1 },
  new Language { Code = "de", Tag = "g", Name = "Deutsch", UserLanguage = 3, PesIndex = 2 },
  new Language { Code = "it", Tag = "i", Name = "Italiano", UserLanguage = 6, PesIndex = 3 },
  new Language { Code = "pt", Tag = "p", Name = "Portugu\u00eas", UserLanguage = 9, PesIndex = 9 },
  new Language { Code = "ja", Tag = "j", Name = "\u65e5\u672c\u8a9e", UserLanguage = 2, PesIndex = 5 },
  new Language { Code = "ko", Tag = "k", Name = "\ud55c\uad6d\uc5b4", UserLanguage = 7, PesIndex = 6 }
 };

 public static Language Describe(string tagOrCode) {
  if (string.IsNullOrEmpty(tagOrCode)) return AllPes6Languages[0];
  string key = tagOrCode.ToLowerInvariant();
  foreach (var lang in AllPes6Languages) {
   if (lang.Code == key || lang.Tag == key) {
    return new Language { Code = lang.Code, Tag = lang.Tag, Name = lang.Name, UserLanguage = lang.UserLanguage, PesIndex = lang.PesIndex };
   }
  }
  return new Language { Code = key, Tag = key, Name = tagOrCode, UserLanguage = 1, PesIndex = 0 };
 }

 public static Language Resolve(IEnumerable<Language> languages, string code) {
  if (string.IsNullOrEmpty(code)) return null;
  code = code.ToLowerInvariant();
  var all = languages.ToList();
  var exact = all.FirstOrDefault(l => l.Code == code || l.Tag == code);
  if (exact != null) return exact;
  if (code.StartsWith("es") || code.StartsWith("spa")) return all.FirstOrDefault(l => l.Code == "es");
  if (code.StartsWith("en") || code.StartsWith("eng")) return all.FirstOrDefault(l => l.Code == "en");
  if (code.StartsWith("fr") || code.StartsWith("fre") || code.StartsWith("fra")) return all.FirstOrDefault(l => l.Code == "fr");
  if (code.StartsWith("de") || code.StartsWith("ger") || code.StartsWith("deu")) return all.FirstOrDefault(l => l.Code == "de");
  if (code.StartsWith("it") || code.StartsWith("ita")) return all.FirstOrDefault(l => l.Code == "it");
  return null;
 }

 public static Catalog Detect(string root, string fallback = null) {
  var catalog = new Catalog();
  if (!string.IsNullOrEmpty(root) && Directory.Exists(root)) {
   foreach (var lang in AllPes6Languages) {
    string rvFile = Path.Combine(root, "rv_" + lang.Tag + ".img");
    if (File.Exists(rvFile) && new FileInfo(rvFile).Length > 0) {
     catalog.Text.Add(Describe(lang.Code));
    }
    string rsFile = Path.Combine(root, "rs_" + lang.Tag + ".img");
    if (File.Exists(rsFile) && new FileInfo(rsFile).Length > 0) {
     catalog.Commentary.Add(Describe(lang.Code));
    }
   }
  }
  if (catalog.Text.Count == 0) {
   foreach (var code in new[] { "es", "en", "fr", "de", "it" })
    catalog.Text.Add(Describe(code));
  }
  if (catalog.Commentary.Count == 0) {
   foreach (var code in new[] { "es", "en", "fr", "de", "it" })
    catalog.Commentary.Add(Describe(code));
  }
  catalog.DefaultText = (Resolve(catalog.Text, "es") ?? catalog.Text[0]).Code;
  catalog.DefaultCommentary = (Resolve(catalog.Commentary, "es") ?? catalog.Commentary[0]).Code;
  return catalog;
 }

 public static void AdoptExtractedLocale(string root) { }
 public static void Configure(string root, string text, string commentary, string fallback) { }
 public static int UserLanguage(string code) { return Describe(code).UserLanguage; }
}
