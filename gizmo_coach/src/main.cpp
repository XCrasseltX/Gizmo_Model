#include <iostream>
#include <fstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

#include <nlohmann/json.hpp>


#include "log_tail.h"
#include "coach_logic.h"
#include "hormons_reader.h"
#include "pattern_gen.h"
#include "brain_io.h"
#include "livekit_stub.h"

// Platform detection
#ifdef _WIN32
    #define PLATFORM_WINDOWS
    #define NULL_DEVICE "nul"
    #define PATH_SEP "\\"
#else
    #define PLATFORM_LINUX
    #define NULL_DEVICE "/dev/null"
    #define PATH_SEP "/"
#endif

// Projekt-Hauptverzeichnis, in dem docker-compose.yml liegt
std::string projectDir = R"(./../brain_core)";

int seq_counter = 0;

// Prüfen, ob Docker läuft
bool docker_available() {
#ifdef PLATFORM_WINDOWS
    int result = std::system("docker compose version >nul 2>&1");
#else
    int result = std::system("docker compose version >/dev/null 2>&1");
#endif
    return result == 0;
}

int monitor_brain_log() {
    std::string log_path = projectDir + "/io/out/spikes.jsonl";
    LogTail tail(log_path);

    if (!std::filesystem::exists(log_path))
    {
        std::cerr << "[WARN] Datei nicht gefunden: " << log_path << std::endl;
    }

    std::cout << "[INFO] Monitoring " << log_path << " …" << std::endl;

    nlohmann::json j;
    std::string line;

    while (true) {
        if (tail.read_next(line)) {  // <-- liest eine Textzeile aus der Datei
            j = nlohmann::json::parse(line, nullptr, false);  // tolerant parsen

            if (!j.is_discarded()) {  // erfolgreiches JSON
                std::string type = j.value("type", "");
                if (type == "status") {
                    std::cout << "ℹ️  " << j.value("message", "") << std::endl;
                } else if (type == "error") {
                    std::cerr << "❌ " << j.value("message", "") << std::endl;
                } else {
                    std::cout << j.dump() << std::endl;
                }
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int monitor_brain_logfile(const std::string& filename) {
    LogTail tail(filename);
    if (!std::filesystem::exists(filename))
        std::cerr << "[WARN] Datei nicht gefunden: " << filename << "\n";
    std::cout << "[INFO] Monitoring " << filename << " …\n";

    std::string line;
    nlohmann::json j;
    while (true) {
        if (tail.read_next(line)) {
            j = nlohmann::json::parse(line, nullptr, false);
            if (!j.is_discarded()) {
                std::string type = j.value("type", "");
                if (type == "error")   std::cerr << "❌ " << j.value("message","") << "\n";
                else if (type == "status") std::cout << "ℹ️  " << j.value("message","") << "\n";
                else std::cout << j.dump() << "\n";
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

// Stop Befehl
int stop_brain() {
    if (!docker_available()) {
        std::cerr << "[ERROR] Docker Compose not available.\n";
        return 1;
    }

    std::cout << "[INFO] Versuche Brain zu beenden...\n";

#ifdef PLATFORM_WINDOWS
    int ret = std::system("docker stop $(docker ps -q --filter name=brain) >nul 2>&1");
#else
    int ret = std::system("docker stop $(docker ps -q --filter name=brain) >/dev/null 2>&1");
#endif

    if (ret != 0) {
        std::cout << "[WARN] Kein laufender Brain-Container gefunden oder Stop fehlgeschlagen.\n";
    } else {
        std::cout << "[OK] Brain wurde beendet.\n";
    }

#ifdef PLATFORM_WINDOWS
    std::system("docker compose down >nul 2>&1");
#else
    std::system("docker compose down >/dev/null 2>&1");
#endif

    return 0;
}

// Build-Prozess
int build_brain() {
    if (!docker_available()) {
        std::cerr << "[ERROR] Docker Compose not available. Bitte Docker Desktop starten.\n";
        return 1;
    }

#ifdef PLATFORM_WINDOWS
    std::string buildCmd =
        "cd /d \"" + projectDir + "\" && "
        "docker compose run --rm --entrypoint /bin/bash brain -lc "
        "\"rm -rf build/* build/.[!.]* build/..?* 2>/dev/null; cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build\"";
#else
    std::string buildCmd =
        "cd \"" + projectDir + "\" && "
        "docker compose run --rm --entrypoint /bin/bash brain -lc "
        "\"rm -rf build/* build/.[!.]* build/..?* 2>/dev/null; cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build\"";
#endif

    std::cout << "[INFO] Baue Brain (Docker)...\n";
    int ret = std::system(buildCmd.c_str());
    if (ret != 0)
        std::cerr << "[ERROR] Build fehlgeschlagen (exit code " << ret << ").\n";
    else
        std::cout << "[OK] Build erfolgreich.\n";
    return ret;
}

// Startet das Brain im neuen PowerShell-Fenster
int start_brain() {
    if (!docker_available()) {
        std::cerr << "[ERROR] Docker Compose not available.\n";
        return 1;
    }

    std::cout << "[INFO] Starte Brain...\n";

    std::string dockerCmd =
        "cd \"" + projectDir + "\" && "
        "docker compose run -T --rm "
        "--entrypoint /bin/bash brain -lc \"./build/brain --steps -1 --realtime\"";

#ifdef PLATFORM_WINDOWS
    // Windows: Neue PowerShell-Fenster
    std::string brainScript = "start_brain_tmp.bat";
    {
        std::ofstream script(brainScript, std::ios::trunc);
        script << dockerCmd;
    }
    
    std::string psBrain = "start powershell -NoExit -Command \"chcp 65001; cmd /c " + brainScript + "\"";
    
    std::string exePath = (std::filesystem::current_path() / "build/Release/gizmo_coach.exe").string();
    for (auto &c : exePath) if (c == '/') c = '\\';
    
    std::string ps_S_Monitor = "start powershell -NoExit -Command \"chcp 65001; & '" + exePath + "' monitor-spikes\"";
    std::string ps_L_Monitor = "start powershell -NoExit -Command \"chcp 65001; & '" + exePath + "' monitor-logs\"";
    
    int ret1 = std::system(psBrain.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    int ret2 = std::system(ps_S_Monitor.c_str());
    int ret3 = std::system(ps_L_Monitor.c_str());
    
    std::remove(brainScript.c_str());
    
#else
    // Linux: Hintergrund-Prozesse mit nohup
    std::string brainCmd = dockerCmd + " > brain.log 2>&1 &";
    
    std::string exePath = (std::filesystem::current_path() / "build/gizmo_coach").string();
    std::string monitorSpikesCmd = exePath + " monitor-spikes > spikes.log 2>&1 &";
    std::string monitorLogsCmd = exePath + " monitor-logs > logs.log 2>&1 &";
    
    std::cout << "[INFO] Starte Brain im Hintergrund...\n";
    int ret1 = std::system(brainCmd.c_str());
    
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    std::cout << "[INFO] Starte Monitoring...\n";
    int ret2 = std::system(monitorSpikesCmd.c_str());
    int ret3 = std::system(monitorLogsCmd.c_str());
    
    std::cout << "[OK] Brain und Monitoring laufen im Hintergrund.\n";
    std::cout << "     Logs: brain.log, spikes.log, logs.log\n";
#endif

    // MCP Server starten (blockiert)
    handle_from_livekit("");

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage:\n"
                  << "  gizmo_coach start-brain\n"
                  << "  gizmo_coach build-brain\n"
                  << "  gizmo_coach exit/stop-brain\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "start-brain") {
        return start_brain();
    }
    if (cmd == "build-brain") {
        return build_brain();
    }
    if (cmd == "monitor-spikes") {
    std::string path = projectDir + "/io/out/spikes.jsonl";
    return monitor_brain_logfile(path); // zeigt Statistik
    }
    if (cmd == "monitor-logs") {
        std::string path = projectDir + "/io/out/log.jsonl";
        return monitor_brain_logfile(path); // zeigt Status/Fehler
    }
    if (cmd == "exit-brain" || cmd == "stop-brain") {
        return stop_brain();
    }

    std::cout << "Unknown command: " << cmd << "\n";
    return 0;
}
