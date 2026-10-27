"""Client for the installed Blender MCP add-on's null-delimited execute protocol."""
import argparse
import json
import socket
from pathlib import Path


def execute(code, timeout=240):
    with socket.create_connection(("127.0.0.1", 9876), timeout=10) as connection:
        connection.settimeout(timeout)
        connection.sendall(json.dumps({"type": "execute", "code": code, "strict_json": True}).encode() + b"\0")
        chunks = bytearray()
        while b"\0" not in chunks:
            block = connection.recv(65536)
            if not block:
                raise RuntimeError("Blender MCP closed without a response")
            chunks.extend(block)
        response = json.loads(chunks.split(b"\0", 1)[0])
        if response.get("status") != "ok":
            raise RuntimeError(response)
        return response


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("file", nargs="?")
    parser.add_argument("--code")
    parser.add_argument("--timeout", type=int, default=240)
    args = parser.parse_args()
    code = Path(args.file).read_text(encoding="utf-8-sig") if args.file else args.code
    print(json.dumps(execute(code, args.timeout), ensure_ascii=True, indent=2))
