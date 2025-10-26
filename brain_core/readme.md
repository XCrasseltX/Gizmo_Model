# Gizmo Brain 🧠⚡

Spiking Neural Network mit Hormon-Modell
Läuft isoliert im Docker-Container für maximale Sicherheit

---

## 🔧 Voraussetzungen

* **Docker Desktop** im **Linux-Container-Modus**
* Projektverzeichnis: `Gizmo_Model/brain_core`

---

## 🚀 Start & Build

### 1. Container starten

```powershell
cd Gizmo_Model\brain_core
docker compose run --rm --entrypoint /bin/bash brain
```

### 2. Projekt im Container bauen

```bash
rm -rf build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 3. AI starten

**Termainal 1 (Powershell)**
```bash
$OutputEncoding = [System.Text.Encoding]::UTF8
cd **Path** Gizmo_Model/brain_core
Get-Content brain_phonemes.log -Wait
```
**Termainal 2 (Docker)**
```bash
$OutputEncoding = [System.Text.Encoding]::UTF8
./build/brain --steps -1 --realtime 2> brain_phonemes.log
```

---

## 👏 Beenden

* Mit `Ctrl+C` die AI stoppen
* Mit `exit` den Container verlassen
* Der Container wird durch `--rm` automatisch gelöscht

---

### Für den fall das ein oder mehr Container noch laufen 

```bash 
# Container Listen:
docker ps -a 
# Container Killen
docker rm -f **Container Namen**
```

## 📝 CLI Cheat Sheet

```bash
--steps N             # Simuliere N Schritte (1 Schritt = dt Sekunden). N < 0 = endlos (bis Ctrl+C)
--seconds S           # Simuliere ca. S Sekunden (überschreibt --steps)
--print-every-ms M    # Log alle M Millisekunden Simulationszeit (Standard: 200)
--realtime            # Simuliere im Echtzeit-Takt (mit Accumulator)
```

---

