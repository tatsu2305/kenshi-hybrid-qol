# Kenshi (v1.0.65 x64) — FCS (Forgotten Construction Set) Yapılandırma Rehberi

Bu belge, `KenshiHybridQoL.dll` C++ eklentisiyle birlikte çalışacak olan `.mod` dosyasının **Forgotten Construction Set (FCS)** üzerinden nasıl yapılandırılacağını adım adım listeler.

---

## 1. Tek Seferde Maksimum Malzeme Taşıma (Smart Bulk Hauling)

C++ eklentisi depodan malzeme çekerken `min(Depodaki_Mevcut_Miktar, Hedef_Binanin_Toplam_Kalan_Ihtiyaci, Karakter_Envanter_Kapasitesi)` formülünü uygular. Karakterlerin envanterinde bu malzemelerin üst üste yığılabilmesi ve AI görevlerinin tek seferde çoklu transferi tetiklemesi için FCS'de aşağıdaki kayıtları güncelleyin:

### A. `Items` Tablosu (Hammadde Stack Limitleri)
**Yol:** `Items` sekmesi -> Aşağıdaki kayıtları aratıp açın:

| Kayıt Adı (Name) | String ID | Değiştirilecek Parametre | Vanilla Değer | Yeni Değer |
| :--- | :--- | :--- | :---: | :---: |
| `Building Material` | `19-gamedata.quack` | `stackable` | `1` (veya `5`) | **`20`** |
| `Iron Plates` | `20-gamedata.quack` | `stackable` | `1` (veya `5`) | **`20`** |
| `Copper` | `53714-rebirth.mod` | `stackable` | `1` (veya `5`) | **`20`** |
| `Copper Alloy Plates` | `53719-rebirth.mod` | `stackable` | `1` | **`20`** |
| `Steel Bars` | `29-gamedata.quack` | `stackable` | `1` (veya `5`) | **`20`** |
| `Fabrics` | `21-gamedata.quack` | `stackable` | `1` (veya `5`) | **`25`** |
| `Leather` | `42874-rebirth.mod` | `stackable` | `5` | **`25`** |
| `Electrical Components` | `28-gamedata.quack` | `stackable` | `1` (veya `5`) | **`25`** |
| `Raw Iron` | `45-gamedata.quack` | `stackable` | `1` (veya `5`) | **`20`** |
| `Raw Stone` | `44-gamedata.quack` | `stackable` | `1` (veya `5`) | **`20`** |

> **İpucu:** Ayrıca `Items -> Backpack` altındaki `Trader's Backpack (Wooden)` ve `Large Backpack` kayıtlarında `stackable bonus` / `max stack` çarpanlarını kontrol ederek çanta dışında da yığılmanın aktif olduğundan emin olun.

### B. `AI Task` / `AI Goals` Tablosu (Taşıma Eşikleri)
**Yol:** `AI -> AI Tasks` ve `AI -> AI Packages`:

| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `AI Tasks` | `Deliver construction resources` | `min haul count` / `batch threshold` | `1` | **`10`** | İnşaata tek malzeme yerine toplu taşıma eşiğini yükseltir. |
| `AI Tasks` | `Fill machine` | `fill threshold` | `1` | **`10`** | Üretim tezgahlarına tek seferde maksimum girdi taşır. |
| `AI Tasks` | `Haul to storage` | `min stack to haul` | `1` | **`5`** | Üretim çıktısında 1 adet oluşur oluşmaz koşmayı önler (C++ hook tam kapasiteyi tamamlar). |

---

## 2. Kapı Saldırılarında Karakter İtişmesi ve Duvar İçine Girme (Anti-Gate Clipping)

> **KARAR (güncel):** NavMesh bozulma riski nedeniyle bu özellik için **C++ tarafında hiçbir pozisyon/çarpışma/itme müdahalesi yapılmaz** (`AntiGateClipping.cpp` devre dışı, ayrıca `GateProximityRadius` / `SeparationForceMultiplier` ayarları artık kullanılmaz). Çözüm yalnızca FCS verisiyle yapılır.
>
> **UYARI:** Aşağıdaki tablolardaki alan adları (`action range`, `access range`, `door collision margin`, `Priority` vb.) ve vanilla değerler FCS'de doğrulanmış değildir; FCS'de ilgili kaydı açıp gerçek alan adlarını kontrol edin, bulamadığınız alanı atlayın. Değerleri tek tek deneyip oyunda test edin.

Kapı dibine yığılmayı azaltmak için FCS tarafında tamircilerin kapının tam dibine girmesini önleyecek ayarlar:

### A. `AI Tasks` Tablosu (Uzaktan Kapı Tamiri)
| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `AI Tasks` | `Repair building` | `action range` / `work range` | `15` - `20` | **`55`** | Tamircilerin kapı kolllisyon kutusuna (collision box) girmeden 55 birim geriden tamir etmesini sağlar. |

### B. `Buildings` Tablosu (Kapı Etkileşim ve Çarpışma Mesafesi)
**Yol:** `Buildings -> Gates`:
- `Defensive Gate`, `Defensive Gate II`, `Defensive Gate III`, `Makeshift Gate` kayıtlarını açın:

| Kayıt Grubu | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :---: | :---: | :--- |
| `Defensive Gate (I - IV)` | `access range` / `use range` | `20.0` | **`50.0`** | İşçilerin kapıya yapışmadan etkileşime geçmesini sağlar. |
| `Defensive Gate (I - IV)` | `door collision margin` | `2.0` | **`6.0`** | Kapı önünde görünmez tampon bölge bırakır. |

### C. `AI Packages` Tablosu (Sivil/İşçi Yığılma Önceliği)
**Yol:** `AI -> AI Packages -> Player Default / Worker`:
- `Repair building` görevinin `Combat` sırasındaki öncelik (`Priority`) değerini yakın dövüş savunmacılarının (`Melee defense`) altına çekin ki tüm işçiler saldırı anında aynı kapı noktasına üst üste binmesin.

---

## 3. Bodyguard ve Korunan Kişi Senkronizasyonu (Tactical Escort)

C++ eklentisi koruma ile VIP arasındaki mesafe `18.0m` (`MaxEscortLeashDistance`) üzerine çıktığında çatışmayı kesip VIP'ye koşmasını ve `10.0m` üzerinde VIP hızını senkronize etmesini sağlar. FCS tarafında:

### A. `AI Goals` & `AI Tasks` Tablosu
| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `AI Goals` | `Bodyguard` (`AI_GOAL_BODYGUARD`) | `maxchase` / `leash range` | `400` - `500` | **`160`** (~16m) | Korumanın VIP'den kopup düşmanı uzağa kovalamasını sınırlar. |
| `AI Goals` | `Bodyguard` (`AI_GOAL_BODYGUARD`) | `follow distance` | `35` | **`20`** | Korumanın seyahat sırasında VIP'ye daha yakın yürümesini sağlar. |
| `AI Tasks` | `Protect ally` | `abandon combat distance` | `350` | **`180`** | VIP uzaklaştığında çatışmayı bırakma mesafesini sıkılaştırır. |

---

## 4. Üs İçi Bayılan Düşmanlar & Ceset Yakıcı (Base Knockout & Corpse Execution)

C++ eklentisi oyuncu üssü (`isPlayerOutpost`) içinde ayılan düşmanların tekrar `Attack Town` görevine dönmesini engeller ve baygın düşmanların **Corpse Furnace**'a atılabilmesini sağlar. FCS tarafında:

| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `AI Tasks` | `Corpse disposal` | `search radius` | `500` | **`1200`** | Üs içindeki tüm baygın/ölü düşmanları tarama yarıçapını genişletir. |
| `Buildings` | `Corpse Furnace` | `storage capacity` | `1` - `2` | **`5`** | Birden fazla cesedin/düşmanın arka arkaya yakılabilmesini sağlar. |

---

## 5. Otomatik Yağmalama & Kalite Filtreli Eritme (Smart Loot & Auto-Smelt)

C++ eklentisi ceset yakıcıya atılmadan önce karakterin üzerindeki tüm ekipmanları soyar ve `SmeltBelowRarity = Blue` ayarına göre:
- **Mavi (`Specialist` / `Mk I-III`) ve üzerini** -> `Weapon Cabinet` / `Armor Storage`'a,
- **Mavi altını (`Prototype`, `Shoddy`, `Standard`, `High`)** -> **Item Furnace**'a yönlendirir.

FCS tarafında **Item Furnace** ve **Storage** kapasitelerini bu otomasyonu besleyecek şekilde ayarlayın:

| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `Buildings` | `Item Furnace` | `inventory width x height` | `12 x 10` | **`30 x 30`** | Otomatik yağmalanan çok sayıda düşük kaliteli silah/zırhı tek seferde alabilmesi için. |
| `Buildings` | `Item Furnace` | `production speed mult` | `1.0` | **`2.5`** | Yığılan düşük kaliteli ekipmanların hızlıca `Iron Plates`'e eritilmesi için. |
| `Buildings` | `Weapon Cabinet` / `Armor Storage` | `stackable` / `slot count` | `20` | **`60`** | Mavi ve üzeri ekipmanların depoda yer kalmadığı için yerde kalmasını önler. |

---

## 6. Arbalet / Taret Nişancılarının Duvarı Terk Etmemesi (Turret Hold Discipline)

C++ eklentisi `ShouldAbandonTurretForMelee` fonksiyonunu hook'layarak taret başındaki karakterin kapı dibindeki/kör noktadaki düşmanlara yakın dövüşe koşmasını engeller. FCS tarafında tareti desteklemek için:

| Tablo | Kayıt Adı (Name) | Parametre | Vanilla Değer | Yeni Değer | Açıklama |
| :--- | :--- | :--- | :---: | :---: | :--- |
| `Buildings` | `Crossbow Turret` / `Harpoon Turret` (Tüm varyantlar) | `max depression angle` (Aşağı eğim açısı) | `45` - `60` | **`80`** | Kapı dibine giren düşmanlara yukarıdan dik açıyla ateş edebilmelerini sağlar. |
| `AI Tasks` | `Use turret` / `Operate turret` | `switch to melee distance` | `50` | **`0`** | Taret başındayken yakın dövüşe geçiş mesafesini FCS düzeyinde de sıfırlar. |

