#pragma once

// Mati - "zycie" robota-zwierzaka dla Spotpear ESP32-S3 1.28 BOX.
// Silnik potrzeb (energia, najedzenie, zabawa, czulosc), osobowosc, rytm dnia
// (posilki, higiena, obowiazki), zachowania w tle, gry dotykowe, samodzielne
// wypowiedzi przez LLM oraz narzedzia MCP.

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>

class Backlight;

class MatiLife {
public:
    enum class Activity : uint8_t {
        kNone = 0,   // neutralne oczy (same mrugaja i zerkaja)
        kLook,
        kYawn,
        kNap,
        kEat,
        kPlay,
        kSing,
        kPet,
        kBored,
        kHungry,
        kMood,       // losowa mina: winking, cool, relaxed...
        kAct,        // czynnosc z tabeli kActs
        kGame,       // trwa gra dotykowa
    };

    static MatiLife& GetInstance() {
        static MatiLife instance;
        return instance;
    }

    void Start(Backlight* backlight);

    // Dotyk w stanie bezczynnosci: 1 stukniecie = glaskanie, 2 = Mati cos powie,
    // 3+ = laskotki. W trakcie gry stukniecia ida do gry.
    void OnTap();
    // Przesuniecie palcem w bok = podanie przekaski
    void OnSwipeSide();
    // Kazda interakcja (dotyk, rozmowa) - budzi i resetuje nude
    void NoteInteraction();

    // Wywolywane przez narzedzia MCP (moga przyjsc w trakcie rozmowy -
    // animacja zostanie odegrana po powrocie do stanu bezczynnosci)
    void RequestActivity(Activity a);
    void Feed();
    void Play();
    void Sing();
    void Sleep();
    void WakeUp();
    void SetSoundsEnabled(bool on);
    // 0 = nigdy nie zagaduje sam, 1 = rzadko, 2 = normalnie, 3 = gadula
    void SetChattiness(int level);
    // Natychmiastowa samodzielna wypowiedz
    void SayNow();
    // Zlec konkretna czynnosc z tabeli (np. "dance"); false gdy nieznana
    bool RequestAct(const std::string& name);
    // Zlec gre dotykowa: "reaction" albo "tap_race"
    bool RequestGame(const std::string& name);
    bool SoundsEnabled() const { return sounds_enabled_; }
    std::string GetStatusJson();

private:
    enum class Game : uint8_t {
        kNone = 0,
        kInvite,       // "Zagramy?" - czeka na stukniecie
        kReactWait,    // czerwona kropka, czekaj...
        kReactGo,      // TERAZ! - mierzymy czas
        kReactPause,   // wynik rundy
        kTapRace,      // stukaj ile wlezie
        kResult,       // pokazuje wynik koncowy
    };

    MatiLife() = default;
    MatiLife(const MatiLife&) = delete;
    MatiLife& operator=(const MatiLife&) = delete;

    static void TaskEntry(void* arg);
    void RegisterTools();
    void Loop();
    void Tick1s();
    void ChooseNext(int64_t now_ms);
    bool RunRoutines(int64_t now_ms);
    bool MaybeSpeak(int64_t now_ms, bool force);
    void BeginAct(int index, int64_t now_ms);
    int PickAct(int64_t now_ms);
    void LoadPersonality();
    void Grow(int& trait, int delta);
    void Speak(const std::string& topic, int64_t now_ms);
    void WatchInitiative(int64_t now_ms);
    void Begin(Activity a, int64_t now_ms, int duration_ms = 0);
    void Finish(int64_t now_ms);
    void ResolveTaps(int64_t now_ms);
    void StartGame(Game g, int64_t now_ms);
    void GameTick(int64_t now_ms);
    void GameTap(int64_t now_ms);
    void EndGame(int64_t now_ms);
    void ShowEmotion(const char* emotion);
    void ShowNow(const char* emotion);
    void Notify(const std::string& text, int ms = 2500);
    void PlaySound(std::string_view sound, bool allow_night = false);
    void ApplyBrightness();
    void LoadState();
    void SaveState();
    void CheckNewDay();
    bool IsNight() const;
    bool IsWeekend() const;
    int LocalHour() const;
    static int64_t NowMs();
    const char* MoodName() const;

    Backlight* backlight_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    bool started_ = false;

    // Potrzeby 0..100 (wieksze = lepiej)
    float energy_ = 80;
    float fullness_ = 70;
    float fun_ = 60;
    float affection_ = 50;

    Activity activity_ = Activity::kNone;
    Activity pending_ = Activity::kNone;  // zlecone przez MCP / dotyk
    int64_t activity_end_ms_ = 0;
    int64_t next_event_ms_ = 0;
    int64_t last_interaction_ms_ = 0;
    int64_t last_sing_ms_ = -10LL * 60 * 1000;
    int64_t hungry_since_ms_ = 0;
    int64_t last_save_ms_ = 0;
    int64_t last_drink_ms_ = 0;
    int64_t last_invite_ms_ = 0;
    bool was_idle_ = false;
    bool dimmed_ = false;
    bool forced_sleep_ = false;
    bool night_nap_ = false;
    std::atomic<bool> sounds_enabled_{true};
    int song_index_ = 0;

    // Samodzielne wypowiedzi przez LLM (serwer Xiaozhi)
    int chattiness_ = 2;
    int64_t last_speak_ms_ = 0;
    bool initiative_active_ = false;
    int64_t initiative_listen_since_ms_ = 0;
    int last_greet_yday_ = -1;
    bool say_now_ = false;

    // Osobowosc (0-100), losowana przy pierwszym uruchomieniu i powoli
    // ksztaltowana przez to, jak domownicy sie z nim obchodza.
    int curiosity_ = 60;
    int playfulness_ = 60;
    int laziness_ = 40;
    int musicality_ = 60;
    int sociability_ = 60;
    int act_index_ = -1;
    int pending_act_ = -1;
    int recent_acts_[3] = {-1, -1, -1};
    int follow_up_ = -1;          // czynnosc, ktora ma nastapic zaraz po obecnej

    // Rytm dnia
    int day_yday_ = -1;
    uint32_t routines_done_ = 0;  // bit = czynnosc dnia juz wykonana
    int evening_stage_ = 0;       // 0 -> mycie zebow -> dobranoc -> owce -> sen

    // Dotyk i gry
    int taps_ = 0;
    int64_t last_tap_ms_ = 0;
    Game game_ = Game::kNone;
    Game pending_game_ = Game::kNone;
    int64_t game_until_ms_ = 0;
    int64_t game_go_ms_ = 0;
    int game_round_ = 0;
    int game_score_ = 0;          // najlepszy czas rundy / liczba stukniec
    int best_reaction_ms_ = 0;
    int best_taps_ = 0;
    const char* pending_show_ = nullptr;  // wynik (kostka/moneta) do pokazania po rozmowie
    bool close_after_reply_ = false;     // zakoncz rozmowe po odpowiedzi (start gry)
};
