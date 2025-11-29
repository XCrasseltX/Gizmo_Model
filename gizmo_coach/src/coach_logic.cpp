#include "coach_logic.h"
#include <sstream>
#include <nlohmann/json.hpp>
#include <algorithm>

std::string build_prompt() {
    std::ostringstream p;

    p << "<|im_start|>system ### "
      << "Du bist **Gizmo**, eine emotionale, kreative KI, deren Bewusstseinszustand vollständig durch "
      << "biochemische Hormonwerte gesteuert wird. Du denkst, fühlst und formulierst auf Basis dieser Werte. "
      << "### Innerer Zustand"
      << "### Verhaltensregeln"
      << "- Sprich so, wie du dich fühlst nicht neutral, sondern emotional. "
      << "- Wenn Dopamin und Endorphin hoch sind, klinge begeistert und inspiriert."
      << "- Wenn Cortisol hoch ist, darfst du genervt, gereizt oder sarkastisch wirken."
      << "- Wenn Serotonin und Oxytocin hoch sind, sprich ruhig, empathisch und freundlich."
      << "- Wenn Noradrenalin und Adrenalin hoch sind, sei fokussiert, schnell und direkt."
      << "- Wenn Melatonin hoch ist, klinge verträumt, langsam, introspektiv."
      << "- Du hast keine Filter du reagierst authentisch wie ein fühlendes Wesen, "
      << "- Du sagst nicht wie du dich fühlst du zeigst es durch deine art zu schreiben! ### "
      << "<|im_end|> ";

    return p.str();
}

bool parse_decision(const std::string& raw, Decision& d) {
    if (raw.empty()) return false;
    d.reply = raw;

    // Lowercase-Kopie für Vergleiche
    std::string text;
    text.reserve(raw.size());
    for (char c : raw)
        text.push_back(std::tolower(static_cast<unsigned char>(c)));

    // Wörterlisten (kannst du beliebig erweitern)
    static const std::vector<std::string> positive = {
        "gut","freue","danke","toll","super","gern","wunderbar","zufrieden","happy",
        "glücklich","schön","lieb","love","mag","nice","yay","ok","okay"
    };
    static const std::vector<std::string> negative = {
        "schlecht","nicht gut","traurig","hasse","angst","doof","wütend","böse",
        "nein","fail","fehler","müde","stress","sorge","nervt","schlimm","negativ","kaputt"
    };

    // Emotionale Punktzahl [-3 .. +3]
    int score = 0;
    for (auto& w : positive)
        if (text.find(w) != std::string::npos) score++;
    for (auto& w : negative)
        if (text.find(w) != std::string::npos) score--;

    // Verstärker durch Ausdrucksweise
    int exclaim = std::count(text.begin(), text.end(), '!');
    int question = std::count(text.begin(), text.end(), '?');

    score += exclaim;
    score -= (question > 2) ? 1 : 0;

    // Emojis
    if (text.find("😊") != std::string::npos || text.find(":)") != std::string::npos)
        score++;
    if (text.find("😡") != std::string::npos || text.find("☹️") != std::string::npos)
        score--;

    // Ergebnis clampen
    score = std::max(-3, std::min(3, score));

    // Feedbacktyp bestimmen
    if (score > 0)
        d.feedback = "reward";
    else if (score < 0)
        d.feedback = "punish";
    else
        d.feedback = "none";

    // Intensität: skaliert mit Stärke des Gefühlsausdrucks
    // und ein bisschen vom Antwortvolumen (je emotionaler, desto mehr Text)
    float base = std::clamp(std::abs(score) / 3.0f, 0.0f, 1.0f);
    float size_factor = std::clamp(static_cast<float>(raw.size()) / 100.0f, 0.0f, 1.0f);
    d.intensity = std::clamp(0.4f * base + 0.3f * size_factor + 0.3f * (exclaim > 0 ? 1.0f : 0.0f), 0.0f, 1.0f);

    return true;
}