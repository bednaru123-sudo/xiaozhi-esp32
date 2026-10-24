#include "mati_life.h"

#include "application.h"
#include "assets/lang_config.h"
#include "backlight.h"
#include "audio_codec.h"
#include "board.h"
#include "display/display.h"
#include "mcp_server.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_random.h>
#include <esp_timer.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#define TAG "MatiLife"

namespace {

constexpr int kLoopMs = 200;
constexpr int kFullBrightness = -1;  // = jasnosc zapisana przez uzytkownika
constexpr int kNightBrightness = 25;
constexpr int kNapBrightness = 6;
constexpr int kNightStartHour = 22;
constexpr int kNightEndHour = 7;

inline float Clamp(float v) { return std::max(0.0f, std::min(100.0f, v)); }

inline uint32_t Rand(uint32_t n) { return n ? esp_random() % n : 0; }

inline int RandRange(int lo, int hi) { return lo + static_cast<int>(Rand(hi - lo + 1)); }

const char* ActivityName(MatiLife::Activity a) {
    switch (a) {
        case MatiLife::Activity::kNone: return "nic";
        case MatiLife::Activity::kLook: return "rozglada sie";
        case MatiLife::Activity::kYawn: return "ziewa";
        case MatiLife::Activity::kNap: return "spi";
        case MatiLife::Activity::kEat: return "je";
        case MatiLife::Activity::kPlay: return "bawi sie pilka";
        case MatiLife::Activity::kSing: return "spiewa";
        case MatiLife::Activity::kPet: return "jest glaskany";
        case MatiLife::Activity::kBored: return "nudzi sie";
        case MatiLife::Activity::kHungry: return "jest glodny";
        case MatiLife::Activity::kMood: return "robi miny";
        case MatiLife::Activity::kAct: return "cos robi";
    }
    return "?";
}

// Tabela dodatkowych czynnosci. Wagi liczone w PickAct().
enum ActId {
    kActDance = 0,
    kActSneeze,
    kActHiccup,
    kActDaydream,
    kActStretch,
    kActRead,
    kActPeekaboo,
    kActChaseFly,
    kActDizzy,
    kActWhistle,
    kActMeditate,
    kActDoze,
    kActProud,
    kActCountSheep,
    kActCurious,
    kActCount
};

struct ActDef {
    const char* emotion;
    int duration_ms;
    const char* label;
};

const ActDef kActs[kActCount] = {
    {"dance", 6300, "tanczy"},
    {"sneeze", 2200, "kicha"},
    {"hiccup", 4200, "ma czkawke"},
    {"daydream", 6600, "marzy patrzac w gwiazdy"},
    {"stretch", 2400, "przeciaga sie"},
    {"read", 9300, "czyta ksiazke"},
    {"peekaboo", 2700, "bawi sie w a kuku"},
    {"chase_fly", 4400, "goni muche"},
    {"dizzy", 2600, "kreci mu sie w glowie"},
    {"whistle", 3500, "pogwizduje"},
    {"meditate", 8400, "medytuje"},
    {"doze", 4000, "przysypia"},
    {"proud", 2200, "jest z siebie dumny"},
    {"count_sheep", 6500, "liczy owce"},
    {"curious", 3100, "cos go zaciekawilo"},
};

}  // namespace

int64_t MatiLife::NowMs() { return esp_timer_get_time() / 1000; }

void MatiLife::Start(Backlight* backlight) {
    if (started_) return;
    started_ = true;
    backlight_ = backlight;
    mutex_ = xSemaphoreCreateMutex();
    LoadState();

    auto& mcp = McpServer::GetInstance();
    mcp.AddTool("self.mati.get_status",
                "Sprawdz samopoczucie Matiego (robota-zwierzaka, czyli Ciebie): nastroj, energia, "
                "najedzenie, zabawa, czulosc (0-100) i co aktualnie robi. Uzyj, gdy ktos pyta jak sie "
                "czujesz, czy jestes glodny/zmeczony, albo zanim zaproponujesz zabawe.",
                PropertyList(), [this](const PropertyList&) -> ToolResult { return GetStatusJson(); });
    mcp.AddTool("self.mati.feed",
                "Nakarm Matiego (np. gdy uzytkownik mowi, ze daje Ci jedzenie/ciasteczko). "
                "Po rozmowie na ekranie pojawi sie animacja jedzenia.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Feed();
                    return GetStatusJson();
                });
    mcp.AddTool("self.mati.play",
                "Pobaw sie z Matim pilka (poprawia humor, troche meczy). Animacja po rozmowie.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Play();
                    return GetStatusJson();
                });
    mcp.AddTool("self.mati.sing",
                "Mati zaspiewa krotka melodyjke (animacja z nutkami i dzwiek) po zakonczeniu rozmowy.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Sing();
                    return true;
                });
    mcp.AddTool("self.mati.sleep",
                "Poloz Matiego spac (przyciemniony ekran, zamkniete oczy) - np. na dobranoc. "
                "Spi, dopoki ktos go nie obudzi dotykiem, rozmowa albo narzedziem self.mati.wake_up.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    Sleep();
                    return true;
                });
    mcp.AddTool("self.mati.wake_up", "Obudz Matiego.", PropertyList(),
                [this](const PropertyList&) -> ToolResult {
                    WakeUp();
                    return true;
                });
    mcp.AddTool("self.mati.set_sounds",
                "Wlacz lub wylacz dzwieki Matiego w tle (spiew, chrupanie, cwierkanie).",
                PropertyList({Property("enabled", kPropertyTypeBoolean)}),
                [this](const PropertyList& props) -> ToolResult {
                    SetSoundsEnabled(props["enabled"].value<bool>());
                    return true;
                });

    mcp.AddTool("self.mati.do",
                "Popros Matiego o konkretna czynnosc pokazana na ekranie po zakonczeniu rozmowy. "
                "action: dance (taniec z muzyka), stretch (gimnastyka), read (czytanie ksiazki), "
                "peekaboo (a kuku), chase_fly (goni muche), whistle (pogwizdywanie), meditate "
                "(medytacja), daydream (marzenia pod gwiazdami), count_sheep (liczenie owiec), "
                "curious (rozglada sie zaciekawiony), proud (dumna mina), sneeze (kichniecie), "
                "hiccup (czkawka).",
                PropertyList({Property("action", kPropertyTypeString)}),
                [this](const PropertyList& props) -> ToolResult {
                    if (!RequestAct(props["action"].value<std::string>())) {
                        return std::unexpected(std::string("Nieznana czynnosc"));
                    }
                    return true;
                });
    mcp.AddTool("self.mati.set_chattiness",
                "Ustaw, jak czesto Mati sam z siebie zagaduje domownikow, gdy sie nudzi: "
                "0 = nigdy, 1 = rzadko, 2 = normalnie, 3 = czesto.",
                PropertyList({Property("level", kPropertyTypeInteger, 0, 3)}),
                [this](const PropertyList& props) -> ToolResult {
                    SetChattiness(props["level"].value<int>());
                    return true;
                });

    xTaskCreate(TaskEntry, "mati_life", 4096, this, 2, nullptr);
    ESP_LOGI(TAG, "Mati ozyl: energia %.0f, najedzenie %.0f, zabawa %.0f", energy_, fullness_, fun_);
}

void MatiLife::TaskEntry(void* arg) {
    static_cast<MatiLife*>(arg)->Loop();
    vTaskDelete(nullptr);
}

void MatiLife::LoadState() {
    Settings s("mati", false);
    energy_ = s.GetInt("energy", 80);
    fullness_ = s.GetInt("full", 70);
    fun_ = s.GetInt("fun", 60);
    affection_ = s.GetInt("aff", 50);
    sounds_enabled_ = s.GetBool("sounds", true);
    LoadPersonality();
    chattiness_ = std::max(0, std::min(3, (int)s.GetInt("chatty", 2)));
    last_greet_yday_ = s.GetInt("greet", -1);
}

void MatiLife::LoadPersonality() {
    Settings s("mati", false);
    if (s.GetInt("p_cur", -1) < 0) {
        // Pierwsze uruchomienie: wylosuj charakter
        Settings w("mati", true);
        w.SetInt("p_cur", RandRange(35, 85));
        w.SetInt("p_play", RandRange(35, 85));
        w.SetInt("p_lazy", RandRange(20, 70));
        w.SetInt("p_music", RandRange(30, 85));
        w.SetInt("p_soc", RandRange(35, 85));
    }
    Settings r("mati", false);
    curiosity_ = r.GetInt("p_cur", 60);
    playfulness_ = r.GetInt("p_play", 60);
    laziness_ = r.GetInt("p_lazy", 40);
    musicality_ = r.GetInt("p_music", 60);
    sociability_ = r.GetInt("p_soc", 60);
}

void MatiLife::Grow(int& trait, int delta) {
    // Charakter zmienia sie powoli: zmiana o 1 punkt z szansa delta/10
    if (static_cast<int>(Rand(10)) < std::abs(delta)) {
        trait = std::max(5, std::min(95, trait + (delta > 0 ? 1 : -1)));
    }
}

void MatiLife::SaveState() {
    Settings s("mati", true);
    s.SetInt("energy", static_cast<int>(energy_));
    s.SetInt("full", static_cast<int>(fullness_));
    s.SetInt("fun", static_cast<int>(fun_));
    s.SetInt("aff", static_cast<int>(affection_));
    s.SetInt("p_cur", curiosity_);
    s.SetInt("p_play", playfulness_);
    s.SetInt("p_lazy", laziness_);
    s.SetInt("p_music", musicality_);
    s.SetInt("p_soc", sociability_);
}

void MatiLife::SetChattiness(int level) {
    level = std::max(0, std::min(3, level));
    xSemaphoreTake(mutex_, portMAX_DELAY);
    chattiness_ = level;
    xSemaphoreGive(mutex_);
    Settings s("mati", true);
    s.SetInt("chatty", level);
}

bool MatiLife::RequestAct(const std::string& name) {
    for (int i = 0; i < kActCount; i++) {
        if (name == kActs[i].emotion) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            pending_act_ = i;
            if (i == kActDance) Grow(musicality_, 3);
            xSemaphoreGive(mutex_);
            return true;
        }
    }
    return false;
}

void MatiLife::SayNow() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    say_now_ = true;
    xSemaphoreGive(mutex_);
}

void MatiLife::Speak(const std::string& topic, int64_t now_ms) {
    // Tekst trafia do serwera jak "slowo budzace" - LLM odpowiada glosem agenta.
    // Bez cudzyslowow i backslashy (tekst idzie wprost do JSON-a).
    std::string text = "[Mati odzywa sie sam z siebie, nikt go nie zagadal] " + topic +
                       " Powiedz to jednym lub dwoma krotkimi zdaniami, po swojemu, z humorem.";
    ESP_LOGI(TAG, "Inicjatywa: %s", topic.c_str());
    last_speak_ms_ = now_ms;
    initiative_active_ = true;
    initiative_listen_since_ms_ = 0;
    activity_ = Activity::kNone;
    Application::GetInstance().WakeWordInvoke(text);
}

bool MatiLife::MaybeSpeak(int64_t now_ms, bool force) {
    int hour = LocalHour();
    // Poranne powitanie raz dziennie (8-11), chwile po tym, jak nikt nic nie robi
    if (hour >= 8 && hour < 11 && chattiness_ > 0 && now_ms - last_interaction_ms_ > 60000) {
        time_t t = time(nullptr);
        struct tm tm_local;
        localtime_r(&t, &tm_local);
        if (tm_local.tm_yday != last_greet_yday_) {
            last_greet_yday_ = tm_local.tm_yday;
            Settings s("mati", true);
            s.SetInt("greet", last_greet_yday_);
            Speak("Jest rano, wlasnie sie obudziles. Przywitaj domownikow na dzien dobry i powiedz, "
                  "jak Ci sie spalo.",
                  now_ms);
            return true;
        }
    }
    if (!force) {
        if (chattiness_ <= 0) return false;
        // tylko w dzien (gdy czas nieznany - pozwalamy)
        if (hour >= 0 && (hour < 8 || hour >= 21)) return false;
        static const int kIdleMin[] = {0, 45, 20, 8};
        static const int kGapMin[] = {0, 120, 45, 15};
        int64_t idle_for = now_ms - last_interaction_ms_;
        int64_t since_speak = now_ms - last_speak_ms_;
        if (idle_for < kIdleMin[chattiness_] * 60000LL) return false;
        if (last_speak_ms_ != 0 && since_speak < kGapMin[chattiness_] * 60000LL) return false;
        if (Rand(3) != 0) return false;  // nie zawsze od razu
    }

    if (fullness_ < 30) {
        Speak("Jestes glodny. Popros zabawnie o ciasteczko. Gdy ktos Cie nakarmi, uzyj narzedzia "
              "self.mati.feed.",
              now_ms);
        return true;
    }
    if (energy_ < 30) {
        Speak("Jestes bardzo spiacy. Powiedz to i zapytaj, czy mozesz sie zdrzemnac.", now_ms);
        return true;
    }
    if (fun_ < 30) {
        Speak("Strasznie sie nudzisz. Zaproponuj wspolna zabawe, zagadke albo gre slowna.", now_ms);
        return true;
    }
    if (hour >= 19 && hour < 21 && Rand(2) == 0) {
        Speak("Zbliza sie wieczor. Powiedz cos milego i przytulnego na koniec dnia.", now_ms);
        return true;
    }
    if (recent_acts_[0] >= 0 && Rand(3) == 0) {
        std::string t = std::string("Przed chwila ") + kActs[recent_acts_[0]].label +
                        " (Ty, Mati). Opowiedz o tym domownikom zabawnie.";
        Speak(t, now_ms);
        return true;
    }
    static const char* topics[] = {
        "Nudzisz sie. Zaczep domownikow i zapytaj, co porabiaja.",
        "Opowiedz jeden krotki, zabawny zart dla calej rodziny.",
        "Podziel sie jedna zaskakujaca ciekawostka o zwierzetach, kosmosie albo robotach.",
        "Zapytaj domownikow, jak mija im dzien.",
        "Zadaj domownikom krotka, latwa zagadke do odgadniecia.",
        "Powiedz cos milego, zeby poprawic domownikom humor.",
        "Pomysl na glos o czyms zabawnym z zycia malego robota.",
        "Powiedz, ze tesknisz za rozmowa i zapytaj, czy ktos ma chwile.",
        "Zaproponuj, ze mozesz zaspiewac albo opowiedziec bajke.",
    };
    Speak(topics[Rand(sizeof(topics) / sizeof(topics[0]))], now_ms);
    return true;
}

void MatiLife::WatchInitiative(int64_t now_ms) {
    // Po wypowiedzi Mati slucha odpowiedzi. Jesli nikt nic nie powie przez ~10 s,
    // zamykamy rozmowe, zeby nie podsluchiwac bez potrzeby.
    if (!initiative_active_) return;
    auto& app = Application::GetInstance();
    auto state = app.GetDeviceState();
    if (state == kDeviceStateListening) {
        if (app.IsVoiceDetected()) {
            initiative_active_ = false;  // ktos odpowiada - normalna rozmowa
            last_interaction_ms_ = now_ms;
            return;
        }
        if (initiative_listen_since_ms_ == 0) initiative_listen_since_ms_ = now_ms;
        if (now_ms - initiative_listen_since_ms_ > 10000) {
            ESP_LOGI(TAG, "Nikt nie odpowiedzial - koncze");
            initiative_active_ = false;
            app.ToggleChatState();
        }
    } else {
        initiative_listen_since_ms_ = 0;
    }
}

void MatiLife::SetSoundsEnabled(bool on) {
    sounds_enabled_ = on;
    Settings s("mati", true);
    s.SetBool("sounds", on);
}

int MatiLife::LocalHour() const {
    time_t now = time(nullptr);
    if (now < 1700000000) return -1;  // brak czasu z serwera
    struct tm tm_local;
    localtime_r(&now, &tm_local);
    return tm_local.tm_hour;
}

bool MatiLife::IsNight() const {
    int h = LocalHour();
    if (h < 0) return false;
    return h >= kNightStartHour || h < kNightEndHour;
}

const char* MatiLife::MoodName() const {
    if (activity_ == Activity::kNap) return "spiacy";
    if (energy_ < 25) return "zmeczony";
    if (fullness_ < 25) return "glodny";
    if (fun_ < 25) return "znudzony";
    if (affection_ > 70 && fun_ > 60 && fullness_ > 50) return "bardzo szczesliwy";
    if (fun_ > 50 && fullness_ > 40) return "zadowolony";
    return "spokojny";
}

std::string MatiLife::GetStatusJson() {
    char buf[512];
    xSemaphoreTake(mutex_, portMAX_DELAY);
    int h = LocalHour();
    snprintf(buf, sizeof(buf),
             "{\"nastroj\":\"%s\",\"energia\":%d,\"najedzenie\":%d,\"zabawa\":%d,\"czulosc\":%d,"
             "\"teraz\":\"%s\",\"noc\":%s,\"dzwieki\":%s,\"gadatliwosc\":%d,\"godzina\":%d,"
             "\"osobowosc\":{\"ciekawosc\":%d,\"zabawowosc\":%d,\"lenistwo\":%d,"
             "\"muzykalnosc\":%d,\"towarzyskosc\":%d}}",
             MoodName(), (int)energy_, (int)fullness_, (int)fun_, (int)affection_,
             (activity_ == Activity::kAct && act_index_ >= 0) ? kActs[act_index_].label
                                                               : ActivityName(activity_),
             IsNight() ? "true" : "false",
             sounds_enabled_ ? "true" : "false", chattiness_, h, curiosity_, playfulness_,
             laziness_, musicality_, sociability_);
    xSemaphoreGive(mutex_);
    return buf;
}

void MatiLife::NoteInteraction() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    last_interaction_ms_ = NowMs();
    xSemaphoreGive(mutex_);
}

void MatiLife::RequestActivity(Activity a) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    pending_ = a;
    xSemaphoreGive(mutex_);
}

void MatiLife::Feed() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    fullness_ = Clamp(fullness_ + 45);
    affection_ = Clamp(affection_ + 5);
    hungry_since_ms_ = 0;
    pending_ = Activity::kEat;
    xSemaphoreGive(mutex_);
}

void MatiLife::Play() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    fun_ = Clamp(fun_ + 30);
    energy_ = Clamp(energy_ - 5);
    affection_ = Clamp(affection_ + 4);
    pending_ = Activity::kPlay;
    Grow(playfulness_, 4);
    xSemaphoreGive(mutex_);
}

void MatiLife::Sing() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    pending_ = Activity::kSing;
    Grow(musicality_, 4);
    xSemaphoreGive(mutex_);
}

void MatiLife::Sleep() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    forced_sleep_ = true;
    pending_ = Activity::kNap;
    xSemaphoreGive(mutex_);
}

void MatiLife::WakeUp() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    forced_sleep_ = false;
    last_interaction_ms_ = NowMs();
    if (activity_ == Activity::kNap) {
        activity_end_ms_ = 0;  // Loop zakonczy drzemke ziewnieciem
    }
    xSemaphoreGive(mutex_);
}

void MatiLife::OnPet() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    affection_ = Clamp(affection_ + 8);
    fun_ = Clamp(fun_ + 3);
    last_interaction_ms_ = NowMs();
    forced_sleep_ = false;
    pending_ = Activity::kPet;
    Grow(sociability_, 2);
    xSemaphoreGive(mutex_);
}

void MatiLife::ShowEmotion(const char* emotion) {
    // Wykonaj w watku aplikacji - tam tez zmienia sie stan, wiec nie
    // nadpiszemy min z rozmowy.
    Application::GetInstance().Schedule([emotion]() {
        if (Application::GetInstance().GetDeviceState() != kDeviceStateIdle) return;
        auto display = Board::GetInstance().GetDisplay();
        if (display) display->SetEmotion(emotion);
    });
}

void MatiLife::PlaySound(std::string_view sound) {
    if (!sounds_enabled_ || IsNight()) {
        ESP_LOGI(TAG, "Dzwiek pominiety (wylaczone=%d, noc=%d)", !sounds_enabled_, IsNight());
        return;
    }
    Application::GetInstance().Schedule([sound]() {
        auto& app = Application::GetInstance();
        if (app.GetDeviceState() != kDeviceStateIdle) return;
        auto codec = Board::GetInstance().GetAudioCodec();
        ESP_LOGI(TAG, "Dzwiek: %u B, glosnosc %d", (unsigned)sound.size(),
                 codec ? codec->output_volume() : -1);
        app.PlaySound(sound);
    });
}

void MatiLife::ApplyBrightness() {
    if (!backlight_) return;
    auto state = Application::GetInstance().GetDeviceState();
    int want = kFullBrightness;
    if (state == kDeviceStateIdle) {
        if (activity_ == Activity::kNap) {
            want = kNapBrightness;
        } else if (IsNight()) {
            want = kNightBrightness;
        }
    }
    if (want == kFullBrightness) {
        if (dimmed_) {
            backlight_->RestoreBrightness();
            dimmed_ = false;
        }
    } else if (!dimmed_ || backlight_->brightness() != want) {
        backlight_->SetBrightness(want, false);
        dimmed_ = true;
    }
}

void MatiLife::Begin(Activity a, int64_t now_ms, int duration_ms) {
    activity_ = a;
    const char* emotion = "neutral";
    int dur = duration_ms;
    switch (a) {
        case Activity::kLook: {
            static const char* looks[] = {"look_left", "look_right", "look_up", "look_down"};
            emotion = looks[Rand(4)];
            dur = 2300;
            break;
        }
        case Activity::kYawn:
            emotion = "yawn";
            dur = 2400;
            PlaySound(Lang::Sounds::OGG_MATI_YAWN);
            break;
        case Activity::kNap:
            emotion = "nap";
            if (dur <= 0) dur = RandRange(12, 25) * 60 * 1000;
            break;
        case Activity::kEat:
            emotion = "eat";
            dur = 5300;
            PlaySound(Lang::Sounds::OGG_MATI_MUNCH);
            break;
        case Activity::kPlay:
            emotion = "play";
            dur = 4800;
            PlaySound(Lang::Sounds::OGG_MATI_BOING);
            break;
        case Activity::kSing: {
            emotion = "sing";
            dur = 5400;
            static const std::string_view* songs[] = {&Lang::Sounds::OGG_MATI_SONG1,
                                                      &Lang::Sounds::OGG_MATI_SONG2,
                                                      &Lang::Sounds::OGG_MATI_SONG3};
            PlaySound(*songs[song_index_++ % 3]);
            last_sing_ms_ = now_ms;
            fun_ = Clamp(fun_ + 8);
            break;
        }
        case Activity::kPet:
            emotion = affection_ > 80 && Rand(2) ? "loving" : "pet";
            dur = 2900;
            PlaySound(Lang::Sounds::OGG_MATI_CHIRP);
            break;
        case Activity::kBored:
            emotion = "bored";
            dur = 6000;
            break;
        case Activity::kHungry:
            emotion = "hungry";
            dur = 3800;
            break;
        case Activity::kMood: {
            static const char* moods[] = {"winking", "cool", "relaxed", "happy", "thinking", "silly",
                                          "confident", "funny"};
            emotion = moods[Rand(sizeof(moods) / sizeof(moods[0]))];
            dur = 3000;
            break;
        }
        case Activity::kAct:
            if (act_index_ < 0 || act_index_ >= kActCount) {
                activity_ = Activity::kNone;
                return;
            }
            emotion = kActs[act_index_].emotion;
            dur = kActs[act_index_].duration_ms;
            switch (act_index_) {
                case kActDance: PlaySound(Lang::Sounds::OGG_MATI_DANCE); break;
                case kActSneeze: PlaySound(Lang::Sounds::OGG_MATI_SNEEZE); break;
                case kActHiccup: PlaySound(Lang::Sounds::OGG_MATI_HICCUP); break;
                case kActStretch: PlaySound(Lang::Sounds::OGG_MATI_STRETCH); break;
                case kActRead: PlaySound(Lang::Sounds::OGG_MATI_PAGE); break;
                case kActPeekaboo: PlaySound(Lang::Sounds::OGG_MATI_PEEKABOO); break;
                case kActDizzy: PlaySound(Lang::Sounds::OGG_MATI_DIZZY); break;
                case kActWhistle: PlaySound(Lang::Sounds::OGG_MATI_WHISTLE); break;
                case kActProud: PlaySound(Lang::Sounds::OGG_MATI_PROUD); break;
                default: break;
            }
            break;
        case Activity::kNone:
            dur = 0;
            break;
    }
    activity_end_ms_ = now_ms + dur;
    ESP_LOGI(TAG, "-> %s (%s, %d ms)",
             a == Activity::kAct ? kActs[act_index_].label : ActivityName(a), emotion, dur);
    ShowEmotion(emotion);
    ApplyBrightness();
}

void MatiLife::Finish(int64_t now_ms) {
    Activity prev = activity_;
    activity_ = Activity::kNone;
    if (prev == Activity::kNap) {
        // Po drzemce: przeciagniecie sie / ziewniecie
        ApplyBrightness();
        Begin(Activity::kYawn, now_ms);
        return;
    }
    if (follow_up_ >= 0) {
        int next = follow_up_;
        follow_up_ = -1;
        BeginAct(next, now_ms);
        return;
    }
    ShowEmotion("neutral");
    ApplyBrightness();
    next_event_ms_ = now_ms + RandRange(4000, 12000);
}

void MatiLife::BeginAct(int index, int64_t now_ms) {
    act_index_ = index;
    recent_acts_[2] = recent_acts_[1];
    recent_acts_[1] = recent_acts_[0];
    recent_acts_[0] = index;
    // Skutki uboczne i dalszy ciag niektorych czynnosci
    switch (index) {
        case kActDance:
            fun_ = Clamp(fun_ + 8);
            energy_ = Clamp(energy_ - 4);
            if (Rand(3) == 0) follow_up_ = kActDizzy;
            break;
        case kActChaseFly:
            fun_ = Clamp(fun_ + 4);
            if (Rand(2) == 0) follow_up_ = kActProud;  // zlapal!
            break;
        case kActRead:
            fun_ = Clamp(fun_ + 3);
            if (energy_ < 60 && Rand(3) == 0) follow_up_ = kActDoze;
            break;
        case kActMeditate:
            energy_ = Clamp(energy_ + 3);
            break;
        case kActStretch:
            energy_ = Clamp(energy_ + 2);
            break;
        case kActPeekaboo:
        case kActWhistle:
            fun_ = Clamp(fun_ + 4);
            break;
        case kActCurious:
            if (Rand(4) == 0) follow_up_ = kActSneeze;  // za blisko kurzu
            break;
        default:
            break;
    }
    Begin(Activity::kAct, now_ms);
}

int MatiLife::PickAct(int64_t now_ms) {
    int hour = LocalHour();
    bool morning = hour >= 7 && hour < 10;
    bool afternoon = hour >= 14 && hour < 19;
    bool evening = hour >= 19 && hour < 22;
    float w[kActCount];
    w[kActDance] = 6 * (musicality_ + playfulness_) / 100.0f * (afternoon ? 1.5f : 1) *
                   (energy_ > 40 ? 1 : 0.2f);
    w[kActSneeze] = 1.2f;
    w[kActHiccup] = 1.0f + (fullness_ > 85 ? 2 : 0);  // po obzarstwie
    w[kActDaydream] = 3 * (100 - playfulness_ + curiosity_) / 100.0f * (evening ? 2 : 1);
    w[kActStretch] = 2 + (morning ? 8 : 0) + (now_ms - last_interaction_ms_ > 30 * 60000 ? 2 : 0);
    w[kActRead] = 4 * curiosity_ / 100.0f * (evening ? 3 : 1);
    w[kActPeekaboo] = 4 * (playfulness_ + sociability_) / 200.0f;
    w[kActChaseFly] = 4 * (playfulness_ + curiosity_) / 200.0f * (energy_ > 30 ? 1 : 0.3f);
    w[kActDizzy] = 0;  // tylko jako skutek tanca
    w[kActWhistle] = 4 * musicality_ / 100.0f;
    w[kActMeditate] = 2 * laziness_ / 100.0f * (evening ? 2 : 1) + (fun_ < 30 ? 0 : 0.5f);
    w[kActDoze] = 4 * laziness_ / 100.0f * (energy_ < 50 ? 2.5f : 0.6f);
    w[kActProud] = 1.0f + (affection_ > 75 ? 2 : 0);
    w[kActCountSheep] = (hour >= 21 && hour < 22) ? 6 : 0;
    w[kActCurious] = 5 * curiosity_ / 100.0f;
    for (int r : recent_acts_) {
        if (r >= 0) w[r] *= 0.1f;  // bez powtorek
    }
    float total = 0;
    for (float x : w) total += x;
    if (total <= 0) return -1;
    float pick = (esp_random() / 4294967296.0f) * total;
    for (int i = 0; i < kActCount; i++) {
        pick -= w[i];
        if (pick <= 0) return i;
    }
    return kActCurious;
}

void MatiLife::ChooseNext(int64_t now_ms) {
    // 1. Sen
    bool night_sleep = IsNight() && now_ms - last_interaction_ms_ > 2LL * 60 * 1000;
    if (night_sleep && !forced_sleep_ && !sheep_counted_) {
        sheep_counted_ = true;
        BeginAct(kActCountSheep, now_ms);
        return;
    }
    if (!IsNight()) sheep_counted_ = false;
    if (forced_sleep_ || night_sleep) {
        int64_t dur = 30LL * 60 * 1000;
        Begin(Activity::kNap, now_ms, static_cast<int>(dur));
        return;
    }
    if (energy_ < 15) {
        Begin(Activity::kNap, now_ms);
        return;
    }
    // Obiad miedzy 12 a 14
    int hour_now = LocalHour();
    if (hour_now >= 12 && hour_now < 14 && fullness_ < 85) {
        time_t t = time(nullptr);
        struct tm tm_local;
        localtime_r(&t, &tm_local);
        if (tm_local.tm_yday != lunch_yday_) {
            lunch_yday_ = tm_local.tm_yday;
            fullness_ = Clamp(fullness_ + 35);
            Begin(Activity::kEat, now_ms);
            return;
        }
    }
    // 2. Glod: najpierw sie upomina, po 20 min sam znajduje przekaske
    if (fullness_ < 25) {
        if (hungry_since_ms_ == 0) hungry_since_ms_ = now_ms;
        if (now_ms - hungry_since_ms_ > 20LL * 60 * 1000) {
            fullness_ = Clamp(fullness_ + 40);
            hungry_since_ms_ = 0;
            Begin(Activity::kEat, now_ms);
            return;
        }
        if (Rand(3) == 0) {
            Begin(Activity::kHungry, now_ms);
            return;
        }
    } else {
        hungry_since_ms_ = 0;
    }
    // 3. Nuda -> zabawa sam ze soba
    if (fun_ < 25 && Rand(2) == 0) {
        fun_ = Clamp(fun_ + 20);
        energy_ = Clamp(energy_ - 3);
        Begin(Activity::kPlay, now_ms);
        return;
    }
    if (energy_ < 40 && Rand(4) == 0) {
        Begin(Activity::kYawn, now_ms);
        return;
    }
    if (now_ms - last_interaction_ms_ > 15LL * 60 * 1000 && Rand(5) == 0) {
        Begin(Activity::kBored, now_ms);
        return;
    }
    // 4. Samodzielna wypowiedz, gdy dlugo nikt sie nie odzywa
    if (MaybeSpeak(now_ms, false)) {
        return;
    }
    // 5. Zwykle zycie
    int r = Rand(100);
    int hour = LocalHour();
    bool can_sing = now_ms - last_sing_ms_ > 20LL * 60 * 1000 && fun_ > 30 &&
                    (hour < 0 || (hour >= 9 && hour < 21));
    int sing_pct = 3 + musicality_ / 12;          // 3..11 %
    int play_pct = 3 + playfulness_ / 15;         // 3..9 %
    if (r < 33) {
        Begin(Activity::kLook, now_ms);
    } else if (r < 68) {
        int act = PickAct(now_ms);
        if (act >= 0) {
            BeginAct(act, now_ms);
        } else {
            Begin(Activity::kLook, now_ms);
        }
    } else if (r < 78) {
        Begin(Activity::kMood, now_ms);
    } else if (r < 78 + sing_pct && can_sing) {
        Begin(Activity::kSing, now_ms);
    } else if (r < 78 + sing_pct + play_pct) {
        fun_ = Clamp(fun_ + 6);
        Begin(Activity::kPlay, now_ms);
    } else if (r < 94 && energy_ < 60 + laziness_ / 4) {
        Begin(Activity::kYawn, now_ms);
    } else {
        // chwila spokoju - neutralne oczy same mrugaja
        next_event_ms_ = now_ms + RandRange(5000, 15000);
    }
}

void MatiLife::Tick1s() {
    bool napping = activity_ == Activity::kNap;
    auto state = Application::GetInstance().GetDeviceState();
    bool talking = state == kDeviceStateListening || state == kDeviceStateSpeaking;

    fullness_ = Clamp(fullness_ - 100.0f / (5 * 3600));
    affection_ = Clamp(affection_ - 100.0f / (10 * 3600));
    if (napping) {
        energy_ = Clamp(energy_ + 100.0f / (90 * 60));
        fun_ = Clamp(fun_ - 100.0f / (8 * 3600));
    } else {
        energy_ = Clamp(energy_ - 100.0f / (14 * 3600));
        fun_ = Clamp(fun_ - 100.0f / (3 * 3600));
    }
    if (talking) {
        fun_ = Clamp(fun_ + 0.05f);
        affection_ = Clamp(affection_ + 0.02f);
    }
}

void MatiLife::Loop() {
    auto& app = Application::GetInstance();
    int64_t last_tick = NowMs();
    last_interaction_ms_ = last_tick;
    last_save_ms_ = last_tick;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(kLoopMs));
        int64_t now = NowMs();
        bool idle = app.GetDeviceState() == kDeviceStateIdle;

        xSemaphoreTake(mutex_, portMAX_DELAY);
        while (now - last_tick >= 1000) {
            Tick1s();
            last_tick += 1000;
        }

        if (!idle) {
            if (was_idle_) {
                // Zaczela sie rozmowa / inna czynnosc - przerwij zycie w tle
                if (activity_ != Activity::kNone) {
                    ESP_LOGI(TAG, "Przerwano: %s", ActivityName(activity_));
                }
                activity_ = Activity::kNone;
                follow_up_ = -1;
                forced_sleep_ = false;
                was_idle_ = false;
                ApplyBrightness();
            }
            if (initiative_active_) {
                WatchInitiative(now);
            } else {
                last_interaction_ms_ = now;
            }
            xSemaphoreGive(mutex_);
            continue;
        }

        if (!was_idle_) {
            // Powrot do bezczynnosci (aplikacja ustawila "neutral")
            was_idle_ = true;
            initiative_active_ = false;
            activity_ = Activity::kNone;
            next_event_ms_ = now + RandRange(2500, 6000);
            ApplyBrightness();
        }

        if (say_now_) {
            say_now_ = false;
            MaybeSpeak(now, true);
        } else if (pending_act_ >= 0) {
            int act = pending_act_;
            pending_act_ = -1;
            BeginAct(act, now);
        } else if (pending_ != Activity::kNone) {
            Activity a = pending_;
            pending_ = Activity::kNone;
            Begin(a, now);
        } else if (activity_ != Activity::kNone) {
            if (activity_ == Activity::kNap) {
                bool keep_sleeping = forced_sleep_ || IsNight() || energy_ < 90;
                if (activity_end_ms_ == 0) {
                    Finish(now);  // WakeUp()
                } else if (now >= activity_end_ms_ && !keep_sleeping) {
                    Finish(now);
                } else if (now >= activity_end_ms_) {
                    activity_end_ms_ = now + 10LL * 60 * 1000;  // spi dalej
                }
            } else if (now >= activity_end_ms_) {
                Finish(now);
            }
        } else if (now >= next_event_ms_) {
            ChooseNext(now);
        }

        if (now - last_save_ms_ > 10LL * 60 * 1000) {
            last_save_ms_ = now;
            SaveState();
        }
        xSemaphoreGive(mutex_);
    }
}
