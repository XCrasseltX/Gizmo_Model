#include "livekit_stub.h"
#include "coach_logic.h"
#include "hormons_reader.h"
#include "brain_io.h"

#include <nlohmann/json.hpp>
#include <httplib.h>
#include <iostream>

using json = nlohmann::json;
using namespace httplib;

static bool server_running = false;

void send_to_livekit(const std::string& reply) {
    std::cout << "[LiveKit OUT] " << reply << std::endl;
}

// MCP-Request-Verarbeitung (Logik bleibt wie gehabt)
json process_request(const json& msg) {
    json reply = { {"type", "response"}, {"id", msg.value("id", 0)} };

    try {
        std::string method = msg.value("method", "");
        if (method == "get_prompt_context") {
            Hormones H;
            if (!read_latest_hormones("./../brain_core/io/out/spikes.jsonl", H)) {
                reply["error"] = { {"message", "Fehler beim Lesen der Hormonwerte"} };
            } else {
                std::string prompt = build_prompt();
                reply["result"] = {
                    {"prompt", prompt},
                    {"emotion", {
                        {"dopamine", H.dopamine},
                        {"serotonin", H.serotonin},
                        {"cortisol", H.cortisol},
                        {"adrenaline", H.adrenaline},
                        {"oxytocin", H.oxytocin},
                        {"melatonin", H.melatonin},
                        {"noradrenaline", H.noradrenaline},
                        {"endorphin", H.endorphin},
                        {"acetylcholine", H.acetylcholine},
                        {"testosterone", H.testosterone}
                    }}
                };
            }
        }
        else if (method == "apply_reward") {
            Decision d;
            d.feedback = msg["params"].value("feedback", "none");
            d.intensity = msg["params"].value("intensity", 0.0f);
            apply_feedback("./../brain_core/io/in/commands.jsonl", d);
            reply["result"] = { {"status", "ok"} };
        }
        else {
            reply["error"] = { {"message", "Unbekannte Methode"} };
        }
    }
    catch (const std::exception& e) {
        reply["error"] = { {"message", e.what()} };
    }

    return reply;
}

// Starte lokalen HTTP-Server → hier läuft dein Coach als MCP-Server
void handle_from_livekit(const std::string&) {
    if (server_running) {
        std::cout << "[LiveKit] Server läuft bereits.\n";
        return;
    }
    server_running = true;

    Server svr;

    svr.Post("/", [](const Request& req, Response& res) {
        try {
            json msg = json::parse(req.body, nullptr, false);
            if (msg.is_discarded()) {
                res.status = 400;
                res.set_content(R"({"error":"Ungültiges JSON"})", "application/json");
                return;
            }

            json reply = process_request(msg);
            send_to_livekit(reply.dump(2));
            res.set_content(reply.dump(2), "application/json");
        }
        catch (const std::exception& e) {
            json err = { {"error", e.what()} };
            res.status = 500;
            res.set_content(err.dump(2), "application/json");
        }
    });

    std::cout << "[LiveKit] MCP-Server läuft auf http://localhost:5001\n";
    svr.listen("0.0.0.0", 5001);
}