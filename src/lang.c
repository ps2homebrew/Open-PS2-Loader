#include "include/usbld.h"
#include "include/lang.h"

// Language support...

char *english[] = {
	"               WELCOME TO OPEN PS2 LOADER. MAIN CODE BASED ON SOURCE CODE OF HD PROJECT <http://psx-scene.com> ADAPTATION TO USB ADVANCE FORMAT AND INITIAL GUI BY IFCARO <http://ps2dev.ifcaro.net> MOST OF LOADER CORE IS MADE BY JIMMIKAELKAEL. ALL THE GUI IMPROVEMENTS ARE MADE BY VOLCA. THANKS FOR USING OUR PROGRAM ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Theme configuration", // gfx.c:502
	"IP configuration", // gfx.c:764
	"Save changes", // gfx.c:586
	"O Back", // gfx.c:736
	"Scroll Slow", // main.c:21 
	"Scroll Medium", 
	"Scroll Fast",
	"Dynamic Menu",  // main.c:22
	"Static Menu",
	"Configure theme",  // main.c:23
	"Network config",
	"Select language",  
	"No Items", // main.c:26
	"Not available yet", // main.c:158
	"Settings saved...", //  main.c:166
	"Error writing settings!", //  main.c:168
	"Exit", // Menu item strings
	"Settings",
	"USB Games",
	"HDD Games",
	"Network Games",
	"Apps",
	"Theme", // Submenu items
	"Language",
	"Language: English",
	"Start network",
	"Network loading: %d",
	"Network startup error",
	"Network startup automatic",
	"On",
	"Off",
	"Ok",
	"Compatibility settings",
	"Remove all settings",
	"Removed all keys for the game",
	"Scrolling", // _STR_SCROLLING
	"Menu type", // _STR_MENUTYPE
	"Slow", // _STR_SLOW
	"Medium", // _STR_MEDIUM
	"Fast", // _STR_FAST
	"Static", // _STR_STATIC
	"Dynamic", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE
};

char *czech[] = {
	"               VITEJTE V PROGRAMU OPEN PS2 LOADER. ZALOZENO NA ZDROJ. KODECH PROJEKTU HD PROJECT <http://psx-scene.com> ADAPTACE NA USB ADVANCE FORMAT A PRVOTNI GUI STVORIL IFCARO <http://ps2dev.ifcaro.net> VETSINU KODU LOADERU SEPSAL JIMMIKAELKAEL. PRIDAVNE PRACE NA GUI PROVEDL VOLCA. DEKUJEME ZE POUZIVATE TENTO PROGRAM ^^ ", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Nastaveni vzhledu", // gfx.c:502
	"IP configuration", // gfx.c:764
	"Ulozit zmeny", // gfx.c:586
	"O Zpet", // gfx.c:736
	"Posuv Pomaly", // main.c:21 
	"Posuv Stredni", 
	"Posuv Rychly",
	"Dynamicke Menu",  // main.c:22
	"Staticke Menu",
	"Nastaveni vzhledu",  // main.c:23
	"Nastaveni site",
	"Volba jazyka",
	"Zadne Polozky", // main.c:26
	"Neni doposud k dispozici", // main.c:158
	"Nastaveni ulozeno...", //  main.c:166
	"Chyba Pri ukladani zmen!", //  main.c:168
	"Konec", // Menu item strings
	"Nastaveni",
	"USB Hry",
	"HDD Hry",
	"Sitove Hry",
	"Aplikace",
	"Vzhled", // Submenu items
	"Jazyk",
	"Language: Czech",
	"Spustit sit",
	"Nacitani site: %d",
	"Chyba pri spousteni site",
	"Automaticke spusteni site",
	"Zap",
	"Vyp",
	"Ok",
	"Nastaveni kompatibility",
	"Odstranit vsechna nastaveni",
	"Vsechna nastaveni hry odstranena",
	"Posuv", // _STR_SCROLLING
	"Typ menu", // _STR_MENUTYPE
	"Pomalu", // _STR_SLOW
	"Stredne", // _STR_MEDIUM
	"Rychle", // _STR_FAST
	"Staticke", // _STR_STATIC
	"Dynamicke", // _STR_DYNAMIC
	"Vychozi nabidka", // _STR_DEF_DEVICE
};

char *spanish[] = {
	"               BIENVENIDO A OPEN PS2 LOADER. EL CODIGO PRINCIPAL ESTA BASADO EN EL DE HD PROJECT <http://psx-scene.com> ADAPTADO AL FORMATO DE USB ADVANCE Y INTERFAZ GRAFICA INICIAL POR IFCARO <http://ps2dev.ifcaro.net> LA MAYOR PARTE DEL NUCLEO DEL CARGADOR ES REALIZADA POR JIMMIKAELKAEL. TODAS LAS MEJORAS EN LA INTERFAZ SON REALIZADAS POR VOLCA. GRACIAS POR USAR NUESTRO PROGRAMA ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Configuraciуn de tema", // gfx.c:502
	"Configuraciуn de red", // gfx.c:764
	"Guardar cambios", // gfx.c:586
	"O Atrбs", // gfx.c:736
	"Desplazamiento Lento", // main.c:21 
	"Desplazamiento Medio", 
	"Desplazamiento Rбpido",
	"Menъ dinбmico",  // main.c:22
	"Menъ estбtico",
	"Configurar tema",  // main.c:23
	"Configurar red",
	"Seleccionar idioma",  
	"No hay elementos", // main.c:26
	"Aъn no disponible", // main.c:158
	"Configuraciуn guardada...", //  main.c:166
	"Error al escribir la configuraciуn!", //  main.c:168
	"Salir", // Menu item strings
	"Configuraciуn",
	"Juegos por USB",
	"Juegos por HDD",
	"Juegos por RED",
	"Apps",
	"Tema", // Submenu items
	"Idioma",
	"Idioma: Espaсol",
	"Iniciar red", 
	"Iniciando red: %d",
	"Error al iniciar red",
	"Iniciar red automбticamente",
	"Activado",
	"Desactivado",
	"Aceptar",
	"Preferencias de compatibilidad",
	"Borrar la configuraciуn",
	"Borrar todas las claves del juego",
	"Desplazamiento", // _STR_SCROLLING
	"Tipo de menъ", // _STR_MENUTYPE
	"Lento", // _STR_SLOW
	"Medio", // _STR_MEDIUM
	"Rбpido", // _STR_FAST
	"Estбtico", // _STR_STATIC
	"Dinбmico", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE
};

char *french[] = {
	"              BIENVENUE DANS OPEN PS2 LOADER. CODE PRINCIPAL FONDЙ SUR LE CODE SOIRCE D'HD PROJECT <http://psx-scene.com> ADAPTATION AU FORMAT USB ADVANCE ET GUI INITIAL PAR IFCARO <http://ps2dev.ifcaro.net> LA PLUPART DU CODE DU CЊUR DU LOADER EST DE JIMMIKAELKAEL. TOUTES LES AMELIORATIONS DU GUI SONT DE VOLCA. MERCI D'UTILISER NOTRE PROGRAMME ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Rйglage du thиme", // gfx.c:502
	"Rйglages IP", // gfx.c:764
	"Sauvegarder les changements", // gfx.c:586
	"O retour", // gfx.c:736
	"Dйfilement lent", // main.c:21 
	"Dйfilement moyen", 
	"Dйfilement rapide",
	"Menu dynamique",  // main.c:22
	"Menu statique",
	"Configurer le thиme",  // main.c:23
	"Rйglages rйseau",
	"Choisir la langue",  
	"Vide", // main.c:26
	"Pas encore disponible", // main.c:158
	"Rйglages sauvegardйs...", //  main.c:166
	"Erreur а l'йcriture des rйglages!", //  main.c:168
	"Sortir", // Menu item strings
	"Rйglages",
	"Jeux sur USB",
	"Jeux sur disque dur",
	"Jeux via rйseau",
	"Applications",
	"Theme", // Submenu items
	"Langue",
	"Langue : franзais",
	"Dйmarrer support rйseau",
	"Chargement rйseau: %d",
	"Erreur de dйmarrage rйseau",
	"Dйmarrage rйseau automatique",
	"On",
	"Off",
	"Ok",
	"Rйglages de compatibilitй",
	"Dйsactiver tous les rйglages",
	"Tous modes dйsactivйs",
	"Dйfilement", // _STR_SCROLLING
	"Type de menu", // _STR_MENUTYPE
	"Lent", // _STR_SLOW
	"Moyen", // _STR_MEDIUM
	"Rapide", // _STR_FAST
	"Statique", // _STR_STATIC
	"Dynamique", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *german[] = {
	"               WILLKOMMEN BEIM OPEN PS2 LOADER. DAS HAUPTPROGRAMM BASIERT AUF DEM QUELLCODE DES HD PROJEKTS <http://psx-scene.com> PORTIERUNG IN DAS USB ADVANCE FORMAT UND STANDARD GUI BY IFCARO <http://ps2dev.ifcaro.net> DAS MEISTE DES LOADER PROGRAMMS IST VON JIMMIKAELKAEL PROGRAMMIERT. ALLE GUI ERWEITERUNGEN WURDEN VON VOLKA ERSTELLT. DANKE FЬR DAS BENUTZEN UNSERES PROGRAMMS ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Theme Einstellungen", // gfx.c:502
	"IP Einstellungen", // gfx.c:764
	"Anderungen speichern", // gfx.c:586
	"O Zurьck", // gfx.c:736
	"Langsam scrollen", // main.c:21 
	"Normal scrollen", 
	"Schnell scrollen",
	"Dynamisches Menь",  // main.c:22
	"Statisches Menь",
	"Theme einstellen",  // main.c:23
	"Netzwerk Einstellungen",
	"Sprache auswдhlen",  
	"Keine Items", // main.c:26
	"Derzeit nicht verfьgbar", // main.c:158
	"Einstellungen gespeichert...", //  main.c:166
	"Fehler beim speichern der Einstellungen!", //  main.c:168
	"Ausgang", // Menu item strings
	"Einstellungen",
	"USB Spiele",
	"HDD Spiele",
	"Netzwerk Spiele",
	"Programme",
	"Theme", // Submenu items
	"Sprache",
	"Sprache: Deutsch",
	"Starte Netzwerk",
	"Lade vom Netzwerk: %d",
	"Fehler beim starten des Netzwerks",
	"Netzwerk automatisch starten",
	"An",
	"Aus",
	"Ok",
	"Kompatibilitдts Einstellungen",
	"Entferne alle Einstellungen",
	"Alle Einstellungen des Spiels entfernt",
	"Scrolling", // _STR_SCROLLING
	"Menь Typ", // _STR_MENUTYPE
	"Langsam", // _STR_SLOW
	"Normal", // _STR_MEDIUM
	"Schnell", // _STR_FAST
	"Statisch", // _STR_STATIC
	"Dymanisch", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *portuguese[] = {
	"               BEM-VINDO AO OPEN PS2 LOADER. CУDIGO PRINCIPAL BASEADO NO CУDIGO FONTE DO HD PROJECT <http://psx-scene.com> ADAPTAЗГO PARA O FORMATO USB ADVANCE E INTERFACE GRБFICA INICIAL POR IFCARO <http://ps2dev.ifcaro.net> GRANDE PARTE DO NЪCLEO DO PROGRAMA Й FEITO POR JIMMIKAELKAEL. TODOS INCREMENTOS DA INTERFACE GRБFICA SГO FEITOS POR VOLCA. OBRIGADO POR UTILIZAR NOSSO PROGRAMA ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Configuraзгo do tema", // gfx.c:502
	"Configuraзгo de IP", // gfx.c:764
	"Salvar alteraзхes", // gfx.c:586
	"O Voltar", // gfx.c:736
	"Rolagem lenta", // main.c:21 
	"Rolagem  mйdia", 
	"Rolagem rбpida",
	"Menu dinвmico",  // main.c:22
	"Menu estбtico",
	"Configurar tema",  // main.c:23
	"Configurar rede",
	"Seleзгo de idioma",  
	"Sem Items", // main.c:26
	"Ainda nгo disponнvel", // main.c:158
	"Opзхes salvas...", //  main.c:166
	"Erro guardando opзхes!", //  main.c:168
	"Sair", // Menu item strings
	"Opзхes",
	"Jogos via USB",
	"Jogos via HDD",
	"Jogos via rede",
	"Aplicativos",
	"Tema", // Submenu items
	"Idioma",
	"Idioma: portuguйs",
	"Iniciar rede",
	"Carregando rede: %d",
	"Erro ao iniciar rede",
	"Inнcio de rede automбtico",
	"Ligado",
	"Desligado",
	"Ok",
	"Opзхes de compatibilidade",
	"Remover todas configuraзхes",
	"Todas entradas do jogo removidas",
	"Rolagem", // _STR_SCROLLING
	"Tipo de menu", // _STR_MENUTYPE
	"Lento", // _STR_SLOW
	"Mйdio", // _STR_MEDIUM
	"Rбpido", // _STR_FAST
	"Estбtico", // _STR_STATIC
	"Dinвmico", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *norwegian[] = {
	"               VELKOMMEN TIL OPEN PS2 LOADER. HOVEDPROGRAMMET ER BASERT PЕ KILDEKODE FRA \"HD PROJECT\" <http://psx-scene.com> TILPASSING TIL USB ADVANCE FORMAT OG FШRSTE BRUKERGRENSESNITT AV IFCARO <http://ps2dev.ifcaro.net> DET MESTE AV OPEN PS2 LOADERS KJERNE ER LAGET AV JIMMIKAELKAEL. ALLE BRUKERGRENSESNITTFORBEDRINGENE ER LAGET AV VOLCA. TAKK FOR AT DU BRUKER VЕRT PROGRAM ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Tema oppsett", // gfx.c:502
	"IP oppsett", // gfx.c:764
	"Lagre endringer", // gfx.c:586
	"O Tilbake", // gfx.c:736
	"Rull sakte", // main.c:21 
	"Rull middels", 
	"Rull raskt",
	"Dynamisk meny",  // main.c:22
	"Statisk meny",
	"Velg tema",  // main.c:23
	"Nettverksinstillinger",
	"Velg sprеk",  
	"No Items", // main.c:26
	"Ikke tilgjengelig enda", // main.c:158
	"Oppsett lagret...", //  main.c:166
	"Feil ved lagring av oppsett!", //  main.c:168
	"Avslutt", // Menu item strings
	"Oppsett",
	"USB Spill",
	"HDD Spill",
	"Nettverksspill",
	"Programmer",
	"Temaer", // Submenu items
	"Sprеk",
	"Sprеk: Norsk",
	"Start nettverk",
	"Nettverk lastes: %d",
	"Nettverk lastings feilet",
	"Nettverk starter automatisk",
	"Pе",
	"Av",
	"Ok",
	"Kompatibilitetsoppsett",
	"Fjern alle instillinge",
	"Fjernet alle instillinger fra spillet",
	"Rulling", // _STR_SCROLLING
	"Meny type", // _STR_MENUTYPE
	"Sakte", // _STR_SLOW
	"Middels", // _STR_MEDIUM
	"Raskt", // _STR_FAST
	"Statisk", // _STR_STATIC
	"Dynamisk", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *turkish[] = {
	"               OPEN PS2 LOADER'A HOS GELDINIZ. ANA KOD HD PROJECT <http://psx-scene.com> PROJESI UZERINE IFCARO <http://ps2dev.ifcaro.net> TARAFINDAN GELISTIRILMISTIR. COGU YUKLEME KODLARI JIMMIKAELKAEL TARAFINDAN YAZILMISTIR. TUM ARAYUZ IYILESTIRMELERI VOLCA TARAFINDAN HAZIRLANMISTIR. PROGRAMIMIZ KULLANDIGINIZ ICIN TESEKKURLER ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Arayuz ayarlari", // gfx.c:502
	"IP ayarlari", // gfx.c:764
	"Ayarlari kaydet", // gfx.c:586
	"O Geri", // gfx.c:736
	"Kaydirma: Yavas", // main.c:21 
	"Kaydirma: Normal", 
	"Kaydirma: Hizli",
	"Hareketli Menu",  // main.c:22
	"Sabit Menu",
	"Arayuzu ayarla",  // main.c:23
	"Ag ayarlari",
	"Dil secin",  
	"Bos", // main.c:26
	"Henuz mevcut degil", // main.c:158
	"Ayarlar kaydedildi...", //  main.c:166
	"Hata: Ayarlar kaydedilemedi!", //  main.c:168
	"Cikis", // Menu item strings
	"Ayarlar",
	"USB Oyunlari",
	"HDD Oyunlari",
	"Ag Oyunlari",
	"Uygulamalar",
	"Arayuz", // Submenu items
	"Dil",
	"Dil: Turkce",
	"Agi baslat",
	"Ag yukleniyor: %d",
	"Ag baslatma hatasi",
	"Agi otomatik baslatma",
	"Acik",
	"Kapali",
	"Tamam",
	"Uyumululuk ayarlari",
	"Tum ayarlari sifirla",
	"Oyun icin tum anahatarlari kaldir",
	"Kaydirma", // _STR_SCROLLING
	"Menu tipi", // _STR_MENUTYPE
	"Yavas", // _STR_SLOW
	"Normal", // _STR_MEDIUM
	"Hizli", // _STR_FAST
	"Sabit", // _STR_STATIC
	"Hareketli", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *polish[] = {
	"               WITAM W OPEN PS2 LOADER. GLУWNY KOD WZOROWANY NA KODZIE ZRУDLOWYM HD PROJEKTU <http://psx-scene.com> IMPLEMENTACJE FORMATU USB ADVANCE I GUI PRZEZ IFCARO <http://ps2dev.ifcaro.net> WIEKSZOSC JADRA LOADER'A WYKONANA PRZEZ JIMMIKAELKAEL. WSZYSTKIE ULEPSZENIA GUI WYKONANE PRZEZ VOLCA. DZIEKUJE ZA SKORZYSTANIE Z NASZEJ APLIKACJI. ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Ustawienia theme'u", // gfx.c:502
	"Ustawienia IP", // gfx.c:764
	"Zapisz Zmiany", // gfx.c:586
	"O Powrуt", // gfx.c:736
	"Wolne przewijanie", // main.c:21 
	"Normalne przewijanie", 
	"Szybkie przewijanie",
	"Dynamiczne Menu",  // main.c:22
	"Statyczne Menu",
	"stawienia Theme'уw",  // main.c:23
	"Ustawienia Sieci",
	"Wybierz Jezyk",  
	"Brak Rzeczy", // main.c:26
	"Jeszcze Niedostepne", // main.c:158
	"Ustawienia zostaly zapisane...", //  main.c:166
	"Blad przy zapisie ustawien!", //  main.c:168
	"Wyjscie", // Menu item strings
	"Ustawienia",
	"Gry z USB",
	"Gry z HDD",
	"Gry z Sieci",
	"Programy",
	"Theme", // Submenu items
	"Jezyk",
	"Jezyk : Polski",
	"Uruchom Siec",
	"Ladowanie z sieci: %d",
	"Blad podczas uruchamiania sieci",
	"Automatyczne uruchamianie sieci",
	"Wlaczone",
	"Wylaczone",
	"Ok",
	"Ustawienia Kompatybilnosci",
	"Usun wszystkie ustawienia",
	"Usunieto wszystkie klucze dla gry",
	"Przewijanie", // _STR_SCROLLING
	"Typ Menu", // _STR_MENUTYPE
	"Powoli", // _STR_SLOW
	"Normalnie", // _STR_MEDIUM
	"Szybko", // _STR_FAST
	"Statycznie", // _STR_STATIC
	"Dynamicznie", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *russian[] = {
	"               Добро пожаловать в программу OPEN PS2 LOADER. Основной код базирован на исходном коде программы HD Project <http://psx-scene.com> Адаптация к формату USB ADVANCE и начальный графический интерфейс выполнены IFCARO <http://ps2dev.ifcaro.net> Большинство кода написанно JIMMIKAELKAEL. Все графические улучшения проделаны VOLCA. Спасибо Вам за использование нашей программы!", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Настройка темы", // gfx.c:502
	"Настройка протокола IP", // gfx.c:764
	"Save changes", // gfx.c:586
	"O Назад", // gfx.c:736
	"Медленный скроллинг", // main.c:21 
	"Нормальный скроллинг", 
	"Быстрый скроллинг",
	"Динамическое меню",  // main.c:22
	"Статическое меню",
	"Настроить тему",  // main.c:23
	"Настройки сети",
	"Выбрать язык",  
	"Контент отсутствует", // main.c:26
	"Пока недоступно", // main.c:158
	"Настройки сохранены успешно...", //  main.c:166
	"Ошибка записи настроек!", //  main.c:168
	"Выход", // Menu item strings
	"Настройки",
	"Игры с USB - накопителя",
	"игры с HDD",
	"Игры с сетевого хранилища",
	"Приложения",
	"Тема", // Submenu items
	"Язык",
	"Язык: Русский",
	"Запуск сети",
	"Загрузка сети: %d",
	"Ошибка запуска сети",
	"Автоматический запуск сети",
	"Включить",
	"Выключить",
	"OK",
	"Настройки совместимости",
	"Очистить все настройки",
	"Убрать все настройки для игры",
	"Прокрутка", // _STR_SCROLLING
	"Тип меню", // _STR_MENUTYPE
	"Медленный", // _STR_SLOW
	"Средний", // _STR_MEDIUM
	"Быстрый", // _STR_FAST
	"Статическое", // _STR_STATIC
	"Динамическое", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *indonesian[] = {
	"               SELAMAT DATANG DI OPEN PS2 LOADER. KODE UTAMA BERDASARKAN SUMBER KODE DARI HD PROJECT <http://psx-scene.com> DI ADAPTASI UNTUK FORMAT USB ADVANCE DAN GUI UTAMA DIBUAT OLEH IFCARO <http://ps2dev.ifcaro.net> SEBAGIAN BESAR CORE DIBUAT OLEH JIMMIKAELKAEL. SEMUA PERBAIKAN GUI DIBUAT OLEH VOLCA. TERIMA KASIH TELAH MENGGUNAKAN PROGRAM KAMI ^^", // gfx.c:85
	"Open PS2 Loader %s", // gfx.c:320
	"Konfigurasi tema", // gfx.c:502
	"Konfigurasi IP", // gfx.c:764
	"Simpan perubahan", // gfx.c:586
	"O Kembali", // gfx.c:736
	"Gulung Lambat", // main.c:21 
	"Gulung Normal", 
	"Gulung Cepat",
	"Menu Dinamis",  // main.c:22
	"Menu Statis",
	"Mengatur tema",  // main.c:23
	"Konfigurasi jaringan",
	"Pilih bahasa",  
	"Tidak ada data", // main.c:26
	"Belum tersedia", // main.c:158
	"Pengaturan disimpan...", //  main.c:166
	"Gagal menulis pengaturan!", //  main.c:168
	"Keluar", // Menu item strings
	"Pengaturan",
	"Game-Game USB",
	"Game-Game HDD",
	"Game-Game Jaringan",
	"Aplikasi",
	"Tema", // Submenu items
	"Bahasa",
	"Bahasa: Bahasa Indonesia",
	"Mulai Jaringan",
	"Memuat jaringan: %d",
	"Gagal memulai jaringan",
	"jaringan memulai otomatis",
	"On",
	"Off",
	"Oke",
	"Pengaturan kompatibilitas",
	"Hapus semua pengaturan",
	"Hapus semua pengaturan gamee",
	"Perputaran", // _STR_SCROLLING
	"Tipe menu", // _STR_MENUTYPE
	"Lambat", // _STR_SLOW
	"Normal", // _STR_MEDIUM
	"Cepat", // _STR_FAST
	"Statis", // _STR_STATIC
	"Dinamis", // _STR_DYNAMIC
	"Default menu", // _STR_DEF_DEVICE	
};

char *bulgarian[] = {
	"               Добре дошли в OPEN PS2 LOADER. Кодът е базиран на  HD PROJECT <http://psx-scene.com> Адаптация на USB ADVANCE формата и инсталатора от IFCARO <http://ps2dev.ifcaro.net> Ядрото на програмата е от JIMMIKAELKAEL. Интерфейс от VOLCA. Благодарим ви, че ползвате програмата ни",
	"Open PS2 Loader %s",
	"Конфигурация на изглед",
	"IP конфигурация",
	"Запази промени",
	"O Назад",
	"Бавно меню", 
	"Бързо меню", 
	"Много бързо меню",
	"Динамично меню",
	"Статично меню",
	"Настройка на изглед",
	"Настройка на мрежата",
	"Избор на език",
	"Няма игри",
	"Не е налично",
	"Настройките са запазени...",
	"Грешка при запис!",
	"Изход", 
	"Настройки",
	"Игри от USB",
	"Игри от HDD",
	"Игри от мрежата",
	"Програми",
	"Теми",
	"Език",
	"Език: Български",
	"Стартирай мрежа",
	"Зареждане на мрежа:", 
	"Грешка при зареждане на мрежа",
	"Автоматично стартиране на мрежа",
	"Включено",
	"Изключено",
	"Ок",
	"Настройка на съвместимост",
	"Изтриване на всички настройки",
	"Изтриване на игрови настройки",
	"Scrolling",
	"Вид на менюто",
	"Бавно",
	"Бързо",
	"Много бързо",
	"Статично",
	"Динамично",
	"Default menu", // _STR_DEF_DEVICE	
};


char **languages[] = {
	english, // ID 0
	czech,   // ID 1
	spanish,  // ID 2
	french,   // ID 3
	german,   // ID 4
	portuguese, // ID 5
	norwegian,  // ID 6
	turkish,   // ID 7
	polish,   // ID 8
	russian,   // ID 9
	indonesian,  // ID 10
	bulgarian  // ID 11
};

const char *language_names[] = {
	"English",
	"Cesky (Czech)",
	"Espaсol (Spanish)",
	"Franзais (French)",
	"Deutsch  (German)",
	"Portuguйs  (Portuguese)",
	"Norsk  (Norwegian)",
	"Turkce  (Turkish)",
	"Polski  (Polish)",
	"Русский (Russian)",
	"Indonesia (Indonesian)",
	"Български (Bulgarian)",
	NULL  // Null termination for UI enumeration to have padding
};

const char *english_name = "English";

char **lang_strs = english;

// localised string getter
char *_l(unsigned int id) {
	return lang_strs[id];
}

// language setter (uses LANG_ID as described in lang.h)
void setLanguage(int langID) {
	if (langID < 0)
		langID = 0;

	if (langID > _LANG_ID_MAX)
		langID = _LANG_ID_MAX;	
		
	lang_strs = languages[langID];
}

const char **getLanguageList() {
	return language_names;
}
