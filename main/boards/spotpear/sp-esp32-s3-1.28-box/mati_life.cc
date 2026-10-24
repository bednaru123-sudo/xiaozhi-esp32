#include "mati_life.h"

#include "application.h"
#include "assets/lang_config.h"
#include "audio_codec.h"
#include "backlight.h"
#include "board.h"
#include "display/display.h"
#include "mati_memory.h"
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
constexpr int kGameLoopMs = 20;
constexpr int kFullBrightness = -1;  // = jasnosc zapisana przez uzytkownika
constexpr int kNightBrightness = 25;
constexpr int kNapBrightness = 6;
constexpr int kNightStartHour = 22;
constexpr int kNightEndHour = 7;
constexpr int kWeekendNightEndHour = 9;
constexpr int kTapResolveMs = 400;
constexpr int kReactionRounds = 3;
constexpr int kNoResult = 99999;
constexpr size_t kMaxSpeakLen = 20;  // dluzsze frazy serwer odrzuca

inline float Clamp(float v) { return std::max(0.0f, std::min(100.0f, v)); }

inline uint32_t Rand(uint32_t n) { return n ? esp_random() % n : 0; }

inline int RandRange(int lo, int hi) { return lo + static_cast<int>(Rand(hi - lo + 1)); }

bool LocalTm(struct tm* out) {
    time_t now = time(nullptr);
    if (now < 1700000000) return false;  // brak czasu z serwera
    localtime_r(&now, out);
    return true;
}

const char* ActivityName(MatiLife::Activity a) {
    switch (a) {
        case MatiLife::Activity::kNone: return "nic";
        case MatiLife::Activity::kLook: return "rozglada sie";
        case MatiLife::Activity::kYawn: return "ziewa";
        case MatiLife::Activity::kNap: return "spi";
        case MatiLife::Activity::kEat: return "je ciasteczko";
        case MatiLife::Activity::kPlay: return "bawi sie pilka";
        case MatiLife::Activity::kSing: return "spiewa";
        case MatiLife::Activity::kPet: return "jest glaskany";
        case MatiLife::Activity::kBored: return "nudzi sie";
        case MatiLife::Activity::kHungry: return "jest glodny";
        case MatiLife::Activity::kMood: return "robi miny";
        case MatiLife::Activity::kAct: return "cos robi";
        case MatiLife::Activity::kGame: return "gra w gre";
    }
    return "?";
}

// Tabela czynnosci. Czynnosci "luzem" wybiera PickAct() (waga bazowa x cecha
// charakteru x pora dnia x "dawno tego nie robil"), czynnosci dnia (posilki,
// obowiazki) uruchamia RunRoutines(), reakcje - dotyk i gry.
enum ActId {
    kActDance,
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
    kActBreakfast,
    kActSnack,
    kActLunch,
    kActCake,
    kActSupper,
    kActDrink,
    kActBrushTeeth,
    kActWaterPlant,
    kActWorkout,
    kActSweep,
    kActGoodnight,
    kActSunrise,
    kActParty,
    kActTickle,
    kActNoThanks,
    kActTrophy,
    kActJuggle,
    kActBubbles,
    kActPaint,
    kActGuitar,
    kActDrums,
    kActCook,
    kActSelfie,
    kActSpy,
    kActBoo,
    kActGlitch,
    kActGum,
    kActMagic,
    kActTyping,
    kActBird,
    kActBalloon,
    kActRubik,
    kActRain,
    kActSnow,
    kActKite,
    kActFishing,
    kActHula,
    kActWish,
    kActFlower,
    kActClock,
    kActLove,
    kActCharge,
    kActIceCream,
    kActMovie,
    kActPiano,
    kActButterfly,
    kActPlanet,
    kActTea,
    kActYoga,
    kActTrampoline,
    kActMirror,
    kActWaveHi,
    kActMoodWink,
    kActMoodCool,
    kActMoodRelaxed,
    kActMoodHappy,
    kActMoodThinking,
    kActMoodSilly,
    kActMoodConfident,
    kActMoodFunny,
    kActMoodLaughing,
    kActMoodEmbarrassed,
    kActMoodKissy,
    kActMoodSurprised,
    kActMoodConfused,
    kActMoodSad,
    kActMoodLoving,
    kActCount
};

// Cecha charakteru, ktora wzmacnia czynnosc
enum Trait : uint8_t { kNone, kCur, kPlay, kMusic, kSoc, kLazy, kActive };
// Pora dnia, w ktorej czynnosc jest czestsza
constexpr uint8_t kTodM = 1;  // rano 6-11
constexpr uint8_t kTodD = 2;  // poludnie 11-14
constexpr uint8_t kTodA = 4;  // popoludnie 14-18
constexpr uint8_t kTodE = 8;  // wieczor 18-22

struct ActDef {
    const char* emotion;
    int duration_ms;
    const char* label;
    const std::string_view* sound;
    int food;     // ile dodaje do najedzenia
    float base;   // waga bazowa (0 = tylko z rytmu dnia / jako reakcja)
    Trait trait;
    uint8_t tod;
};

const ActDef kActs[kActCount] = {
    {"dance", 6300, "tanczy", &Lang::Sounds::OGG_MATI_DANCE, 0, 3.0f, kMusic, kTodA},
    {"sneeze", 2200, "kicha", &Lang::Sounds::OGG_MATI_SNEEZE, 0, 0.8f, kNone, 0},
    {"hiccup", 4200, "ma czkawke", &Lang::Sounds::OGG_MATI_HICCUP, 0, 0.6f, kNone, 0},
    {"daydream", 6600, "marzy patrzac w gwiazdy", nullptr, 0, 1.5f, kCur, kTodE},
    {"stretch", 2400, "przeciaga sie", &Lang::Sounds::OGG_MATI_STRETCH, 0, 1.2f, kActive, kTodM},
    {"read", 9300, "czyta ksiazke", &Lang::Sounds::OGG_MATI_PAGE, 0, 2.0f, kCur, kTodE},
    {"peekaboo", 2700, "bawi sie w a kuku", &Lang::Sounds::OGG_MATI_PEEKABOO, 0, 2.0f, kSoc, 0},
    {"chase_fly", 4400, "goni muche", nullptr, 0, 2.0f, kPlay, kTodA},
    {"dizzy", 2600, "kreci mu sie w glowie", &Lang::Sounds::OGG_MATI_DIZZY, 0, 0.0f, kNone, 0},
    {"whistle", 3500, "pogwizduje", &Lang::Sounds::OGG_MATI_WHISTLE, 0, 2.0f, kMusic, 0},
    {"meditate", 8400, "medytuje", nullptr, 0, 1.2f, kLazy, kTodE},
    {"doze", 4000, "przysypia", nullptr, 0, 1.2f, kLazy, kTodD|kTodA},
    {"proud", 2200, "jest z siebie dumny", &Lang::Sounds::OGG_MATI_PROUD, 0, 0.8f, kNone, 0},
    {"count_sheep", 6500, "liczy owce", nullptr, 0, 0.0f, kNone, 0},
    {"curious", 3100, "cos go zaciekawilo", nullptr, 0, 2.5f, kCur, 0},
    {"breakfast", 6600, "je sniadanie: tost z jajkiem i kakao", &Lang::Sounds::OGG_MATI_MUNCH, 35, 0.0f, kNone, 0},
    {"snack_apple", 4000, "je drugie sniadanie: jabluszko", &Lang::Sounds::OGG_MATI_MUNCH, 15, 0.0f, kNone, 0},
    {"lunch", 6600, "je obiad: zupe pomidorowa", &Lang::Sounds::OGG_MATI_GULP, 40, 0.0f, kNone, 0},
    {"cake", 3600, "je podwieczorek: kawalek ciasta", &Lang::Sounds::OGG_MATI_MUNCH, 15, 0.0f, kNone, 0},
    {"supper", 6000, "je kolacje: kanapke", &Lang::Sounds::OGG_MATI_MUNCH, 30, 0.0f, kNone, 0},
    {"drink", 4000, "pije wode", &Lang::Sounds::OGG_MATI_GULP, 3, 0.0f, kNone, 0},
    {"brush_teeth", 4800, "myje zeby", &Lang::Sounds::OGG_MATI_SCRUB, 0, 0.0f, kNone, 0},
    {"water_plant", 4000, "podlewa kwiatek", &Lang::Sounds::OGG_MATI_POUR, 0, 0.6f, kNone, kTodA},
    {"workout", 4800, "cwiczy z hantlami", &Lang::Sounds::OGG_MATI_STRETCH, 0, 1.2f, kActive, kTodM},
    {"sweep", 4800, "zamiata", &Lang::Sounds::OGG_MATI_SWISH, 0, 0.6f, kActive, kTodD|kTodA},
    {"goodnight", 5200, "mowi dobranoc", &Lang::Sounds::OGG_MATI_LULLABY, 0, 0.0f, kNone, 0},
    {"sunrise", 4000, "wita slonce", &Lang::Sounds::OGG_MATI_MORNING, 0, 0.0f, kNone, 0},
    {"party", 6300, "robi weekendowa imprezke", &Lang::Sounds::OGG_MATI_DANCE, 0, 0.0f, kNone, 0},
    {"tickle", 2600, "smieje sie od laskotek", &Lang::Sounds::OGG_MATI_GIGGLE, 0, 0.0f, kNone, 0},
    {"no_thanks", 1900, "odmawia, bo jest najedzony", &Lang::Sounds::OGG_MATI_NOPE, 0, 0.0f, kNone, 0},
    {"trophy", 3300, "cieszy sie z rekordu", &Lang::Sounds::OGG_MATI_TADA, 0, 0.0f, kNone, 0},
    {"juggle", 3800, "zongluje pilkami", &Lang::Sounds::OGG_MATI_BOING, 0, 2.0f, kPlay, kTodA},
    {"bubbles", 4900, "puszcza banki mydlane", &Lang::Sounds::OGG_MATI_POP, 0, 2.0f, kPlay, 0},
    {"paint", 5400, "maluje obrazek", nullptr, 0, 1.8f, kCur, kTodA},
    {"guitar", 5800, "gra na gitarze", &Lang::Sounds::OGG_MATI_STRUM, 0, 2.0f, kMusic, kTodA|kTodE},
    {"drums", 4300, "gra na bebnach", &Lang::Sounds::OGG_MATI_DRUMS, 0, 1.6f, kMusic, kTodA},
    {"cook", 4900, "gotuje jajecznice", &Lang::Sounds::OGG_MATI_SIZZLE, 5, 1.5f, kCur, kTodD|kTodE},
    {"selfie", 3200, "robi sobie selfie", &Lang::Sounds::OGG_MATI_CLICK, 0, 1.2f, kSoc, 0},
    {"spy", 4200, "bawi sie w szpiega", nullptr, 0, 1.2f, kPlay, kTodE},
    {"boo", 2900, "straszy jak duszek", &Lang::Sounds::OGG_MATI_BOO, 0, 1.2f, kPlay, kTodE},
    {"glitch", 2700, "ma maly blad systemu", &Lang::Sounds::OGG_MATI_BLEEP, 0, 0.5f, kNone, 0},
    {"gum", 3500, "zuje gume balonowa", &Lang::Sounds::OGG_MATI_POP, 0, 1.2f, kPlay, 0},
    {"magic", 4900, "pokazuje sztuczke magiczna", &Lang::Sounds::OGG_MATI_SPARKLE, 0, 1.5f, kCur, 0},
    {"typing", 4900, "programuje na komputerze", &Lang::Sounds::OGG_MATI_KEYS, 0, 1.5f, kCur, kTodM|kTodD},
    {"bird", 4000, "patrzy na ptaszka za oknem", &Lang::Sounds::OGG_MATI_TWEET, 0, 1.5f, kCur, kTodM},
    {"balloon", 4000, "puscil balonik i odlecial", nullptr, 0, 0.8f, kNone, 0},
    {"rubik", 4800, "uklada kostke Rubika", nullptr, 0, 1.5f, kCur, 0},
    {"rain", 4900, "chowa sie pod parasolem przed deszczem", &Lang::Sounds::OGG_MATI_RAIN, 0, 1.0f, kNone, 0},
    {"snow", 4000, "lapie platki sniegu", nullptr, 0, 0.3f, kNone, 0},
    {"kite", 3600, "puszcza latawca", &Lang::Sounds::OGG_MATI_SWISH, 0, 1.2f, kPlay, kTodA},
    {"fishing", 4400, "lowi ryby", &Lang::Sounds::OGG_MATI_SPLASH, 0, 1.2f, kLazy, kTodA},
    {"hula", 3400, "kreci hula hop", nullptr, 0, 1.5f, kActive, kTodA},
    {"wish", 4400, "widzi spadajaca gwiazde i mysli zyczenie", &Lang::Sounds::OGG_MATI_SPARKLE, 0, 1.2f, kCur, kTodE},
    {"flower", 4000, "wacha kwiatek", nullptr, 0, 1.2f, kNone, kTodM|kTodA},
    {"clock", 3800, "patrzy na zegar i czeka na domownikow", &Lang::Sounds::OGG_MATI_TICK, 0, 0.8f, kNone, 0},
    {"love", 2600, "czuje milosc do domownikow", &Lang::Sounds::OGG_MATI_HEARTBEAT, 0, 0.5f, kSoc, 0},
    {"charge", 4000, "laduje baterie", &Lang::Sounds::OGG_MATI_CHARGE, 0, 0.5f, kNone, 0},
    {"ice_cream", 3800, "je lody", &Lang::Sounds::OGG_MATI_MUNCH, 10, 0.8f, kPlay, kTodA},
    {"movie", 6000, "oglada film z popcornem", &Lang::Sounds::OGG_MATI_POP, 0, 1.2f, kLazy, kTodE},
    {"piano", 4800, "gra na pianinie", &Lang::Sounds::OGG_MATI_PIANO, 0, 1.8f, kMusic, 0},
    {"butterfly", 3600, "goni motyla", nullptr, 0, 1.5f, kPlay, kTodM|kTodA},
    {"planet", 3600, "obserwuje planete przez okno", nullptr, 0, 1.2f, kCur, kTodE},
    {"tea", 4300, "pije herbatke", &Lang::Sounds::OGG_MATI_GULP, 2, 1.0f, kLazy, kTodA|kTodE},
    {"yoga", 5000, "cwiczy joge", nullptr, 0, 1.2f, kActive, kTodM},
    {"trampoline", 2900, "skacze na trampolinie", &Lang::Sounds::OGG_MATI_BOING, 0, 1.5f, kPlay, kTodA},
    {"mirror", 4500, "przeglada sie w lusterku", nullptr, 0, 1.0f, kSoc, kTodM},
    {"wave_hi", 2800, "macha na powitanie", &Lang::Sounds::OGG_MATI_CHIRP, 0, 1.0f, kSoc, kTodM},
    {"winking", 3000, "puszcza oczko", nullptr, 0, 0.7f, kSoc, 0},
    {"cool", 3000, "udaje twardziela", nullptr, 0, 0.7f, kNone, 0},
    {"relaxed", 3000, "odpoczywa", nullptr, 0, 0.7f, kLazy, 0},
    {"happy", 3000, "cieszy sie", nullptr, 0, 0.8f, kNone, 0},
    {"thinking", 3000, "mysli o czyms", nullptr, 0, 0.7f, kCur, 0},
    {"silly", 3000, "robi glupie miny", nullptr, 0, 0.7f, kPlay, 0},
    {"confident", 3000, "jest pewny siebie", nullptr, 0, 0.5f, kNone, 0},
    {"funny", 3000, "wyglupia sie", nullptr, 0, 0.7f, kPlay, 0},
    {"laughing", 3000, "smieje sie", nullptr, 0, 0.6f, kPlay, 0},
    {"embarrassed", 3000, "jest zawstydzony", nullptr, 0, 0.4f, kNone, 0},
    {"kissy", 3000, "posyla buziaka", nullptr, 0, 0.4f, kSoc, 0},
    {"surprised", 3000, "jest zaskoczony", nullptr, 0, 0.5f, kNone, 0},
    {"confused", 3000, "jest zdezorientowany", nullptr, 0, 0.5f, kNone, 0},
    {"sad", 3000, "jest troche smutny", nullptr, 0, 0.2f, kNone, 0},
    {"loving", 3000, "patrzy z czuloscia", nullptr, 0, 0.4f, kSoc, 0},
};

static_assert(kActCount <= MatiLife::kMaxActs, "Zwieksz kMaxActs");

// Czynnosci dnia: [od, do) godziny, przesuniecie w weekend, maska dni (bit0 = niedziela)
struct Routine {
    int act;
    int from_h;
    int to_h;
    int weekend_shift;
    uint8_t days;
};

const Routine kRoutines[] = {
    {kActBreakfast, 7, 9, 2, 0x7F},
    {kActWorkout, 9, 12, 1, 0x7F},
    {kActSnack, 10, 12, 1, 0x7F},
    {kActSweep, 11, 17, 0, 0x7F},
    {kActLunch, 13, 15, 0, 0x7F},
    {kActWaterPlant, 15, 18, 0, 0x7F},
    {kActCake, 16, 18, 0, 0x7F},
    {kActSupper, 18, 20, 0, 0x7F},
    {kActParty, 19, 21, 0, (1 << 5) | (1 << 6)},  // piatek, sobota
};
constexpr int kRoutineCount = sizeof(kRoutines) / sizeof(kRoutines[0]);

}  // namespace

int64_t MatiLife::NowMs() { return esp_timer_get_time() / 1000; }

void MatiLife::Start(Backlight* backlight) {
    if (started_) return;
    started_ = true;
    backlight_ = backlight;
    mutex_ = xSemaphoreCreateMutex();
    LoadState();
    RegisterTools();
    MatiMemory::GetInstance().Start();
    xTaskCreate(TaskEntry, "mati_life", 4096, this, 2, nullptr);
    ESP_LOGI(TAG, "Mati ozyl: energia %.0f, najedzenie %.0f, zabawa %.0f", energy_, fullness_, fun_);
}

void MatiLife::RegisterTools() {
    auto& mcp = McpServer::GetInstance();
    mcp.AddTool("self.mati.get_status",
                "Sprawdz samopoczucie Matiego (robota-zwierzaka, czyli Ciebie): nastroj, energia, "
                "najedzenie, zabawa, czulosc (0-100), co aktualnie robi, osobowosc i rekordy w grach. "
                "Uzyj, gdy ktos pyta jak sie czujesz, co robiles, albo zanim zaproponujesz zabawe.",
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
    std::string do_desc =
        "Popros Matiego o konkretna czynnosc pokazana na ekranie po zakonczeniu rozmowy. "
        "Nazwy mowia, co to jest (np. guitar = gra na gitarze, cook = gotuje, movie = film). "
        "action: ";
    for (int i = 0; i < kActCount; i++) {
        if (i == kActTickle || i == kActNoThanks || i == kActTrophy || i == kActDizzy) continue;
        do_desc += kActs[i].emotion;
        do_desc += (i + 1 < kActCount) ? ", " : ".";
    }
    mcp.AddTool("self.mati.do", do_desc,
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

    // --- Gry ---
    mcp.AddTool("self.mati.roll_dice",
                "Rzuc kostka do gry (1-6). Wynik pojawia sie na ekranie Matiego. Uzyj do gier "
                "slownych, zgadywanek i planszowek.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    static const char* faces[] = {"dice_1", "dice_2", "dice_3",
                                                  "dice_4", "dice_5", "dice_6"};
                    int n = RandRange(1, 6);
                    ShowNow(faces[n - 1]);
                    xSemaphoreTake(mutex_, portMAX_DELAY);
                    pending_show_ = faces[n - 1];
                    xSemaphoreGive(mutex_);
                    return std::string("Wypadlo: ") + std::to_string(n);
                });
    mcp.AddTool("self.mati.flip_coin", "Rzuc moneta: orzel albo reszka. Wynik widac na ekranie.",
                PropertyList(), [this](const PropertyList&) -> ToolResult {
                    bool heads = Rand(2) == 0;
                    const char* e = heads ? "coin_heads" : "coin_tails";
                    ShowNow(e);
                    xSemaphoreTake(mutex_, portMAX_DELAY);
                    pending_show_ = e;
                    xSemaphoreGive(mutex_);
                    return std::string(heads ? "Orzel" : "Reszka");
                });
    mcp.AddTool("self.mati.rock_paper_scissors",
                "Zagraj z domownikiem w papier, kamien, nozyce. Najpierw zapytaj o wybor gracza, "
                "potem wywolaj z player = kamien, papier albo nozyce. Mati losuje swoj wybor, "
                "pokazuje go na ekranie, a narzedzie zwraca, kto wygral.",
                PropertyList({Property("player", kPropertyTypeString, std::string(""))}),
                [this](const PropertyList& props) -> ToolResult {
                    static const char* names[] = {"kamien", "papier", "nozyce"};
                    static const char* emos[] = {"rps_rock", "rps_paper", "rps_scissors"};
                    int m = Rand(3);
                    ShowNow(emos[m]);
                    xSemaphoreTake(mutex_, portMAX_DELAY);
                    pending_show_ = emos[m];
                    xSemaphoreGive(mutex_);
                    std::string p = props["player"].value<std::string>();
                    int pi = -1;
                    if (p.find("kam") != std::string::npos || p.find("rock") != std::string::npos) pi = 0;
                    if (p.find("pap") != std::string::npos || p.find("paper") != std::string::npos) pi = 1;
                    if (p.find("no") != std::string::npos || p.find("scis") != std::string::npos) pi = 2;
                    std::string r = std::string("Mati wybral: ") + names[m] + ".";
                    if (pi >= 0) {
                        if (pi == m) {
                            r += " Remis!";
                        } else if ((pi + 1) % 3 == m) {
                            r += " Wygrywa Mati!";
                        } else {
                            r += " Wygrywa gracz!";
                        }
                    }
                    return r;
                });
    mcp.AddTool("self.mati.start_game",
                "Wlacz gre na ekranie Matiego. game: reaction (refleks - stuknij w ekran, gdy oczy "
                "zrobia sie zielone i pojawi sie TERAZ, 3 rundy) albo tap_race (stukaj w ekran ile "
                "sie da przez 5 sekund). Gra rusza zaraz po Twojej odpowiedzi - rozmowa sie konczy, "
                "wiec wyjasnij krotko zasady.",
                PropertyList({Property("game", kPropertyTypeString)}),
                [this](const PropertyList& props) -> ToolResult {
                    if (!RequestGame(props["game"].value<std::string>())) {
                        return std::unexpected(std::string("Nieznana gra"));
                    }
                    return true;
                });
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
    day_yday_ = s.GetInt("day", -1);
    routines_done_ = static_cast<uint32_t>(s.GetInt("rdone", 0));
    best_reaction_ms_ = s.GetInt("best_rt", 0);
    best_taps_ = s.GetInt("best_taps", 0);
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

void MatiLife::CheckNewDay() {
    struct tm tm_local;
    if (!LocalTm(&tm_local)) return;
    if (tm_local.tm_yday != day_yday_) {
        day_yday_ = tm_local.tm_yday;
        routines_done_ = 0;
        Settings s("mati", true);
        s.SetInt("day", day_yday_);
        s.SetInt("rdone", 0);
    }
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
            if (i == kActDance || i == kActParty) Grow(musicality_, 3);
            fullness_ = Clamp(fullness_ + kActs[i].food);
            xSemaphoreGive(mutex_);
            return true;
        }
    }
    return false;
}

bool MatiLife::RequestGame(const std::string& name) {
    Game g = Game::kNone;
    if (name.find("reac") != std::string::npos || name.find("refl") != std::string::npos) {
        g = Game::kReactWait;
    } else if (name.find("tap") != std::string::npos || name.find("stuk") != std::string::npos) {
        g = Game::kTapRace;
    }
    if (g == Game::kNone) return false;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    pending_game_ = g;
    close_after_reply_ = true;
    xSemaphoreGive(mutex_);
    return true;
}

void MatiLife::SayNow() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    say_now_ = true;
    xSemaphoreGive(mutex_);
}

void MatiLife::Speak(const std::string& phrase, int64_t now_ms) {
    // Serwer Xiaozhi przyjmuje tu tylko krotkie "slowo budzace" (dlugie teksty odrzuca
    // bledem "Detect is only for wake words"). Wysylamy wiec krotka fraze, ktora brzmi
    // jak zaczepka domownika - LLM odpowiada na nia jako Mati, swoim glosem.
    std::string text = phrase;
    if (text.size() > kMaxSpeakLen) text.resize(kMaxSpeakLen);
    ESP_LOGI(TAG, "Inicjatywa: %s", text.c_str());
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
        struct tm tm_local;
        if (LocalTm(&tm_local) && tm_local.tm_yday != last_greet_yday_) {
            last_greet_yday_ = tm_local.tm_yday;
            Settings s("mati", true);
            s.SetInt("greet", last_greet_yday_);
            Speak("Dzien dobry, Mati!", now_ms);
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

    // Krotkie frazy (max ~20 znakow), dobrane do potrzeb, pamieci i pory dnia
    if (fullness_ < 30) {
        Speak("Mati, jestes glodny?", now_ms);
        return true;
    }
    if (energy_ < 30) {
        Speak("Chce ci sie spac?", now_ms);
        return true;
    }
    if (fun_ < 30) {
        Speak("Zagrajmy w cos!", now_ms);
        return true;
    }
    int known = MatiMemory::GetInstance().Count();
    if (known > 0 && Rand(100) < 30) {
        Speak(Rand(2) ? "Co o mnie pamietasz?" : "Co u mnie slychac?", now_ms);
        return true;
    }
    if (Rand(100) < (known < 10 ? 35 : 12)) {
        Speak(Rand(2) ? "Poznajmy sie lepiej" : "Zapytaj mnie o cos", now_ms);
        return true;
    }
    if (recent_acts_[0] >= 0 && Rand(4) == 0) {
        Speak("Mati, co robiles?", now_ms);
        return true;
    }

    static const char* morning[] = {"Jakie mam plany?", "Pocwiczmy razem!", "Co na sniadanie?"};
    static const char* midday[] = {"Co dzis na obiad?", "Przypomnij o wodzie", "Krotka przerwa?"};
    static const char* evening[] = {"Jak minal dzien?", "Opowiedz bajke", "Co na kolacje?",
                                    "Powiedz cos milego"};
    static const char* games[] = {
        "Papier kamien nozyce", "Rzuc kostka", "Zagrajmy w refleks", "Zagrajmy w stukanie",
        "Zagrajmy w 20 pytan", "Wymyslmy historie", "Zadaj mi quiz", "Pobawmy sie w rymy",
        "Powiedz lamaniec", "Zagrajmy w kalambury", "Zadaj mi zagadke"};
    static const char* general[] = {
        "Opowiedz zart", "Jakas ciekawostka?", "Mati, co slychac?", "Powiedz cos milego",
        "O czym myslisz?", "Zaspiewaj cos", "Nudzisz sie, Mati?", "Co cie ciekawi?"};
    auto pick = [](const char* const* arr, size_t n) { return arr[Rand(n)]; };
    int r = Rand(100);
    if (IsWeekend() && r < 10) {
        Speak("Co robimy w weekend?", now_ms);
    } else if (r < 40) {
        Speak(pick(games, sizeof(games) / sizeof(games[0])), now_ms);
    } else if (r < 70 && hour >= 7 && hour < 11) {
        Speak(pick(morning, sizeof(morning) / sizeof(morning[0])), now_ms);
    } else if (r < 70 && hour >= 11 && hour < 17) {
        Speak(pick(midday, sizeof(midday) / sizeof(midday[0])), now_ms);
    } else if (r < 70 && hour >= 17) {
        Speak(pick(evening, sizeof(evening) / sizeof(evening[0])), now_ms);
    } else {
        Speak(pick(general, sizeof(general) / sizeof(general[0])), now_ms);
    }
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
    struct tm tm_local;
    if (!LocalTm(&tm_local)) return -1;
    return tm_local.tm_hour;
}

bool MatiLife::IsWeekend() const {
    struct tm tm_local;
    if (!LocalTm(&tm_local)) return false;
    return tm_local.tm_wday == 0 || tm_local.tm_wday == 6;
}

bool MatiLife::IsNight() const {
    int h = LocalHour();
    if (h < 0) return false;
    int end = IsWeekend() ? kWeekendNightEndHour : kNightEndHour;
    return h >= kNightStartHour || h < end;
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
    char buf[640];
    xSemaphoreTake(mutex_, portMAX_DELAY);
    int h = LocalHour();
    std::string eaten;
    for (int i = 0; i < kRoutineCount; i++) {
        if (routines_done_ & (1u << i)) {
            if (!eaten.empty()) eaten += ", ";
            eaten += kActs[kRoutines[i].act].emotion;
        }
    }
    snprintf(buf, sizeof(buf),
             "{\"nastroj\":\"%s\",\"energia\":%d,\"najedzenie\":%d,\"zabawa\":%d,\"czulosc\":%d,"
             "\"teraz\":\"%s\",\"noc\":%s,\"weekend\":%s,\"dzwieki\":%s,\"gadatliwosc\":%d,"
             "\"godzina\":%d,\"dzis_zrobione\":\"%s\","
             "\"osobowosc\":{\"ciekawosc\":%d,\"zabawowosc\":%d,\"lenistwo\":%d,"
             "\"muzykalnosc\":%d,\"towarzyskosc\":%d},"
             "\"rekordy\":{\"refleks_ms\":%d,\"stukniecia_5s\":%d},\"faktow_w_pamieci\":%d}",
             MoodName(), (int)energy_, (int)fullness_, (int)fun_, (int)affection_,
             (activity_ == Activity::kAct && act_index_ >= 0) ? kActs[act_index_].label
                                                               : ActivityName(activity_),
             IsNight() ? "true" : "false", IsWeekend() ? "true" : "false",
             sounds_enabled_ ? "true" : "false", chattiness_, h, eaten.c_str(), curiosity_,
             playfulness_, laziness_, musicality_, sociability_, best_reaction_ms_, best_taps_,
             MatiMemory::GetInstance().Count());
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

void MatiLife::OnTap() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    int64_t now = NowMs();
    last_interaction_ms_ = now;
    if (game_ != Game::kNone) {
        GameTap(now);
    } else {
        taps_++;
        last_tap_ms_ = now;
    }
    xSemaphoreGive(mutex_);
}

void MatiLife::OnSwipeSide() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    last_interaction_ms_ = NowMs();
    if (game_ == Game::kNone) {
        if (fullness_ < 90) {
            fullness_ = Clamp(fullness_ + 20);
            affection_ = Clamp(affection_ + 4);
            hungry_since_ms_ = 0;
            pending_ = Activity::kEat;
        } else {
            pending_act_ = kActNoThanks;
        }
    }
    xSemaphoreGive(mutex_);
}

void MatiLife::ResolveTaps(int64_t now_ms) {
    if (taps_ == 0 || now_ms - last_tap_ms_ < kTapResolveMs) return;
    int n = taps_;
    taps_ = 0;
    forced_sleep_ = false;
    if (n == 1) {
        // Glaskanie
        affection_ = Clamp(affection_ + 8);
        fun_ = Clamp(fun_ + 3);
        Grow(sociability_, 2);
        Begin(Activity::kPet, now_ms);
    } else if (n == 2) {
        MaybeSpeak(now_ms, true);
    } else {
        // Laskotki
        fun_ = Clamp(fun_ + 10);
        affection_ = Clamp(affection_ + 5);
        Grow(playfulness_, 2);
        BeginAct(kActTickle, now_ms);
    }
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

void MatiLife::ShowNow(const char* emotion) {
    // Pokaz od razu, takze w trakcie rozmowy (wynik kostki, monety...)
    Application::GetInstance().Schedule([emotion]() {
        auto display = Board::GetInstance().GetDisplay();
        if (display) display->SetEmotion(emotion);
    });
}

void MatiLife::Notify(const std::string& text, int ms) {
    Application::GetInstance().Schedule([text, ms]() {
        auto display = Board::GetInstance().GetDisplay();
        if (display) display->ShowNotification(text, ms);
    });
}

void MatiLife::PlaySound(std::string_view sound, bool allow_night) {
    if (!sounds_enabled_ || (IsNight() && !allow_night)) {
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
    if (state == kDeviceStateIdle && game_ == Game::kNone) {
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
            dur = dur > 0 ? dur : 6000;
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
        case Activity::kAct: {
            if (act_index_ < 0 || act_index_ >= kActCount) {
                activity_ = Activity::kNone;
                return;
            }
            const ActDef& def = kActs[act_index_];
            emotion = def.emotion;
            dur = def.duration_ms;
            if (def.sound) {
                // Kolysanka na dobranoc moze zagrac jeszcze przed 23
                bool night_ok = act_index_ == kActGoodnight && LocalHour() < 23;
                PlaySound(*def.sound, night_ok);
            }
            break;
        }
        case Activity::kGame:
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
        ApplyBrightness();
        if (night_nap_ && !IsNight()) {
            // Poranek: ziewniecie -> witanie slonca -> przeciaganie
            night_nap_ = false;
            follow_up_ = kActSunrise;
        }
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
    if (index < 0 || index >= kActCount) return;
    act_index_ = index;
    last_done_ms_[index] = now_ms;
    if (done_count_[index] < 65535) done_count_[index]++;
    recent_acts_[2] = recent_acts_[1];
    recent_acts_[1] = recent_acts_[0];
    recent_acts_[0] = index;
    // Skutki uboczne i dalszy ciag niektorych czynnosci
    switch (index) {
        case kActDance:
        case kActParty:
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
        case kActWorkout:
            energy_ = Clamp(energy_ - 3);
            fun_ = Clamp(fun_ + 4);
            follow_up_ = kActDrink;  // po cwiczeniach woda
            break;
        case kActPeekaboo:
        case kActWhistle:
            fun_ = Clamp(fun_ + 4);
            break;
        case kActCurious:
            if (Rand(4) == 0) follow_up_ = kActSneeze;  // za blisko kurzu
            break;
        case kActSweep:
            if (Rand(3) == 0) follow_up_ = kActSneeze;  // kurz!
            break;
        case kActBreakfast:
            follow_up_ = kActBrushTeeth;
            break;
        case kActSunrise:
            follow_up_ = kActStretch;
            break;
        case kActDrink:
        case kActTea:
            last_drink_ms_ = now_ms;
            break;
        case kActRubik:
        case kActMagic:
            if (Rand(2) == 0) follow_up_ = kActProud;
            break;
        case kActBalloon:
            follow_up_ = kActMoodSad;
            break;
        case kActBoo:
            follow_up_ = kActMoodLaughing;
            break;
        case kActGlitch:
            follow_up_ = kActMoodConfused;
            break;
        case kActTrampoline:
        case kActHula:
            fun_ = Clamp(fun_ + 5);
            energy_ = Clamp(energy_ - 2);
            if (Rand(3) == 0) follow_up_ = kActDizzy;
            break;
        case kActCharge:
            energy_ = Clamp(energy_ + 10);
            break;
        case kActFlower:
            if (Rand(4) == 0) follow_up_ = kActSneeze;  // pylek!
            break;
        case kActSelfie:
        case kActMirror:
            Grow(sociability_, 1);
            break;
        case kActGuitar:
        case kActPiano:
        case kActDrums:
            fun_ = Clamp(fun_ + 6);
            Grow(musicality_, 1);
            break;
        case kActJuggle:
        case kActBubbles:
        case kActKite:
        case kActButterfly:
        case kActGum:
            fun_ = Clamp(fun_ + 5);
            break;
        case kActPaint:
        case kActTyping:
        case kActBird:
        case kActPlanet:
            fun_ = Clamp(fun_ + 3);
            Grow(curiosity_, 1);
            break;
        default:
            break;
    }
    if (kActs[index].food > 0) {
        fullness_ = Clamp(fullness_ + kActs[index].food);
        hungry_since_ms_ = 0;
        if (fullness_ > 95 && Rand(3) == 0 && follow_up_ < 0) follow_up_ = kActHiccup;
    }
    Begin(Activity::kAct, now_ms);
}

int MatiLife::PickAct(int64_t now_ms) {
    // Sprawiedliwy wybor: waga bazowa x cecha charakteru x pora dnia x potrzeby
    // x "dawno tego nie robil". Czynnosc swiezo wykonana ma ~0 szans, a taka,
    // ktorej nie bylo od godzin (albo nigdy), dostaje kilkukrotny bonus - dzieki
    // temu wszystkie animacje przewijaja sie po kolei, a nie tylko kilka.
    struct tm tm_local;
    bool has_time = LocalTm(&tm_local);
    int hour = has_time ? tm_local.tm_hour : -1;
    int month = has_time ? tm_local.tm_mon + 1 : 0;
    uint8_t tod = 0;
    if (hour >= 6 && hour < 11) tod = kTodM;
    else if (hour >= 11 && hour < 14) tod = kTodD;
    else if (hour >= 14 && hour < 18) tod = kTodA;
    else if (hour >= 18 && hour < 22) tod = kTodE;

    auto trait_value = [this](Trait t) -> int {
        switch (t) {
            case kCur: return curiosity_;
            case kPlay: return playfulness_;
            case kMusic: return musicality_;
            case kSoc: return sociability_;
            case kLazy: return laziness_;
            case kActive: return 100 - laziness_;
            default: return 50;
        }
    };
    int64_t idle_min = (now_ms - last_interaction_ms_) / 60000;

    float w[kActCount];
    float total = 0;
    for (int i = 0; i < kActCount; i++) {
        const ActDef& d = kActs[i];
        float x = d.base;
        if (x <= 0) {
            w[i] = 0;
            continue;
        }
        x *= 0.4f + trait_value(d.trait) / 60.0f;              // ok. 0.5 .. 2.0
        if (d.tod && tod) x *= (d.tod & tod) ? 2.5f : 0.4f;
        // potrzeby i okolicznosci
        switch (i) {
            case kActDance: case kActJuggle: case kActHula: case kActTrampoline:
            case kActChaseFly: case kActButterfly: case kActKite: case kActWorkout:
                if (energy_ < 40) x *= 0.2f;
                break;
            case kActHiccup:
                if (fullness_ > 85) x *= 3;
                break;
            case kActDoze: case kActMeditate: case kActMovie:
                if (energy_ < 50) x *= 2.5f;
                break;
            case kActCharge:
                x *= energy_ < 40 ? 6 : 0.3f;
                break;
            case kActLove: case kActMoodLoving: case kActMoodKissy:
                x *= affection_ > 70 ? 4 : 0.3f;
                break;
            case kActMoodSad: case kActClock:
                if (affection_ < 35 || idle_min > 30) x *= 4;
                break;
            case kActSnow:
                if (month == 12 || month <= 2) x *= 8;
                else if (month == 11 || month == 3) x *= 2;
                else x *= 0.1f;
                break;
            case kActFlower:
                if (month >= 4 && month <= 9) x *= 2;
                break;
            case kActRain:
                if (month >= 9 && month <= 11) x *= 1.8f;
                break;
            case kActCook: case kActIceCream:
                if (fullness_ < 50) x *= 2;
                if (i == kActIceCream && (month >= 5 && month <= 8)) x *= 2;
                break;
            default:
                break;
        }
        // "dawno tego nie bylo": 0.05 tuz po, 1.0 po 30 min, max 4.0 (po 2 h / nigdy)
        float since_min = last_done_ms_[i] == 0 ? 240.0f : (now_ms - last_done_ms_[i]) / 60000.0f;
        x *= std::min(4.0f, 0.05f + since_min / 30.0f);
        for (int r : recent_acts_) {
            if (r == i) x *= 0.05f;  // bez powtorek z rzedu
        }
        w[i] = x;
        total += x;
    }
    if (total <= 0) return -1;
    float pick = (esp_random() / 4294967296.0f) * total;
    for (int i = 0; i < kActCount; i++) {
        if (w[i] <= 0) continue;
        pick -= w[i];
        if (pick <= 0) return i;
    }
    return kActCurious;
}

bool MatiLife::RunRoutines(int64_t now_ms) {
    struct tm tm_local;
    if (!LocalTm(&tm_local)) return false;
    CheckNewDay();
    bool weekend = tm_local.tm_wday == 0 || tm_local.tm_wday == 6;
    for (int i = 0; i < kRoutineCount; i++) {
        const Routine& r = kRoutines[i];
        if (routines_done_ & (1u << i)) continue;
        if (!(r.days & (1u << tm_local.tm_wday))) continue;
        int from = r.from_h + (weekend ? r.weekend_shift : 0);
        int to = r.to_h + (weekend ? r.weekend_shift : 0);
        if (tm_local.tm_hour < from || tm_local.tm_hour >= to) continue;
        // Nie zawsze o pelnej godzinie: losowo, ale najpozniej w ostatnim pol godzinie okna
        bool last_chance = tm_local.tm_hour == to - 1 && tm_local.tm_min >= 30;
        if (!last_chance && Rand(25) != 0) continue;
        routines_done_ |= (1u << i);
        Settings s("mati", true);
        s.SetInt("rdone", static_cast<int>(routines_done_));
        ESP_LOGI(TAG, "Rytm dnia: %s", kActs[r.act].label);
        BeginAct(r.act, now_ms);
        return true;
    }
    return false;
}

void MatiLife::StartGame(Game g, int64_t now_ms) {
    activity_ = Activity::kGame;
    game_ = g;
    switch (g) {
        case Game::kInvite:
            last_invite_ms_ = now_ms;
            game_until_ms_ = now_ms + 10000;
            ShowEmotion("invite");
            PlaySound(Lang::Sounds::OGG_MATI_HMM);
            Notify("Stuknij, zeby zagrac!", 4000);
            break;
        case Game::kReactWait:
            game_go_ms_ = now_ms + RandRange(1500, 4500);
            game_until_ms_ = game_go_ms_;
            ShowEmotion("game_wait");
            PlaySound(Lang::Sounds::OGG_MATI_BEEP);
            if (game_round_ == 0) Notify("Refleks: stuknij przy TERAZ!", 2500);
            break;
        case Game::kTapRace:
            game_score_ = 0;
            game_until_ms_ = now_ms + 5000;
            ShowEmotion("game_tap");
            PlaySound(Lang::Sounds::OGG_MATI_GO);
            Notify("Stukaj! 5 sekund", 2000);
            break;
        default:
            break;
    }
    ApplyBrightness();
    ESP_LOGI(TAG, "Gra: stan %d", static_cast<int>(g));
}

void MatiLife::GameTap(int64_t now_ms) {
    switch (game_) {
        case Game::kInvite:
            fun_ = Clamp(fun_ + 5);
            Grow(sociability_, 3);
            if (Rand(2) == 0) {
                game_round_ = 0;
                game_score_ = kNoResult;
                StartGame(Game::kReactWait, now_ms);
            } else {
                StartGame(Game::kTapRace, now_ms);
            }
            break;
        case Game::kReactWait:
            // Za wczesnie!
            game_round_++;
            game_ = Game::kReactPause;
            game_until_ms_ = now_ms + 2200;
            ShowEmotion("laughing");
            PlaySound(Lang::Sounds::OGG_MATI_NOPE);
            Notify("Falstart!", 2000);
            break;
        case Game::kReactGo: {
            int rt = static_cast<int>(now_ms - game_go_ms_);
            game_score_ = std::min(game_score_, rt);
            game_round_++;
            game_ = Game::kReactPause;
            game_until_ms_ = now_ms + 2200;
            ShowEmotion(rt < 300 ? "surprised" : (rt < 450 ? "happy" : "funny"));
            char buf[40];
            snprintf(buf, sizeof(buf), "Runda %d: %d ms", game_round_, rt);
            Notify(buf, 2000);
            break;
        }
        case Game::kTapRace:
            game_score_++;
            break;
        default:
            break;
    }
}

void MatiLife::GameTick(int64_t now_ms) {
    switch (game_) {
        case Game::kInvite:
            if (now_ms > game_until_ms_) {
                // Nikt nie chcial grac
                game_ = Game::kNone;
                Begin(Activity::kBored, now_ms, 3000);
            }
            break;
        case Game::kReactWait:
            if (now_ms >= game_go_ms_) {
                game_ = Game::kReactGo;
                game_go_ms_ = now_ms;
                game_until_ms_ = now_ms + 3000;
                ShowEmotion("game_go");
                PlaySound(Lang::Sounds::OGG_MATI_GO);
            }
            break;
        case Game::kReactGo:
            if (now_ms > game_until_ms_) {
                game_round_++;
                game_ = Game::kReactPause;
                game_until_ms_ = now_ms + 2000;
                ShowEmotion("sleepy");
                Notify("Za wolno!", 2000);
            }
            break;
        case Game::kReactPause:
            if (now_ms > game_until_ms_) {
                if (game_round_ < kReactionRounds) {
                    StartGame(Game::kReactWait, now_ms);
                } else {
                    // Podsumowanie
                    game_ = Game::kResult;
                    game_until_ms_ = now_ms + 4000;
                    fun_ = Clamp(fun_ + 15);
                    affection_ = Clamp(affection_ + 5);
                    char buf[48];
                    if (game_score_ >= kNoResult) {
                        ShowEmotion("laughing");
                        Notify("Same falstarty! :)", 3000);
                    } else if (best_reaction_ms_ == 0 || game_score_ < best_reaction_ms_) {
                        best_reaction_ms_ = game_score_;
                        Settings s("mati", true);
                        s.SetInt("best_rt", best_reaction_ms_);
                        recent_acts_[0] = kActTrophy;
                        ShowEmotion("trophy");
                        PlaySound(Lang::Sounds::OGG_MATI_TADA);
                        snprintf(buf, sizeof(buf), "REKORD! %d ms", game_score_);
                        Notify(buf, 3500);
                    } else {
                        ShowEmotion("proud");
                        PlaySound(Lang::Sounds::OGG_MATI_PROUD);
                        snprintf(buf, sizeof(buf), "Najlepiej: %d ms (rekord %d)", game_score_,
                                 best_reaction_ms_);
                        Notify(buf, 3500);
                    }
                }
            }
            break;
        case Game::kTapRace:
            if (now_ms > game_until_ms_) {
                game_ = Game::kResult;
                game_until_ms_ = now_ms + 4000;
                fun_ = Clamp(fun_ + 15);
                energy_ = Clamp(energy_ - 2);
                char buf[48];
                if (game_score_ > best_taps_) {
                    best_taps_ = game_score_;
                    Settings s("mati", true);
                    s.SetInt("best_taps", best_taps_);
                    ShowEmotion("trophy");
                    PlaySound(Lang::Sounds::OGG_MATI_TADA);
                    snprintf(buf, sizeof(buf), "REKORD! %d stukniec", game_score_);
                } else {
                    ShowEmotion(game_score_ >= 20 ? "shocked" : "happy");
                    PlaySound(Lang::Sounds::OGG_MATI_PROUD);
                    snprintf(buf, sizeof(buf), "Wynik: %d (rekord %d)", game_score_, best_taps_);
                }
                Notify(buf, 3500);
            }
            break;
        case Game::kResult:
            if (now_ms > game_until_ms_) EndGame(now_ms);
            break;
        case Game::kNone:
            break;
    }
}

void MatiLife::EndGame(int64_t now_ms) {
    game_ = Game::kNone;
    activity_ = Activity::kNone;
    ShowEmotion("neutral");
    ApplyBrightness();
    next_event_ms_ = now_ms + RandRange(4000, 8000);
}

void MatiLife::ChooseNext(int64_t now_ms) {
    // 1. Sen: wieczorem mycie zebow -> dobranoc -> owce -> sen
    bool night_sleep = IsNight() && now_ms - last_interaction_ms_ > 2LL * 60 * 1000;
    if (!IsNight()) evening_stage_ = 0;
    if (night_sleep && !forced_sleep_ && evening_stage_ < 4) {
        int stage = evening_stage_++;
        if (stage == 0) {
            BeginAct(kActBrushTeeth, now_ms);
            return;
        }
        if (stage == 1) {
            BeginAct(kActGoodnight, now_ms);
            return;
        }
        if (stage == 2 && chattiness_ > 0 && LocalHour() == kNightStartHour) {
            Speak("Dobranoc, Mati!", now_ms);
            return;
        }
        if (stage <= 3) {
            evening_stage_ = 4;
            BeginAct(kActCountSheep, now_ms);
            return;
        }
    }
    if (forced_sleep_ || night_sleep) {
        night_nap_ = night_sleep;
        Begin(Activity::kNap, now_ms, 30 * 60 * 1000);
        return;
    }
    if (energy_ < 15) {
        Begin(Activity::kNap, now_ms);
        return;
    }
    // 2. Rytm dnia: posilki i obowiazki
    if (RunRoutines(now_ms)) {
        return;
    }
    // 3. Glod: najpierw sie upomina, po 20 min sam znajduje przekaske
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
    // Woda co ok. 2 godziny
    if (now_ms - last_drink_ms_ > 2LL * 3600 * 1000 && Rand(8) == 0) {
        BeginAct(kActDrink, now_ms);
        return;
    }
    // 4. Nuda -> zabawa sam ze soba
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
    // 5. Zaproszenie do gry dotykowej (towarzyski Mati zaprasza czesciej)
    int hour = LocalHour();
    int invite_gap_min = 75 - sociability_ / 2;  // ok. 28-73 min
    if ((hour < 0 || (hour >= 9 && hour < 21)) &&
        now_ms - last_invite_ms_ > invite_gap_min * 60000LL && Rand(6) == 0) {
        StartGame(Game::kInvite, now_ms);
        return;
    }
    if (now_ms - last_interaction_ms_ > 15LL * 60 * 1000 && Rand(5) == 0) {
        Begin(Activity::kBored, now_ms);
        return;
    }
    // 6. Samodzielna wypowiedz, gdy dlugo nikt sie nie odzywa
    if (MaybeSpeak(now_ms, false)) {
        return;
    }
    // 7. Zwykle zycie: najczesciej czynnosc z tabeli (sprawiedliwe losowanie),
    // czasem rozgladanie sie, spiew, pilka, ziewniecie albo chwila spokoju
    int r = Rand(100);
    bool can_sing = now_ms - last_sing_ms_ > 20LL * 60 * 1000 && fun_ > 30 &&
                    (hour < 0 || (hour >= 9 && hour < 21));
    int sing_pct = 2 + musicality_ / 20;          // 2..6 %
    int play_pct = 2 + playfulness_ / 25;         // 2..5 %
    if (r < 18) {
        Begin(Activity::kLook, now_ms);
    } else if (r < 84) {
        int act = PickAct(now_ms);
        if (act >= 0) {
            BeginAct(act, now_ms);
        } else {
            Begin(Activity::kLook, now_ms);
        }
    } else if (r < 84 + sing_pct && can_sing) {
        Begin(Activity::kSing, now_ms);
    } else if (r < 84 + sing_pct + play_pct) {
        fun_ = Clamp(fun_ + 6);
        Begin(Activity::kPlay, now_ms);
    } else if (r < 96 && energy_ < 50 + laziness_ / 4) {
        Begin(Activity::kYawn, now_ms);
    } else {
        // chwila spokoju - neutralne oczy same mrugaja
        next_event_ms_ = now_ms + RandRange(4000, 10000);
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
    last_drink_ms_ = last_tick;
    last_invite_ms_ = last_tick;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(game_ != Game::kNone ? kGameLoopMs : kLoopMs));
        int64_t now = NowMs();
        auto state = app.GetDeviceState();
        bool idle = state == kDeviceStateIdle;

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
                game_ = Game::kNone;
                follow_up_ = -1;
                taps_ = 0;
                forced_sleep_ = false;
                was_idle_ = false;
                ApplyBrightness();
            }
            if (close_after_reply_ && state == kDeviceStateListening) {
                // Gra zaczyna sie od razu po odpowiedzi
                close_after_reply_ = false;
                app.ToggleChatState();
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
            close_after_reply_ = false;
            activity_ = Activity::kNone;
            next_event_ms_ = now + RandRange(2500, 6000);
            ApplyBrightness();
        }

        if (game_ != Game::kNone) {
            GameTick(now);
        } else if (pending_game_ != Game::kNone) {
            Game g = pending_game_;
            pending_game_ = Game::kNone;
            game_round_ = 0;
            game_score_ = kNoResult;
            StartGame(g, now);
        } else if (pending_show_ != nullptr) {
            // Wynik kostki / monety / papier-kamien-nozyce jeszcze chwile na ekranie
            const char* e = pending_show_;
            pending_show_ = nullptr;
            activity_ = Activity::kMood;
            activity_end_ms_ = now + 4000;
            ShowEmotion(e);
        } else if (taps_ > 0) {
            ResolveTaps(now);
        } else if (say_now_) {
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
