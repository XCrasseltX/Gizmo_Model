"""
Gizmo Python LLM Server
Orchestriert: HA → C++ Coach → Gemini → HA
"""
import asyncio
import aiohttp
import logging
from typing import Optional, AsyncGenerator
from fastapi import FastAPI, HTTPException
from fastapi.responses import StreamingResponse
from pydantic import BaseModel
import httpx
import subprocess
import shlex
import json
import os
from dotenv import load_dotenv
from mcp import ClientSession, StdioServerParameters
from mcp.client.stdio import stdio_client
import redis
import time

load_dotenv()

# --- Port-Konfiguration ---
PORT_PYTHON_SERVER = 5000     # FastAPI Server
PORT_CPP_COACH = 5001         # C++ Coach Server
PORT_MCP_GATEWAY = 5002       # MCP Gateway (Docker oder lokal)
PORT_REDIS = 5003             # Redis Database Port

# Logging Setup
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("gizmo_server")

# Configuration
CPP_COACH_URL = os.getenv("CPP_COACH_URL", f"http://localhost:{PORT_CPP_COACH}")
GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "your-api-key-here")
GEMINI_MODEL = os.getenv("GEMINI_MODEL", "gemini-2.5-flash")
REDIS_URL = os.getenv("REDIS_URL", f"redis://localhost:{PORT_REDIS}/0")

app = FastAPI(title="Gizmo AI Server", version="1.0.1")


# Request Models
class ConversationRequest(BaseModel):
    text: str
    conversation_id: str
    language: str = "de"


class ConversationResponse(BaseModel):
    response: str
    conversation_id: str
    intent: Optional[dict] = None


class HealthResponse(BaseModel):
    status: str
    cpp_coach_available: bool
    gemini_configured: bool

# --- NEU: Redis Client für Memory ---
class ChatStorage:
    """Speichert vollständige Chats, Metadaten & Summaries."""

    def __init__(self, redis_url: str):
        self.r = redis.from_url(redis_url, decode_responses=True)

    def key_chat(self, cid): return f"gizmo:chat:{cid}"
    def key_meta(self, cid): return f"gizmo:meta:{cid}"
    def key_summary(self, cid): return f"gizmo:summary:{cid}"

    async def load_messages(self, cid: str) -> list:
        try:
            raw = await asyncio.to_thread(self.r.get, self.key_chat(cid))
            if raw:
                return json.loads(raw)
            return []
        except Exception:
            return []

    async def save_messages(self, cid: str, messages: list):
        await asyncio.to_thread(self.r.set, self.key_chat(cid), json.dumps(messages))

    async def save_meta(self, cid: str, created_ts: int, last_message: str, title="Chat"):
        meta = {
            "id": cid,
            "created": created_ts,
            "updated": int(time.time()),
            "title": title,
            "last_message": last_message
        }
        await asyncio.to_thread(self.r.set, self.key_meta(cid), json.dumps(meta))

    async def save_summary(self, cid: str, summary_text: str):
        await asyncio.to_thread(self.r.set, self.key_summary(cid), summary_text)

    async def get_meta(self, cid: str):
        raw = await asyncio.to_thread(self.r.get, self.key_meta(cid))
        if not raw:
            return None
        return json.loads(raw)

    async def ping(self):
        await asyncio.to_thread(self.r.ping)

# --- C++ Coach Client ---
class CppCoachClient:
    """Kommunikation mit dem C++ Coach — holt angereicherten Prompt & Hormonwerte"""

    def __init__(self, base_url: str):
        self.base_url = base_url
        self.timeout = httpx.Timeout(10.0, connect=5.0)

    async def get_enriched_prompt(self):
        """Ruft C++ Coach auf und gibt dict mit 'prompt' und 'emotion' zurück"""
        try:
            logger.info(f"Rufe C++ Coach auf für")
            async with httpx.AsyncClient(timeout=self.timeout) as client:
                payload = {
                    "method": "get_prompt_context",
                    "id": 1
                }
                response = await client.post(
                    self.base_url,
                    json=payload,
                    headers={"Content-Type": "application/json"}
                )
                response.raise_for_status()
                data = response.json()

                result = data.get("result", {})
                prompt = result.get("prompt")
                emotion = result.get("emotion", {})

                if not prompt:
                    logger.warning("Coach gab keinen Prompt zurück, nutze Fallback")
                    prompt = f"Du bist Gizmo. Der Nutzer fragt:"

                logger.info(f"Prompt erhalten: {prompt[:100]}...")
                return {"prompt": prompt, "emotion": emotion}

        except httpx.TimeoutException:
            logger.error("Timeout beim C++ Coach")
            raise HTTPException(status_code=504, detail="C++ Coach Timeout")
        except httpx.HTTPError as e:
            logger.error(f"HTTP Fehler beim C++ Coach: {e}")
            raise HTTPException(status_code=502, detail=f"C++ Coach Error: {str(e)}")
        except Exception as e:
            logger.exception("Unerwarteter Fehler beim C++ Coach")
            raise HTTPException(status_code=500, detail=f"Coach Error: {str(e)}")

# --- Docker MCP Client (verbindet sich mit laufendem Gateway) ---
class MCPGatewayClient:
    """Verbindet sich mit dem MCP Gateway über Streamable HTTP Protocol"""
    
    def __init__(self, gateway_host: str = "localhost", gateway_port: int = PORT_MCP_GATEWAY):
        self.gateway_url = f"http://{gateway_host}:{gateway_port}"
        self.mcp_endpoint = "/mcp"  # <-- NEU: Endpoint definieren
        self.client = None
        self.tools = []
        self.message_id = 0
        self.session_id = None
    
    async def connect(self):
        """Verbinde mit dem Gateway"""
        try:
            import httpx
            self.client = httpx.AsyncClient(
                base_url=self.gateway_url,
                timeout=30.0
            )
            
            logger.info(f"✅ Verbunden mit MCP Gateway auf {self.gateway_url}")
            
            # Initialize Session
            await self._initialize_session()
            
            return True
            
        except Exception as e:
            logger.error(f"Gateway Verbindung fehlgeschlagen: {e}")
            return False
    
    async def _initialize_session(self):
        """Initialize MCP Session"""
        try:
            result = await self._send_request("initialize", {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {
                    "name": "gizmo-server",
                    "version": "1.0.0"
                }
            })
            
            if result:
                logger.info("✅ MCP Session initialisiert")
                # Send initialized notification
                await self._send_notification("notifications/initialized")
            
        except Exception as e:
            logger.error(f"Session Init Error: {e}")
    
    async def _send_request(self, method: str, params: dict = None):
        """Sende MCP Request über HTTP POST und warte auf Response"""
        try:
            self.message_id += 1
            
            request_body = {
                "jsonrpc": "2.0",
                "id": self.message_id,
                "method": method
            }
            
            if params:
                request_body["params"] = params
            
            headers = {
                "Accept": "application/json, text/event-stream",
                "Content-Type": "application/json"
            }
            
            if self.session_id:
                headers["Mcp-Session-Id"] = self.session_id
            
            logger.info(f"🔧 Sende MCP Request: {method}")
            
            # POST Request an root endpoint
            response = await self.client.post(
                self.mcp_endpoint,  # <-- Statt "/" verwende "/mcp"
                json=request_body,
                headers=headers
            )
            
            logger.info(f"Response status: {response.status_code}")
            logger.info(f"Response headers: {dict(response.headers)}")
            
            # Check for session ID in response
            if "Mcp-Session-Id" in response.headers:
                self.session_id = response.headers["Mcp-Session-Id"]
                logger.info(f"Session ID: {self.session_id}")
            
            # Check Content-Type
            content_type = response.headers.get("content-type", "")
            
            if "text/event-stream" in content_type:
                # Server sendet SSE Stream
                logger.info("Empfange SSE Stream...")
                return await self._read_sse_response(response)
            else:
                # Server sendet JSON Response
                logger.info("Empfange JSON Response...")
                data = response.json()
                
                if "error" in data:
                    logger.error(f"MCP Error: {data['error']}")
                    return None
                
                return data.get("result")
            
        except Exception as e:
            logger.exception(f"Request Error für {method}")
            return None
    
    async def _read_sse_response(self, response):
        """Lese SSE Stream und extrahiere Response"""
        try:
            result = None
            
            async for line in response.aiter_lines():
                if not line or not line.startswith("data: "):
                    continue
                
                data_str = line[6:].strip()
                if not data_str or data_str == "[DONE]":
                    continue
                
                try:
                    event = json.loads(data_str)
                    
                    # Prüfe ob es unsere Response ist
                    if event.get("id") == self.message_id:
                        if "error" in event:
                            logger.error(f"MCP Error: {event['error']}")
                            return None
                        
                        result = event.get("result")
                        break
                        
                except json.JSONDecodeError:
                    logger.warning(f"Konnte JSON nicht dekodieren: {data_str}")
                    continue
            
            return result
            
        except Exception as e:
            logger.exception("SSE Read Error")
            return None
    
    async def _send_notification(self, method: str, params: dict = None):
        """Sende MCP Notification (keine Response erwartet)"""
        try:
            request_body = {
                "jsonrpc": "2.0",
                "method": method
            }
            
            if params:
                request_body["params"] = params
            
            headers = {
                "Content-Type": "application/json"
            }
            
            if self.session_id:
                headers["Mcp-Session-Id"] = self.session_id
            
            await self.client.post(
                self.mcp_endpoint,  # <-- Auch hier
                json=request_body,
                headers=headers
            )
            
        except Exception as e:
            logger.error(f"Notification Error: {e}")
    
    async def fetch_tools(self):
        """Hole Tools über MCP Protokoll"""
        try:
            result = await self._send_request("tools/list")
            
            if result:
                mcp_tools = result.get("tools", [])
                
                # Konvertiere MCP Tools zu Gemini Format
                self.tools = []
                for tool in mcp_tools:
                    gemini_tool = {
                        "name": tool.get("name"),
                        "description": tool.get("description", "")
                    }
                    
                    # Konvertiere inputSchema zu parameters
                    input_schema = tool.get("inputSchema", {})
                    if input_schema:
                        # Entferne MCP-spezifische Felder
                        parameters = {
                            "type": input_schema.get("type", "object"),
                            "properties": input_schema.get("properties", {}),
                            "required": input_schema.get("required", [])
                        }
                        gemini_tool["parameters"] = parameters
                    
                    self.tools.append(gemini_tool)
                
                logger.info(f"✅ {len(self.tools)} Tools geladen und konvertiert")
                logger.debug(f"Gemini Tools: {json.dumps(self.tools, indent=2)}")
                
                return self.tools
            else:
                logger.error("Keine Tools erhalten")
                return []
            
        except Exception as e:
            logger.error(f"Fetch Tools Error: {e}")
            return []
    
    async def call_tool(self, tool_name: str, parameters: dict):
        """Rufe Tool über MCP Protokoll auf"""
        try:
            result = await self._send_request("tools/call", {
                "name": tool_name,
                "arguments": parameters
            })
            
            if result:
                # MCP gibt content array zurück
                content = result.get("content", [])
                
                # Extrahiere Text aus content
                output_text = ""
                for item in content:
                    if isinstance(item, dict) and "text" in item:
                        output_text += item["text"]
                
                return {
                    "success": True,
                    "output": output_text or json.dumps(result)
                }
            else:
                return {
                    "success": False,
                    "error": "Keine Response vom Gateway"
                }
            
        except Exception as e:
            logger.exception(f"Tool Call Error")
            return {"success": False, "error": str(e)}
    
    async def disconnect(self):
        if self.client:
            await self.client.aclose()



# --- Gemini Client ---
class GeminiClient:
    """Client für Google Gemini API mit Streaming-Unterstützung"""

    def __init__(self, api_key: str, model: str):
        self.api_key = api_key
        self.model = model
        self.base_url = "https://generativelanguage.googleapis.com/v1beta"
        self.tools = []
    
    def register_tools(self, tools: list):
        """Registriere MCP Tools für Gemini"""
        self.tools = tools
        logger.info(f"Registriert {len(tools)} MCP Tools für Gemini")

    async def _stream_gemini_response(self, client, url, params, headers, body, contents_list, out_function_calls: list):
        """
        Interne Hilfsfunktion, um eine einzelne API-Anfrage zu streamen
        und die Antworten (Text und Tool-Calls) zu sammeln.
        
        MODIFIZIERT: Akzeptiert 'out_function_calls'-Liste und entfernt 'return'.
        """
        collected_text = ""
        # function_calls = []  <- ENTFERNT (wir verwenden out_function_calls)
        
        # Die Antwort der KI (ob Text oder Tool-Call) muss für den Verlauf gespeichert werden
        model_response_part = {"role": "model", "parts": []}

        async with client.stream("POST", url, json=body, headers=headers, params=params) as resp:
            if resp.status_code >= 400:
                error = await resp.aread()
                logger.error(f"Gemini Error {resp.status_code}: {error.decode()}")
                resp.raise_for_status()
            
            async for line in resp.aiter_lines():
                if not line or not line.startswith("data: "):
                    continue
                
                data_str = line[6:].strip()
                if data_str == "[DONE]":
                    break
                
                try:
                    event = json.loads(data_str)
                except json.JSONDecodeError:
                    logger.warning(f"Konnte JSON nicht dekodieren: {data_str}")
                    continue
                
                candidates = event.get("candidates", [])
                if not candidates:
                    continue
                
                content = candidates[0].get("content", {})
                parts = content.get("parts", [])
                
                for part in parts:
                    # Text sammeln UND streamen
                    if "text" in part:
                        text = part["text"]
                        collected_text += text
                        model_response_part["parts"].append({"text": text})
                        yield text  # Stream an User
                    
                    # Function Calls sammeln
                    if "functionCall" in part:
                        model_response_part["parts"].append(part)
                        
                        func = part["functionCall"]
                        # MODIFIZIERT: Füge zur 'out'-Liste hinzu
                        out_function_calls.append({
                            "name": func.get("name"),
                            "args": func.get("args", {})
                        })

        # Füge die gesamte Antwort der KI (Text und/oder Tool-Calls) zum Verlauf hinzu
        if model_response_part["parts"]:
            contents_list.append(model_response_part)

    async def generate_with_tools(self, contents: list, mcp_client, emotion: str) -> AsyncGenerator[str, None]:
        """
        Streamt Text + Tool Calls. Verwendet und aktualisiert den übergebenen 'contents' in-place.
        Die aktuelle Emotion wird als TEMPORÄRER Eintrag in 'contents' hinzugefügt.
        """
        
        url = f"{self.base_url}/models/{self.model}:streamGenerateContent"
        params = {"key": self.api_key, "alt": "sse"}
        headers = {"Content-Type": "application/json"}
        tool_config = {"function_declarations": self.tools} if self.tools else None
        
        # HIER ERFOLGT DIE VORBEREITUNG DER INHALTE VOR DER ERSTEN RUNDE
        # -----------------------------------------------------------------------
        
        # 1. Füge die aktuelle User-Nachricht hinzu (wird vom Endpunkt gemacht)
        # 2. Füge die TEMPORÄRE Emotions-Info hinzu, damit Gemini diese EINE Runde beachtet.
        # WICHTIG: Da diese Logik VOR dem Gemini-Aufruf erfolgt, muss der Endpunkt
        # nur den History-Teil des Contents übergeben. Wir verschieben das Anfügen
        # der User-Nachricht HIERHER.
        
        # Der User-Text ist der letzte Eintrag im contents-Array, den wir hier extrahieren.
        user_text_entry = contents.pop() 
        current_user_text = user_text_entry["parts"][0]["text"]

        # Füge die aktuelle, dynamische Emotion als temporären Kontext hinzu
        contents.append({"role": "user", "parts": [{"text": f"Aktuelle Stimmung: {emotion}"}]})
        contents.append({"role": "model", "parts": [{"text": "Ich werde darauf achten."}]})
        
        # Füge die eigentliche User-Nachricht hinzu
        contents.append(user_text_entry)
        
        # -----------------------------------------------------------------------
        
        async with httpx.AsyncClient(timeout=60.0) as client:
            
            # --- Schritt 1: Runde 1 (Aktueller Verlauf + TEMPORÄRER Emotion -> KI) ---
            logger.info(f"Starte Runde 1. Aktuelle dynamische Emotion: {emotion}")
            body_r1 = {
                "contents": contents, 
                "tools": [tool_config] if tool_config else []
            }

            # Rufe die Stream-Funktion auf
            # Sie `yield`et Text direkt an den User und gibt Tool-Calls zurück
            function_calls = []

            async for text_chunk in self. _stream_gemini_response(
                client, url, params, headers, body_r1, contents, function_calls
            ):
                yield text_chunk # Streamt den Text-Chunk weiter an den User

            # --- Schritt 3: Prüfen, ob Tools aufgerufen wurden ---
            if not function_calls:
                logger.info("Runde 1: Keine Tool-Calls. Antwort ist final.")
                return  # Fertig, aller Text wurde bereits gestreamt

            # --- Schritt 4: Tool-Ausführung (Zwischenschritt) ---
            logger.info(f"Runde 1: Führe {len(function_calls)} Tool Call(s) aus.")
            
            for call in function_calls:
                tool_name = call["name"]
                tool_args = call["args"]
                
                # Optional: Feedback an den User, dass etwas passiert
                yield f"\n\n[🔧 Denke nach... (Rufe Tool {tool_name} auf)]\n"
                logger.info(f"Tool Call: {tool_name}({tool_args})")
                
                result = await mcp_client.call_tool(tool_name, tool_args)
                
                # *** DAS IST DER WICHTIGSTE TEIL ***
                # Formatiere das Tool-Ergebnis für die API (Runde 2)
                
                tool_result_data = {}
                if result.get("success"):
                    output = result.get("output", "")
                    # Versuche, das Ergebnis als JSON zu laden, da MCP oft JSON-Strings liefert
                    try:
                        tool_result_data = json.loads(output)
                    except (json.JSONDecodeError, TypeError):
                        # Wenn es kein JSON ist, verpacke es einfach
                        tool_result_data = {"content": str(output)}
                else:
                    error = result.get("error", "Unbekannter Fehler")
                    tool_result_data = {"error": error}
                    logger.error(f"Tool {tool_name} fehlgeschlagen: {error}")

                # Füge das Tool-Ergebnis zum Verlauf (contents) hinzu
                contents.append({
                    "role": "tool",
                    "parts": [
                        {
                            "functionResponse": {
                                "name": tool_name,
                                "response": tool_result_data
                            }
                        }
                    ]
                })

            # --- Schritt 5: Runde 2 (Tool-Ergebnisse -> KI) ---
            logger.info("Starte Runde 2: Sende Tool-Ergebnisse an Gemini für finale Antwort.")
            
            # Der Body enthält jetzt den *gesamten* Verlauf
            body_r2 = {
                "contents": contents,
                "tools": [tool_config] if tool_config else [] # Tools müssen erneut gesendet werden
            }

            # --- Schritt 6: Finale Antwort streamen ---
            # Wir rufen dieselbe Stream-Funktion erneut auf.
            # Diesmal wird der `yield`ete Text die finale, zusammengefasste Antwort sein.
            # Eventuelle (unerwartete) Tool-Calls in Runde 2 ignorieren wir hier.
            async for text_chunk in self. _stream_gemini_response(
                client, url, params, headers, body_r2, contents, [] 
            ):
                yield text_chunk
            
            logger.info("Runde 2: Finale Antwort gestreamt.")

# Globale Clients
coach = CppCoachClient(CPP_COACH_URL)
gemini = GeminiClient(GEMINI_API_KEY, GEMINI_MODEL)
mcp_client = MCPGatewayClient(gateway_host="localhost", gateway_port=PORT_MCP_GATEWAY)
#mcp_client = DockerMCPClient() 
memory_client = ChatStorage(REDIS_URL)


@app.on_event("startup")
async def startup():
    # Verbinde Memory
    await memory_client.ping()
    logger.info("✅ Redis ChatStorage verbunden")
    # Verbinde mit Gateway
    await mcp_client.connect()
    
    # Hole Tools
    tools = await mcp_client.fetch_tools()
    if tools:
        gemini.register_tools(tools)
        logger.info("🚀 Gizmo Server ready mit Docker MCP Tools!")
    else:
        logger.warning("⚠️ Keine MCP Tools verfügbar")

@app.on_event("shutdown")
async def shutdown():
    await mcp_client.disconnect()

# API Endpoints
@app.get("/health", response_model=HealthResponse)
async def health_check():
    """Health Check für HA Integration"""
    
    # Prüfe C++ Coach
    coach_available = False
    try:
        async with httpx.AsyncClient(timeout=2.0) as client:
            response = await client.get(f"{CPP_COACH_URL}/health")
            coach_available = response.status_code == 200
    except:
        pass
    
    return HealthResponse(
        status="ok",
        cpp_coach_available=coach_available,
        gemini_configured=GEMINI_API_KEY != "your-api-key-here"
    )

@app.get("/debug/gemini")
async def debug_gemini():
    """Debug Endpoint - Test Gemini direkt"""
    logger.info("Debug: Teste Gemini direkt")
    
    test_prompt = "Du bist ein freundlicher Assistent."
    test_text = "Sage hallo in einem kurzen Satz."
    
    full_response = ""
    chunk_count = 0
    
    try:
        async for chunk in gemini.generate_stream(test_prompt, test_text):
            full_response += chunk
            chunk_count += 1
            logger.info(f"Debug Chunk {chunk_count}: {chunk}")
        
        return {
            "success": True,
            "chunks": chunk_count,
            "response": full_response,
            "length": len(full_response)
        }
    except Exception as e:
        logger.exception("Debug Gemini failed")
        return {
            "success": False,
            "error": str(e)
        }

@app.post("/api/conversation", response_model=ConversationResponse)
# --- Haupt-Funktion für non-streaming Konversation ---
async def process_conversation(request: ConversationRequest) -> ConversationResponse:
    """Non-streaming Hauptendpoint mit Server-Side Memory, dynamischer Emotion (nicht gespeichert)."""
    logger.info(f"Konversation {request.conversation_id}: {request.text}")

    try:
        # 1. Hol den Prompt und Emotion vom Coach (dynamisch)
        enriched = await coach.get_enriched_prompt()
        prompt = str(enriched["prompt"])
        emotion = str(enriched["emotion"])
        system_prompt = prompt 

        # 2. Lade den persistenten Verlauf aus Redis
        contents = await memory_client.load_messages(request.conversation_id)
        
        # 3. Wenn der Verlauf leer ist, initialisiere nur den persistenten System-Prompt
        if not contents:
            created_ts = int(time.time())

            # System Prompt wird immer gespeichert
            contents.append({
                "role": "system",
                "text": prompt,
                "timestamp": created_ts
            })

            contents.append({
                "role": "model",
                "text": "Verstanden, ich bin bereit.",
                "timestamp": created_ts + 1
            })

            await memory_client.save_meta(
                request.conversation_id,
                created_ts,
                last_message="Verstanden, ich bin bereit."
            )
        
        # 4. Füge die aktuelle User-Nachricht hinzu (persistent)
        # Die eigentliche User-Anfrage wird ZUERST hinzugefügt.
        contents.append({
            "role": "user",
            "text": request.text,
            "timestamp": int(time.time())
        })

        # 5. Führe Gemini-Generierung durch
        full_response = ""
        # Wir übergeben den Verlauf, den MCP Client und die AKTUELLE Emotion
        async for chunk in gemini.generate_with_tools(contents, mcp_client, emotion): 
            full_response += chunk

        if not full_response:
            full_response = "Entschuldigung, ich konnte keine Antwort generieren."

        # --- HIER Punkt 6 EINSETZEN ---

        # 6A: Modellantwort speichern (normale KI-Antwort)
        model_ts = int(time.time())
        contents.append({
            "role": "model",
            "text": full_response,
            "timestamp": model_ts
        })

        # 6B: Toolcalls speichern (falls welche passiert sind)
        # generate_with_tools hat eine Liste mit toolcalls geliefert
        for call in mcp_client.collected_toolcalls:
            contents.append({
                "role": "tool",
                "tool_name": call["name"],
                "request": call["args"],
                "response": call.get("result_data", {}),
                "timestamp": int(time.time())
            })
    

        # 6. Bereinige temporäre Einträge im Verlauf (besserer Filter)
        clean_contents = []
        for entry in contents:
            role = entry.get("role")
            parts = entry.get("parts", [])

            # Überspringe nur temporäre Systeminfos
            if role == "user" and parts and parts[0].get("text", "").startswith("Aktuelle Stimmung:"):
                continue
            if role == "model" and any(p.get("text") == "Ich werde darauf achten." for p in parts):
                continue

            # Kombiniere Texte in einem Model-Part, falls mehrere Stücke vorhanden
            if role == "model":
                text_parts = [p.get("text", "") for p in parts if "text" in p]
                combined_text = "".join(text_parts).strip()
                if combined_text:
                    entry = {"role": "model", "parts": [{"text": combined_text}]}

            clean_contents.append(entry)

        # 7. Speichere den bereinigten Verlauf zurück
        await memory_client.save_meta(
            request.conversation_id,
            created_ts=memory_client[0]["timestamp"],
            last_message=full_response
        )
        logger.info(f"💾 Verlauf für {request.conversation_id} gespeichert: {len(clean_contents)} Einträge")

        logger.info(f"Antwort generiert. Neuer bereinigter Verlauf: {len(contents)} Teile.")
        return ConversationResponse(response=full_response, conversation_id=request.conversation_id)
    
        # todo: summary generation after 6 messages

    except HTTPException:
        raise
    except Exception as e:
        logger.exception("Fehler bei Konversationsverarbeitung")
        raise HTTPException(status_code=500, detail=str(e))


@app.post("/api/conversation/stream")
# --- Streaming-Haupt-Funktion ---
async def process_conversation_stream(request: ConversationRequest):
    """Streaming Endpoint: sendet Text-Events Stück für Stück an Client"""
    async def event_generator():
        try:
            enriched = await coach.get_enriched_prompt(request.text)
            system_prompt = enriched["prompt"] + enriched["emotion"]

            print(system_prompt)

            async for chunk in gemini.generate_stream(system_prompt, request.text):
                yield f"data: {json.dumps({'text': chunk})}\n\n"

            yield "data: [DONE]\n\n"

        except Exception as e:
            logger.exception("Stream Error")
            yield f"data: {json.dumps({'error': str(e)})}\n\n"

    return StreamingResponse(event_generator(), media_type="text/event-stream")


if __name__ == "__main__":
    import uvicorn
    
    logger.info("🚀 Starte Gizmo Python LLM Server")
    logger.info(f"   C++ Coach: {CPP_COACH_URL}")
    logger.info(f"   Gemini Modell: {GEMINI_MODEL}")
    logger.info(f"   Gemini API KEY: {GEMINI_API_KEY}")
    
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=PORT_PYTHON_SERVER,
        log_level="info"
    )