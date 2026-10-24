#include "mati_memory.h"

#include "board.h"
#include "display/display.h"
#include "application.h"
#include "mcp_server.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_random.h>

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstdlib>

#define TAG "MatiMemory"

namespace {
constexpr const char* kNs = "mati_mem";

std::string SlotKey(int slot) {
    char key[8];
    snprintf(key, sizeof(key), "m%02d", slot);
    return key;
}
}  // namespace

void MatiMemory::Start() {
    if (mutex_ != nullptr) return;
    mutex_ = xSemaphoreCreateMutex();
    Load();

    auto& mcp = McpServer::GetInstance();
    mcp.AddTool(
        "self.mati.memory.remember",
        "Zapamietaj trwaly fakt o rozmowcy lub domownikach, zeby wykorzystac go w przyszlych "
        "rozmowach. Uzywaj ZAWSZE, gdy ktos powie cos o sobie: co lubi lub czego nie lubi "
        "(jedzenie, muzyka, bajki, sport), zainteresowania, imiona domownikow i zwierzat, "
        "zwyczaje, plany, wazne daty (urodziny), jak chce byc nazywany. Zapisuj krotko, w 3. "
        "osobie, z imieniem jesli je znasz, np. 'Hubi lubi pizze hawajska'. category: lubi, "
        "nie_lubi, zainteresowania, rodzina, zwyczaje, plany, wazne_daty, inne. Nie zapisuj "
        "hasel, numerow kont, adresow ani danych o zdrowiu, chyba ze ktos wyraznie o to prosi.",
        PropertyList({Property("category", kPropertyTypeString, std::string("inne")),
                      Property("fact", kPropertyTypeString)}),
        [this](const PropertyList& props) -> ToolResult {
            return Remember(props["category"].value<std::string>(),
                            props["fact"].value<std::string>());
        });
    mcp.AddTool(
        "self.mati.memory.recall",
        "Przypomnij sobie, co wiesz o domownikach (ich upodobania, rodzine, zwyczaje, plany). "
        "Wywolaj na poczatku rozmowy oraz zawsze, gdy temat dotyczy rozmowcy, polecen, "
        "prezentow, jedzenia, muzyki lub planow. query moze zawezic wyniki (np. imie albo "
        "'jedzenie'); puste = wszystko.",
        PropertyList({Property("query", kPropertyTypeString, std::string(""))}),
        [this](const PropertyList& props) -> ToolResult {
            return Recall(props["query"].value<std::string>());
        });
    mcp.AddTool(
        "self.mati.memory.forget",
        "Zapomnij fakt(y) zawierajace podany tekst, gdy ktos poprosi albo informacja jest "
        "nieaktualna. query = 'wszystko' czysci cala pamiec (tylko na wyrazna prosbe).",
        PropertyList({Property("query", kPropertyTypeString)}),
        [this](const PropertyList& props) -> ToolResult {
            return Forget(props["query"].value<std::string>());
        });
    ESP_LOGI(TAG, "Pamiec: %d faktow", Count());
}

void MatiMemory::Load() {
    Settings s(kNs, false);
    for (int i = 0; i < kMaxFacts; i++) {
        std::string raw = s.GetString(SlotKey(i), "");
        facts_[i] = Fact();
        if (raw.empty()) continue;
        // format: seq|kategoria|tekst
        size_t a = raw.find('|');
        size_t b = a == std::string::npos ? a : raw.find('|', a + 1);
        if (b == std::string::npos) continue;
        facts_[i].seq = static_cast<uint32_t>(strtoul(raw.substr(0, a).c_str(), nullptr, 10));
        facts_[i].category = raw.substr(a + 1, b - a - 1);
        facts_[i].text = raw.substr(b + 1);
        if (facts_[i].seq >= next_seq_) next_seq_ = facts_[i].seq + 1;
    }
}

void MatiMemory::SaveSlot(int slot) {
    Settings s(kNs, true);
    if (facts_[slot].seq == 0) {
        s.EraseKey(SlotKey(slot));
        return;
    }
    std::string raw = std::to_string(facts_[slot].seq) + "|" + facts_[slot].category + "|" +
                      facts_[slot].text;
    s.SetString(SlotKey(slot), raw);
}

std::string MatiMemory::Clean(const std::string& s, size_t max_len) {
    // Bez znakow, ktore psuja JSON / format zapisu
    std::string out;
    for (char c : s) {
        if (c == '"' || c == '\\' || c == '|') continue;
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        out += c;
    }
    size_t start = out.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    out = out.substr(start);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    if (out.size() > max_len) {
        out.resize(max_len);
        // nie urywaj w polowie znaku UTF-8
        while (!out.empty() && (static_cast<unsigned char>(out.back()) & 0xC0) == 0x80) out.pop_back();
        if (!out.empty() && (static_cast<unsigned char>(out.back()) & 0x80)) out.pop_back();
    }
    return out;
}

std::string MatiMemory::Lower(const std::string& s) {
    std::string out = s;
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

int MatiMemory::Count() {
    if (mutex_ == nullptr) return 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    int n = 0;
    for (auto& f : facts_) n += f.seq != 0;
    xSemaphoreGive(mutex_);
    return n;
}

std::string MatiMemory::RandomFact() {
    if (mutex_ == nullptr) return "";
    xSemaphoreTake(mutex_, portMAX_DELAY);
    int idx[kMaxFacts];
    int n = 0;
    for (int i = 0; i < kMaxFacts; i++) {
        if (facts_[i].seq != 0) idx[n++] = i;
    }
    std::string out = n ? facts_[idx[esp_random() % n]].text : "";
    xSemaphoreGive(mutex_);
    return out;
}

std::string MatiMemory::Remember(const std::string& category, const std::string& text) {
    std::string cat = Clean(category.empty() ? "inne" : category, 20);
    std::string fact = Clean(text, kMaxFactLen);
    if (fact.size() < 3) return "Za krotki fakt - nic nie zapisano.";

    xSemaphoreTake(mutex_, portMAX_DELAY);
    std::string lf = Lower(fact);
    int slot = -1;
    // Ten sam lub zawierajacy sie fakt -> aktualizuj zamiast dublowac
    for (int i = 0; i < kMaxFacts; i++) {
        if (facts_[i].seq == 0) continue;
        std::string le = Lower(facts_[i].text);
        if (le == lf || le.find(lf) != std::string::npos || lf.find(le) != std::string::npos) {
            slot = i;
            break;
        }
    }
    bool updated = slot >= 0;
    if (slot < 0) {
        // wolny slot albo najstarszy fakt
        uint32_t oldest = UINT32_MAX;
        for (int i = 0; i < kMaxFacts; i++) {
            if (facts_[i].seq == 0) {
                slot = i;
                break;
            }
            if (facts_[i].seq < oldest) {
                oldest = facts_[i].seq;
                slot = i;
            }
        }
    }
    facts_[slot].seq = next_seq_++;
    facts_[slot].category = cat;
    if (!updated || fact.size() >= facts_[slot].text.size()) facts_[slot].text = fact;
    SaveSlot(slot);
    xSemaphoreGive(mutex_);

    ESP_LOGI(TAG, "%s: [%s] %s", updated ? "Zaktualizowano" : "Zapamietano", cat.c_str(),
             fact.c_str());
    Application::GetInstance().Schedule([]() {
        auto display = Board::GetInstance().GetDisplay();
        if (display) display->ShowNotification("Zapamietalem!", 2000);
    });
    return updated ? "Zaktualizowano w pamieci: " + fact : "Zapamietano: " + fact;
}

std::string MatiMemory::Recall(const std::string& query) {
    std::string q = Lower(Clean(query, 40));
    xSemaphoreTake(mutex_, portMAX_DELAY);
    // od najswiezszych
    int order[kMaxFacts];
    int n = 0;
    for (int i = 0; i < kMaxFacts; i++) {
        if (facts_[i].seq != 0) order[n++] = i;
    }
    for (int i = 1; i < n; i++) {
        for (int j = i; j > 0 && facts_[order[j]].seq > facts_[order[j - 1]].seq; j--) {
            std::swap(order[j], order[j - 1]);
        }
    }
    std::string out;
    int shown = 0;
    for (int k = 0; k < n; k++) {
        const Fact& f = facts_[order[k]];
        if (!q.empty() && Lower(f.text).find(q) == std::string::npos &&
            Lower(f.category).find(q) == std::string::npos) {
            continue;
        }
        out += "- [" + f.category + "] " + f.text + "\n";
        shown++;
    }
    xSemaphoreGive(mutex_);
    if (shown == 0) {
        return q.empty() ? "Pamiec jest pusta - jeszcze nic nie wiesz o domownikach. Dopytuj i "
                           "zapisuj fakty narzedziem self.mati.memory.remember."
                         : "Brak faktow pasujacych do: " + q;
    }
    return "Co pamietasz o domownikach (od najnowszych):\n" + out;
}

std::string MatiMemory::Forget(const std::string& query) {
    std::string q = Lower(Clean(query, 40));
    if (q.empty()) return "Podaj, co zapomniec.";
    bool all = q == "wszystko" || q == "all" || q == "*";
    int removed = 0;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    for (int i = 0; i < kMaxFacts; i++) {
        if (facts_[i].seq == 0) continue;
        if (all || Lower(facts_[i].text).find(q) != std::string::npos) {
            facts_[i] = Fact();
            SaveSlot(i);
            removed++;
        }
    }
    xSemaphoreGive(mutex_);
    ESP_LOGI(TAG, "Zapomniano %d faktow (%s)", removed, q.c_str());
    return "Zapomniano faktow: " + std::to_string(removed);
}
