"""Small stdlib client for the project's local Unreal MCP server."""
import argparse
import json
import urllib.error
import urllib.request


class UnrealMCP:
    def __init__(self, url="http://127.0.0.1:8000/mcp"):
        self.url = url
        self.headers = {"Accept": "application/json, text/event-stream", "Content-Type": "application/json"}
        self.sequence = 0
        self.request("initialize", {"protocolVersion": "2025-11-25", "capabilities": {},
                                   "clientInfo": {"name": "jts-frontend-authoring", "version": "1.0"}})
        self.request("notifications/initialized", {}, notification=True)

    def request(self, method, params, notification=False):
        self.sequence += 1
        body = {"jsonrpc": "2.0", "method": method, "params": params}
        if not notification:
            body["id"] = self.sequence
        request = urllib.request.Request(self.url, json.dumps(body).encode(), self.headers)
        try:
            with urllib.request.urlopen(request, timeout=1200) as response:
                if response.headers.get("Mcp-Session-Id"):
                    self.headers["Mcp-Session-Id"] = response.headers["Mcp-Session-Id"]
                raw = response.read().decode("utf-8")
        except urllib.error.HTTPError as error:
            raise RuntimeError(error.read().decode("utf-8")) from error
        if not raw.strip():
            return None
        for line in raw.splitlines():
            if line.startswith("data: "):
                raw = line[6:]
        data = json.loads(raw)
        if "error" in data:
            raise RuntimeError(data["error"])
        return data.get("result", data)

    def call(self, name, arguments=None, toolset=None):
        args = {"tool_name": name, "arguments": arguments or {}}
        if toolset:
            args["toolset_name"] = toolset
        return self.request("tools/call", {"name": "call_tool", "arguments": args})


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["list", "describe", "call"])
    parser.add_argument("name", nargs="?")
    parser.add_argument("--toolset")
    parser.add_argument("--args", default="{}")
    args = parser.parse_args()
    client = UnrealMCP()
    if args.command == "list":
        result = client.request("tools/call", {"name": "list_toolsets", "arguments": {}})
    elif args.command == "describe":
        result = client.request("tools/call", {"name": "describe_toolset", "arguments": {"toolset_name": args.name}})
    else:
        result = client.call(args.name, json.loads(args.args), args.toolset)
    print(json.dumps(result, ensure_ascii=True, indent=2))
