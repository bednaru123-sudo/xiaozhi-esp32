#pragma once

// Mati - "zycie" robota-zwierzaka dla Spotpear ESP32-S3 1.28 BOX.
// Silnik potrzeb (energia, najedzenie, zabawa, czulosc) + zachowania w tle
// (mruganie, rozgladanie, ziewanie, drzemka, jedzenie, zabawa, spiew),
// reakcje na dotyk oraz narzedzia MCP dla LLM.

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
    };

    static MatiLife& GetInstance() {
        static MatiLife instance;
        return instance;
    }

    void Start(Backlight* backlight);

    // Dotyk: krotkie stukniecie gdy Mati jest bezczynny
    void OnPet();
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
    bool SoundsEnabled() const { return sounds_enabled_; }
    std::string GetStatusJson();

private:
    MatiLife() = default;
    MatiLife(const MatiLife&) = delete;
    MatiLife& operator=(const MatiLife&) = delete;

    static void TaskEntry(void* arg);
    void Loop();
    void Tick1s();
    void ChooseNext(int64_t now_ms);
    void Begin(Activity a, int64_t now_ms, int duration_ms = 0);
    void Finish(int64_t now_ms);
    void ShowEmotion(const char* emotion);
    void PlaySound(std::string_view sound);
    void ApplyBrightness();
    void LoadState();
    void SaveState();
    bool IsNight() const;
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
    bool was_idle_ = false;
    bool dimmed_ = false;
    bool forced_sleep_ = false;
    std::atomic<bool> sounds_enabled_{true};
    int song_index_ = 0;
};
