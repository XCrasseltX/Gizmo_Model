# Gizmo_Model
Local Spiking Neuronal network for Hormon simulation to get a LLM for Homeassistant assist a Changing personality


Windows:
```bash
python gizmo_start.py
```
Linux:
```bash
chmod +x gizmo_start.py
./gizmo_start.py
```
Oder:
```bash
python3 gizmo_start.py

```

```bash
docker stop mcp-gateway
docker rm mcp-gateway

docker run -d   -p 5002:5002   --restart=always   --name=mcp-gateway   -v /var/run/docker.sock:/var/run/docker.sock   docker/mcp-gateway  --port=5002   --transport=streaming


docker logs -f mcp-gateway

# Status prüfen
sudo systemctl status gizmo.service

# Logs ansehen
sudo journalctl -u gizmo.service -f

# Service neu starten
sudo systemctl restart gizmo.service

# Status prüfen
sudo systemctl status gizmo.service

# Auto-Start deaktivieren (läuft nicht mehr bei Boot)
sudo systemctl disable gizmo.service

# Auto-Start wieder aktivieren
sudo systemctl enable gizmo.service

# redis daten anschauen:
docker exec -it redis-server redis-cli

keys gizmo:conv:*
get gizmo:conv:"Conversation-ID" | python3 -m json.tool

```

Anhalten:
```bash

# Service stoppen
sudo systemctl stop gizmo.service

# MCP Beenden
docker stop mcp-gateway
docker rm mcp-gateway

# Schauen welche container noch laufen
docker ps -a

# Container für brain und redis stoppen
docker rm -f # Namen der Container

# Service starten
sudo systemctl start gizmo.service
```