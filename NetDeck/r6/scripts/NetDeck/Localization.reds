// Native Codeware localization for Infiltration.
// This is a real, loaded REDscript localization provider, not an uncooked
// WolvenKit/ArchiveXL source placeholder. The provider supplies en-us as
// fallback to all supported UI languages until translations are available.
module NetDeck

import Codeware.Localization.*

public class NetDeckEnglish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NΣT");
    this.Text("NetDeckDeckDescription",
      "NetWatch Netdriver Mk.2. Iconic network-infiltration cyberdeck with a built-in Infiltration quickhack.");
    this.Text("NetDeckInfiltrationName", "Infiltration");
    this.Text("NetDeckInfiltrationDescription",
      "Upload to any Ping-capable target. Disable connected cameras, turrets and mines, or distract compatible electronics. Unlinked targets affect only their own supported functions.");
  }
}

public class NetDeckLocalizationProvider extends ModLocalizationProvider {
  public func GetPackage(language: CName) -> ref<ModLocalizationPackage> {
    if Equals(language, n"en-us") {
      return new NetDeckEnglish();
    }
    return null;
  }

  public func GetFallback() -> CName {
    return n"en-us";
  }
}
