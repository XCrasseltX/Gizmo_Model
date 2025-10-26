# Gizmo Coach - Cross-Platform

## 🏗️ Compile Coach

### Windows & Linux/Raspberry Pi:
```bash
mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Binary liegt in:** `build/Release/gizmo_coach[.exe]`

---

## 🚀 Start Brain

### Windows:
```bash
.\build\Release\gizmo_coach.exe start-brain
```

### Linux/Raspberry Pi:
```bash
./build/Release/gizmo_coach start-brain
```

---

## 🧪 MCP Anfragen testen

### Prompt Context abrufen:
```bash
curl -X POST http://localhost:5001 \
     -H "Content-Type: application/json" \
     -d '{"method":"get_prompt_context","params":{}}'
```

### Hormone anpassen (Reward):
```bash
curl -X POST http://localhost:5001 \
     -H "Content-Type: application/json" \
     -d '{"method":"apply_reward","params":{"feedback":"reward","intensity":0.8}}'
```

---

## 🎮 Cheatsheet - Hormon-Befehle

Schreibe diese in `brain_core/io/in/commands.jsonl`:

### 🧠 Belohnungsphase
```json
{"ts":1234567890,"seq":1,"source":"manual","cmd":"set_hormones","data":{"dopamine":1.0,"cortisol":0.0,"adrenaline":0.2}}
```

### 😤 Stressphase
```json
{"ts":1234567890,"seq":2,"source":"manual","cmd":"set_hormones","data":{"dopamine":0.1,"cortisol":0.9,"adrenaline":0.6}}
```

### ⚡ Actionphase
```json
{"ts":1234567890,"seq":3,"source":"manual","cmd":"set_hormones","data":{"dopamine":0.4,"cortisol":0.2,"adrenaline":1.0}}
```

### 💤 Ruhephase
```json
{"ts":1234567890,"seq":4,"source":"manual","cmd":"set_hormones","data":{"dopamine":0.1,"cortisol":0.1,"adrenaline":0.0}}
```

---

## 📦 Voraussetzungen

### Beide Plattformen:
- Docker & Docker Compose
- CMake 3.16+
- C++17 Compiler

### Windows:
```powershell
# Docker Desktop installieren
# Visual Studio 2019+ mit C++ Workload
choco install cmake
```

### Linux/Raspberry Pi:
```bash
sudo apt update
sudo apt install -y build-essential cmake docker.io docker-compose git
sudo usermod -aG docker $USER
# Neu einloggen!
```

---

## 🐛 Troubleshooting

### Linux: "Permission denied"
```bash
chmod +x build/Release/gizmo_coach
```

### Linux: Docker Permission
```bash
sudo usermod -aG docker $USER
# Neu einloggen!
```

### Binary nicht gefunden
```bash
ls -la build/Release/
```
