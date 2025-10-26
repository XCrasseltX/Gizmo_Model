# Gizmo Python Server - Windows Setup

## Voraussetzungen

- Python 3.12 installiert
- C++ Coach läuft auf Port 5001
- Gemini API Key

## Installation

### 1. Projekt-Verzeichnis erstellen

```powershell
mkdir gizmo-server
cd gizmo-server
```

### 2. Virtuelle Umgebung erstellen

```powershell
# Virtuelle Umgebung erstellen
py -3.11 -m venv venv

# Aktivieren
.\venv\Scripts\activate
```

### 3. Dependencies installieren

```powershell
pip install -r requirements.txt
```

### 4. Konfiguration

Erstelle `.env` Datei und füge deinen Gemini API Key ein:

```env
CPP_COACH_URL=http://localhost:5001
GEMINI_API_KEY=dein-api-key-hier
GEMINI_MODEL=gemini-2.0-flash-exp
```

**Gemini API Key holen:**
- Gehe zu: https://aistudio.google.com/apikey
- Erstelle neuen API Key
- Kopiere in `.env`

### 5. alles starten

Stelle sicher, dass dein C++ Coach auf Port 5001 läuft:

```powershell
# Test ob Coach erreichbar ist
curl -X POST http://localhost:5001 `
     -H "Content-Type: application/json" `
     -d '{"method":"get_prompt_context","params":{"user_text":"test"}}'
```
redis starten

```powershell
docker run --name redis-server -d -p 5003:6379 redis
```

docker mcp starten

```powershell
docker mcp gateway run
```

### 6. Python Server starten

```powershell
python gizmo_server.py
```

**Ausgabe sollte sein:**
```
🚀 Starte Gizmo Python LLM Server
   C++ Coach: http://localhost:5001
   Gemini Modell: gemini-2.0-flash-exp
INFO:     Started server process
INFO:     Uvicorn running on http://0.0.0.0:5000
```

### 7. Tests ausführen

```powershell
# In neuem Terminal (venv aktiviert)
python test_gizmo.py
```

## Verzeichnisstruktur

```
gizmo-server/
├── venv/                 # Virtuelle Umgebung
├── gizmo_server.py      # Hauptserver
├── requirements.txt     # Dependencies
├── .env                 # Konfiguration
└── SETUP.md             # Diese Datei
```

## API Endpoints

### Health Check
```powershell
curl http://localhost:5000/health
```

### Konversation (wie HA es nutzt)
```powershell
curl -X POST http://localhost:5000/api/conversation `
     -H "Content-Type: application/json" `
     -d '{\"text\":\"Hallo Gizmo\",\"conversation_id\":\"test\",\"language\":\"de\"}'
```

## Troubleshooting

### Problem: "C++ Coach nicht erreichbar"
- Prüfe ob Coach auf Port 5001 läuft
- Teste direkt: `curl http://localhost:5001`

### Problem: "Gemini API Error"
- Prüfe API Key in `.env`
- Teste API Key: https://aistudio.google.com/apikey


## Windows Firewall

Falls HA von anderem Gerät zugreifen soll:

```powershell
# PowerShell als Admin
New-NetFirewallRule -DisplayName "Gizmo Server" -Direction Inbound -LocalPort 5000 -Protocol TCP -Action Allow
```

## Raspberry Pi Migration

Wenn alles auf Windows funktioniert:

1. **Projekt kopieren**
   ```bash
   scp -r gizmo-server/ pi@raspi-ip:/home/pi/
   ```

2. **Auf Raspi installieren**
   ```bash
   ssh pi@raspi-ip
   cd gizmo-server
   python3 -m venv venv
   source venv/bin/activate
   pip install -r requirements.txt
   ```

3. **Als Service einrichten** (siehe SYSTEMD.md)

## Wichtig für Raspi mit PiHole

Der Raspi läuft bereits:
- **PiHole** (Port 53, 80, 443)
- **Minecraft Server** (Port 25565)

→ **Port 5000 ist frei** ✓
→ **Port 5001 für C++ Coach** ✓

Keine Konflikte!