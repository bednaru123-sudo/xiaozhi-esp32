#pragma once

// Pamiec Matiego o domownikach: krotkie fakty (upodobania, rodzina, zwyczaje,
// plany...) zapisywane przez LLM narzedziami MCP i trzymane w NVS na plytce.
// Sluza w rozmowie (self.mati.memory.recall) i w samodzielnych wypowiedziach.

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <cstdint>
#include <string>
#include <vector>

class MatiMemory {
public:
    static constexpr int kMaxFacts = 30;
    static constexpr size_t kMaxFactLen = 100;

    static MatiMemory& GetInstance() {
        static MatiMemory instance;
        return instance;
    }

    void Start();  // wczytuje fakty i rejestruje narzedzia MCP
    int Count();
    // Losowy fakt do wplecenia w wypowiedz ("" gdy pusto)
    std::string RandomFact();

private:
    struct Fact {
        uint32_t seq = 0;  // im wieksze, tym swiezsze (0 = pusty slot)
        std::string category;
        std::string text;
    };

    MatiMemory() = default;
    void Load();
    void SaveSlot(int slot);
    std::string Remember(const std::string& category, const std::string& text);
    std::string Recall(const std::string& query);
    std::string Forget(const std::string& query);
    static std::string Clean(const std::string& s, size_t max_len);
    static std::string Lower(const std::string& s);

    SemaphoreHandle_t mutex_ = nullptr;
    Fact facts_[kMaxFacts];
    uint32_t next_seq_ = 1;
};
