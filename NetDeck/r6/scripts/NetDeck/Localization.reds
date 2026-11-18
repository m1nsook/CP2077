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
    this.Text("NetDeckInfiltrationDescription", "Spreads through nearby enemies, vehicles and connected devices in chained hops. Glitches weapons, brakes vehicles, opens eligible doors and barriers, and siphons €$. Device uploads can burn hostile netrunners.");
    this.Text("NetDeckSpreadComplete", "NETWORM: NETWORK INFILTRATED");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO COMPATIBLE DEVICES");
    this.Text("NetDeckDevicesAffected", "DEVICES");
    this.Text("NetDeckEddiesStolen", "STOLE");
    this.Text("NetDeckStolenTargets", "TARGETS");
    this.Text("NetDeckWaveProgress", "NETWORM: SPREADING");
    this.Text("NetDeckTargetsAffected", "TARGETS");
  }
}

public class NetDeckArabic extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. سطح إلكتروني أيقوني مزود باختراق NETWORM المدمج الذي ينتشر عبر الأنظمة المتصلة.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "ينتشر عبر الأعداء والمركبات والأجهزة المتصلة القريبة على مراحل. يعطّل أسلحة الأعداء ويوقف المركبات ويفتح الأبواب والحواجز المتاحة ويسحب المال. اختراق الأجهزة قد يحرق الشبكات العصبية للعدائين المعادين.");
    this.Text("NetDeckSpreadComplete", "NETWORM: تم اختراق الشبكة");
    this.Text("NetDeckSpreadEmpty", "NETWORM: لا توجد أجهزة متوافقة");
    this.Text("NetDeckDevicesAffected", "الأجهزة");
    this.Text("NetDeckEddiesStolen", "سُرِقَ");
    this.Text("NetDeckStolenTargets", "أهداف");
    this.Text("NetDeckWaveProgress", "NETWORM: جارٍ الانتشار");
    this.Text("NetDeckTargetsAffected", "الأهداف");
  }
}

public class NetDeckChineseSimplified extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。标志性网络接入仓，内置 NETWORM 快速破解，可沿连接的系统传播。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "在附近敌人、载具和联网设备之间连续传播。干扰武器、紧急制动车辆、开启可控制的门和路障，并窃取欧元。通过设备传播可烧毁敌方网络黑客的突触。");
    this.Text("NetDeckSpreadComplete", "NETWORM：网络入侵成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：没有兼容设备");
    this.Text("NetDeckDevicesAffected", "设备");
    this.Text("NetDeckEddiesStolen", "窃取");
    this.Text("NetDeckStolenTargets", "目标");
    this.Text("NetDeckWaveProgress", "NETWORM：正在传播");
    this.Text("NetDeckTargetsAffected", "目标");
  }
}

public class NetDeckChineseTraditional extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。經典網路接入倉，內建 NETWORM 快速破解，可沿著相連系統擴散。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "在附近敵人、載具和連網裝置間連鎖擴散。干擾武器、緊急煞停載具、開啟可控制的門與路障並竊取歐元。從裝置入侵可燒毀敵方竄網使的突觸。");
    this.Text("NetDeckSpreadComplete", "NETWORM：網路入侵成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：沒有相容裝置");
    this.Text("NetDeckDevicesAffected", "裝置");
    this.Text("NetDeckEddiesStolen", "竊取");
    this.Text("NetDeckStolenTargets", "目標");
    this.Text("NetDeckWaveProgress", "NETWORM：正在擴散");
    this.Text("NetDeckTargetsAffected", "目標");
  }
}

public class NetDeckCzech extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonický kyberdeck s integrovaným rychlým hackem NETWORM, který se šíří propojenými systémy.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Řetězově se šíří mezi blízkými nepřáteli, vozidly a připojenými zařízeními. Ruší zbraně, brzdí auta, otevírá dostupné dveře a závory a vysává eurodolary. Přes zařízení může zasáhnout nepřátelské netrunnery.");
    this.Text("NetDeckSpreadComplete", "NETWORM: SÍŤ INFILTROVÁNA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: ŽÁDNÁ KOMPATIBILNÍ ZAŘÍZENÍ");
    this.Text("NetDeckDevicesAffected", "ZAŘÍZENÍ");
    this.Text("NetDeckEddiesStolen", "UKRADENO");
    this.Text("NetDeckStolenTargets", "CÍLŮ");
    this.Text("NetDeckWaveProgress", "NETWORM: ŠÍŘENÍ");
    this.Text("NetDeckTargetsAffected", "CÍLE");
  }
}

public class NetDeckFrench extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck iconique intégrant NETWORM, un piratage rapide qui se propage dans les systèmes connectés.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Se propage de proche en proche entre ennemis, véhicules et appareils connectés. Enraye les armes, freine les véhicules, ouvre les portes et barrières accessibles et siphonne des eurodollars. Les appareils peuvent brûler les synapses des netrunners ennemis.");
    this.Text("NetDeckSpreadComplete", "NETWORM : RÉSEAU INFILTRÉ");
    this.Text("NetDeckSpreadEmpty", "NETWORM : AUCUN APPAREIL COMPATIBLE");
    this.Text("NetDeckDevicesAffected", "APPAREILS");
    this.Text("NetDeckEddiesStolen", "VOLÉS");
    this.Text("NetDeckStolenTargets", "CIBLES");
    this.Text("NetDeckWaveProgress", "NETWORM : PROPAGATION");
    this.Text("NetDeckTargetsAffected", "CIBLES");
  }
}

public class NetDeckGerman extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonisches Cyberdeck mit integriertem NETWORM-Quickhack, der sich über verbundene Systeme ausbreitet.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Springt zwischen nahen Feinden, Fahrzeugen und vernetzten Geräten weiter. Stört Waffen, bremst Fahrzeuge, öffnet geeignete Türen und Schranken und zweigt Eurodollars ab. Über Geräte können feindliche Netrunner Synapsenbrand erleiden.");
    this.Text("NetDeckSpreadComplete", "NETWORM: NETZWERK INFILTRIERT");
    this.Text("NetDeckSpreadEmpty", "NETWORM: KEINE KOMPATIBLEN GERÄTE");
    this.Text("NetDeckDevicesAffected", "GERÄTE");
    this.Text("NetDeckEddiesStolen", "GESTOHLEN");
    this.Text("NetDeckStolenTargets", "ZIELE");
    this.Text("NetDeckWaveProgress", "NETWORM: VERBREITUNG");
    this.Text("NetDeckTargetsAffected", "ZIELE");
  }
}

public class NetDeckHungarian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ikonikus kiberdeck, beépített NETWORM gyorshackkel, amely a csatlakoztatott rendszereken terjed.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Láncszerűen terjed a közeli ellenségek, járművek és hálózati eszközök között. Akadályozza a fegyvereket, fékezi a járműveket, nyitja az elérhető ajtókat és sorompókat, valamint eurodollárt lop. Az eszközökről az ellenséges netrunnereket is támadja.");
    this.Text("NetDeckSpreadComplete", "NETWORM: HÁLÓZAT FELTÖRVE");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NINCS KOMPATIBILIS ESZKÖZ");
    this.Text("NetDeckDevicesAffected", "ESZKÖZÖK");
    this.Text("NetDeckEddiesStolen", "ELLOPVA");
    this.Text("NetDeckStolenTargets", "CÉLPONT");
    this.Text("NetDeckWaveProgress", "NETWORM: TERJEDÉS");
    this.Text("NetDeckTargetsAffected", "CÉLPONTOK");
  }
}

public class NetDeckItalian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck iconico con NETWORM integrato, un hack rapido che si propaga nei sistemi collegati.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Si diffonde a catena fra nemici vicini, veicoli e dispositivi collegati. Blocca le armi, frena i veicoli, apre porte e barriere compatibili e sottrae eurodollari. Dagli apparecchi può infliggere Bruciatura sinaptica ai netrunner ostili.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RETE INFILTRATA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NESSUN DISPOSITIVO COMPATIBILE");
    this.Text("NetDeckDevicesAffected", "DISPOSITIVI");
    this.Text("NetDeckEddiesStolen", "RUBATI");
    this.Text("NetDeckStolenTargets", "BERSAGLI");
    this.Text("NetDeckWaveProgress", "NETWORM: PROPAGAZIONE");
    this.Text("NetDeckTargetsAffected", "BERSAGLI");
  }
}

public class NetDeckJapanese extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1。接続されたシステムに拡散するクイックハック「NETWORM」を内蔵したアイコニック・サイバーデッキ。");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "周囲の敵、車両、接続機器へ連鎖的に拡散。武器を妨害し、車両を急停止させ、対応するドアや遮断機を開き、ユーロドルを奪取する。機器からの感染は敵ネットランナーにシナプス焼却を引き起こす。");
    this.Text("NetDeckSpreadComplete", "NETWORM：ネットワークへの侵入成功");
    this.Text("NetDeckSpreadEmpty", "NETWORM：対応する機器なし");
    this.Text("NetDeckDevicesAffected", "デバイス");
    this.Text("NetDeckEddiesStolen", "奪取");
    this.Text("NetDeckStolenTargets", "対象");
    this.Text("NetDeckWaveProgress", "NETWORM：拡散中");
    this.Text("NetDeckTargetsAffected", "対象");
  }
}

public class NetDeckKorean extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. 연결된 시스템을 따라 확산되는 내장 퀵핵 NETWORM을 탑재한 신화 사이버덱입니다.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "주변 적, 차량, 연결된 장치 사이로 연쇄 확산됩니다. 무기를 교란하고 차량을 급정지시키며 작동 가능한 문과 차단기를 열고 유로달러를 탈취합니다. 장치에서 확산될 때 적 넷러너에게 시냅스 과열을 가할 수 있습니다.");
    this.Text("NetDeckSpreadComplete", "NETWORM: 네트워크 침투 성공");
    this.Text("NetDeckSpreadEmpty", "NETWORM: 호환 장치 없음");
    this.Text("NetDeckDevicesAffected", "장치");
    this.Text("NetDeckEddiesStolen", "탈취");
    this.Text("NetDeckStolenTargets", "대상");
    this.Text("NetDeckWaveProgress", "NETWORM: 확산 중");
    this.Text("NetDeckTargetsAffected", "대상");
  }
}

public class NetDeckPolish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Kultowy cyberdeck z wbudowanym quickhackiem NETWORM, który rozprzestrzenia się po połączonych systemach.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Rozprzestrzenia się łańcuchowo między pobliskimi wrogami, pojazdami i urządzeniami. Zakłóca broń, zatrzymuje auta, otwiera dostępne drzwi i szlabany oraz kradnie eurodolary. Z urządzeń może wywołać Wypalenie synaps u wrogich netrunnerów.");
    this.Text("NetDeckSpreadComplete", "NETWORM: SIEĆ ZINFILTROWANA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: BRAK ZGODNYCH URZĄDZEŃ");
    this.Text("NetDeckDevicesAffected", "URZĄDZENIA");
    this.Text("NetDeckEddiesStolen", "SKRADZIONO");
    this.Text("NetDeckStolenTargets", "CELÓW");
    this.Text("NetDeckWaveProgress", "NETWORM: ROZPRZESTRZENIANIE");
    this.Text("NetDeckTargetsAffected", "CELE");
  }
}

public class NetDeckPortugueseBrazil extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck icônico com NETWORM integrado, um hack rápido que se espalha pelos sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Espalha-se em cadeia entre inimigos próximos, veículos e dispositivos conectados. Sabota armas, freia veículos, abre portas e barreiras compatíveis e rouba eurodólares. A partir de dispositivos pode causar Queima Sináptica em netrunners inimigos.");
    this.Text("NetDeckSpreadComplete", "NETWORM: REDE INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NENHUM DISPOSITIVO COMPATÍVEL");
    this.Text("NetDeckDevicesAffected", "DISPOSITIVOS");
    this.Text("NetDeckEddiesStolen", "ROUBADOS");
    this.Text("NetDeckStolenTargets", "ALVOS");
    this.Text("NetDeckWaveProgress", "NETWORM: PROPAGANDO");
    this.Text("NetDeckTargetsAffected", "ALVOS");
  }
}

public class NetDeckRussian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Культовая кибердека со встроенным скриптом NETWORM, распространяющимся по связанным системам.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Цепная атака по ближайшим врагам, машинам и сетевым устройствам. Вызывает сбои оружия, останавливает транспорт, открывает доступные двери и шлагбаумы и похищает евродоллары. Через устройства может выжигать синапсы вражеских нетраннеров.");
    this.Text("NetDeckSpreadComplete", "NETWORM: СЕТЬ ВЗЛОМАНА");
    this.Text("NetDeckSpreadEmpty", "NETWORM: НЕТ СОВМЕСТИМЫХ УСТРОЙСТВ");
    this.Text("NetDeckDevicesAffected", "УСТРОЙСТВА");
    this.Text("NetDeckEddiesStolen", "ПОХИЩЕНО");
    this.Text("NetDeckStolenTargets", "ЦЕЛЕЙ");
    this.Text("NetDeckWaveProgress", "NETWORM: РАСПРОСТРАНЕНИЕ");
    this.Text("NetDeckTargetsAffected", "ЦЕЛИ");
  }
}

public class NetDeckSpanishSpain extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Ciberterminal icónico con NETWORM integrado, un hackeo rápido que se propaga por sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Se propaga en cadena entre enemigos cercanos, vehículos y dispositivos conectados. Bloquea armas, frena vehículos, abre puertas y barreras compatibles y roba eurodólares. Desde dispositivos puede quemar las sinapsis de netrunners hostiles.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RED INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO HAY DISPOSITIVOS COMPATIBLES");
    this.Text("NetDeckDevicesAffected", "DISPOSITIVOS");
    this.Text("NetDeckEddiesStolen", "ROBADOS");
    this.Text("NetDeckStolenTargets", "OBJETIVOS");
    this.Text("NetDeckWaveProgress", "NETWORM: PROPAGACIÓN");
    this.Text("NetDeckTargetsAffected", "OBJETIVOS");
  }
}

public class NetDeckSpanishLatinAmerica extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Cyberdeck icónico con NETWORM integrado, un hackeo rápido que se propaga por los sistemas conectados.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Se propaga en cadena entre enemigos cercanos, vehículos y dispositivos conectados. Interfiere armas, frena vehículos, abre puertas y barreras compatibles y roba eurodólares. Desde dispositivos puede quemar las sinapsis de netrunners enemigos.");
    this.Text("NetDeckSpreadComplete", "NETWORM: RED INFILTRADA");
    this.Text("NetDeckSpreadEmpty", "NETWORM: NO HAY DISPOSITIVOS COMPATIBLES");
    this.Text("NetDeckDevicesAffected", "DISPOSITIVOS");
    this.Text("NetDeckEddiesStolen", "ROBADOS");
    this.Text("NetDeckStolenTargets", "OBJETIVOS");
    this.Text("NetDeckWaveProgress", "NETWORM: PROPAGACIÓN");
    this.Text("NetDeckTargetsAffected", "OBJETIVOS");
  }
}

public class NetDeckThai extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1 ไซเบอร์เด็คระดับไอคอนิกที่มี NETWORM ในตัว ควิกแฮ็กที่แพร่กระจายผ่านระบบที่เชื่อมต่อกัน");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "แพร่กระจายต่อเนื่องระหว่างศัตรู ยานพาหนะ และอุปกรณ์ที่เชื่อมต่ออยู่ใกล้กัน รบกวนอาวุธ เบรกยานพาหนะ เปิดประตูและไม้กั้นที่รองรับ และขโมยเงินยูโรดอลลาร์ การแฮ็กจากอุปกรณ์อาจเผาไซแนปส์ของเน็ตรันเนอร์ศัตรู");
    this.Text("NetDeckSpreadComplete", "NETWORM: แทรกซึมเครือข่ายสำเร็จ");
    this.Text("NetDeckSpreadEmpty", "NETWORM: ไม่พบอุปกรณ์ที่รองรับ");
    this.Text("NetDeckDevicesAffected", "อุปกรณ์");
    this.Text("NetDeckEddiesStolen", "ขโมย");
    this.Text("NetDeckStolenTargets", "เป้าหมาย");
    this.Text("NetDeckWaveProgress", "NETWORM: กำลังแพร่กระจาย");
    this.Text("NetDeckTargetsAffected", "เป้าหมาย");
  }
}

public class NetDeckTurkish extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Bağlı sistemlere yayılan NETWORM hızlı hackini içeren ikonik siber deste.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Yakındaki düşmanlar, araçlar ve bağlı cihazlar arasında zincirleme yayılır. Silahları bozar, araçları frenler, uygun kapı ve bariyerleri açar, eurodolar çalar. Cihazlardan yayıldığında düşman netrunner'lara Sinaps Yanığı uygulayabilir.");
    this.Text("NetDeckSpreadComplete", "NETWORM: AĞA SIZILDI");
    this.Text("NetDeckSpreadEmpty", "NETWORM: UYUMLU CİHAZ BULUNAMADI");
    this.Text("NetDeckDevicesAffected", "CİHAZLAR");
    this.Text("NetDeckEddiesStolen", "ÇALINDI");
    this.Text("NetDeckStolenTargets", "HEDEF");
    this.Text("NetDeckWaveProgress", "NETWORM: YAYILIYOR");
    this.Text("NetDeckTargetsAffected", "HEDEFLER");
  }
}

public class NetDeckUkrainian extends ModLocalizationPackage {
  protected func DefineTexts() -> Void {
    this.Text("NetDeckDeckName", "NetWatch Σ MK.1");
    this.Text("NetDeckDeckDescription", "NetWatch Σ MK.1. Культова кібердека з вбудованим скриптом NETWORM, що поширюється через з’єднані системи.");
    this.Text("NetDeckInfiltrationName", "NETWORM");
    this.Text("NetDeckInfiltrationDescription", "Ланцюгово поширюється між найближчими ворогами, транспортом і підключеними пристроями. Псує зброю, гальмує авто, відчиняє доступні двері й шлагбауми та викрадає євродолари. Через пристрої може випалювати синапси ворожих нетранерів.");
    this.Text("NetDeckSpreadComplete", "NETWORM: МЕРЕЖУ ЗЛАМАНО");
    this.Text("NetDeckSpreadEmpty", "NETWORM: НЕМАЄ СУМІСНИХ ПРИСТРОЇВ");
    this.Text("NetDeckDevicesAffected", "ПРИСТРОЇ");
    this.Text("NetDeckEddiesStolen", "ВИКРАДЕНО");
    this.Text("NetDeckStolenTargets", "ЦІЛЕЙ");
    this.Text("NetDeckWaveProgress", "NETWORM: ПОШИРЕННЯ");
    this.Text("NetDeckTargetsAffected", "ЦІЛІ");
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
