// Infiltration / NetWatch Σ MK.1 — UI-language localization.
// Codeware provider uses the player's OnScreen language, independent of dubbing.
// Locale IDs match the game's localization names (including jp-jp, kr-kr, cz-cz, ua-ua).
// English is fallback for unrecognized UI locales.
module NetDeck

import Codeware.Localization.*

public class NetDeckEnglish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Iconic cyberdeck with a built-in NETWORM quickhack that propagates through connected systems.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Upload to any Ping-capable target. Disable connected cameras, turrets and mines, or distract compatible electronics. Unlinked targets affect only their own supported functions.");
    this.Text("NetDeckSpreadComplete", "NETWORM: NETWORK INFILTRATED");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO COMPATIBLE DEVICES");
  }
}

public class NetDeckArabic extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. سطح إلكتروني أيقوني مزود باختراق NETWORM المدمج الذي ينتشر عبر الأنظمة المتصلة.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "ارفع الاختراق إلى أي هدف يدعم Ping. عطّل الكاميرات والأبراج والألغام المتصلة أو شتّت الأجهزة الإلكترونية المتوافقة. الأهداف غير المتصلة تؤثر في وظائفها المدعومة فقط.");
    this.Text("NetDeckSpreadComplete", "NETWORM: تم اختراق الشبكة");
    this.Text("NetDeckSpreadEmpty", "NETWORM: لا توجد أجهزة متوافقة");
  }
}

public class NetDeckChineseSimplified extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。标志性网络接入仓，内置 NETWORM 快速破解，可沿连接的系统传播。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "可上传至任何支持 Ping 的目标。关闭联网的摄像头、炮塔和地雷，或干扰兼容的电子设备。未联网的目标只会受到其自身支持的效果。");
    this.Text("NetDeckSpreadComplete", "NETWORM：网络入侵成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：没有兼容设备");
  }
}

public class NetDeckChineseTraditional extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。經典網路接入倉，內建 NETWORM 快速破解，可沿著相連系統擴散。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "可上傳至任何支援 Ping 的目標。關閉相連的攝影機、砲塔和地雷，或干擾相容的電子裝置。未連線的目標僅會觸發自身支援的效果。");
    this.Text("NetDeckSpreadComplete", "NETWORM：網路入侵成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：沒有相容裝置");
  }
}

public class NetDeckCzech extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonický kyberdeck s integrovaným rychlým hackem NETWORM, který se šíří propojenými systémy.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Nahraj do libovolného cíle podporujícího Ping. Vyřaď propojené kamery, věže a miny nebo rozptyl kompatibilní elektroniku. Nepropojené cíle ovlivní jen své podporované funkce.");
    this.Text("NetDeckSpreadComplete", "NETWORM: SÍŤ INFILTROVÁNA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: ŽÁDNÁ KOMPATIBILNÍ ZAŘÍZENÍ");
  }
}

public class NetDeckFrench extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck iconique intégrant NETWORM, un piratage rapide qui se propage dans les systèmes connectés.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Téléchargez-le sur toute cible compatible avec Ping. Neutralisez caméras, tourelles et mines connectées, ou distrayez les appareils compatibles. Les cibles isolées ne subissent que leurs effets pris en charge.");
    this.Text("NetDeckSpreadComplete", "NETWORM : RÉSEAU INFILTRÉ");
    this.Text("NetDeckSpreadEmpty", "NETWORM : AUCUN APPAREIL COMPATIBLE");
  }
}

public class NetDeckGerman extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonisches Cyberdeck mit integriertem NETWORM-Quickhack, der sich über verbundene Systeme ausbreitet.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Auf jedes Ping-fähige Ziel hochladen. Verbundene Kameras, Geschütze und Minen abschalten oder kompatible Elektronik ablenken. Isolierte Ziele sind nur von ihren unterstützten Funktionen betroffen.");
    this.Text("NetDeckSpreadComplete", "NETWORM: NETZWERK INFILTRIERT");
    this.Text("NetDeckSpreadEmpty", "NETWORM: KEINE KOMPATIBLEN GERÄTE");
  }
}

public class NetDeckHungarian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonikus kiberdeck, beépített NETWORM gyorshackkel, amely a csatlakoztatott rendszereken terjed.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Feltölthető bármely Ping-kompatibilis célpontra. Kikapcsolja a csatlakoztatott kamerákat, tornyokat és aknákat, vagy megzavarja a kompatibilis elektronikát. Az elszigetelt célpontokra csak a támogatott hatások érvényesek.");
    this.Text("NetDeckSpreadComplete", "NETWORM: HÁLÓZAT FELTÖRVE");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NINCS KOMPATIBILIS ESZKÖZ");
  }
}

public class NetDeckItalian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck iconico con NETWORM integrato, un hack rapido che si propaga nei sistemi collegati.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Caricalo su qualsiasi bersaglio compatibile con Ping. Disattiva telecamere, torrette e mine collegate o distrae i dispositivi compatibili. I bersagli isolati subiscono solo gli effetti supportati.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RETE INFILTRATA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NESSUN DISPOSITIVO COMPATIBILE");
  }
}

public class NetDeckJapanese extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。接続されたシステムに拡散するクイックハック「NETWORM」を内蔵したアイコニック・サイバーデッキ。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Ping可能な対象にアップロード。接続されたカメラ、タレット、地雷を停止し、対応する電子機器をかく乱する。孤立した対象には、その機器が対応する効果のみが適用される。");
    this.Text("NetDeckSpreadComplete", "NETWORM：ネットワークへの侵入成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：対応する機器なし");
  }
}

public class NetDeckKorean extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. 연결된 시스템을 따라 확산되는 내장 퀵핵 NETWORM을 탑재한 신화 사이버덱입니다.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Ping이 가능한 대상에 업로드합니다. 연결된 카메라, 포탑, 지뢰를 비활성화하거나 호환되는 전자 장치를 교란합니다. 연결되지 않은 대상에는 해당 장치가 지원하는 효과만 적용됩니다.");
    this.Text("NetDeckSpreadComplete", "NETWORM: 네트워크 침투 성공");
    this.Text("NetDeckSpreadEmpty", "NETWORM: 호환 장치 없음");
  }
}

public class NetDeckPolish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Kultowy cyberdeck z wbudowanym quickhackiem NETWORM, który rozprzestrzenia się po połączonych systemach.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Wgraj na dowolny cel obsługujący Ping. Wyłącz połączone kamery, wieżyczki i miny albo odwróć uwagę zgodnych urządzeń. Odizolowane cele podlegają tylko obsługiwanym przez nie efektom.");
    this.Text("NetDeckSpreadComplete", "NETWORM: SIEĆ ZINFILTROWANA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: BRAK ZGODNYCH URZĄDZEŃ");
  }
}

public class NetDeckPortugueseBrazil extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck icônico com NETWORM integrado, um hack rápido que se espalha pelos sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Envie para qualquer alvo compatível com Ping. Desative câmeras, torretas e minas conectadas ou distraia dispositivos compatíveis. Alvos isolados recebem apenas os efeitos que suportam.");
    this.Text("NetDeckSpreadComplete", "NETWORM: REDE INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NENHUM DISPOSITIVO COMPATÍVEL");
  }
}

public class NetDeckRussian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Культовая кибердека со встроенным скриптом NETWORM, распространяющимся по связанным системам.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Загрузите на любую цель, поддерживающую Ping. Отключайте связанные камеры, турели и мины или отвлекайте совместимую электронику. На изолированные цели действуют только поддерживаемые ими эффекты.");
    this.Text("NetDeckSpreadComplete", "NETWORM: СЕТЬ ВЗЛОМАНА");
    this.Text("NetDeckSpreadEmpty", "NETWORM: НЕТ СОВМЕСТИМЫХ УСТРОЙСТВ");
  }
}

public class NetDeckSpanishSpain extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ciberterminal icónico con NETWORM integrado, un hackeo rápido que se propaga por sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Cárgalo en cualquier objetivo compatible con Ping. Desactiva cámaras, torretas y minas conectadas o distrae dispositivos compatibles. Los objetivos aislados solo reciben los efectos que admiten.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RED INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO HAY DISPOSITIVOS COMPATIBLES");
  }
}

public class NetDeckSpanishLatinAmerica extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck icónico con NETWORM integrado, un hackeo rápido que se propaga por los sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Súbelo a cualquier objetivo compatible con Ping. Desactiva cámaras, torretas y minas conectadas o distrae dispositivos compatibles. Los objetivos aislados solo reciben los efectos que admiten.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RED INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO HAY DISPOSITIVOS COMPATIBLES");
  }
}

public class NetDeckThai extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1 ไซเบอร์เด็คระดับไอคอนิกที่มี NETWORM ในตัว ควิกแฮ็กที่แพร่กระจายผ่านระบบที่เชื่อมต่อกัน");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "อัปโหลดไปยังเป้าหมายที่ใช้ Ping ได้ ปิดกล้อง ป้อมปืน และทุ่นระเบิดที่เชื่อมต่อกัน หรือรบกวนอุปกรณ์อิเล็กทรอนิกส์ที่รองรับ เป้าหมายที่ไม่เชื่อมต่อจะได้รับผลเฉพาะที่อุปกรณ์รองรับ");
    this.Text("NetDeckSpreadComplete", "NETWORM: แทรกซึมเครือข่ายสำเร็จ");
    this.Text("NetDeckSpreadEmpty", "NETWORM: ไม่พบอุปกรณ์ที่รองรับ");
  }
}

public class NetDeckTurkish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Bağlı sistemlere yayılan NETWORM hızlı hackini içeren ikonik siber deste.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Ping destekleyen herhangi bir hedefe yükle. Bağlı kameraları, taretleri ve mayınları devre dışı bırak veya uyumlu elektronik cihazları şaşırt. Bağlantısız hedeflerde yalnızca desteklenen etkiler uygulanır.");
    this.Text("NetDeckSpreadComplete", "NETWORM: AĞA SIZILDI");
    this.Text("NetDeckSpreadEmpty", "NETWORM: UYUMLU CİHAZ BULUNAMADI");
  }
}

public class NetDeckUkrainian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Культова кібердека з вбудованим скриптом NETWORM, що поширюється через з’єднані системи.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Завантажуйте на будь-яку ціль, що підтримує Ping. Вимикайте підключені камери, турелі та міни або відволікайте сумісну електроніку. На ізольовані цілі діють лише підтримувані ними ефекти.");
    this.Text("NetDeckSpreadComplete", "NETWORM: МЕРЕЖУ ЗЛАМАНО");
    this.Text("NetDeckSpreadEmpty", "NETWORM: НЕМАЄ СУМІСНИХ ПРИСТРОЇВ");
  }
}

public class NetDeckLocalizationProvider extends ModLocalizationProvider {
  public func GetPackage(language: CName) -> ref<ModLocalizationPackage> {
    if Equals(language, n"en-us") { return new NetDeckEnglish(); }
    if Equals(language, n"ar-ar") { return new NetDeckArabic(); }
    if Equals(language, n"zh-cn") { return new NetDeckChineseSimplified(); }
    if Equals(language, n"zh-tw") { return new NetDeckChineseTraditional(); }
    if Equals(language, n"cz-cz") { return new NetDeckCzech(); }
    if Equals(language, n"fr-fr") { return new NetDeckFrench(); }
    if Equals(language, n"de-de") { return new NetDeckGerman(); }
    if Equals(language, n"hu-hu") { return new NetDeckHungarian(); }
    if Equals(language, n"it-it") { return new NetDeckItalian(); }
    if Equals(language, n"jp-jp") { return new NetDeckJapanese(); }
    if Equals(language, n"kr-kr") { return new NetDeckKorean(); }
    if Equals(language, n"pl-pl") { return new NetDeckPolish(); }
    if Equals(language, n"pt-br") { return new NetDeckPortugueseBrazil(); }
    if Equals(language, n"ru-ru") { return new NetDeckRussian(); }
    if Equals(language, n"es-es") { return new NetDeckSpanishSpain(); }
    if Equals(language, n"es-mx") { return new NetDeckSpanishLatinAmerica(); }
    if Equals(language, n"th-th") { return new NetDeckThai(); }
    if Equals(language, n"tr-tr") { return new NetDeckTurkish(); }
    if Equals(language, n"ua-ua") { return new NetDeckUkrainian(); }
    return null;
  }

  public func GetFallback() -> CName {
    return n"en-us";
  }
}
