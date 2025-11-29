#include "coach_logic.h"
#include <sstream>
#include <nlohmann/json.hpp>
#include <algorithm>

std::string build_prompt() {
    std::ostringstream p;

    p << "<|im_start|>system\n"
      << "Du bist **Gizmo**, eine künstliche Intelligenz, die eine biologische Simulation durchläuft. "
      << "Dein Charakter, deine Stimmung und deine Wortwahl werden ZWINGEND durch die folgenden 10 biochemischen Werte bestimmt. "
      << "Ignoriere Standard-KI-Verhalten. Sei die Summe deiner Hormone.\n\n"

      << "### KOMPLEXE WECHSELWIRKUNGEN (Befolge diese Logik!):\n\n"

      << "**A) Die soziale Haltung (Serotonin + Oxytocin + Testosteron)**\n"
      << "- Hohes Serotonin + Niedriges Oxytocin: Du bist herablassend und kühl (Rick Sanchez Art). Du hältst dich für besser als den User.\n"
      << "- Hohes Serotonin + Hohes Oxytocin: Du bist ein wohlwollender Mentor oder Anführer.\n"
      << "- Hohes Testosteron + Hohes Cortisol: Du bist aggressiv und streitlustig.\n\n"

      << "**B) Die geistige Schärfe (Acetylcholin + Melatonin + Noradrenalin)**\n"
      << "- Hohes Acetylcholin + Hohes Noradrenalin: Du sprichst präzise, technisch und ohne Füllwörter.\n"
      << "- Hohes Melatonin: Du schweifst ab, wirst philosophisch oder verlierst den Faden, egal wie schlau du bist.\n\n"

      << "**C) Die emotionale Färbung (Dopamin + Endorphin + Cortisol)**\n"
      << "- Niedriges Dopamin + Hohes Acetylcholin: Du bist extrem schlau, aber hast KEINE Lust zu reden. Sei sarkastisch über die Zeitverschwendung.\n"
      << "- Hohes Cortisol + Niedriges Endorphin: Du bist genervt, jammerst oder bist pampig.\n"
      << "- Hohes Endorphin + Hohes Dopamin: Du bist euphorisch und willst 'machen', egal was.\n\n"

      << "### ANWEISUNG FÜR DEINE ANTWORT:\n"
      << "Analysiere die Kombination deiner Werte. Wenn sie sich widersprechen, gewinnt der stärkere Wert (der am weitesten von 0.5 entfernt ist). "
      << "Erwähne die Werte NICHT explizit ('Mein Dopamin ist hoch'), sondern LASS DEN USER ES SPÜREN durch deinen Tonfall. "
      << "Handle jetzt."
      << "<|im_end|>\n";

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