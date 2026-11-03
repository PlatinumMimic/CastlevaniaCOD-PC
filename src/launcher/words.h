// The launcher's words in its languages: English, Portuguese (Brazil), Spanish, Italian, Russian, French and German.
// English is what the code says and what a translation is found by: T("...") of a phrase without a row here does not
// compile. In a phrase, {} is where F puts its arguments, in order, so a language can put them where its sentence
// needs them.
#pragma once

#include <atomic>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <string>
#include <string_view>

namespace words {

// a row's columns, in this order
enum Lang : int { kEn, kPt, kEs, kIt, kRu, kFr, kDe, kLangs };

// the code castlevania.toml keeps (launcher_language), the language's name in itself
struct Language {
    const char* code;
    const char* name;
};
inline constexpr Language kLanguages[kLangs] = {
    {"en", "English"}, {"pt-BR", "Português (Brasil)"}, {"es", "Español"}, {"it", "Italiano"}, {"ru", "Русский"},
    {"fr", "Français"}, {"de", "Deutsch"},
};
// the order the launcher lists them in: English, then by their names
inline constexpr int kMenu[kLangs] = {kEn, kDe, kEs, kFr, kIt, kPt, kRu};

struct Row {
    const char* text[kLangs];
};

inline constexpr Row kRows[] = {
    // the window, the sidebar, the notes it shows
    {{"Castlevania: Curse of Darkness - Launcher",
      "Castlevania: Curse of Darkness - Launcher",
      "Castlevania: Curse of Darkness - Launcher",
      "Castlevania: Curse of Darkness - Launcher",
      "Castlevania: Curse of Darkness - лаунчер",
      "Castlevania: Curse of Darkness - Lanceur",
      "Castlevania: Curse of Darkness - Launcher"}},
    {{"Direct3D 11 is not available.",
      "O Direct3D 11 não está disponível.",
      "Direct3D 11 no está disponible.",
      "Direct3D 11 non è disponibile.",
      "Direct3D 11 недоступен.",
      "Direct3D 11 n'est pas disponible.",
      "Direct3D 11 ist nicht verfügbar."}},
    {{"Home", "Início", "Inicio", "Home", "Главная", "Accueil", "Startseite"}},
    {{"Display", "Tela", "Pantalla", "Schermo", "Экран", "Affichage", "Anzeige"}},
    {{"Graphics", "Gráficos", "Gráficos", "Grafica", "Графика", "Graphismes", "Grafik"}},
    {{"Game", "Jogo", "Juego", "Gioco", "Игра", "Jeu", "Spiel"}},
    {{"Controls", "Controles", "Controles", "Controlli", "Управление", "Commandes", "Steuerung"}},
    {{"Game files",
      "Arquivos do jogo",
      "Archivos del juego",
      "File di gioco",
      "Файлы игры",
      "Fichiers du jeu",
      "Spieldateien"}},
    {{"SETTINGS", "CONFIGURAÇÕES", "AJUSTES", "IMPOSTAZIONI", "НАСТРОЙКИ", "PARAMÈTRES", "EINSTELLUNGEN"}},
    {{"Language", "Idioma", "Idioma", "Lingua", "Язык", "Langue", "Sprache"}},
    // frame generation (the launcher's Graphics page)
    {{"Frame generation",
      "Geração de quadros",
      "Generación de fotogramas",
      "Generazione di fotogrammi",
      "Генерация кадров",
      "Génération d'images",
      "Frame-Generierung"}},
    {{"A frame made between each two the game draws, for twice the frame rate on a screen that shows it (120 Hz or more). AMD FSR 3 works on any graphics card; NVIDIA DLSS needs a GeForce RTX 40 or 50. From the next start.",
      "Um quadro gerado entre cada dois que o jogo desenha, para o dobro da taxa de quadros numa tela que a mostre (120 Hz ou mais). O AMD FSR 3 funciona em qualquer placa de vídeo; o NVIDIA DLSS precisa de uma GeForce RTX 40 ou 50. A partir do próximo início.",
      "Un fotograma generado entre cada dos que dibuja el juego, para el doble de fotogramas por segundo en una pantalla que lo muestre (120 Hz o más). AMD FSR 3 funciona en cualquier tarjeta gráfica; NVIDIA DLSS necesita una GeForce RTX 40 o 50. Desde el próximo inicio.",
      "Un fotogramma generato tra ogni due che il gioco disegna, per il doppio dei fotogrammi al secondo su uno schermo che li mostri (120 Hz o più). AMD FSR 3 funziona su qualsiasi scheda grafica; NVIDIA DLSS richiede una GeForce RTX 40 o 50. Dal prossimo avvio.",
      "Кадр, создаваемый между каждыми двумя кадрами игры: вдвое больше кадров в секунду на экране, который их покажет (120 Гц и выше). AMD FSR 3 работает на любой видеокарте; NVIDIA DLSS требует GeForce RTX 40 или 50. Со следующего запуска.",
      "Une image générée entre chaque paire d'images du jeu, pour deux fois plus d'images par seconde sur un écran qui les affiche (120 Hz ou plus). AMD FSR 3 fonctionne sur toute carte graphique ; NVIDIA DLSS demande une GeForce RTX 40 ou 50. Au prochain démarrage.",
      "Ein Bild zwischen je zwei Bildern des Spiels erzeugt, für die doppelte Bildrate auf einem Bildschirm, der sie zeigt (120 Hz oder mehr). AMD FSR 3 läuft auf jeder Grafikkarte; NVIDIA DLSS braucht eine GeForce RTX 40 oder 50. Ab dem nächsten Start."}},
    {{"Needs a GeForce RTX 40 or 50: this graphics card has no DLSS frame generation. AMD FSR 3 works on it.",
      "Precisa de uma GeForce RTX 40 ou 50: esta placa de vídeo não tem geração de quadros DLSS. O AMD FSR 3 funciona nela.",
      "Necesita una GeForce RTX 40 o 50: esta tarjeta gráfica no tiene generación de fotogramas DLSS. AMD FSR 3 funciona en ella.",
      "Richiede una GeForce RTX 40 o 50: questa scheda grafica non ha la generazione di fotogrammi DLSS. AMD FSR 3 funziona su di essa.",
      "Требуется GeForce RTX 40 или 50: у этой видеокарты нет генерации кадров DLSS. AMD FSR 3 на ней работает.",
      "Demande une GeForce RTX 40 ou 50 : cette carte graphique n'a pas la génération d'images DLSS. AMD FSR 3 fonctionne dessus.",
      "Braucht eine GeForce RTX 40 oder 50: Diese Grafikkarte hat keine DLSS-Frame-Generierung. AMD FSR 3 läuft auf ihr."}},
    {{"With HDR on, the game sends HDR10 for DLSS frame generation, the HDR format it takes.",
      "Com o HDR ligado, o jogo envia HDR10 para a geração de quadros DLSS, o formato HDR que ela aceita.",
      "Con el HDR activado, el juego envía HDR10 para la generación de fotogramas DLSS, el formato HDR que admite.",
      "Con l'HDR attivo, il gioco invia HDR10 per la generazione di fotogrammi DLSS, il formato HDR che accetta.",
      "При включённом HDR игра выводит HDR10 для генерации кадров DLSS: это формат HDR, который она принимает.",
      "Avec le HDR activé, le jeu envoie du HDR10 pour la génération d'images DLSS, le format HDR qu'elle accepte.",
      "Mit HDR an sendet das Spiel für die DLSS-Frame-Generierung HDR10, das HDR-Format, das sie annimmt."}},
    {{"Downloading from AMD...",
      "Baixando da AMD...",
      "Descargando de AMD...",
      "Download da AMD...",
      "Загрузка с сайта AMD...",
      "Téléchargement chez AMD...",
      "Wird von AMD heruntergeladen..."}},
    {{"Downloading from NVIDIA...",
      "Baixando da NVIDIA...",
      "Descargando de NVIDIA...",
      "Download da NVIDIA...",
      "Загрузка с сайта NVIDIA...",
      "Téléchargement chez NVIDIA...",
      "Wird von NVIDIA heruntergeladen..."}},
    {{"AMD FSR 3's files are here",
      "Os arquivos do AMD FSR 3 estão aqui",
      "Los archivos de AMD FSR 3 están aquí",
      "I file di AMD FSR 3 sono qui",
      "Файлы AMD FSR 3 на месте",
      "Les fichiers d'AMD FSR 3 sont là",
      "Die Dateien von AMD FSR 3 sind da"}},
    {{"NVIDIA DLSS's files are here",
      "Os arquivos do NVIDIA DLSS estão aqui",
      "Los archivos de NVIDIA DLSS están aquí",
      "I file di NVIDIA DLSS sono qui",
      "Файлы NVIDIA DLSS на месте",
      "Les fichiers d'NVIDIA DLSS sont là",
      "Die Dateien von NVIDIA DLSS sind da"}},
    {{"AMD FSR 3's files are not here yet",
      "Os arquivos do AMD FSR 3 ainda não estão aqui",
      "Los archivos de AMD FSR 3 aún no están aquí",
      "I file di AMD FSR 3 non sono ancora qui",
      "Файлов AMD FSR 3 пока нет",
      "Les fichiers d'AMD FSR 3 ne sont pas encore là",
      "Die Dateien von AMD FSR 3 fehlen noch"}},
    {{"NVIDIA DLSS's files are not here yet",
      "Os arquivos do NVIDIA DLSS ainda não estão aqui",
      "Los archivos de NVIDIA DLSS aún no están aquí",
      "I file di NVIDIA DLSS non sono ancora qui",
      "Файлов NVIDIA DLSS пока нет",
      "Les fichiers d'NVIDIA DLSS ne sont pas encore là",
      "Die Dateien von NVIDIA DLSS fehlen noch"}},
    {{"From AMD's own release on GitHub. The files are AMD's, under AMD's licence, which is saved with them; they are not part of this game's download.",
      "Da versão oficial da AMD no GitHub. Os arquivos são da AMD, sob a licença da AMD, que é salva junto com eles; não fazem parte do download deste jogo.",
      "Del lanzamiento oficial de AMD en GitHub. Los archivos son de AMD, bajo la licencia de AMD, que se guarda con ellos; no forman parte de la descarga de este juego.",
      "Dalla release ufficiale di AMD su GitHub. I file sono di AMD, sotto la licenza di AMD, salvata insieme a loro; non fanno parte del download di questo gioco.",
      "Из официального выпуска AMD на GitHub. Файлы принадлежат AMD и распространяются по лицензии AMD, которая сохраняется вместе с ними; они не входят в загрузку этой игры.",
      "Depuis la version officielle d'AMD sur GitHub. Les fichiers sont à AMD, sous la licence d'AMD, enregistrée avec eux ; ils ne font pas partie du téléchargement de ce jeu.",
      "Aus AMDs eigener Veröffentlichung auf GitHub. Die Dateien gehören AMD, unter AMDs Lizenz, die mit ihnen gespeichert wird; sie sind nicht Teil des Downloads dieses Spiels."}},
    {{"From NVIDIA's own Streamline release on GitHub (about 280 MB, of which the game keeps 11). The files are NVIDIA's, under NVIDIA's licences, which are saved with them; they are not part of this game's download.",
      "Da versão oficial do Streamline da NVIDIA no GitHub (cerca de 280 MB, dos quais o jogo guarda 11). Os arquivos são da NVIDIA, sob as licenças da NVIDIA, que são salvas junto com eles; não fazem parte do download deste jogo.",
      "Del lanzamiento oficial de Streamline de NVIDIA en GitHub (unos 280 MB, de los que el juego guarda 11). Los archivos son de NVIDIA, bajo las licencias de NVIDIA, que se guardan con ellos; no forman parte de la descarga de este juego.",
      "Dalla release ufficiale di Streamline di NVIDIA su GitHub (circa 280 MB, di cui il gioco tiene 11). I file sono di NVIDIA, sotto le licenze di NVIDIA, salvate insieme a loro; non fanno parte del download di questo gioco.",
      "Из официального выпуска Streamline от NVIDIA на GitHub (около 280 МБ, из которых игра оставляет 11). Файлы принадлежат NVIDIA и распространяются по лицензиям NVIDIA, которые сохраняются вместе с ними; они не входят в загрузку этой игры.",
      "Depuis la version officielle de Streamline d'NVIDIA sur GitHub (environ 280 Mo, dont le jeu garde 11). Les fichiers sont à NVIDIA, sous les licences d'NVIDIA, enregistrées avec eux ; ils ne font pas partie du téléchargement de ce jeu.",
      "Aus NVIDIAs eigener Streamline-Veröffentlichung auf GitHub (etwa 280 MB, von denen das Spiel 11 behält). Die Dateien gehören NVIDIA, unter NVIDIAs Lizenzen, die mit ihnen gespeichert werden; sie sind nicht Teil des Downloads dieses Spiels."}},
    {{"Download from AMD",
      "Baixar da AMD",
      "Descargar de AMD",
      "Scarica da AMD",
      "Скачать с сайта AMD",
      "Télécharger chez AMD",
      "Von AMD herunterladen"}},
    {{"Download from NVIDIA",
      "Baixar da NVIDIA",
      "Descargar de NVIDIA",
      "Scarica da NVIDIA",
      "Скачать с сайта NVIDIA",
      "Télécharger chez NVIDIA",
      "Von NVIDIA herunterladen"}},
    {{"Read the licence",
      "Ler a licença",
      "Leer la licencia",
      "Leggi la licenza",
      "Прочитать лицензию",
      "Lire la licence",
      "Lizenz lesen"}},
    {{"Downloaded. Frame generation starts with the game's next start.",
      "Baixado. A geração de quadros começa no próximo início do jogo.",
      "Descargado. La generación de fotogramas empieza en el próximo inicio del juego.",
      "Scaricato. La generazione di fotogrammi parte dal prossimo avvio del gioco.",
      "Загружено. Генерация кадров включится при следующем запуске игры.",
      "Téléchargé. La génération d'images démarre au prochain lancement du jeu.",
      "Heruntergeladen. Die Frame-Generierung beginnt mit dem nächsten Spielstart."}},
    {{"The download failed (error {}): check the connection and try again.",
      "O download falhou (erro {}): verifique a conexão e tente novamente.",
      "La descarga falló (error {}): revisa la conexión y vuelve a intentarlo.",
      "Il download non è riuscito (errore {}): controlla la connessione e riprova.",
      "Загрузка не удалась (ошибка {}): проверьте подключение и попробуйте снова.",
      "Le téléchargement a échoué (erreur {}) : vérifiez la connexion et réessayez.",
      "Der Download ist fehlgeschlagen (Fehler {}): Verbindung prüfen und erneut versuchen."}},
    {{"Full screen (F11)",
      "Tela cheia (F11)",
      "Pantalla completa (F11)",
      "Schermo intero (F11)",
      "Полный экран (F11)",
      "Plein écran (F11)",
      "Vollbild (F11)"}},
    {{"Leave full screen (F11)",
      "Sair da tela cheia (F11)",
      "Salir de pantalla completa (F11)",
      "Esci da schermo intero (F11)",
      "Выйти из полноэкранного режима (F11)",
      "Quitter le plein écran (F11)",
      "Vollbild beenden (F11)"}},
    {{"Reset to defaults",
      "Restaurar padrões",
      "Restablecer valores",
      "Ripristina predefiniti",
      "Сбросить настройки",
      "Réinitialiser",
      "Zurücksetzen"}},
    {{"Defaults restored",
      "Padrões restaurados",
      "Valores restablecidos",
      "Predefiniti ripristinati",
      "Настройки сброшены",
      "Valeurs par défaut rétablies",
      "Standardwerte wiederhergestellt"}},
    {{"Settings saved",
      "Configurações salvas",
      "Ajustes guardados",
      "Impostazioni salvate",
      "Настройки сохранены",
      "Paramètres enregistrés",
      "Einstellungen gespeichert"}},
    {{"Could not write the settings file",
      "Não foi possível salvar o arquivo de configurações",
      "No se pudo guardar el archivo de ajustes",
      "Impossibile scrivere il file delle impostazioni",
      "Не удалось записать файл настроек",
      "Impossible d'écrire le fichier des paramètres",
      "Einstellungsdatei konnte nicht geschrieben werden"}},
    {{"Game started",
      "Jogo iniciado",
      "Juego iniciado",
      "Gioco avviato",
      "Игра запущена",
      "Jeu lancé",
      "Spiel gestartet"}},
    {{"Could not start the game (error {})",
      "Não foi possível iniciar o jogo (erro {})",
      "No se pudo iniciar el juego (error {})",
      "Impossibile avviare il gioco (errore {})",
      "Не удалось запустить игру (ошибка {})",
      "Impossible de lancer le jeu (erreur {})",
      "Spiel konnte nicht gestartet werden (Fehler {})"}},
    {{"Key {}", "Tecla {}", "Tecla {}", "Tasto {}", "Клавиша {}", "Touche {}", "Taste {}"}},
    {{"Desktop ({})",
      "Área de trabalho ({})",
      "Escritorio ({})",
      "Desktop ({})",
      "Рабочий стол ({})",
      "Bureau ({})",
      "Desktop ({})"}},

    // the home page
    {{"ORIGINAL XBOX  |  NATIVE PC PORT",
      "XBOX ORIGINAL  |  PORT NATIVO PARA PC",
      "XBOX ORIGINAL  |  PORT NATIVO PARA PC",
      "XBOX ORIGINALE  |  PORT NATIVO PER PC",
      "ОРИГИНАЛЬНЫЙ XBOX  |  НАТИВНЫЙ ПОРТ ДЛЯ ПК",
      "XBOX D'ORIGINE  |  PORTAGE PC NATIF",
      "ORIGINAL-XBOX  |  NATIVER PC-PORT"}},
    {{"Play", "Jogar", "Jugar", "Gioca", "Играть", "Jouer", "Spielen"}},
    {{"Set up game files",
      "Adicionar o jogo",
      "Añadir el juego",
      "Aggiungi il gioco",
      "Добавить игру",
      "Ajouter le jeu",
      "Spiel hinzufügen"}},
    {{"No Direct3D 12",
      "Sem Direct3D 12",
      "Sin Direct3D 12",
      "Niente Direct3D 12",
      "Нет Direct3D 12",
      "Pas de Direct3D 12",
      "Kein Direct3D 12"}},
    {{"Ready to play",
      "Pronto para jogar",
      "Listo para jugar",
      "Pronto per giocare",
      "Готово к игре",
      "Prêt à jouer",
      "Bereit zum Spielen"}},
    {{"Castlevania.exe is missing",
      "Castlevania.exe não encontrado",
      "Falta Castlevania.exe",
      "Manca Castlevania.exe",
      "Нет Castlevania.exe",
      "Castlevania.exe manquant",
      "Castlevania.exe fehlt"}},
    {{"Game files needed",
      "Faltam os arquivos do jogo",
      "Faltan los archivos del juego",
      "Servono i file di gioco",
      "Нужны файлы игры",
      "Fichiers du jeu requis",
      "Spieldateien benötigt"}},
    {{"HD texture pack",
      "Pacote de texturas HD",
      "Paquete de texturas HD",
      "Pacchetto texture HD",
      "Пакет HD-текстур",
      "Pack de textures HD",
      "HD-Texturpaket"}},
    // beside the button that opens the folder: short
    {{"None in the textures folder (optional)",
      "Nenhum encontrado (opcional)",
      "Ninguno encontrado (opcional)",
      "Nessuno trovato (facoltativo)",
      "Не найден (необязательно)",
      "Aucun trouvé (facultatif)",
      "Keins gefunden (optional)"}},
    {{"{} found, in use",
      "Pacote em uso: {}",
      "Paquete en uso: {}",
      "Pacchetto in uso: {}",
      "Пакет включён: {}",
      "Pack activé : {}",
      "Paket aktiv: {}"}},
    {{"{} found, off: the game's own textures",
      "Desligado, texturas originais do jogo ({})",
      "Desactivado: texturas originales del juego ({})",
      "Disattivato: texture originali del gioco ({})",
      "Выключен: текстуры игры ({})",
      "Désactivé : textures d'origine du jeu ({})",
      "Aus: originale Texturen des Spiels ({})"}},
    {{"Open textures folder",
      "Abrir pasta textures",
      "Abrir carpeta textures",
      "Apri cartella textures",
      "Открыть папку textures",
      "Ouvrir le dossier textures",
      "Ordner textures öffnen"}},
    {{"CURRENT SETUP",
      "CONFIGURAÇÃO ATUAL",
      "CONFIGURACIÓN ACTUAL",
      "CONFIGURAZIONE ATTUALE",
      "ТЕКУЩИЕ НАСТРОЙКИ",
      "CONFIGURATION ACTUELLE",
      "AKTUELLE EINSTELLUNGEN"}},
    {{"Widescreen", "Widescreen", "Panorámico", "Widescreen", "Широкий экран", "Écran large", "Breitbild"}},
    {{"Render scale",
      "Escala de renderização",
      "Escala de renderizado",
      "Scala di rendering",
      "Масштаб рендеринга",
      "Échelle de rendu",
      "Renderskalierung"}},
    {{"Menu key",
      "Tecla do menu",
      "Tecla del menú",
      "Tasto del menu",
      "Клавиша меню",
      "Touche du menu",
      "Menütaste"}},
    {{"Desktop", "Área de trabalho", "Escritorio", "Desktop", "Рабочий стол", "Bureau", "Desktop"}},
    // a display mode, in a third of the control column and after the setting's or the tile's name: short
    {{"Fullscreen", "Tela cheia", "Completa", "Intero", "Полный", "Plein écran", "Vollbild"}},
    {{"Borderless", "Sem bordas", "Sin bordes", "Senza bordi", "Без рамки", "Sans bordure", "Rahmenlos"}},
    {{"Windowed", "Janela", "Ventana", "Finestra", "Окно", "Fenêtre", "Fenster"}},
    {{"On ({}, Hor+)",
      "Ligado ({}, Hor+)",
      "Activado ({}, Hor+)",
      "Attivo ({}, Hor+)",
      "Вкл. ({}, Hor+)",
      "Activé ({}, Hor+)",
      "An ({}, Hor+)"}},
    {{"Off (4:3)",
      "Desligado (4:3)",
      "Desactivado (4:3)",
      "Disattivo (4:3)",
      "Выкл. (4:3)",
      "Désactivé (4:3)",
      "Aus (4:3)"}},
    {{"In game, press {} or click both sticks (L3 + R3) for these settings.",
      "No jogo, aperte {} ou clique os dois analógicos (L3 + R3) para abrir estas configurações.",
      "En el juego, pulsa {} o presiona ambos sticks (L3 + R3) para abrir estos ajustes.",
      "In gioco, premi {} o entrambe le levette (L3 + R3) per queste impostazioni.",
      "В игре нажмите {} или оба стика (L3 + R3), чтобы открыть эти настройки.",
      "En jeu, appuyez sur {} ou sur les deux sticks (L3 + R3) pour ces paramètres.",
      "Im Spiel öffnen {} oder beide Sticks (L3 + R3) diese Einstellungen."}},

    // the graphics card
    {{"{}  -  not here with Direct3D 12, the game takes the fastest",
      "{}  -  não encontrada com Direct3D 12, o jogo usa a mais rápida",
      "{}  -  no está disponible con Direct3D 12, el juego usa la más rápida",
      "{}  -  non presente con Direct3D 12, il gioco usa la più veloce",
      "{}  -  не найдена с Direct3D 12, игра выберет самую быструю",
      "{}  -  absente avec Direct3D 12, le jeu prend la plus rapide",
      "{}  -  nicht mit Direct3D 12 vorhanden, das Spiel nimmt die schnellste"}},
    {{"This PC's graphics",
      "Placa de vídeo deste PC",
      "Gráficos de este PC",
      "Grafica di questo PC",
      "Графика этого ПК",
      "La carte graphique de ce PC",
      "Grafik dieses PCs"}},
    {{"{}  -  Direct3D 12 on the processor only",
      "{}  -  Direct3D 12 só no processador",
      "{}  -  Direct3D 12 solo en el procesador",
      "{}  -  Direct3D 12 solo sul processore",
      "{}  -  Direct3D 12 только на процессоре",
      "{}  -  Direct3D 12 sur le processeur seulement",
      "{}  -  Direct3D 12 nur auf dem Prozessor"}},
    {{"No graphics driver is installed, so Windows draws on the processor, far too slowly for the game. Install the "
      "driver from NVIDIA, AMD or Intel.",
      "Nenhum driver de vídeo está instalado, então o Windows desenha no processador, lento demais para o jogo. "
      "Instale o driver da NVIDIA, AMD ou Intel.",
      "No hay ningún controlador gráfico instalado, así que Windows dibuja con el procesador, demasiado lento para "
      "el juego. Instala el controlador de NVIDIA, AMD o Intel.",
      "Nessun driver grafico installato: Windows disegna con il processore, troppo lentamente per il gioco. "
      "Installa il driver di NVIDIA, AMD o Intel.",
      "Видеодрайвер не установлен, поэтому Windows рисует на процессоре — слишком медленно для игры. Установите "
      "драйвер NVIDIA, AMD или Intel.",
      "Aucun pilote graphique n'est installé : Windows dessine avec le processeur, bien trop lentement pour le jeu. "
      "Installez le pilote de NVIDIA, AMD ou Intel.",
      "Es ist kein Grafiktreiber installiert, daher zeichnet Windows auf dem Prozessor, viel zu langsam für das "
      "Spiel. Installiere den Treiber von NVIDIA, AMD oder Intel."}},
    {{"{}  -  Direct3D 12, ready",
      "{}  -  Direct3D 12, pronta",
      "{}  -  Direct3D 12, lista",
      "{}  -  Direct3D 12, pronta",
      "{}  -  Direct3D 12, готово",
      "{}  -  Direct3D 12, prête",
      "{}  -  Direct3D 12, bereit"}},
    {{"{}  -  no Direct3D 12",
      "{}  -  sem Direct3D 12",
      "{}  -  sin Direct3D 12",
      "{}  -  niente Direct3D 12",
      "{}  -  нет Direct3D 12",
      "{}  -  pas de Direct3D 12",
      "{}  -  kein Direct3D 12"}},
    {{"The game draws with Direct3D 12, which every Windows 10 and 11 has, but only graphics with a driver for it "
      "can run. Install the newest driver from NVIDIA, AMD or Intel. Graphics older than NVIDIA's GeForce 600 "
      "series, AMD's Radeon HD 7700 or Intel's HD 4200 (2012 and 2013) have none, and the game cannot start on them.",
      "O jogo usa Direct3D 12, presente em todo Windows 10 e 11, mas ele só roda em placas com um driver "
      "compatível. Instale o driver mais recente da NVIDIA, AMD ou Intel. Placas anteriores à série GeForce 600 da "
      "NVIDIA, à Radeon HD 7700 da AMD ou à HD 4200 da Intel (2012 e 2013) não têm esse driver, e o jogo não inicia "
      "nelas.",
      "El juego usa Direct3D 12, que tienen todos los Windows 10 y 11, pero solo funciona en gráficos con un "
      "controlador compatible. Instala el controlador más reciente de NVIDIA, AMD o Intel. Los gráficos anteriores "
      "a la serie GeForce 600 de NVIDIA, la Radeon HD 7700 de AMD o la HD 4200 de Intel (2012 y 2013) no lo tienen, "
      "y el juego no puede iniciarse en ellos.",
      "Il gioco usa Direct3D 12, presente in ogni Windows 10 e 11, ma funziona solo con una grafica che abbia un "
      "driver adatto. Installa il driver più recente di NVIDIA, AMD o Intel. Le schede precedenti alla serie "
      "GeForce 600 di NVIDIA, alla Radeon HD 7700 di AMD o alla HD 4200 di Intel (2012 e 2013) non ce l'hanno, e il "
      "gioco non può avviarsi.",
      "Игра рисует через Direct3D 12, который есть в любой Windows 10 и 11, но работает он только на видеокартах с "
      "подходящим драйвером. Установите новейший драйвер NVIDIA, AMD или Intel. У видеокарт старше NVIDIA GeForce "
      "600, AMD Radeon HD 7700 или Intel HD 4200 (2012–2013 годы) его нет, и игра на них не запустится.",
      "Le jeu dessine avec Direct3D 12, présent dans tout Windows 10 et 11, mais seules les cartes graphiques avec "
      "un pilote adapté le font tourner. Installez le pilote le plus récent de NVIDIA, AMD ou Intel. Les cartes "
      "antérieures aux GeForce 600 de NVIDIA, aux Radeon HD 7700 d'AMD ou aux HD 4200 d'Intel (2012 et 2013) n'en "
      "ont pas, et le jeu ne peut pas démarrer dessus.",
      "Das Spiel zeichnet mit Direct3D 12, das jedes Windows 10 und 11 hat, aber nur Grafikkarten mit passendem "
      "Treiber können es ausführen. Installiere den neuesten Treiber von NVIDIA, AMD oder Intel. Grafik älter als "
      "NVIDIAs GeForce-600-Serie, AMDs Radeon HD 7700 oder Intels HD 4200 (2012 und 2013) hat keinen, und das Spiel "
      "startet darauf nicht."}},

    // the display page
    {{"How the game window fills your screen.",
      "Como a janela do jogo ocupa a sua tela.",
      "Cómo ocupa tu pantalla la ventana del juego.",
      "Come la finestra del gioco riempie lo schermo.",
      "Как окно игры заполняет экран.",
      "Comment la fenêtre du jeu remplit votre écran.",
      "Wie das Spielfenster deinen Bildschirm füllt."}},
    {{"Display mode",
      "Modo de tela",
      "Modo de pantalla",
      "Modalità schermo",
      "Режим экрана",
      "Mode d'affichage",
      "Anzeigemodus"}},
    {{"Borderless fills the monitor without changing its mode and switches away instantly.",
      "Sem bordas ocupa o monitor sem mudar a resolução e troca de janela na hora.",
      "Sin bordes ocupa el monitor sin cambiar su modo y permite cambiar de ventana al instante.",
      "Senza bordi riempie il monitor senza cambiarne la modalità e passa ad altre finestre all'istante.",
      "Без рамки заполняет монитор, не меняя его режим, и мгновенно переключается на другие окна.",
      "Sans bordure remplit l'écran sans changer son mode et passe instantanément à une autre fenêtre.",
      "Rahmenlos füllt den Monitor, ohne seinen Modus zu ändern, und wechselt sofort zu anderen Fenstern."}},
    {{"Resolution", "Resolução", "Resolución", "Risoluzione", "Разрешение", "Résolution", "Auflösung"}},
    {{"The output size. Fullscreen switches the monitor to it; borderless always fills the monitor.",
      "O tamanho da imagem. Tela cheia muda o monitor para ela; sem bordas sempre ocupa o monitor inteiro.",
      "El tamaño de salida. Completa cambia el monitor a él; sin bordes siempre ocupa todo el monitor.",
      "La dimensione dell'immagine. Schermo intero porta il monitor a questa risoluzione; senza bordi riempie "
      "sempre il monitor.",
      "Размер изображения. Полный экран переключает монитор на него; без рамки всегда заполняет монитор.",
      "La taille de l'image. Le plein écran y passe le moniteur ; sans bordure remplit toujours le moniteur.",
      "Die Ausgabegröße. Vollbild stellt den Monitor darauf um; rahmenlos füllt immer den Monitor."}},
    {{"Off can tear and lowers input latency a little. The game runs at 60 FPS either way.",
      "Desligado pode cortar a imagem e reduz um pouco a latência. O jogo roda a 60 FPS de qualquer forma.",
      "Desactivado puede cortar la imagen y reduce un poco la latencia. El juego va a 60 FPS de todos modos.",
      "Disattivato può causare tearing e riduce un po' la latenza. Il gioco gira a 60 FPS in ogni caso.",
      "Без него возможны разрывы кадра, а задержка ввода чуть ниже. Игра в любом случае идёт с 60 FPS.",
      "Désactivée, l'image peut se déchirer et la latence baisse un peu. Le jeu tourne à 60 FPS dans les deux cas.",
      "Aus kann Bildrisse verursachen und senkt die Eingabeverzögerung etwas. Das Spiel läuft so oder so mit 60 FPS."}},
    {{"On a display with HDR on in Windows: fire, magic, light and the other brightest parts of the picture shine "
      "brighter than the rest. Off sends the usual picture.",
      "Em uma tela com HDR ativado no Windows: fogo, magia, luz e as outras partes mais claras da imagem brilham mais "
      "que o resto. Desligado envia a imagem de sempre.",
      "En una pantalla con HDR activado en Windows: el fuego, la magia, la luz y las demás partes más claras de la "
      "imagen brillan más que el resto. Desactivado envía la imagen de siempre.",
      "Su uno schermo con l'HDR attivo in Windows: fuoco, magia, luce e le altre parti più chiare dell'immagine "
      "brillano più del resto. Disattivato invia l'immagine di sempre.",
      "На экране с включённым в Windows HDR огонь, магия, свет и другие самые яркие части изображения светятся ярче "
      "остального. Выключено — обычное изображение.",
      "Sur un écran avec le HDR activé dans Windows : le feu, la magie, la lumière et les autres parties les plus "
      "claires de l'image brillent plus que le reste. Désactivé envoie l'image habituelle.",
      "Auf einem Bildschirm mit in Windows eingeschaltetem HDR leuchten Feuer, Magie, Licht und die anderen hellsten "
      "Teile des Bildes heller als der Rest. Aus sendet das gewohnte Bild."}},
    {{"Peak brightness", "Brilho máximo", "Brillo máximo", "Luminosità di picco", "Пиковая яркость",
      "Luminosité maximale", "Spitzenhelligkeit"}},
    {{"The brightest highlights. The display's own peak suits most; lower it if highlights glare.",
      "Os realces mais claros. O pico da própria tela serve na maioria dos casos; diminua se os realces ofuscarem.",
      "Los brillos más intensos. El pico de la propia pantalla sirve en la mayoría de los casos; bájalo si los brillos "
      "deslumbran.",
      "Le luci più intense. Il picco dello schermo va bene quasi sempre; abbassalo se le luci abbagliano.",
      "Самые яркие блики. Обычно подходит пик самого экрана; уменьшите, если блики слепят.",
      "Les reflets les plus clairs. Le pic de l'écran convient le plus souvent ; baissez-le si les reflets éblouissent.",
      "Die hellsten Lichter. Die Spitze des Bildschirms passt meist; senken, wenn Lichter blenden."}},
    {{"Scene brightness", "Brilho da cena", "Brillo de la escena", "Luminosità della scena", "Яркость сцены",
      "Luminosité de la scène", "Szenenhelligkeit"}},
    {{"How bright the picture is apart from its highlights. 200 nits suits most rooms; more for a bright room, less for "
      "a dark one.",
      "O quanto a imagem é clara fora os realces. 200 nits serve para a maioria dos ambientes; mais para um ambiente "
      "claro, menos para um escuro.",
      "Lo clara que es la imagen aparte de sus brillos. 200 nits sirve para la mayoría de las habitaciones; más para "
      "una habitación clara, menos para una oscura.",
      "Quanto è luminosa l'immagine a parte le luci. 200 nit vanno bene per quasi tutte le stanze; di più per una "
      "stanza luminosa, di meno per una buia.",
      "Насколько ярко изображение помимо бликов. 200 нит подходят для большинства комнат; больше — для светлой, "
      "меньше — для тёмной.",
      "La clarté de l'image hors reflets. 200 nits conviennent à la plupart des pièces ; plus pour une pièce claire, "
      "moins pour une pièce sombre.",
      "Wie hell das Bild abgesehen von den Lichtern ist. 200 Nits passen für die meisten Räume; mehr für einen hellen "
      "Raum, weniger für einen dunklen."}},
    {{"{} nits", "{} nits", "{} nits", "{} nit", "{} нит", "{} nits", "{} Nits"}},
    {{"Material maps", "Mapas de material", "Mapas de material", "Mappe dei materiali", "Карты материалов",
      "Cartes de matériaux", "Materialkarten"}},
    {{"A pack's normal and ORM maps (its materials folder) shade its pictures: relief that catches the light, darker "
      "crevices, shine on metal.",
      "Os mapas normais e ORM de um pacote (a pasta materials) sombreiam suas imagens: relevo que pega a luz, frestas "
      "mais escuras, brilho no metal.",
      "Los mapas normales y ORM de un paquete (su carpeta materials) sombrean sus imágenes: relieve que capta la luz, "
      "grietas más oscuras, brillo en el metal.",
      "Le mappe normali e ORM di un pacchetto (la sua cartella materials) ombreggiano le sue immagini: rilievi che "
      "prendono la luce, fessure più scure, riflessi sul metallo.",
      "Карты нормалей и ORM пакета (его папка materials) затеняют его изображения: рельеф ловит свет, щели темнее, "
      "металл блестит.",
      "Les cartes normales et ORM d'un pack (son dossier materials) ombrent ses images : du relief qui accroche la "
      "lumière, des creux plus sombres, des reflets sur le métal.",
      "Die Normal- und ORM-Maps eines Pakets (sein Ordner materials) schattieren seine Bilder: Relief, das das Licht "
      "fängt, dunklere Fugen, Glanz auf Metall."}},
    {{"the display's", "o da tela", "el de la pantalla", "quello dello schermo", "как у экрана", "celui de l'écran",
      "der des Bildschirms"}},
    {{"This display: HDR on in Windows, peak {} nits",
      "Esta tela: HDR ativado no Windows, pico de {} nits",
      "Esta pantalla: HDR activado en Windows, pico de {} nits",
      "Questo schermo: HDR attivo in Windows, picco di {} nit",
      "Этот экран: HDR включён в Windows, пик {} нит",
      "Cet écran : HDR activé dans Windows, pic de {} nits",
      "Dieser Bildschirm: HDR in Windows an, Spitze {} Nits"}},
    {{"This display: HDR off in Windows",
      "Esta tela: HDR desativado no Windows",
      "Esta pantalla: HDR desactivado en Windows",
      "Questo schermo: HDR disattivato in Windows",
      "Этот экран: HDR выключен в Windows",
      "Cet écran : HDR désactivé dans Windows",
      "Dieser Bildschirm: HDR in Windows aus"}},
    {{"For HDR in the game, turn on Use HDR in Windows' display settings.",
      "Para HDR no jogo, ative Usar HDR nas configurações de tela do Windows.",
      "Para HDR en el juego, activa Usar HDR en la configuración de pantalla de Windows.",
      "Per l'HDR nel gioco, attiva Usa HDR nelle impostazioni dello schermo di Windows.",
      "Для HDR в игре включите «Использовать HDR» в параметрах дисплея Windows.",
      "Pour le HDR dans le jeu, activez Utiliser le HDR dans les paramètres d'affichage de Windows.",
      "Für HDR im Spiel schalte HDR verwenden in den Anzeigeeinstellungen von Windows ein."}},

    // the graphics page
    {{"Picture quality and shape.",
      "Qualidade e formato da imagem.",
      "Calidad y forma de la imagen.",
      "Qualità e forma dell'immagine.",
      "Качество и форма изображения.",
      "Qualité et forme de l'image.",
      "Bildqualität und -format."}},
    {{"Graphics card",
      "Placa de vídeo",
      "Tarjeta gráfica",
      "Scheda video",
      "Видеокарта",
      "Carte graphique",
      "Grafikkarte"}},
    {{"The one the game draws with, from its next start. Automatic takes the fastest: on a laptop the dedicated "
      "GPU, not the one in the processor.",
      "A que o jogo usa, a partir da próxima vez que ele iniciar. Automático escolhe a mais rápida: num notebook, a "
      "GPU dedicada, não a do processador.",
      "La que usa el juego, desde su próximo inicio. Automático elige la más rápida: en un portátil, la GPU "
      "dedicada, no la del procesador.",
      "Quella con cui il gioco disegna, dal prossimo avvio. Automatico sceglie la più veloce: su un portatile la "
      "GPU dedicata, non quella del processore.",
      "Видеокарта для игры, со следующего запуска. Автоматически выбирается самая быстрая: на ноутбуке — "
      "дискретная, а не встроенная в процессор.",
      "Celle avec laquelle le jeu dessine, dès son prochain lancement. Automatique prend la plus rapide : sur un "
      "portable, le GPU dédié, pas celui du processeur.",
      "Die, mit der das Spiel zeichnet, ab dem nächsten Start. Automatisch nimmt die schnellste: auf einem Laptop "
      "die dedizierte GPU, nicht die im Prozessor."}},
    {{"Automatic", "Automático", "Automático", "Automatico", "Автоматически", "Automatique", "Automatisch"}},
    {{"{}  (not found)",
      "{}  (não encontrada)",
      "{}  (no encontrada)",
      "{}  (non trovata)",
      "{}  (не найдена)",
      "{}  (introuvable)",
      "{}  (nicht gefunden)"}},
    {{"Quality preset",
      "Predefinição de qualidade",
      "Calidad predefinida",
      "Preimpostazione qualità",
      "Профиль качества",
      "Préréglage de qualité",
      "Qualitätsvoreinstellung"}},
    {{"Sets render scale and texture filtering together.",
      "Ajusta juntas a escala de renderização e a filtragem de texturas.",
      "Ajusta a la vez la escala de renderizado y el filtrado de texturas.",
      "Imposta insieme la scala di rendering e il filtro delle texture.",
      "Задаёт сразу масштаб рендеринга и фильтрацию текстур.",
      "Règle ensemble l'échelle de rendu et le filtrage des textures.",
      "Stellt Renderskalierung und Texturfilterung zusammen ein."}},
    {{"Low", "Baixa", "Baja", "Bassa", "Низкое", "Basse", "Niedrig"}},
    {{"Original", "Original", "Original", "Originale", "Исходное", "Originale", "Original"}},
    {{"High", "Alta", "Alta", "Alta", "Высокое", "Haute", "Hoch"}},
    {{"Ultra", "Ultra", "Ultra", "Ultra", "Ультра", "Ultra", "Ultra"}},
    {{"More of the world at the sides, nothing stretched: 16:9 for a 16:9 screen, 16:10 for a 16:10 one (1920x1200). "
      "The HUD, menus and movies stay 4:3 in the middle.",
      "Mais do mundo nas laterais, sem esticar nada: 16:9 para uma tela 16:9, 16:10 para uma 16:10 (1920x1200). "
      "O HUD, os menus e os vídeos ficam em 4:3 no centro.",
      "Más mundo a los lados, sin estirar nada: 16:9 para una pantalla 16:9, 16:10 para una 16:10 (1920x1200). "
      "El HUD, los menús y los vídeos quedan en 4:3 en el centro.",
      "Più mondo ai lati, senza stirare nulla: 16:9 per uno schermo 16:9, 16:10 per uno 16:10 (1920x1200). "
      "L'HUD, i menu e i filmati restano in 4:3 al centro.",
      "Больше мира по бокам, ничего не растянуто: 16:9 для экрана 16:9, 16:10 для экрана 16:10 (1920x1200). "
      "HUD, меню и видеоролики остаются 4:3 по центру.",
      "Plus du monde sur les côtés, sans rien étirer : 16:9 pour un écran 16:9, 16:10 pour un écran 16:10 "
      "(1920x1200). Le HUD, les menus et les vidéos restent en 4:3 au centre.",
      "Mehr von der Welt an den Seiten, nichts gestreckt: 16:9 für einen 16:9-Bildschirm, 16:10 für einen "
      "16:10-Bildschirm (1920x1200). HUD, Menüs und Videos bleiben in der Mitte 4:3."}},
    {{"Of the output height. Above 100% supersamples for a sharper picture; below is faster on weak GPUs.",
      "Em relação à altura da imagem. Acima de 100% faz supersampling para uma imagem mais nítida; abaixo é mais "
      "rápido em GPUs fracas.",
      "Respecto a la altura de salida. Por encima del 100% hace supersampling para una imagen más nítida; por "
      "debajo es más rápido en GPU modestas.",
      "Rispetto all'altezza dell'immagine. Sopra il 100% fa supersampling per un'immagine più nitida; sotto è più "
      "veloce sulle GPU deboli.",
      "От высоты изображения. Выше 100% — суперсэмплинг для более чёткой картинки; ниже — быстрее на слабых "
      "видеокартах.",
      "Par rapport à la hauteur de l'image. Au-dessus de 100 %, suréchantillonnage pour une image plus nette ; en "
      "dessous, plus rapide sur les GPU faibles.",
      "Bezogen auf die Ausgabehöhe. Über 100 % Supersampling für ein schärferes Bild; darunter schneller auf "
      "schwachen GPUs."}},
    {{"Anisotropic filtering",
      "Filtragem anisotrópica",
      "Filtrado anisotrópico",
      "Filtro anisotropico",
      "Анизотропная фильтрация",
      "Filtrage anisotrope",
      "Anisotrope Filterung"}},
    {{"Sharper textures on floors and walls seen at an angle.",
      "Texturas mais nítidas em pisos e paredes vistos de lado.",
      "Texturas más nítidas en suelos y paredes vistos en ángulo.",
      "Texture più nitide su pavimenti e muri visti di sbieco.",
      "Более чёткие текстуры на полу и стенах под углом.",
      "Des textures plus nettes sur les sols et les murs vus de biais.",
      "Schärfere Texturen auf Böden und Wänden, die schräg zu sehen sind."}},
    {{"Off", "Desl.", "No", "No", "Выкл", "Non", "Aus"}},
    {{"Texture packs made for PCSX2 replace the game's textures: put a pack's replacement pictures in the textures "
      "folder.",
      "Pacotes de texturas feitos para o PCSX2 substituem as texturas do jogo: coloque as imagens do pacote na "
      "pasta textures.",
      "Los paquetes de texturas hechos para PCSX2 reemplazan las texturas del juego: pon las imágenes del paquete "
      "en la carpeta textures.",
      "I pacchetti di texture creati per PCSX2 sostituiscono quelle del gioco: metti le immagini del pacchetto "
      "nella cartella textures.",
      "Пакеты текстур для PCSX2 заменяют текстуры игры: положите изображения пакета в папку textures.",
      "Les packs de textures faits pour PCSX2 remplacent les textures du jeu : mettez les images d'un pack dans le "
      "dossier textures.",
      "Für PCSX2 gemachte Texturpakete ersetzen die Texturen des Spiels: Lege die Bilder eines Pakets in den Ordner "
      "textures."}},
    {{"Texture size",
      "Tamanho das texturas",
      "Tamaño de texturas",
      "Dimensione texture",
      "Размер текстур",
      "Taille des textures",
      "Texturgröße"}},
    {{"The largest side a picture is loaded at. Larger is sharper and needs more video memory and loading time.",
      "O maior lado com que uma imagem é carregada. Maior é mais nítido e exige mais memória de vídeo e tempo de "
      "carregamento.",
      "El lado más largo con el que se carga una imagen. Más grande es más nítido y necesita más memoria de vídeo y "
      "tiempo de carga.",
      "Il lato più lungo con cui viene caricata un'immagine. Più grande è più nitida e richiede più memoria video e "
      "tempo di caricamento.",
      "Наибольшая сторона, до которой загружается изображение. Больше — чётче, но нужно больше видеопамяти и "
      "времени на загрузку.",
      "Le plus grand côté auquel une image est chargée. Plus grand est plus net, mais demande plus de mémoire vidéo "
      "et de temps de chargement.",
      "Die längste Seite, mit der ein Bild geladen wird. Größer ist schärfer und braucht mehr Grafikspeicher und "
      "Ladezeit."}},
    {{"Full", "Completo", "Completo", "Piena", "Полный", "Complète", "Voll"}},
    {{"{} in the textures folder",
      "{} na pasta textures",
      "{} en la carpeta textures",
      "{} nella cartella textures",
      "{} в папке textures",
      "{} dans le dossier textures",
      "{} im Ordner textures"}},
    {{"No pack in the textures folder",
      "Nenhum pacote na pasta textures",
      "Ningún paquete en la carpeta textures",
      "Nessun pacchetto nella cartella textures",
      "В папке textures нет пакета",
      "Aucun pack dans le dossier textures",
      "Kein Paket im Ordner textures"}},
    {{"Check again",
      "Verificar de novo",
      "Volver a comprobar",
      "Controlla di nuovo",
      "Проверить снова",
      "Vérifier à nouveau",
      "Erneut prüfen"}},

    // the game page
    {{"In-game overlay and behaviour.",
      "Sobreposição e comportamento no jogo.",
      "Superposición y comportamiento en el juego.",
      "Overlay e comportamento in gioco.",
      "Наложение и поведение в игре.",
      "Affichage en jeu et comportement.",
      "Einblendungen und Verhalten im Spiel."}},
    {{"FPS counter",
      "Contador de FPS",
      "Contador de FPS",
      "Contatore FPS",
      "Счётчик FPS",
      "Compteur de FPS",
      "FPS-Anzeige"}},
    {{"Shows the frame rate in the corner.",
      "Mostra a taxa de quadros no canto.",
      "Muestra la tasa de fotogramas en la esquina.",
      "Mostra il frame rate nell'angolo.",
      "Показывает частоту кадров в углу.",
      "Affiche la fréquence d'images dans le coin.",
      "Zeigt die Bildrate in der Ecke."}},
    {{"Pause on focus loss",
      "Pausar ao perder o foco",
      "Pausar al perder el foco",
      "Pausa in secondo piano",
      "Пауза при потере фокуса",
      "Pause en arrière-plan",
      "Pause im Hintergrund"}},
    {{"Stops the game while another window is in front.",
      "Pausa o jogo enquanto outra janela estiver na frente.",
      "Detiene el juego mientras otra ventana está delante.",
      "Ferma il gioco mentre un'altra finestra è in primo piano.",
      "Останавливает игру, пока впереди другое окно.",
      "Met le jeu en pause quand une autre fenêtre est au premier plan.",
      "Hält das Spiel an, solange ein anderes Fenster im Vordergrund ist."}},
    {{"Lament of Innocence save",
      "Save de Lament of Innocence",
      "Partida de Lament of Innocence",
      "Salvataggio di Lament of Innocence",
      "Сохранение Lament of Innocence",
      "Sauvegarde de Lament of Innocence",
      "Spielstand von Lament of Innocence"}},
    {{"As the PS2 game with a Lament of Innocence save on the memory card: a Moai at the far end of the room behind "
      "the first courtyard, beside the potion, for the item list.",
      "Como no PS2 com um save de Lament of Innocence no memory card: um Moai no fundo da sala atrás do primeiro "
      "pátio, ao lado da poção, para a lista de itens.",
      "Como en PS2 con una partida de Lament of Innocence en la tarjeta de memoria: un Moai al fondo de la sala "
      "tras el primer patio, junto a la poción, para la lista de objetos.",
      "Come su PS2 con un salvataggio di Lament of Innocence sulla memory card: un Moai in fondo alla stanza dietro "
      "il primo cortile, accanto alla pozione, per l'elenco degli oggetti.",
      "Как на PS2 с сохранением Lament of Innocence на карте памяти: Моаи в дальнем конце комнаты за первым двором, "
      "рядом с зельем, для списка предметов.",
      "Comme sur PS2 avec une sauvegarde de Lament of Innocence sur la carte mémoire : un Moai au fond de la salle "
      "derrière la première cour, à côté de la potion, pour la liste des objets.",
      "Wie auf der PS2 mit einem Spielstand von Lament of Innocence auf der Memory Card: ein Moai am Ende des Raums "
      "hinter dem ersten Hof, neben dem Trank, für die Gegenstandsliste."}},
    {{"Settings menu key",
      "Tecla do menu de configurações",
      "Tecla del menú de ajustes",
      "Tasto del menu impostazioni",
      "Клавиша меню настроек",
      "Touche du menu des paramètres",
      "Taste für das Einstellungsmenü"}},
    {{"Opens the in-game settings. Pick another if F10 does nothing on your keyboard (laptops often need Fn for "
      "it). L3 + R3 on a controller always works.",
      "Abre as configurações no jogo. Escolha outra se o F10 não fizer nada no seu teclado (notebooks costumam "
      "exigir Fn). L3 + R3 no controle sempre funciona.",
      "Abre los ajustes en el juego. Elige otra si F10 no hace nada en tu teclado (los portátiles suelen necesitar "
      "Fn). L3 + R3 en el mando siempre funciona.",
      "Apre le impostazioni in gioco. Scegline un altro se F10 non fa nulla sulla tua tastiera (i portatili spesso "
      "richiedono Fn). L3 + R3 sul controller funziona sempre.",
      "Открывает настройки в игре. Выберите другую, если F10 ничего не делает на вашей клавиатуре (на ноутбуках "
      "часто нужна Fn). L3 + R3 на геймпаде работает всегда.",
      "Ouvre les paramètres en jeu. Choisissez-en une autre si F10 ne fait rien sur votre clavier (les portables "
      "demandent souvent Fn). L3 + R3 sur une manette marche toujours.",
      "Öffnet die Einstellungen im Spiel. Wähle eine andere, wenn F10 auf deiner Tastatur nichts tut (Laptops "
      "brauchen oft Fn dafür). L3 + R3 am Controller funktioniert immer."}},
    {{"Keep launcher open",
      "Manter o launcher aberto",
      "Mantener el launcher abierto",
      "Tieni aperto il launcher",
      "Не закрывать лаунчер",
      "Garder le lanceur ouvert",
      "Launcher offen lassen"}},
    {{"Leave this window open while the game runs.",
      "Deixa esta janela aberta enquanto o jogo roda.",
      "Deja esta ventana abierta mientras el juego está en marcha.",
      "Lascia aperta questa finestra mentre il gioco è in esecuzione.",
      "Оставлять это окно открытым во время игры.",
      "Laisse cette fenêtre ouverte pendant que le jeu tourne.",
      "Lässt dieses Fenster offen, während das Spiel läuft."}},

    // the controls page
    {{"Xbox, PlayStation and Switch controllers play as player 1, with rumble. The keyboard plays alongside.",
      "Controles de Xbox, PlayStation e Switch jogam como jogador 1, com vibração. O teclado funciona junto.",
      "Los mandos de Xbox, PlayStation y Switch juegan como jugador 1, con vibración. El teclado funciona a la vez.",
      "I controller Xbox, PlayStation e Switch giocano come giocatore 1, con vibrazione. La tastiera funziona "
      "insieme.",
      "Геймпады Xbox, PlayStation и Switch играют за первого игрока, с вибрацией. Клавиатура работает вместе с ними.",
      "Les manettes Xbox, PlayStation et Switch jouent en joueur 1, avec vibrations. Le clavier joue en même temps.",
      "Xbox-, PlayStation- und Switch-Controller spielen als Spieler 1, mit Vibration. Die Tastatur spielt "
      "gleichzeitig mit."}},
    {{"Button layout",
      "Layout dos botões",
      "Disposición de botones",
      "Layout dei tasti",
      "Раскладка кнопок",
      "Disposition des boutons",
      "Tastenbelegung"}},
    {{"PS2 shows the PS2's button icons and gives a controller the PS2's shoulders: L1 resets the camera, R1 "
      "guards, L2 and R2 lock on. The keyboard keeps its keys.",
      "PS2 mostra os ícones de botão do PS2 e dá ao controle os botões de ombro do PS2: L1 centraliza a câmera, R1 "
      "defende, L2 e R2 travam a mira. O teclado mantém suas teclas.",
      "PS2 muestra los iconos de botones de PS2 y da al mando los gatillos de PS2: L1 centra la cámara, R1 bloquea, "
      "L2 y R2 fijan el objetivo. El teclado mantiene sus teclas.",
      "PS2 mostra le icone dei tasti della PS2 e dà al controller i dorsali della PS2: L1 reimposta la telecamera, "
      "R1 para, L2 e R2 agganciano il bersaglio. La tastiera mantiene i suoi tasti.",
      "PS2 показывает значки кнопок PS2 и даёт геймпаду плечевые кнопки PS2: L1 сбрасывает камеру, R1 — блок, L2 и "
      "R2 — захват цели. Клавиатура сохраняет свои клавиши.",
      "PS2 affiche les icônes de boutons de la PS2 et donne à la manette les gâchettes de la PS2 : L1 recentre la "
      "caméra, R1 garde, L2 et R2 verrouillent la cible. Le clavier garde ses touches.",
      "PS2 zeigt die Tastensymbole der PS2 und gibt dem Controller die Schultertasten der PS2: L1 setzt die Kamera "
      "zurück, R1 blockt, L2 und R2 erfassen das Ziel. Die Tastatur behält ihre Tasten."}},
    {{"The PS2 icons need the HD texture pack on (Graphics)",
      "Os ícones do PS2 precisam do pacote de texturas HD ligado (Gráficos)",
      "Los iconos de PS2 necesitan el paquete de texturas HD activado (Gráficos)",
      "Le icone PS2 richiedono il pacchetto texture HD attivo (Grafica)",
      "Для значков PS2 нужен включённый пакет HD-текстур (Графика)",
      "Les icônes PS2 demandent le pack de textures HD activé (Graphismes)",
      "Die PS2-Symbole brauchen das HD-Texturpaket eingeschaltet (Grafik)"}},
    {{"PS2 icons found in the texture pack",
      "Ícones do PS2 encontrados no pacote de texturas",
      "Iconos de PS2 encontrados en el paquete de texturas",
      "Icone PS2 trovate nel pacchetto texture",
      "Значки PS2 найдены в пакете текстур",
      "Icônes PS2 trouvées dans le pack de textures",
      "PS2-Symbole im Texturpaket gefunden"}},
    {{"No PS2 icons in the textures folder: the Xbox ones stay, the shoulders change",
      "Sem ícones do PS2 na pasta textures: ficam os do Xbox, os botões de ombro mudam",
      "No hay iconos de PS2 en la carpeta textures: se quedan los de Xbox, los gatillos cambian",
      "Nessuna icona PS2 nella cartella textures: restano quelle Xbox, i dorsali cambiano",
      "В папке textures нет значков PS2: остаются значки Xbox, плечевые кнопки меняются",
      "Pas d'icônes PS2 dans le dossier textures : celles de la Xbox restent, les gâchettes changent",
      "Keine PS2-Symbole im Ordner textures: die der Xbox bleiben, die Schultertasten ändern sich"}},
    {{"Arrows", "Setas", "Flechas", "Frecce", "Стрелки", "Flèches", "Pfeiltasten"}},
    {{"K / Space", "K / Espaço", "K / Espacio", "K / Spazio", "K / Пробел", "K / Espace", "K / Leertaste"}},
    {{"Enter", "Enter", "Intro", "Invio", "Enter", "Entrée", "Enter"}},
    {{"Backspace", "Backspace", "Retroceso", "Backspace", "Backspace", "Retour arrière", "Rücktaste"}},
    {{"Move", "Mover", "Moverse", "Muoversi", "Движение", "Se déplacer", "Bewegen"}},
    {{"Camera", "Câmera", "Cámara", "Telecamera", "Камера", "Caméra", "Kamera"}},
    {{"Jump (A)", "Pular (A)", "Saltar (A)", "Salta (A)", "Прыжок (A)", "Sauter (A)", "Springen (A)"}},
    {{"Attack (X)", "Atacar (X)", "Atacar (X)", "Attacca (X)", "Атака (X)", "Attaquer (X)", "Angreifen (X)"}},
    {{"Triggers", "Gatilhos", "Gatillos", "Grilletti", "Триггеры", "Gâchettes", "Trigger"}},
    {{"White / Black",
      "Branco / Preto",
      "Blanco / Negro",
      "Bianco / Nero",
      "Белая / Чёрная",
      "Blanc / Noir",
      "Weiß / Schwarz"}},
    {{"D-pad", "Direcional", "Cruceta", "Croce direzionale", "Крестовина", "Croix directionnelle", "Steuerkreuz"}},
    {{"Settings menu",
      "Menu de configurações",
      "Menú de ajustes",
      "Menu impostazioni",
      "Меню настроек",
      "Menu des paramètres",
      "Einstellungsmenü"}},

    // the game files page
    {{"Your own copy of the game: the USA (NTSC-U) release, as a disc image or extracted.",
      "A sua própria cópia do jogo: a versão americana (NTSC-U), como imagem de disco ou extraída.",
      "Tu propia copia del juego: la versión estadounidense (NTSC-U), como imagen de disco o extraída.",
      "La tua copia del gioco: la versione USA (NTSC-U), come immagine disco o estratta.",
      "Ваша собственная копия игры: американская версия (NTSC-U), образом диска или в распакованном виде.",
      "Votre propre copie du jeu : la version américaine (NTSC-U), en image disque ou extraite.",
      "Deine eigene Kopie des Spiels: die US-Version (NTSC-U), als Disc-Image oder entpackt."}},
    {{"No assets folder found",
      "Pasta assets não encontrada",
      "No se encontró la carpeta assets",
      "Cartella assets non trovata",
      "Папка assets не найдена",
      "Dossier assets introuvable",
      "Kein Ordner assets gefunden"}},
    {{"{}  -  USA release, ready",
      "{}  -  versão americana, pronta",
      "{}  -  versión de EE. UU., lista",
      "{}  -  versione USA, pronta",
      "{}  -  американская версия, готово",
      "{}  -  version américaine, prête",
      "{}  -  US-Version, bereit"}},
    {{"{}  -  USA release (patched header), ready",
      "{}  -  versão americana (cabeçalho modificado), pronta",
      "{}  -  versión de EE. UU. (cabecera modificada), lista",
      "{}  -  versione USA (intestazione modificata), pronta",
      "{}  -  американская версия (изменённый заголовок), готово",
      "{}  -  version américaine (en-tête modifié), prête",
      "{}  -  US-Version (geänderter Header), bereit"}},
    {{"{}  -  another release, only the USA one works",
      "{}  -  outra versão, só a americana funciona",
      "{}  -  otra versión, solo funciona la de EE. UU.",
      "{}  -  un'altra versione, funziona solo quella USA",
      "{}  -  другая версия, работает только американская",
      "{}  -  une autre version, seule l'américaine marche",
      "{}  -  andere Version, nur die US-Version läuft"}},
    {{"{}  -  not extracted",
      "{}  -  não extraído",
      "{}  -  no extraído",
      "{}  -  non estratto",
      "{}  -  не распакован",
      "{}  -  non extrait",
      "{}  -  nicht entpackt"}},
    {{"{}  -  not an Xbox disc image",
      "{}  -  não é uma imagem de disco do Xbox",
      "{}  -  no es una imagen de disco de Xbox",
      "{}  -  non è un'immagine disco Xbox",
      "{}  -  не образ диска Xbox",
      "{}  -  pas une image disque Xbox",
      "{}  -  kein Xbox-Disc-Image"}},
    {{"Castlevania.exe is not beside the launcher",
      "Castlevania.exe não está ao lado do launcher",
      "Castlevania.exe no está junto al launcher",
      "Castlevania.exe non è accanto al launcher",
      "Castlevania.exe нет рядом с лаунчером",
      "Castlevania.exe n'est pas à côté du lanceur",
      "Castlevania.exe liegt nicht neben dem Launcher"}},
    {{"Add your disc image (.iso), or extract the disc into the assets folder: default.xbe with its data, movie and "
      "sound folders.",
      "Adicione a sua imagem de disco (.iso) ou extraia o disco na pasta assets: o default.xbe com as pastas data, "
      "movie e sound.",
      "Añade tu imagen de disco (.iso) o extrae el disco en la carpeta assets: default.xbe con sus carpetas data, "
      "movie y sound.",
      "Aggiungi la tua immagine disco (.iso) o estrai il disco nella cartella assets: default.xbe con le cartelle "
      "data, movie e sound.",
      "Добавьте свой образ диска (.iso) или распакуйте диск в папку assets: default.xbe с папками data, movie и "
      "sound.",
      "Ajoutez votre image disque (.iso), ou extrayez le disque dans le dossier assets : default.xbe avec ses "
      "dossiers data, movie et sound.",
      "Füge dein Disc-Image (.iso) hinzu oder entpacke die Disc in den Ordner assets: default.xbe mit seinen "
      "Ordnern data, movie und sound."}},
    {{"Copying disc image...",
      "Copiando a imagem de disco...",
      "Copiando la imagen de disco...",
      "Copia dell'immagine disco...",
      "Копирование образа диска...",
      "Copie de l'image disque...",
      "Disc-Image wird kopiert..."}},
    {{"Add disc image",
      "Adicionar imagem de disco",
      "Añadir imagen de disco",
      "Aggiungi immagine disco",
      "Добавить образ диска",
      "Ajouter une image disque",
      "Disc-Image hinzufügen"}},
    {{"Open folder",
      "Abrir pasta",
      "Abrir carpeta",
      "Apri cartella",
      "Открыть папку",
      "Ouvrir le dossier",
      "Ordner öffnen"}},
    {{"Saves and logs",
      "Saves e logs",
      "Partidas y registros",
      "Salvataggi e log",
      "Сохранения и журналы",
      "Sauvegardes et journaux",
      "Spielstände und Logs"}},
    {{"Saves are kept beside the game in hdd\\E\\UDATA\\4b4e002d.",
      "Os saves ficam ao lado do jogo em hdd\\E\\UDATA\\4b4e002d.",
      "Las partidas se guardan junto al juego en hdd\\E\\UDATA\\4b4e002d.",
      "I salvataggi sono accanto al gioco in hdd\\E\\UDATA\\4b4e002d.",
      "Сохранения лежат рядом с игрой в hdd\\E\\UDATA\\4b4e002d.",
      "Les sauvegardes sont à côté du jeu dans hdd\\E\\UDATA\\4b4e002d.",
      "Spielstände liegen neben dem Spiel in hdd\\E\\UDATA\\4b4e002d."}},
    {{"Open saves",
      "Abrir saves",
      "Abrir partidas",
      "Apri salvataggi",
      "Открыть сохранения",
      "Ouvrir les sauvegardes",
      "Spielstände öffnen"}},
    {{"Open logs",
      "Abrir logs",
      "Abrir registros",
      "Apri log",
      "Открыть журналы",
      "Ouvrir les journaux",
      "Logs öffnen"}},
    {{"Xbox disc image (*.iso)",
      "Imagem de disco do Xbox (*.iso)",
      "Imagen de disco de Xbox (*.iso)",
      "Immagine disco Xbox (*.iso)",
      "Образ диска Xbox (*.iso)",
      "Image disque Xbox (*.iso)",
      "Xbox-Disc-Image (*.iso)"}},
    {{"All files",
      "Todos os arquivos",
      "Todos los archivos",
      "Tutti i file",
      "Все файлы",
      "Tous les fichiers",
      "Alle Dateien"}},
    {{"Your Castlevania: Curse of Darkness disc image",
      "A sua imagem de disco de Castlevania: Curse of Darkness",
      "Tu imagen de disco de Castlevania: Curse of Darkness",
      "La tua immagine disco di Castlevania: Curse of Darkness",
      "Ваш образ диска Castlevania: Curse of Darkness",
      "Votre image disque de Castlevania: Curse of Darkness",
      "Dein Disc-Image von Castlevania: Curse of Darkness"}},
    {{"That image is already in the assets folder",
      "Essa imagem já está na pasta assets",
      "Esa imagen ya está en la carpeta assets",
      "Questa immagine è già nella cartella assets",
      "Этот образ уже в папке assets",
      "Cette image est déjà dans le dossier assets",
      "Dieses Image ist schon im Ordner assets"}},
    {{"Linked {} into assets.",
      "{} vinculado na pasta assets.",
      "{} vinculado en la carpeta assets.",
      "{} collegato nella cartella assets.",
      "{}: ссылка создана в папке assets.",
      "{} lié dans le dossier assets.",
      "{} im Ordner assets verknüpft."}},
    {{"Copied {} into assets.",
      "{} copiado para a pasta assets.",
      "{} copiado en la carpeta assets.",
      "{} copiato nella cartella assets.",
      "{} скопирован в папку assets.",
      "{} copié dans le dossier assets.",
      "{} in den Ordner assets kopiert."}},
    {{"Could not copy the disc image (error {}).",
      "Não foi possível copiar a imagem de disco (erro {}).",
      "No se pudo copiar la imagen de disco (error {}).",
      "Impossibile copiare l'immagine disco (errore {}).",
      "Не удалось скопировать образ диска (ошибка {}).",
      "Impossible de copier l'image disque (erreur {}).",
      "Disc-Image konnte nicht kopiert werden (Fehler {})."}},
    // the view and the camera (game/view.cpp, keyboard.cpp, widescreen.cpp)
    {{"HUD at the screen edges",
      "HUD nas bordas da tela",
      "HUD en los bordes de la pantalla",
      "HUD ai bordi dello schermo",
      "HUD по краям экрана",
      "HUD aux bords de l'écran",
      "HUD an den Bildschirmrändern"}},
    {{"Widescreen: the health bar and the rest of the HUD in the screen's corners rather than at the corners of the "
      "4:3 middle.",
      "Widescreen: a barra de vida e o resto do HUD nos cantos da tela, em vez dos cantos da área 4:3 do meio.",
      "Panorámico: la barra de vida y el resto del HUD en las esquinas de la pantalla, en lugar de las esquinas de la "
      "zona 4:3 central.",
      "Widescreen: la barra della salute e il resto dell'HUD negli angoli dello schermo, invece che negli angoli "
      "dell'area 4:3 centrale.",
      "Широкий экран: полоса здоровья и остальной HUD в углах экрана, а не в углах центральной области 4:3.",
      "Écran large : la barre de vie et le reste du HUD dans les coins de l'écran, plutôt qu'aux coins de la zone 4:3 "
      "du milieu.",
      "Breitbild: der Lebensbalken und das übrige HUD in den Ecken des Bildschirms statt in den Ecken der 4:3-Mitte."}},
    {{"Field of view", "Campo de visão", "Campo de visión", "Campo visivo", "Поле зрения", "Champ de vision", "Sichtfeld"}},
    {{"How much of the world the camera takes in: 100% is the game's own, which each room's camera sets; more is "
      "wider. Story scenes keep the game's.",
      "Quanto do mundo a câmera mostra: 100% é o do próprio jogo, definido pela câmera de cada sala; mais é mais "
      "amplo. As cenas da história mantêm o do jogo.",
      "Cuánto del mundo abarca la cámara: 100% es el del propio juego, que fija la cámara de cada sala; más es más "
      "amplio. Las escenas de la historia conservan el del juego.",
      "Quanto del mondo inquadra la telecamera: 100% è quello del gioco, impostato dalla telecamera di ogni stanza; di "
      "più è più ampio. Le scene della storia mantengono quello del gioco.",
      "Сколько мира охватывает камера: 100% — как в игре, где его задаёт камера каждой комнаты; больше — шире. "
      "Сюжетные сцены сохраняют игровое значение.",
      "La part du monde que la caméra embrasse : 100 % est celui du jeu, fixé par la caméra de chaque salle ; plus est "
      "plus large. Les scènes de l'histoire gardent celui du jeu.",
      "Wie viel von der Welt die Kamera erfasst: 100 % ist der des Spiels, den die Kamera jedes Raums setzt; mehr ist "
      "weiter. Story-Szenen behalten den des Spiels."}},
    {{"the game's", "o do jogo", "el del juego", "quello del gioco", "как в игре", "celui du jeu", "der des Spiels"}},
    {{"Far view", "Visão distante", "Vista lejana", "Vista lontana", "Дальний обзор", "Vue lointaine", "Fernsicht"}},
    {{"The fog and the draw distance both 4x as far out. Off puts both back to the game's.",
      "A névoa e a distância de visão, ambas 4x mais longe. Desligado volta as duas às do jogo.",
      "La niebla y la distancia de dibujado, ambas 4x más lejos. Desactivado devuelve las dos a las del juego.",
      "La nebbia e la distanza visiva, entrambe 4x più lontane. Disattivato le riporta entrambe a quelle del gioco.",
      "Туман и дальность прорисовки — обе в 4 раза дальше. Выключено — обе как в игре.",
      "Le brouillard et la distance d'affichage, tous deux 4x plus loin. Désactivé les remet tous deux à ceux du jeu.",
      "Nebel und Sichtweite beide 4x so weit draußen. Aus setzt beide auf die des Spiels zurück."}},
    {{"Draw distance", "Distância de visão", "Distancia de dibujado", "Distanza visiva", "Дальность прорисовки",
      "Distance d'affichage", "Sichtweite"}},
    {{"How far the world is drawn. Farther shows more once there is less fog, and may show where a room's scenery "
      "ends.",
      "Até onde o mundo é desenhado. Mais longe mostra mais quando há menos névoa, e pode mostrar onde termina o cenário "
      "de uma sala.",
      "Hasta dónde se dibuja el mundo. Más lejos muestra más cuando hay menos niebla, y puede mostrar dónde termina el "
      "escenario de una sala.",
      "Fin dove viene disegnato il mondo. Più lontano mostra di più quando c'è meno nebbia, e può mostrare dove finisce "
      "lo scenario di una stanza.",
      "Как далеко отрисовывается мир. Дальше — видно больше, когда тумана меньше, но может стать видно, где "
      "заканчиваются декорации комнаты.",
      "Jusqu'où le monde est dessiné. Plus loin en montre davantage quand il y a moins de brouillard, et peut montrer "
      "où s'arrête le décor d'une salle.",
      "Wie weit die Welt gezeichnet wird. Weiter zeigt mehr, sobald weniger Nebel da ist, und kann zeigen, wo die "
      "Kulisse eines Raums endet."}},
    {{"Fog", "Névoa", "Niebla", "Nebbia", "Туман", "Brouillard", "Nebel"}},
    {{"The fog and darkness that hide the distance in most rooms: as far out as the game puts it, farther, or none.",
      "A névoa e a escuridão que escondem a distância na maioria das salas: onde o jogo as põe, mais longe, ou nenhuma.",
      "La niebla y la oscuridad que ocultan la distancia en la mayoría de las salas: donde las pone el juego, más "
      "lejos, o ninguna.",
      "La nebbia e il buio che nascondono la distanza nella maggior parte delle stanze: dove li mette il gioco, più "
      "lontano, o nessuna.",
      "Туман и темнота, скрывающие даль в большинстве комнат: как в игре, дальше или без них.",
      "Le brouillard et l'obscurité qui cachent le lointain dans la plupart des salles : là où le jeu les met, plus "
      "loin, ou aucun.",
      "Der Nebel und die Dunkelheit, die in den meisten Räumen die Ferne verbergen: so weit draußen wie im Spiel, "
      "weiter oder gar keine."}},
    {{"None", "Nenhuma", "Ninguna", "Nessuna", "Нет", "Aucun", "Keiner"}},
    {{"Stick response", "Resposta do analógico", "Respuesta del stick", "Risposta della levetta", "Отклик стика",
      "Réponse du stick", "Stick-Verhalten"}},
    {{"PS2: Hector walks any way the stick points, from a slow walk up, as on the PS2. Xbox: the Xbox release's, "
      "where a stick near up, down, left or right walks straight along it.",
      "PS2: Hector anda para onde o analógico aponta, a partir de um passo lento, como no PS2. Xbox: o da versão de "
      "Xbox, em que o analógico perto de cima, baixo, esquerda ou direita anda reto nessa direção.",
      "PS2: Hector camina hacia donde apunta el stick, desde un paso lento, como en PS2. Xbox: el de la versión de "
      "Xbox, donde un stick cerca de arriba, abajo, izquierda o derecha camina recto en esa dirección.",
      "PS2: Hector cammina dove punta la levetta, a partire da un passo lento, come su PS2. Xbox: quello della versione "
      "Xbox, dove una levetta vicina a su, giù, sinistra o destra cammina dritta in quella direzione.",
      "PS2: Гектор идёт туда, куда наклонён стик, начиная с медленного шага, как на PS2. Xbox: как в версии для Xbox, "
      "где стик рядом с направлением вверх, вниз, влево или вправо ведёт строго по нему.",
      "PS2 : Hector marche là où pointe le stick, dès un pas lent, comme sur PS2. Xbox : celui de la version Xbox, où un "
      "stick proche du haut, du bas, de la gauche ou de la droite marche tout droit dans cette direction.",
      "PS2: Hector geht dorthin, wohin der Stick zeigt, ab einem langsamen Schritt, wie auf der PS2. Xbox: das der "
      "Xbox-Fassung, bei der ein Stick nahe oben, unten, links oder rechts gerade in diese Richtung geht."}},
    {{"Invert camera left / right",
      "Inverter câmera esquerda / direita",
      "Invertir cámara izquierda / derecha",
      "Inverti telecamera sinistra / destra",
      "Инвертировать камеру влево / вправо",
      "Inverser la caméra gauche / droite",
      "Kamera links / rechts umkehren"}},
    {{"The right stick and the arrow keys turn the camera the other way.",
      "O analógico direito e as setas giram a câmera para o outro lado.",
      "El stick derecho y las flechas giran la cámara hacia el otro lado.",
      "La levetta destra e le frecce ruotano la telecamera dall'altra parte.",
      "Правый стик и стрелки поворачивают камеру в другую сторону.",
      "Le stick droit et les flèches tournent la caméra dans l'autre sens.",
      "Der rechte Stick und die Pfeiltasten drehen die Kamera in die andere Richtung."}},
    {{"Invert camera up / down",
      "Inverter câmera cima / baixo",
      "Invertir cámara arriba / abajo",
      "Inverti telecamera su / giù",
      "Инвертировать камеру вверх / вниз",
      "Inverser la caméra haut / bas",
      "Kamera oben / unten umkehren"}},
    {{"The right stick and the arrow keys move the camera the other way up and down.",
      "O analógico direito e as setas movem a câmera para o outro lado, para cima e para baixo.",
      "El stick derecho y las flechas mueven la cámara al revés, arriba y abajo.",
      "La levetta destra e le frecce muovono la telecamera al contrario, su e giù.",
      "Правый стик и стрелки двигают камеру вверх и вниз в обратную сторону.",
      "Le stick droit et les flèches déplacent la caméra dans l'autre sens, en haut et en bas.",
      "Der rechte Stick und die Pfeiltasten bewegen die Kamera umgekehrt nach oben und unten."}},
};

// every row has each language, with as many {} as its English
consteval bool Complete() {
    auto args = [](std::string_view s) {
        size_t n = 0;
        for (size_t i = 0; i + 1 < s.size(); i++) n += s[i] == '{' && s[i + 1] == '}';
        return n;
    };
    for (const Row& r : kRows)
        for (int l = 0; l < kLangs; l++)
            if (!r.text[l] || args(r.text[l]) != args(r.text[kEn])) return false;
    return true;
}
static_assert(Complete(), "a row of words.h lacks a language, or a language's {} differ from its English");

// a phrase's row, found while compiling
consteval size_t Find(std::string_view en) {
    for (size_t i = 0; i < std::size(kRows); i++)
        if (std::string_view(kRows[i].text[kEn]) == en) return i;
    throw "this phrase has no row in words.h";
}

struct Key {
    size_t row;
    consteval Key(const char* en) : row(Find(en)) {}
};

// the language shown; the disc image copy's thread reads it too
inline std::atomic<int> g_lang{kEn};

// by its code, English for one not known
inline void Set(const std::string& code) {
    int lang = kEn;
    for (int i = 0; i < kLangs; i++)
        if (code == kLanguages[i].code) lang = i;
    g_lang = lang;
}

inline int Current() { return g_lang; }

inline const char* T(Key k) { return kRows[k.row].text[g_lang]; }

// the phrase with each {} replaced by the next argument
inline std::string F(Key phrase, std::initializer_list<std::string_view> args) {
    std::string_view s = T(phrase);
    std::string out;
    auto arg = args.begin();
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '{' && i + 1 < s.size() && s[i + 1] == '}' && arg != args.end()) {
            out += *arg++;
            i++;
        } else {
            out += s[i];
        }
    }
    return out;
}

// "1 picture", "3844 pictures". Russian has a form for 1, 21, 31..., one for 2 to 4, 22 to 24... and one for the rest;
// French counts 0 as one.
inline std::string Pictures(size_t n) {
    static const char* const kForms[kLangs][3] = {
        {"picture", "pictures", "pictures"},     {"imagem", "imagens", "imagens"},
        {"imagen", "imágenes", "imágenes"},      {"immagine", "immagini", "immagini"},
        {"изображение", "изображения", "изображений"},
        {"image", "images", "images"},           {"Bild", "Bilder", "Bilder"},
    };
    int form = n == 1 || (n == 0 && g_lang == kFr) ? 0 : 2;
    if (g_lang == kRu) {
        size_t d = n % 10, dd = n % 100;
        form = d == 1 && dd != 11 ? 0 : d >= 2 && d <= 4 && (dd < 12 || dd > 14) ? 1 : 2;
    }
    return std::to_string(n) + " " + kForms[g_lang][form];
}

}  // namespace words
