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

```
