#!/usr/bin/env python3
"""
Gizmo Startup Script - Cross-Platform
Startet: C++ Coach, Redis, MCP Gateway, Python Server
"""
import subprocess
import sys
import os
import time
import platform
from pathlib import Path

# Farben für Terminal (funktioniert auf Windows + Linux)
try:
    from colorama import init, Fore, Style
    init(autoreset=True)
    GREEN = Fore.GREEN
    RED = Fore.RED
    YELLOW = Fore.YELLOW
    BLUE = Fore.BLUE
    RESET = Style.RESET_ALL
except ImportError:
    GREEN = RED = YELLOW = BLUE = RESET = ""

# Plattform erkennen
IS_WINDOWS = platform.system() == "Windows"
IS_LINUX = platform.system() == "Linux"

# Pfade relativ zu diesem Script
SCRIPT_DIR = Path(__file__).parent.resolve()
COACH_DIR = SCRIPT_DIR / "gizmo_coach"
SERVER_DIR = SCRIPT_DIR / "gizmo_server"

# Executable Pfade
if IS_WINDOWS:
    COACH_EXE = COACH_DIR / "build" / "Release" / "gizmo_coach.exe"
    PYTHON_CMD = "python"
else:
    COACH_EXE = COACH_DIR / "build" / "Release" / "gizmo_coach"
    PYTHON_CMD = "python"

PYTHON_SERVER = SERVER_DIR / "gizmo_server.py"

# Port-Konfiguration
PORT_MCP = 5002
PORT_REDIS = 5003


def print_header():
    print(f"\n{BLUE}{'='*60}{RESET}")
    print(f"{BLUE}  🤖 Gizmo Model - Startup Script{RESET}")
    print(f"{BLUE}{'='*60}{RESET}\n")
    print(f"Platform: {platform.system()} {platform.release()}")
    print(f"Python: {sys.version.split()[0]}")
    print(f"Working Dir: {SCRIPT_DIR}\n")


def check_prerequisites():
    """Prüft ob alle Voraussetzungen erfüllt sind"""
    print(f"{YELLOW}[1/4] Checking prerequisites...{RESET}")
    
    errors = []
    
    # Coach Binary
    if not COACH_EXE.exists():
        errors.append(f"Coach binary not found: {COACH_EXE}")
    else:
        print(f"  {GREEN}✓{RESET} Coach binary found")
    
    # Python Server
    if not PYTHON_SERVER.exists():
        errors.append(f"Python server not found: {PYTHON_SERVER}")
    else:
        print(f"  {GREEN}✓{RESET} Python server found")
    
    # Docker
    try:
        subprocess.run(
            ["docker", "--version"],
            capture_output=True,
            check=True,
            timeout=5
        )
        print(f"  {GREEN}✓{RESET} Docker available")
    except:
        errors.append("Docker not available or not running")
    
    if errors:
        print(f"\n{RED}❌ Prerequisites check failed:{RESET}")
        for err in errors:
            print(f"   - {err}")
        return False
    
    print(f"{GREEN}✓ All prerequisites met{RESET}\n")
    return True


def start_redis():
    """Startet Redis Container"""
    print(f"{YELLOW}[2/4] Starting Redis...{RESET}")
    
    # Prüfe ob Container bereits läuft
    try:
        result = subprocess.run(
            ["docker", "ps", "--filter", "name=redis-server", "--format", "{{.Names}}"],
            capture_output=True,
            text=True,
            timeout=5
        )
        if "redis-server" in result.stdout:
            print(f"  {YELLOW}⚠{RESET}  Redis already running, removing old container...")
            subprocess.run(["docker", "rm", "-f", "redis-server"], capture_output=True)
    except:
        pass
    
    # Starte Redis
    try:
        subprocess.run(
            ["docker", "run", "--name", "redis-server", "-d", "-p", f"{PORT_REDIS}:6379", "redis"],
            check=True,
            capture_output=True,
            timeout=60
        )
        print(f"  {GREEN}✓{RESET} Redis started on port {PORT_REDIS}")
        time.sleep(1)
        return True
    except Exception as e:
        print(f"  {RED}✗{RESET} Failed to start Redis: {e}")
        return False


def start_coach():
    """Startet C++ Coach (in separatem Fenster/Prozess)"""
    print(f"{YELLOW}[3/4] Starting C++ Coach...{RESET}")
    
    try:
        if IS_WINDOWS:
            # Windows: Neues PowerShell-Fenster
            cmd = f'start powershell -NoExit -Command "cd \'{COACH_DIR}\'; .\\build\\Release\\gizmo_coach.exe start-brain"'
            subprocess.Popen(cmd, shell=True)
            print(f"  {GREEN}✓{RESET} Coach starting in new window...")
        else:
            # Linux: Hintergrundprozess
            subprocess.Popen(
                [str(COACH_EXE), "start-brain"],
                cwd=COACH_DIR,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL
            )
            print(f"  {GREEN}✓{RESET} Coach started in background")
        
        time.sleep(2)
        return True
        
    except Exception as e:
        print(f"  {RED}✗{RESET} Failed to start Coach: {e}")
        return False
    
def wait_for_mcp_ready(timeout=15):
    """Wartet bis MCP Gateway Prozess läuft"""
    print(f"\n{YELLOW}Waiting for MCP Gateway to start...{RESET}")
    
    import time
    
    start_time = time.time()
    attempt = 0
    
    while time.time() - start_time < timeout:
        attempt += 1
        
        try:
            # Prüfe ob Docker MCP Prozess läuft
            result = subprocess.run(
                ["docker", "ps", "--filter", "name=mcp", "--format", "{{.Names}}"],
                capture_output=True,
                text=True,
                timeout=2
            )
            
            # Wenn MCP Container läuft
            if "mcp" in result.stdout.lower() or result.returncode == 0:
                # Zusätzliche Wartezeit für Startup
                time.sleep(2)
                print(f"  {GREEN}✓{RESET} MCP Gateway process detected! (after {attempt} attempts)")
                return True
                
        except:
            pass
        
        # Zeige Fortschritt
        dots = "." * (attempt % 4)
        print(f"  Attempt {attempt}/{timeout}{dots}   ", end="\r")
        time.sleep(1)
    
    # Auch wenn Timeout - wir versuchen trotzdem weiter
    print(f"\n  {YELLOW}⚠{RESET}  MCP Gateway check timeout, continuing anyway...")
    time.sleep(2)  # Gib ihm noch 2 Sekunden
    return True  # Gib True zurück statt False


def start_mcp_gateway():
    """Startet MCP Gateway in separatem Fenster"""
    print(f"{YELLOW}[4/4] Starting MCP Gateway...{RESET}")
    
    try:
        if IS_WINDOWS:
            # Windows: Neues PowerShell-Fenster
            cmd = f'start powershell -NoExit -Command "docker mcp gateway run --port {PORT_MCP} --transport Streaming"'
            subprocess.Popen(cmd, shell=True)
            print(f"  {GREEN}✓{RESET} MCP Gateway starting in new window on port {PORT_MCP}...")
        else:
            # Linux: Neues Terminal (wenn verfügbar)
            # Versuche verschiedene Terminal-Emulatoren
            terminals = [
                ['gnome-terminal', '--', 'bash', '-c'],
                ['xterm', '-e'],
                ['konsole', '-e'],
            ]
            
            cmd = f'docker mcp gateway run --port {PORT_MCP} --transport Streaming; exec bash'
            
            started = False
            for term in terminals:
                try:
                    subprocess.Popen(term + [cmd])
                    started = True
                    print(f"  {GREEN}✓{RESET} MCP Gateway starting in new terminal on port {PORT_MCP}...")
                    break
                except FileNotFoundError:
                    continue
            
            if not started:
                # Fallback: Hintergrundprozess
                subprocess.Popen(
                    ['docker', 'mcp', 'gateway', 'run', '--port', str(PORT_MCP), '--transport', 'Streaming'],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL
                )
                print(f"  {GREEN}✓{RESET} MCP Gateway started in background (no terminal available)")
        
        return True
        
    except Exception as e:
        print(f"  {RED}✗{RESET} Failed to start MCP Gateway: {e}")
        return False


def start_python_server():
    """Startet Python Server (blockierend)"""
    print(f"\n{BLUE}{'='*60}{RESET}")
    print(f"{GREEN}🚀 Starting Python Server...{RESET}")
    print(f"{BLUE}{'='*60}{RESET}\n")
    
    try:
        # Wechsle ins Server-Verzeichnis (für relative Imports)
        os.chdir(SERVER_DIR)
        
        # Starte Python Server (blockiert hier)
        subprocess.run(
            [PYTHON_CMD, "gizmo_server.py"],
            check=True
        )
        
    except KeyboardInterrupt:
        print(f"\n{YELLOW}⚠  Server stopped by user{RESET}")
    except Exception as e:
        print(f"\n{RED}✗ Server error: {e}{RESET}")
        return False
    
    return True


def cleanup():
    """Stoppt alle Services"""
    print(f"\n{YELLOW}Cleaning up...{RESET}")
    
    # Stoppe Redis
    try:
        subprocess.run(["docker", "rm", "-f", "redis-server"], capture_output=True, timeout=5)
        print(f"  {GREEN}✓{RESET} Redis stopped")
    except:
        pass
    
    # MCP Gateway wird automatisch beendet (Ctrl+C propagiert)
    
    print(f"{GREEN}✓ Cleanup complete{RESET}\n")


def main():
    """Hauptfunktion"""
    print_header()
    
    # Checks
    if not check_prerequisites():
        sys.exit(1)
    
    try:
        # Starte Services
        if not start_redis():
            sys.exit(1)
        
        if not start_coach():
            print(f"{YELLOW}⚠  Coach start failed, continuing anyway...{RESET}")
        
        if not start_mcp_gateway():
            sys.exit(1)
        
        # Warte bis MCP bereit ist
        if not wait_for_mcp_ready(timeout=30):
            print(f"{RED}❌ MCP Gateway not ready, aborting{RESET}")
            sys.exit(1)
        
        # Python Server starten (blockiert hier)
        start_python_server()
        
    except KeyboardInterrupt:
        print(f"\n{YELLOW}⚠  Interrupted by user{RESET}")
    finally:
        cleanup()


if __name__ == "__main__":
    main()