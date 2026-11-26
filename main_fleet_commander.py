import yaml
import subprocess
import time
import sys
import os


def load_fleet_config(path="config/fleet.yaml"):
    try:
        with open(path, 'r') as f:
            return yaml.safe_load(f)
    except FileNotFoundError:
        print(f"Error: {path} not found. Please create the fleet configuration.")
        sys.exit(1)


def run_fleet():
    config = load_fleet_config()
    ipc_bus = config['orchestrator']['ipc_bus']
    fleet = config.get('fleet', {})

    print("==================================================")
    print(" Axiom Kernel: Initializing AI Fleet Commander")
    print(f" Target IPC Bus: {ipc_bus}")
    print("==================================================")

    active_processes = {}

    # 1. Parse config and spawn enabled agents
    for agent_name, settings in fleet.items():
        if settings.get('enabled', False):
            script_path = settings.get('script')

            if not os.path.exists(script_path):
                print(f"[WARNING] Skipping {agent_name}: Script {script_path} not found.")
                continue

            print(f"[LAUNCHING] {agent_name.upper()} | {settings['description']}")

            # Spawn the agent as an independent process
            # We pass the IPC bus as a command-line argument so the agent knows where to listen
            process = subprocess.Popen(
                [sys.executable, script_path, "--ipc", ipc_bus],
                stdout=sys.stdout,
                stderr=sys.stderr
            )
            active_processes[agent_name] = process
            time.sleep(0.2)  # Stagger launches slightly to prevent CPU spiking

    if not active_processes:
        print("\n[ALERT] No agents enabled in fleet.yaml. Exiting.")
        sys.exit(0)

    print("\n[STATUS] Fleet deployed successfully. Awaiting sensor telemetry...")
    print("Press Ctrl+C to terminate the fleet.\n")

    # 2. Monitor processes and handle graceful shutdown
    try:
        while True:
            # Keep the orchestrator alive while agents run in the background
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n==================================================")
        print(" Axiom Kernel: Commencing Fleet Shutdown")
        print("==================================================")

        for agent_name, process in active_processes.items():
            print(f"[TERMINATING] {agent_name.upper()}...")
            process.terminate()
            process.wait()

        print("Fleet successfully grounded.")
        sys.exit(0)


if __name__ == "__main__":
    run_fleet()