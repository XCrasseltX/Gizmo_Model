#include <iostream>
#include <numeric>
#include <iomanip>
#include <string>
#include <csignal>
#include <atomic>
#include <chrono>
#include "net.h"
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <locale>
#include <thread>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sys/stat.h>

#include "net.h"
#include "io_logger.h"

static std::atomic<bool> running{true};
static void on_sigint(int){ running = false; }


void process_commands(Net& net) {
    static std::string path = "./../io/in/commands.jsonl";
    static off_t last_size = 0;

    struct stat st;
    if (stat(path.c_str(), &st) != 0) return;

    if (st.st_size <= last_size)
        return;

    std::ifstream f(path, std::ios::in);
    if (!f.is_open()) return;

    f.seekg(last_size);
    std::string line;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            auto j = nlohmann::json::parse(line);
            std::string cmd = j.value("cmd", "");

            // Unterstütze neues Format mit data-Objekt
            nlohmann::json data = j.contains("data") ? j["data"] : j;

            if (cmd == "set_hormones") {
                if (data.contains("dopamine"))
                    net.H.set_dopamine_drive(data["dopamine"]);
                if (data.contains("cortisol"))
                    net.H.set_cortisol_drive(data["cortisol"]);
                if (data.contains("adrenaline"))
                    net.H.set_adrenaline_drive(data["adrenaline"]);

                IoLogger::instance().log_status("🧠 Hormone drives updated via command");
            }
            else if (cmd == "input_pattern" || cmd == "input") {
                auto pattern = data.value("pattern", std::vector<int>{});
                if (!pattern.empty()) {
                    net.external_input_pattern.assign(pattern.begin(), pattern.end());
                    net.external_input_active = true;
                    IoLogger::instance().log_status("🧠 External input pattern applied");
                }
            }
            else if (cmd == "exit") {
                IoLogger::instance().log_status("🛑 Exit command received");
                running = false;
            }
        }
        catch (std::exception& e) {
            IoLogger::instance().log_error(std::string("Command parse error: ") + e.what());
        }
    }

    last_size = st.st_size;
}

int main(int argc, char** argv) {

    try {
        std::locale utf8_locale(""); 
        std::locale::global(utf8_locale);
        std::cout.imbue(utf8_locale);
        std::cerr.imbue(utf8_locale);
    } catch (const std::runtime_error& e) {
        std::cerr << "⚠ UTF-8 Locale nicht gefunden, benutze Standard." << std::endl;
    }
    std::signal(SIGINT, on_sigint);
    // Defaults
    long   steps = 2000;       // 2 s bei dt=1 ms
    double seconds = -1.0;     // wenn >=0, überschreibt steps
    int    print_every_ms = 100;
    bool   realtime = false;

    // CLI
    for (int i=1; i<argc; ++i) {
        std::string a = argv[i];
        if ((a=="--steps" || a=="-n") && i+1<argc) {
            steps = std::stol(argv[++i]);
        } else if ((a=="--seconds" || a=="-s") && i+1<argc) {
            seconds = std::stod(argv[++i]);
        } else if ((a=="--print-every-ms" || a=="-p") && i+1<argc) {
            print_every_ms = std::stoi(argv[++i]);
            if (print_every_ms < 1) print_every_ms = 1;
        } else if (a=="--realtime") {
            realtime = true;
        } else if (a=="--help" || a=="-h") {
            std::cout <<
            "Usage: ./brain [--steps N|-n N] [--seconds S|-s S] [--print-every-ms M|-p M] [--realtime]\n"
            "  --steps N        : simuliere N Schritte (1 Schritt = dt Sekunden). N<0 => endlos bis Ctrl+C.\n"
            "  --seconds S      : simuliere ~S Sekunden (überschreibt --steps).\n"
            "  --print-every-ms M : Log alle M Millisekunden Simulationszeit (Default 200).\n"
            "  --realtime       : simuliere im Echtzeit-Takt (Accumulator).\n"
            "Ctrl+C beendet sauber.\n";
            return 0;
        }
    }

    // Netz aufbauen
    Net net;
    const int N = 1000, FAN_IN = 30, Input_Neurons = 120, Output_Neurons = 120;
    net.build_small_demo(N, FAN_IN, Input_Neurons, Output_Neurons);
    IoLogger::instance().set_layer_info(Input_Neurons, Output_Neurons);

    //Logger Öffnen
    IoLogger::instance().open("./../../io/out/");
    IoLogger::instance().log_status("Brain initialized");

    // kleine Pause, damit der Coach/Monitor bereit ist
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // stdin non-blocking setzen
    {
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }

    // (optional) Phoneminventar: erste 8 Neuronen als Laute
    std::vector<std::string> PH = {"a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z","ä","ö","ü",",","."," "};

    // seconds -> steps (dt aus dem Netz)
    if (seconds >= 0.0) {
        steps = static_cast<long>(seconds / net.neu.dt);
    }

    float total_spikes = 0.0f;
    long  t = 0;                                  // Sim-Schrittzähler
    bool  infinite = (steps < 0);

    using clock = std::chrono::steady_clock;
    auto last = clock::now();
    std::chrono::duration<double> acc(0.0);
    const double sim_dt = net.neu.dt;             // z.B. 0.001 s
    const auto   sim_dt_dur = std::chrono::duration<double>(sim_dt);
    const int    max_steps_per_frame = 2000;      // Sicherheitsbremse

    // wie oft loggen (in Schritten)
    const long print_every_steps = std::max<long>(1, static_cast<long>((print_every_ms / 1000.0) / sim_dt));

    auto do_one_step = [&](long step_idx){
        process_commands(net);
        net.step_once(0.0f);

        int sp = std::accumulate(net.neu.spk.begin(), net.neu.spk.end(), 0);
        total_spikes += sp;

        if (step_idx % print_every_steps == 0) {

            IoLogger::instance().log_spike_matrix(net.neu.spk, step_idx);
            IoLogger::instance().log_spike(&net.H, step_idx, sp);
        }
    };

    while (running && (infinite || t < steps)) {
        if (realtime) {
            auto now = clock::now();
            acc += now - last;
            last = now;

            int steps_this_frame = 0;
            while (acc >= sim_dt_dur
                   && steps_this_frame < max_steps_per_frame
                   && (running && (infinite || t < steps))) {
                do_one_step(t++);
                acc -= sim_dt_dur;
                steps_this_frame++;
            }

        } else {
            do_one_step(t++);
        }
    }

    IoLogger::instance().log_status("Brain stopped");
    // Save hormones

    return 0;
}
