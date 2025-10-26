### Compile Coach
```bash
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release .. 
cmake --build build --config Release
```
### Start Brain
```bash
.\build\Release\gizmo_coach.exe start-brain
```
### sende eine mcp anfrage damit der prompt zurück kommt
```bash
curl -X POST http://localhost:5001 -H "Content-Type: application/json" -d '{"method":"get_prompt_context","params":{"user_text":"mach das licht an"}}'
```

### sende eine mcp anfrage um die hormone anzupassen
```bash
curl -X POST http://localhost:5001 `
     -H "Content-Type: application/json" `
     -d '{"method":"get_prompt_context","params":{"user_text":"mach das licht an"}}'
```

### Cheatcheet

# 🧠 Belohnungsphase starten
{"cmd": "set_hormones", "dopamine": 1.0, "cortisol": 0.0, "adrenaline": 0.2}
→ Dopamin-Schub (Freude)

# 😤 Stressphase
{"cmd": "set_hormones", "dopamine": 0.1, "cortisol": 0.9, "adrenaline": 0.6}
→ Cortisol hoch, Dopamin runter

# ⚡ Actionphase
{"cmd": "set_hormones", "dopamine": 0.4, "cortisol": 0.2, "adrenaline": 1.0}
→ Adrenalin pusht alles

# 💤 Ruhephase
{"cmd": "set_hormones", "dopamine": 0.1, "cortisol": 0.1, "adrenaline": 0.0}
→ Entspannung
