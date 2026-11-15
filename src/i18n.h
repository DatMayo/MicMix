#pragma once

// UI translations. The active language follows the TeamSpeak client by default
// (settings.db, table Application, key Language), falls back to the Windows UI
// language and finally to English. Users can override it in the settings window.

#include <string>
#include <vector>

enum class Str : int {
    NavSource, NavMix, NavHotkeys, NavAppearance, NavDiagnostics,
    MiniModeButton,
    StatusSending, StatusMuted, StatusError, StatusOff, StatusOffHint, StatusReadyPrefix,
    NoSourceSelected, DefaultMicrophone, SourceErrorFallback,
    PowerOn, PowerOff,
    RowAudioSource, RowAudioSourceSub, Refresh,
    RowMicrophone, RowMicrophoneSub,
    RowAutostart, RowAutostartSub,
    RowRestart, RowRestartSub, Restart,
    RowVolume, RowVolumeSub,
    RowMusicLevel, RowMusicLevelSub,
    RowMicLevel, RowMicLevelSub,
    RowForceTx, RowForceTxSub,
    MonitorOn, MonitorOff,
    MuteMusic, MuteMic, MusicMuted, MicMuted,
    HotkeySet, HotkeyCurrentPrefix, HotkeyNotSet, HotkeyPress, HotkeyHint,
    HotkeyConflictMic, HotkeyConflictMusic, HotkeyCancelled,
    KeyCtrl, KeyShift,
    RowTheme, RowThemeSub, ThemeSystem, ThemeLight, ThemeDark,
    RowLanguage, RowLanguageSub, LanguageAuto,
    RowLiveStatus,
    NoSignal, LevelOk, LevelHot, LevelWarm, LevelQuiet, ClipEvents, ClipMeter,
    RefreshingSources, RefreshFailed,
    SourceHeaderChannels, SourceNoChannels, SourceHeaderApps, SourceNoApps, DefaultSuffix,
    MiniLive, MiniOff, MiniChooseSource, MiniMusic, MiniMic, MiniHotkeysHint, MiniHotkeyMusic, MiniHotkeyMic,
    WindowTitle,
    Count
};

struct UiLanguage {
    const char* code;          // "en", "de", ...
    const wchar_t* nativeName; // "English", "Deutsch", ...
};

// All languages that ship with MicMix, English first.
const std::vector<UiLanguage>& I18nLanguages();

// Directory that contains TeamSpeak's settings.db (the plugin config path).
void I18nSetTeamSpeakConfigDir(const std::string& dirUtf8);

// "auto" or a language code from I18nLanguages(). Unknown codes behave like "auto".
void I18nApply(const std::string& setting);

// Code of the language currently in use ("en", "de", ...).
std::string I18nActiveCode();

// Language "auto" resolves to right now (TeamSpeak, then Windows, then English).
std::string I18nAutoCode();

// Raw language value from TeamSpeak's settings.db ("de_DE", "zh_CN", ...), or empty.
std::string I18nReadTeamSpeakLanguage();

// Language value stored under the key "Language" in a SQLite file image (exposed for tests).
std::string I18nFindLanguageInBytes(const std::vector<unsigned char>& sqliteImage);

// "de_DE" / "de-DE" / "pt_BR" -> "de" / "de" / "pt"; empty when unsupported.
std::string I18nNormalize(const std::string& raw);

const wchar_t* Tr(Str id);
std::string TrUtf8(Str id);
