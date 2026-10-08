"""Local editor Python helper. Restricts remote execution to the space project."""
import argparse
import sys
import time
from pathlib import Path

sys.path.insert(0, "D:/Software/UE_5.8/Engine/Plugins/Experimental/PythonScriptPlugin/Content/Python")
import remote_execution

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--file")
    parser.add_argument("--code")
    args = parser.parse_args()
    client = remote_execution.RemoteExecution()
    client.start()
    try:
        for _ in range(20):
            project = Path(__file__).resolve().parents[2]
            nodes = [n for n in client.remote_nodes if Path(n.get("project_root", "")).resolve() == project]
            if nodes:
                break
            time.sleep(0.25)
        if not nodes:
            raise RuntimeError("No local space editor Python node discovered")
        print("NODE", nodes)
        client.open_command_connection(nodes[0]["node_id"])
        code = Path(args.file).read_text(encoding="utf-8") if args.file else args.code
        response = client.run_command(code, unattended=True, exec_mode=remote_execution.MODE_EXEC_FILE)
        print({k: response[k] for k in ("success", "result", "output") if k in response})
        if not response.get("success"):
            raise RuntimeError("Editor Python failed")
    finally:
        client.stop()
