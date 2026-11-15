#include "i18n.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace {

// Column order of the translation table below.
enum Lang : int { EN, DE, FR, ES, IT, PT, NL, PL, RU, TR, CS, ZH, LANG_COUNT };

const std::vector<UiLanguage> kLanguages = {
    { "en", L"English" },
    { "de", L"Deutsch" },
    { "fr", L"Français" },
    { "es", L"Español" },
    { "it", L"Italiano" },
    { "pt", L"Português" },
    { "nl", L"Nederlands" },
    { "pl", L"Polski" },
    { "ru", L"Русский" },
    { "tr", L"Türkçe" },
    { "cs", L"Čeština" },
    { "zh", L"中文" },
};

struct Row {
    Str id;
    std::array<const wchar_t*, LANG_COUNT> text;
};

// Keep texts short: buttons and rows have fixed widths.
// en, de, fr, es, it, pt, nl, pl, ru, tr, cs, zh
const Row kRows[] = {
    { Str::NavSource, { L"Source", L"Quelle", L"Source", L"Fuente", L"Sorgente", L"Fonte", L"Bron", L"Źródło", L"Источник", L"Kaynak", L"Zdroj", L"音源" } },
    { Str::NavMix, { L"Mix", L"Mix", L"Mixage", L"Mezcla", L"Mix", L"Mixagem", L"Mix", L"Miks", L"Микс", L"Miks", L"Mix", L"混音" } },
    { Str::NavHotkeys, { L"Hotkeys", L"Tastenkürzel", L"Raccourcis", L"Atajos", L"Scorciatoie", L"Atalhos", L"Sneltoetsen", L"Skróty", L"Горячие клавиши", L"Kısayollar", L"Zkratky", L"快捷键" } },
    { Str::NavAppearance, { L"Appearance", L"Darstellung", L"Apparence", L"Apariencia", L"Aspetto", L"Aparência", L"Weergave", L"Wygląd", L"Оформление", L"Görünüm", L"Vzhled", L"外观" } },
    { Str::NavDiagnostics, { L"Diagnostics", L"Diagnose", L"Diagnostic", L"Diagnóstico", L"Diagnostica", L"Diagnóstico", L"Diagnose", L"Diagnostyka", L"Диагностика", L"Tanılama", L"Diagnostika", L"诊断" } },
    { Str::MiniModeButton, { L"Mini mode", L"Mini-Modus", L"Mode mini", L"Modo mini", L"Modalità mini", L"Modo mini", L"Minimodus", L"Tryb mini", L"Мини-режим", L"Mini mod", L"Mini režim", L"迷你模式" } },

    { Str::StatusSending, { L"MicMix is sending music", L"MicMix sendet Musik", L"MicMix diffuse la musique", L"MicMix está enviando música", L"MicMix sta inviando musica", L"MicMix está enviando música", L"MicMix verzendt muziek", L"MicMix wysyła muzykę", L"MicMix передаёт музыку", L"MicMix müzik gönderiyor", L"MicMix odesílá hudbu", L"MicMix 正在发送音乐" } },
    { Str::StatusMuted, { L"Music is muted", L"Musik ist stummgeschaltet", L"La musique est coupée", L"La música está silenciada", L"La musica è disattivata", L"A música está silenciada", L"Muziek is gedempt", L"Muzyka jest wyciszona", L"Музыка отключена", L"Müzik sessizde", L"Hudba je ztlumená", L"音乐已静音" } },
    { Str::StatusError, { L"Source error", L"Fehler bei der Quelle", L"Erreur de source", L"Error de la fuente", L"Errore della sorgente", L"Erro na fonte", L"Bronfout", L"Błąd źródła", L"Ошибка источника", L"Kaynak hatası", L"Chyba zdroje", L"音源错误" } },
    { Str::StatusOff, { L"MicMix is off", L"MicMix ist aus", L"MicMix est désactivé", L"MicMix está apagado", L"MicMix è spento", L"MicMix está desligado", L"MicMix staat uit", L"MicMix jest wyłączony", L"MicMix выключен", L"MicMix kapalı", L"MicMix je vypnutý", L"MicMix 已关闭" } },
    { Str::StatusOffHint, { L"Choose a source, then switch MicMix on", L"Quelle wählen, dann MicMix einschalten", L"Choisissez une source, puis activez MicMix", L"Elige una fuente y activa MicMix", L"Scegli una sorgente, poi attiva MicMix", L"Escolha uma fonte e ligue o MicMix", L"Kies een bron en zet MicMix aan", L"Wybierz źródło, a potem włącz MicMix", L"Выберите источник и включите MicMix", L"Bir kaynak seçin, sonra MicMix'i açın", L"Vyberte zdroj a zapněte MicMix", L"选择音源，然后打开 MicMix" } },
    { Str::StatusReadyPrefix, { L"Ready: ", L"Bereit: ", L"Prêt : ", L"Listo: ", L"Pronto: ", L"Pronto: ", L"Klaar: ", L"Gotowe: ", L"Готово: ", L"Hazır: ", L"Připraveno: ", L"就绪：" } },
    { Str::NoSourceSelected, { L"No source selected", L"Keine Quelle gewählt", L"Aucune source choisie", L"Ninguna fuente elegida", L"Nessuna sorgente scelta", L"Nenhuma fonte escolhida", L"Geen bron gekozen", L"Nie wybrano źródła", L"Источник не выбран", L"Kaynak seçilmedi", L"Není vybrán zdroj", L"未选择音源" } },
    { Str::DefaultMicrophone, { L"default microphone", L"Standardmikrofon", L"micro par défaut", L"micrófono predeterminado", L"microfono predefinito", L"microfone padrão", L"standaardmicrofoon", L"mikrofon domyślny", L"микрофон по умолчанию", L"varsayılan mikrofon", L"výchozí mikrofon", L"默认麦克风" } },
    { Str::SourceErrorFallback, { L"The source stopped unexpectedly", L"Die Quelle wurde unerwartet beendet", L"La source s'est arrêtée de façon inattendue", L"La fuente se detuvo inesperadamente", L"La sorgente si è interrotta inaspettatamente", L"A fonte parou inesperadamente", L"De bron is onverwacht gestopt", L"Źródło nieoczekiwanie się zatrzymało", L"Источник неожиданно остановился", L"Kaynak beklenmedik şekilde durdu", L"Zdroj se nečekaně zastavil", L"音源意外停止" } },
    { Str::PowerOn, { L"On", L"An", L"Activé", L"Activado", L"Attivo", L"Ligado", L"Aan", L"Wł.", L"Вкл.", L"Açık", L"Zap.", L"开" } },
    { Str::PowerOff, { L"Off", L"Aus", L"Désactivé", L"Desactivado", L"Spento", L"Desligado", L"Uit", L"Wył.", L"Выкл.", L"Kapalı", L"Vyp.", L"关" } },

    { Str::RowAudioSource, { L"Audio source", L"Audioquelle", L"Source audio", L"Fuente de audio", L"Sorgente audio", L"Fonte de áudio", L"Audiobron", L"Źródło dźwięku", L"Источник звука", L"Ses kaynağı", L"Zdroj zvuku", L"音频来源" } },
    { Str::RowAudioSourceSub, { L"App or output device to mix in", L"App oder Ausgabegerät zum Einmischen", L"Appli ou sortie audio à mixer", L"App o salida de audio a mezclar", L"App o uscita audio da mixare", L"App ou saída de áudio a mixar", L"App of uitvoerapparaat om te mixen", L"Aplikacja lub wyjście do zmiksowania", L"Приложение или устройство вывода", L"Karıştırılacak uygulama veya çıkış", L"Aplikace nebo výstup k přimíchání", L"要混入的应用或输出设备" } },
    { Str::Refresh, { L"Refresh", L"Aktualisieren", L"Actualiser", L"Actualizar", L"Aggiorna", L"Atualizar", L"Vernieuwen", L"Odśwież", L"Обновить", L"Yenile", L"Obnovit", L"刷新" } },
    { Str::RowMicrophone, { L"Microphone", L"Mikrofon", L"Microphone", L"Micrófono", L"Microfono", L"Microfone", L"Microfoon", L"Mikrofon", L"Микрофон", L"Mikrofon", L"Mikrofon", L"麦克风" } },
    { Str::RowMicrophoneSub, { L"Device the music is mixed into", L"Gerät, in das die Musik gemischt wird", L"Périphérique qui reçoit la musique", L"Dispositivo que recibe la música", L"Dispositivo in cui mixare la musica", L"Dispositivo que recebe a música", L"Apparaat waarin de muziek komt", L"Urządzenie, do którego trafia muzyka", L"Куда подмешивается музыка", L"Müziğin karıştırılacağı aygıt", L"Zařízení, kam se hudba mísí", L"音乐混入的设备" } },
    { Str::RowAutostart, { L"Start with TeamSpeak", L"Mit TeamSpeak starten", L"Démarrer avec TeamSpeak", L"Iniciar con TeamSpeak", L"Avvia con TeamSpeak", L"Iniciar com o TeamSpeak", L"Starten met TeamSpeak", L"Uruchamiaj z TeamSpeak", L"Запуск вместе с TeamSpeak", L"TeamSpeak ile başlat", L"Spouštět s TeamSpeakem", L"随 TeamSpeak 启动" } },
    { Str::RowAutostartSub, { L"Enable MicMix automatically when TeamSpeak starts", L"MicMix beim Start von TeamSpeak automatisch einschalten", L"Active MicMix au lancement de TeamSpeak", L"Activa MicMix al iniciar TeamSpeak", L"Attiva MicMix all'avvio di TeamSpeak", L"Liga o MicMix ao iniciar o TeamSpeak", L"Zet MicMix aan zodra TeamSpeak start", L"Włącz MicMix przy starcie TeamSpeak", L"Включать MicMix при запуске TeamSpeak", L"TeamSpeak açılınca MicMix'i etkinleştir", L"Zapnout MicMix při spuštění TeamSpeaku", L"TeamSpeak 启动时自动开启 MicMix" } },
    { Str::RowRestart, { L"Restart source", L"Quelle neu starten", L"Redémarrer la source", L"Reiniciar la fuente", L"Riavvia la sorgente", L"Reiniciar a fonte", L"Bron herstarten", L"Uruchom źródło ponownie", L"Перезапустить источник", L"Kaynağı yeniden başlat", L"Restartovat zdroj", L"重启音源" } },
    { Str::RowRestartSub, { L"Reconnect the selected source after device changes", L"Quelle nach Geräteänderungen neu verbinden", L"Reconnecte la source après un changement de périphérique", L"Reconecta la fuente tras cambios de dispositivo", L"Ricollega la sorgente dopo modifiche ai dispositivi", L"Reconecta a fonte após mudanças de dispositivo", L"Verbindt de bron opnieuw na apparaatwijzigingen", L"Połącz źródło ponownie po zmianie urządzeń", L"Переподключить источник после смены устройств", L"Aygıt değişince kaynağı yeniden bağla", L"Znovu připojit zdroj po změně zařízení", L"设备变更后重新连接音源" } },
    { Str::Restart, { L"Restart", L"Neu starten", L"Redémarrer", L"Reiniciar", L"Riavvia", L"Reiniciar", L"Herstarten", L"Restartuj", L"Перезапуск", L"Yeniden başlat", L"Restartovat", L"重启" } },

    { Str::RowVolume, { L"Music volume", L"Musiklautstärke", L"Volume de la musique", L"Volumen de la música", L"Volume musica", L"Volume da música", L"Muziekvolume", L"Głośność muzyki", L"Громкость музыки", L"Müzik sesi", L"Hlasitost hudby", L"音乐音量" } },
    { Str::RowVolumeSub, { L"Max \x2212" L"2 dB to avoid clipping", L"Max. \x2212" L"2 dB gegen Übersteuerung", L"Max \x2212" L"2 dB contre la saturation", L"Máx. \x2212" L"2 dB para evitar saturación", L"Max \x2212" L"2 dB per evitare il clipping", L"Máx. \x2212" L"2 dB para evitar distorção", L"Max \x2212" L"2 dB tegen oversturing", L"Maks. \x2212" L"2 dB, bez przesteru", L"Макс. \x2212" L"2 дБ без перегрузки", L"Bozulmaya karşı en fazla \x2212" L"2 dB", L"Max. \x2212" L"2 dB proti přebuzení", L"最高 \x2212" L"2 dB，避免削波" } },
    { Str::RowMusicLevel, { L"Music level", L"Musikpegel", L"Niveau musique", L"Nivel de música", L"Livello musica", L"Nível da música", L"Muziekniveau", L"Poziom muzyki", L"Уровень музыки", L"Müzik seviyesi", L"Úroveň hudby", L"音乐电平" } },
    { Str::RowMusicLevelSub, { L"Sent level and clipping", L"Sendepegel und Übersteuerung", L"Niveau envoyé et saturation", L"Nivel enviado y saturación", L"Livello inviato e clipping", L"Nível enviado e distorção", L"Verzonden niveau en oversturing", L"Poziom wysyłany i przester", L"Уровень и перегрузка", L"Gönderilen seviye ve bozulma", L"Odesílaná úroveň a přebuzení", L"发送电平与削波" } },
    { Str::RowMicLevel, { L"Mic level", L"Mikrofonpegel", L"Niveau micro", L"Nivel del micrófono", L"Livello microfono", L"Nível do microfone", L"Microfoonniveau", L"Poziom mikrofonu", L"Уровень микрофона", L"Mikrofon seviyesi", L"Úroveň mikrofonu", L"麦克风电平" } },
    { Str::RowMicLevelSub, { L"Only while connected", L"Nur während einer Verbindung", L"Seulement une fois connecté", L"Solo mientras estás conectado", L"Solo durante la connessione", L"Somente enquanto conectado", L"Alleen tijdens verbinding", L"Tylko po połączeniu", L"Только при подключении", L"Yalnızca bağlıyken", L"Jen během připojení", L"仅在连接时" } },
    { Str::RowForceTx, { L"Send music when mic is silent", L"Musik senden, wenn das Mikro still ist", L"Envoyer la musique quand le micro est muet", L"Enviar música con el micro en silencio", L"Invia musica a microfono silenzioso", L"Enviar música com o microfone em silêncio", L"Muziek verzenden als de microfoon stil is", L"Wysyłaj muzykę, gdy mikrofon milczy", L"Передавать музыку, когда микрофон молчит", L"Mikrofon sessizken müzik gönder", L"Posílat hudbu, když mikrofon mlčí", L"麦克风无声时也发送音乐" } },
    { Str::RowForceTxSub, { L"Music keeps sending while you are not speaking", L"Musik läuft weiter, auch wenn du nicht sprichst", L"La musique continue quand vous ne parlez pas", L"La música sigue aunque no hables", L"La musica continua anche quando non parli", L"A música continua mesmo quando você não fala", L"Muziek gaat door terwijl je niet praat", L"Muzyka gra dalej, gdy nie mówisz", L"Музыка играет, даже когда вы молчите", L"Konuşmasanız da müzik gönderilir", L"Hudba hraje dál, i když nemluvíte", L"不说话时音乐也会继续发送" } },
    { Str::MonitorOn, { L"Monitor Mix: On", L"Mithören: An", L"Écoute : activée", L"Escucha: activada", L"Ascolto: attivo", L"Monitorar: ligado", L"Meeluisteren: aan", L"Odsłuch: wł.", L"Прослушка: вкл.", L"Dinleme: açık", L"Odposlech: zap.", L"监听：开" } },
    { Str::MonitorOff, { L"Monitor Mix: Off", L"Mithören: Aus", L"Écoute : désactivée", L"Escucha: desactivada", L"Ascolto: spento", L"Monitorar: desligado", L"Meeluisteren: uit", L"Odsłuch: wył.", L"Прослушка: выкл.", L"Dinleme: kapalı", L"Odposlech: vyp.", L"监听：关" } },
    { Str::MuteMusic, { L"Mute music", L"Musik stumm", L"Couper la musique", L"Silenciar música", L"Muta musica", L"Silenciar música", L"Muziek dempen", L"Wycisz muzykę", L"Без музыки", L"Müziği sustur", L"Ztlumit hudbu", L"静音音乐" } },
    { Str::MuteMic, { L"Mute mic", L"Mikro stumm", L"Couper le micro", L"Silenciar micro", L"Muta microfono", L"Silenciar micro", L"Microfoon dempen", L"Wycisz mikrofon", L"Без микрофона", L"Mikrofonu sustur", L"Ztlumit mikrofon", L"静音麦克风" } },
    { Str::MusicMuted, { L"Music muted", L"Musik stumm", L"Musique coupée", L"Música silenciada", L"Musica muta", L"Música silenciada", L"Muziek gedempt", L"Muzyka wycisz.", L"Музыка выкл.", L"Müzik kapalı", L"Hudba ztlumena", L"音乐已静音" } },
    { Str::MicMuted, { L"Mic muted", L"Mikro stumm", L"Micro coupé", L"Micro silenciado", L"Micro muto", L"Micro silenciado", L"Microfoon gedempt", L"Mikrofon wycisz.", L"Микрофон выкл.", L"Mikrofon kapalı", L"Mikrofon ztlumen", L"麦克风已静音" } },

    { Str::HotkeySet, { L"Set\x2026", L"Festlegen\x2026", L"Définir\x2026", L"Asignar\x2026", L"Imposta\x2026", L"Definir\x2026", L"Instellen\x2026", L"Ustaw\x2026", L"Задать\x2026", L"Ata\x2026", L"Nastavit\x2026", L"设置\x2026" } },
    { Str::HotkeyCurrentPrefix, { L"Current: ", L"Aktuell: ", L"Actuel : ", L"Actual: ", L"Attuale: ", L"Atual: ", L"Huidig: ", L"Obecnie: ", L"Сейчас: ", L"Geçerli: ", L"Aktuálně: ", L"当前：" } },
    { Str::HotkeyNotSet, { L"Not set", L"Nicht festgelegt", L"Non défini", L"Sin asignar", L"Non impostato", L"Não definido", L"Niet ingesteld", L"Nie ustawiono", L"Не задано", L"Ayarlanmadı", L"Nenastaveno", L"未设置" } },
    { Str::HotkeyPress, { L"Press a key\x2026 (Esc = cancel, Del = clear)", L"Taste drücken\x2026 (Esc = Abbrechen, Entf = Löschen)", L"Appuyez sur une touche\x2026 (Échap = annuler, Suppr = effacer)", L"Pulsa una tecla\x2026 (Esc = cancelar, Supr = borrar)", L"Premi un tasto\x2026 (Esc = annulla, Canc = cancella)", L"Pressione uma tecla\x2026 (Esc = cancelar, Del = limpar)", L"Druk op een toets\x2026 (Esc = annuleren, Del = wissen)", L"Naciśnij klawisz\x2026 (Esc = anuluj, Del = usuń)", L"Нажмите клавишу\x2026 (Esc \x2014 отмена, Del \x2014 очистить)", L"Bir tuşa basın\x2026 (Esc = iptal, Del = temizle)", L"Stiskněte klávesu\x2026 (Esc = zrušit, Del = smazat)", L"按下按键\x2026（Esc 取消，Del 清除）" } },
    { Str::HotkeyHint, { L"Click Set\x2026 and press the key combination. Esc cancels, Del clears.", L"Auf \x201E" L"Festlegen\x2026\x201C klicken und die Tastenkombination drücken. Esc bricht ab, Entf löscht.", L"Cliquez sur Définir\x2026 puis appuyez sur la combinaison. Échap annule, Suppr efface.", L"Pulsa Asignar\x2026 y luego la combinación. Esc cancela, Supr borra.", L"Fai clic su Imposta\x2026 e premi la combinazione. Esc annulla, Canc cancella.", L"Clique em Definir\x2026 e pressione a combinação. Esc cancela, Del limpa.", L"Klik op Instellen\x2026 en druk op de toetscombinatie. Esc annuleert, Del wist.", L"Kliknij Ustaw\x2026 i naciśnij kombinację klawiszy. Esc anuluje, Del usuwa.", L"Нажмите \x00AB" L"Задать\x2026\x00BB, затем сочетание клавиш. Esc \x2014 отмена, Del \x2014 очистить.", L"Ata\x2026 düğmesine tıklayıp tuş kombinasyonuna basın. Esc iptal eder, Del temizler.", L"Klikněte na Nastavit\x2026 a stiskněte kombinaci kláves. Esc zruší, Del smaže.", L"点击\x201C设置\x2026\x201D并按下组合键。Esc 取消，Del 清除。" } },
    { Str::HotkeyConflictMic, { L"Hotkey conflict: already used for mic mute.", L"Konflikt: bereits für Mikro stumm vergeben.", L"Conflit : déjà utilisé pour couper le micro.", L"Conflicto: ya se usa para silenciar el micro.", L"Conflitto: già usato per mutare il microfono.", L"Conflito: já usado para silenciar o microfone.", L"Conflict: al gebruikt voor microfoon dempen.", L"Konflikt: już używany do wyciszania mikrofonu.", L"Конфликт: уже назначено на отключение микрофона.", L"Çakışma: mikrofon susturmada kullanılıyor.", L"Konflikt: už použito pro ztlumení mikrofonu.", L"快捷键冲突：已用于麦克风静音。" } },
    { Str::HotkeyConflictMusic, { L"Hotkey conflict: already used for music mute.", L"Konflikt: bereits für Musik stumm vergeben.", L"Conflit : déjà utilisé pour couper la musique.", L"Conflicto: ya se usa para silenciar la música.", L"Conflitto: già usato per mutare la musica.", L"Conflito: já usado para silenciar a música.", L"Conflict: al gebruikt voor muziek dempen.", L"Konflikt: już używany do wyciszania muzyki.", L"Конфликт: уже назначено на отключение музыки.", L"Çakışma: müzik susturmada kullanılıyor.", L"Konflikt: už použito pro ztlumení hudby.", L"快捷键冲突：已用于音乐静音。" } },
    { Str::HotkeyCancelled, { L"Hotkey capture cancelled.", L"Tastenaufnahme abgebrochen.", L"Saisie du raccourci annulée.", L"Captura del atajo cancelada.", L"Acquisizione scorciatoia annullata.", L"Captura do atalho cancelada.", L"Sneltoets vastleggen geannuleerd.", L"Przechwytywanie skrótu anulowane.", L"Ввод сочетания отменён.", L"Kısayol yakalama iptal edildi.", L"Zadávání zkratky zrušeno.", L"已取消快捷键录入。" } },
    { Str::KeyCtrl, { L"Ctrl", L"Strg", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl", L"Ctrl" } },
    { Str::KeyShift, { L"Shift", L"Umschalt", L"Maj", L"Mayús", L"Maiusc", L"Shift", L"Shift", L"Shift", L"Shift", L"Shift", L"Shift", L"Shift" } },

    { Str::RowTheme, { L"App theme", L"Farbschema", L"Thème", L"Tema", L"Tema", L"Tema", L"Thema", L"Motyw", L"Тема", L"Tema", L"Motiv", L"主题" } },
    { Str::RowThemeSub, { L"System follows your Windows app mode", L"System folgt dem Windows-App-Modus", L"Système suit le mode des applis Windows", L"Sistema sigue el modo de apps de Windows", L"Sistema segue la modalità app di Windows", L"Sistema segue o modo de apps do Windows", L"Systeem volgt de Windows-appmodus", L"System: jak tryb aplikacji Windows", L"\x00AB" L"Система\x00BB следует режиму Windows", L"Sistem, Windows uygulama modunu izler", L"Systém podle režimu aplikací Windows", L"\x201C系统\x201D跟随 Windows 应用模式" } },
    { Str::ThemeSystem, { L"System", L"System", L"Système", L"Sistema", L"Sistema", L"Sistema", L"Systeem", L"System", L"Система", L"Sistem", L"Systém", L"系统" } },
    { Str::ThemeLight, { L"Light", L"Hell", L"Clair", L"Claro", L"Chiaro", L"Claro", L"Licht", L"Jasny", L"Светлая", L"Açık", L"Světlý", L"浅色" } },
    { Str::ThemeDark, { L"Dark", L"Dunkel", L"Sombre", L"Oscuro", L"Scuro", L"Escuro", L"Donker", L"Ciemny", L"Тёмная", L"Koyu", L"Tmavý", L"深色" } },
    { Str::RowLanguage, { L"Language", L"Sprache", L"Langue", L"Idioma", L"Lingua", L"Idioma", L"Taal", L"Język", L"Язык", L"Dil", L"Jazyk", L"语言" } },
    { Str::RowLanguageSub, { L"Automatic follows TeamSpeak", L"Automatisch folgt TeamSpeak", L"Automatique suit TeamSpeak", L"Automático sigue a TeamSpeak", L"Automatico segue TeamSpeak", L"Automático segue o TeamSpeak", L"Automatisch volgt TeamSpeak", L"Automatycznie: jak w TeamSpeak", L"\x00AB" L"Авто\x00BB следует TeamSpeak", L"Otomatik, TeamSpeak'i izler", L"Automaticky podle TeamSpeaku", L"\x201C自动\x201D跟随 TeamSpeak" } },
    { Str::LanguageAuto, { L"Automatic", L"Automatisch", L"Automatique", L"Automático", L"Automatico", L"Automático", L"Automatisch", L"Automatycznie", L"Автоматически", L"Otomatik", L"Automaticky", L"自动" } },
    { Str::RowLiveStatus, { L"Live status", L"Live-Status", L"État en direct", L"Estado en vivo", L"Stato in tempo reale", L"Status ao vivo", L"Live-status", L"Stan na żywo", L"Текущее состояние", L"Canlı durum", L"Aktuální stav", L"实时状态" } },

    { Str::NoSignal, { L"No signal", L"Kein Signal", L"Pas de signal", L"Sin señal", L"Nessun segnale", L"Sem sinal", L"Geen signaal", L"Brak sygnału", L"Нет сигнала", L"Sinyal yok", L"Žádný signál", L"无信号" } },
    { Str::LevelOk, { L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"OK", L"正常" } },
    { Str::LevelHot, { L"Hot", L"Laut", L"Fort", L"Alto", L"Alto", L"Alto", L"Hoog", L"Głośno", L"Громко", L"Yüksek", L"Hlasité", L"过高" } },
    { Str::LevelWarm, { L"Warm", L"Kräftig", L"Élevé", L"Medio", L"Medio", L"Médio", L"Warm", L"Średnio", L"Средне", L"Orta", L"Střední", L"偏高" } },
    { Str::LevelQuiet, { L"Quiet", L"Leise", L"Faible", L"Bajo", L"Basso", L"Baixo", L"Zacht", L"Cicho", L"Тихо", L"Düşük", L"Tiché", L"偏低" } },
    { Str::ClipEvents, { L"Clip events: %llu", L"Übersteuert: %llu", L"Saturations : %llu", L"Saturaciones: %llu", L"Clipping: %llu", L"Distorções: %llu", L"Oversturingen: %llu", L"Przestery: %llu", L"Перегрузки: %llu", L"Bozulma: %llu", L"Přebuzení: %llu", L"削波次数：%llu" } },
    { Str::ClipMeter, { L"Clip meter", L"Übersteuerung", L"Saturation", L"Saturación", L"Clipping", L"Distorção", L"Oversturing", L"Przester", L"Перегрузка", L"Bozulma", L"Přebuzení", L"削波" } },
    { Str::RefreshingSources, { L"Refreshing source list\x2026", L"Quellenliste wird aktualisiert\x2026", L"Actualisation des sources\x2026", L"Actualizando fuentes\x2026", L"Aggiornamento sorgenti\x2026", L"Atualizando fontes\x2026", L"Bronnen vernieuwen\x2026", L"Odświeżanie źródeł\x2026", L"Обновление списка источников\x2026", L"Kaynaklar yenileniyor\x2026", L"Obnovování zdrojů\x2026", L"正在刷新音源列表\x2026" } },
    { Str::RefreshFailed, { L"Refresh failed to start", L"Aktualisierung konnte nicht starten", L"Impossible de lancer l'actualisation", L"No se pudo iniciar la actualización", L"Impossibile avviare l'aggiornamento", L"Não foi possível atualizar", L"Vernieuwen kon niet starten", L"Nie udało się odświeżyć", L"Не удалось обновить", L"Yenileme başlatılamadı", L"Obnovení se nepodařilo spustit", L"无法开始刷新" } },
    { Str::SourceHeaderChannels, { L"Audio channels:", L"Audiokanäle:", L"Canaux audio :", L"Canales de audio:", L"Canali audio:", L"Canais de áudio:", L"Audiokanalen:", L"Kanały audio:", L"Аудиоканалы:", L"Ses kanalları:", L"Zvukové kanály:", L"音频通道：" } },
    { Str::SourceNoChannels, { L"(no audio channels found)", L"(keine Audiokanäle gefunden)", L"(aucun canal audio trouvé)", L"(no se encontraron canales)", L"(nessun canale audio trovato)", L"(nenhum canal de áudio encontrado)", L"(geen audiokanalen gevonden)", L"(nie znaleziono kanałów)", L"(аудиоканалы не найдены)", L"(ses kanalı bulunamadı)", L"(nenalezeny žádné kanály)", L"（未找到音频通道）" } },
    { Str::SourceHeaderApps, { L"Running apps:", L"Laufende Apps:", L"Applis en cours :", L"Apps en ejecución:", L"App in esecuzione:", L"Apps em execução:", L"Actieve apps:", L"Uruchomione aplikacje:", L"Запущенные приложения:", L"Çalışan uygulamalar:", L"Spuštěné aplikace:", L"正在运行的应用：" } },
    { Str::SourceNoApps, { L"(no running app found)", L"(keine laufende App gefunden)", L"(aucune appli en cours)", L"(ninguna app en ejecución)", L"(nessuna app in esecuzione)", L"(nenhum app em execução)", L"(geen actieve app gevonden)", L"(brak uruchomionych aplikacji)", L"(нет запущенных приложений)", L"(çalışan uygulama yok)", L"(žádná spuštěná aplikace)", L"（未找到正在运行的应用）" } },
    { Str::DefaultSuffix, { L" (Default)", L" (Standard)", L" (par défaut)", L" (predeterminado)", L" (predefinito)", L" (padrão)", L" (standaard)", L" (domyślne)", L" (по умолчанию)", L" (varsayılan)", L" (výchozí)", L"（默认）" } },

    { Str::MiniLive, { L"LIVE", L"LIVE", L"DIRECT", L"EN VIVO", L"LIVE", L"AO VIVO", L"LIVE", L"NA ŻYWO", L"ЭФИР", L"CANLI", L"ŽIVĚ", L"直播中" } },
    { Str::MiniOff, { L"OFF", L"AUS", L"ARRÊT", L"APAGADO", L"SPENTO", L"DESLIGADO", L"UIT", L"WYŁ.", L"ВЫКЛ.", L"KAPALI", L"VYP.", L"关闭" } },
    { Str::MiniChooseSource, { L"Choose source", L"Quelle wählen", L"Choisir une source", L"Elegir fuente", L"Scegli sorgente", L"Escolher fonte", L"Bron kiezen", L"Wybierz źródło", L"Выбрать источник", L"Kaynak seç", L"Vybrat zdroj", L"选择音源" } },
    { Str::MiniMusic, { L"Music", L"Musik", L"Musique", L"Música", L"Musica", L"Música", L"Muziek", L"Muzyka", L"Музыка", L"Müzik", L"Hudba", L"音乐" } },
    { Str::MiniMic, { L"Mic", L"Mikro", L"Micro", L"Micro", L"Micro", L"Micro", L"Micro", L"Mikr.", L"Микр.", L"Mik.", L"Mikr.", L"麦克风" } },
    { Str::MiniHotkeysHint, { L"Set hotkeys in the main window", L"Tastenkürzel im Hauptfenster festlegen", L"Raccourcis : voir la fenêtre principale", L"Atajos: en la ventana principal", L"Scorciatoie nella finestra principale", L"Atalhos na janela principal", L"Sneltoetsen in het hoofdvenster", L"Skróty ustaw w oknie głównym", L"Клавиши задаются в главном окне", L"Kısayollar ana pencerede ayarlanır", L"Zkratky v hlavním okně", L"在主窗口中设置快捷键" } },
    { Str::MiniHotkeyMusic, { L" music", L" Musik", L" musique", L" música", L" musica", L" música", L" muziek", L" muzyka", L" музыка", L" müzik", L" hudba", L" 音乐" } },
    { Str::MiniHotkeyMic, { L" mic", L" Mikro", L" micro", L" micro", L" microfono", L" microfone", L" microfoon", L" mikrofon", L" микрофон", L" mikrofon", L" mikrofon", L" 麦克风" } },
    { Str::UpdateAvailable, { L"Update %s available", L"Update %s verfügbar", L"Mise à jour %s dispo", L"Actualización %s", L"Aggiornamento %s", L"Atualização %s", L"Update %s beschikbaar", L"Aktualizacja %s", L"Обновление %s", L"Güncelleme %s", L"Aktualizace %s", L"可更新到 %s" } },
    { Str::RowUpdateCheck, { L"Check for updates", L"Nach Updates suchen", L"Rechercher des mises à jour", L"Buscar actualizaciones", L"Cerca aggiornamenti", L"Procurar atualizações", L"Zoeken naar updates", L"Sprawdzaj aktualizacje", L"Проверять обновления", L"Güncellemeleri denetle", L"Hledat aktualizace", L"检查更新" } },
    { Str::RowUpdateCheckSub, { L"Asks GitHub for a newer release when opened", L"Fragt beim Öffnen GitHub nach neuen Versionen", L"Interroge GitHub à l'ouverture", L"Consulta GitHub al abrir", L"Controlla GitHub all'apertura", L"Consulta o GitHub ao abrir", L"Vraagt GitHub bij openen naar nieuwe versies", L"Pyta GitHub o nowe wersje przy otwarciu", L"Проверяет GitHub при открытии", L"Açılışta GitHub'da yeni sürüm arar", L"Při otevření se zeptá GitHubu na novou verzi", L"打开时向 GitHub 查询新版本" } },
    { Str::WindowTitle, { L"MicMix Settings", L"MicMix-Einstellungen", L"Paramètres MicMix", L"Ajustes de MicMix", L"Impostazioni MicMix", L"Configurações do MicMix", L"MicMix-instellingen", L"Ustawienia MicMix", L"Настройки MicMix", L"MicMix Ayarları", L"Nastavení MicMix", L"MicMix 设置" } },
};
static_assert(sizeof(kRows) / sizeof(kRows[0]) == static_cast<size_t>(Str::Count), "every Str needs a translation row");

std::array<const wchar_t* const*, static_cast<size_t>(Str::Count)> BuildIndex() {
    std::array<const wchar_t* const*, static_cast<size_t>(Str::Count)> index{};
    for (const Row& row : kRows) {
        index[static_cast<size_t>(row.id)] = row.text.data();
    }
    return index;
}

const auto kIndex = BuildIndex();

std::atomic<int> g_activeLang{ EN };
std::mutex g_configMutex;
std::string g_tsConfigDir;

int LangIndex(const std::string& code) {
    for (size_t i = 0; i < kLanguages.size(); ++i) {
        if (code == kLanguages[i].code) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::wstring Utf8ToWideLocal(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

bool LooksLikeLanguageValue(const std::string& v) {
    // "de", "de_DE", "pt-BR", "zh_CN", "es_419"
    if (v.size() < 2 || v.size() > 10) return false;
    size_t i = 0;
    while (i < v.size() && std::isalpha(static_cast<unsigned char>(v[i]))) ++i;
    if (i < 2 || i > 3) return false;
    if (i == v.size()) return true;
    if (v[i] != '_' && v[i] != '-') return false;
    ++i;
    const size_t rest = v.size() - i;
    if (rest < 2 || rest > 4) return false;
    for (; i < v.size(); ++i) {
        if (!std::isalnum(static_cast<unsigned char>(v[i]))) return false;
    }
    return true;
}

// Reads one SQLite varint (1-9 bytes); returns its length or 0 on overflow.
size_t ReadVarint(const unsigned char* p, size_t avail, uint64_t& out) {
    out = 0;
    for (size_t i = 0; i < 9 && i < avail; ++i) {
        if (i == 8) {
            out = (out << 8) | p[i];
            return 9;
        }
        out = (out << 7) | (p[i] & 0x7F);
        if ((p[i] & 0x80) == 0) {
            return i + 1;
        }
    }
    return 0;
}

// Finds the value stored next to the TEXT key "Language" in a SQLite file image.
// SQLite records are [header-size][serial types...][column bodies...]; for a key/value
// row the key "Language" (serial type 29 = 8-byte text) is followed directly by the
// value's text body. We locate "Language", walk back to a consistent record header and
// read the next column. No SQLite library is needed for this single lookup.
std::string FindLanguageInSqliteImage(const std::vector<unsigned char>& data) {
    static const char kKey[] = "Language";
    constexpr size_t kKeyLen = sizeof(kKey) - 1;
    std::string found;
    for (size_t pos = 0; pos + kKeyLen < data.size();) {
        const void* hit = std::memchr(data.data() + pos, 'L', data.size() - pos);
        if (!hit) break;
        const size_t p = static_cast<size_t>(static_cast<const unsigned char*>(hit) - data.data());
        pos = p + 1;
        if (p + kKeyLen > data.size() || std::memcmp(data.data() + p, kKey, kKeyLen) != 0) continue;
        // Try header sizes 2..10 bytes ending right before the key body.
        for (size_t hdr = 2; hdr <= 10 && hdr <= p; ++hdr) {
            const size_t start = p - hdr;
            if (data[start] != hdr) continue;
            std::vector<uint64_t> types;
            size_t q = start + 1;
            bool ok = true;
            while (q < p) {
                uint64_t t = 0;
                const size_t n = ReadVarint(data.data() + q, p - q, t);
                if (n == 0) { ok = false; break; }
                types.push_back(t);
                q += n;
            }
            if (!ok || q != p) continue;
            // Columns before the key must have no body (NULL rowid alias, 0 or 1).
            size_t k = 0;
            while (k < types.size() && (types[k] == 0 || types[k] == 8 || types[k] == 9)) ++k;
            if (k + 1 >= types.size() || types[k] != 13 + 2 * kKeyLen) continue;
            const uint64_t vt = types[k + 1];
            if (vt < 13 || (vt % 2) == 0) continue;
            const size_t vlen = static_cast<size_t>((vt - 13) / 2);
            const size_t vstart = p + kKeyLen;
            if (vstart + vlen > data.size()) continue;
            std::string value(reinterpret_cast<const char*>(data.data() + vstart), vlen);
            if (LooksLikeLanguageValue(value)) {
                found = value; // keep the last match: later pages tend to hold newer rows
            }
            break;
        }
    }
    return found;
}

bool ReadFileShared(const std::wstring& path, std::vector<unsigned char>& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) != FALSE && size.QuadPart > 0 && size.QuadPart < (64LL << 20);
    if (ok) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &read, nullptr) != FALSE;
        out.resize(read);
    }
    CloseHandle(h);
    return ok && !out.empty();
}

std::string WindowsUiLanguage() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH]{};
    if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), name, LOCALE_NAME_MAX_LENGTH, 0) <= 0) {
        return {};
    }
    std::string out;
    for (const wchar_t* c = name; *c; ++c) out.push_back(static_cast<char>(*c & 0x7F));
    return out;
}

} // namespace

std::string I18nFindLanguageInBytes(const std::vector<unsigned char>& sqliteImage) {
    return FindLanguageInSqliteImage(sqliteImage);
}

const std::vector<UiLanguage>& I18nLanguages() {
    return kLanguages;
}

std::string I18nNormalize(const std::string& raw) {
    std::string base;
    for (char c : raw) {
        if (c == '_' || c == '-') break;
        base.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return LangIndex(base) >= 0 ? base : std::string();
}

void I18nSetTeamSpeakConfigDir(const std::string& dirUtf8) {
    std::lock_guard<std::mutex> lock(g_configMutex);
    g_tsConfigDir = dirUtf8;
}

std::string I18nReadTeamSpeakLanguage() {
    std::string dir;
    {
        std::lock_guard<std::mutex> lock(g_configMutex);
        dir = g_tsConfigDir;
    }
    if (dir.empty()) return {};
    const std::filesystem::path base(Utf8ToWideLocal(dir));
    std::vector<unsigned char> data;
    // A write-ahead log holds changes not yet copied into the main file: check it first.
    if (ReadFileShared((base / L"settings.db-wal").wstring(), data)) {
        const std::string v = FindLanguageInSqliteImage(data);
        if (!v.empty()) return v;
    }
    if (ReadFileShared((base / L"settings.db").wstring(), data)) {
        return FindLanguageInSqliteImage(data);
    }
    return {};
}

std::string I18nAutoCode() {
    std::string code = I18nNormalize(I18nReadTeamSpeakLanguage());
    if (code.empty()) code = I18nNormalize(WindowsUiLanguage());
    return code.empty() ? std::string("en") : code;
}

void I18nApply(const std::string& setting) {
    std::string code = setting == "auto" ? std::string() : I18nNormalize(setting);
    if (code.empty()) code = I18nAutoCode();
    const int idx = LangIndex(code);
    g_activeLang.store(idx >= 0 ? idx : EN, std::memory_order_release);
}

std::string I18nActiveCode() {
    return kLanguages[static_cast<size_t>(g_activeLang.load(std::memory_order_acquire))].code;
}

const wchar_t* Tr(Str id) {
    const size_t i = static_cast<size_t>(id);
    if (i >= kIndex.size() || !kIndex[i]) return L"";
    const wchar_t* text = kIndex[i][g_activeLang.load(std::memory_order_acquire)];
    return (text && *text) ? text : kIndex[i][EN];
}

std::string TrUtf8(Str id) {
    const wchar_t* w = Tr(id);
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string out(static_cast<size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), n, nullptr, nullptr);
    return out;
}
