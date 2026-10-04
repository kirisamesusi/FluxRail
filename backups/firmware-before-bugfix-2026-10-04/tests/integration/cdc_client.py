"""FluxRail CDC v1 example using an already-open caller-owned binary stream.
No port discovery, device open, upload, or automatic Flash save.
The stream must implement write(bytes), readline(), with caller-set read timeout.
"""
import json
import zlib

class FluxRailClient:
    def __init__(self, stream):
        self.stream = stream
        self.request_id = 0
        self.revision = None

    def request(self, operation, payload=None):
        self.request_id = self.request_id % 0xFFFFFFFF + 1
        message = {"protocol_version": 1, "id": self.request_id,
                   "op": operation, "payload": payload or {}}
        raw = json.dumps(message, separators=(",", ":"), ensure_ascii=True).encode("ascii")
        if len(raw) > 4096:
            raise ValueError("Request exceeds device line limit")
        # Partial stream writes are supported; no flush loop or retry of an accepted command.
        pending = raw + b"\n"
        while pending:
            count = self.stream.write(pending)
            if not count:
                raise TimeoutError("Write stalled; read device status before retrying")
            pending = pending[count:]
        reply = self.stream.readline()
        if not reply or len(reply.rstrip(b"\n")) > 4096:
            raise TimeoutError("Outcome unknown; reconnect/read status before retrying")
        response = json.loads(reply)
        if response.get("protocol_version") != 1 or response.get("id") != self.request_id:
            raise RuntimeError("Unexpected protocol or response id; reconnect session")
        self.revision = response.get("revision")
        if not response.get("ok"):
            raise RuntimeError(response.get("error"))
        return response["result"]

    def profile(self, profile_id):
        return self.request("get_profile", {"profile_id": profile_id})

    def set_profile(self, profile_id, config, motion):
        return self.request("set_profile", {"expected_revision": self.revision,
            "profile_id": profile_id, "config": config, "motion": motion})

    def save(self):
        # Caller should set a >=12-second stream timeout and present maintenance UI.
        return self.request("save_config", {"expected_revision": self.revision})

    def usb_mode(self):
        return self.request("get_usb_mode")

    def set_startup_usb_mode(self, mode):
        return self.request("set_startup_usb_mode", {"expected_revision": self.revision, "mode": mode})

    def set_io4_bindings(self, profile_id, bindings):
        return self.request("set_io4_bindings", {"expected_revision": self.revision, "profile_id": profile_id, "bindings": bindings})

    def reboot_to_saved_mode(self, target):
        # Application reboot only; never implicitly calls save or retries.
        return self.request("reboot_to_saved_mode", {"expected_revision": self.revision, "target": target})

    def export_snapshot(self):
        data, offset, version, crc, total = bytearray(), 0, None, None, None
        while True:
            chunk = self.request("export_config", {"offset": offset})
            if chunk.get("schema") != 2 or chunk.get("encoding") != "hex-le-schema2":
                raise ValueError("Unsupported snapshot schema")
            if version is None:
                version, crc, total = self.revision, chunk["crc32"], chunk["length"]
                if not 1 <= total <= 7680:
                    raise ValueError("Invalid snapshot length")
            if (self.revision, chunk["crc32"], chunk["length"], chunk["offset"]) != (version, crc, total, offset):
                raise RuntimeError("Snapshot changed; restart export at offset zero")
            block = bytes.fromhex(chunk["data"])
            if not 0 < len(block) <= 512 or chunk["next_offset"] != offset + len(block):
                raise ValueError("Invalid export chunk")
            data.extend(block)
            offset += len(block)
            if offset >= total:
                if offset != total or zlib.crc32(data) != crc:
                    raise ValueError("Snapshot CRC/length mismatch")
                return bytes(data)

    def import_snapshot(self, data):
        # Import previews RAM; save() is a separate explicit decision.
        if not 1 <= len(data) <= 7680:
            raise ValueError("Invalid snapshot length")
        expected = self.revision
        self.request("begin_import", {"expected_revision": expected,
            "length": len(data), "crc32": zlib.crc32(data)})
        for offset in range(0, len(data), 512):
            self.request("import_chunk", {"expected_revision": expected,
                "offset": offset, "data": data[offset:offset+512].hex()})
        return self.request("commit_import", {"expected_revision": expected})

# Example (stream belongs to the caller):
# client = FluxRailClient(stream)
# capabilities = client.request("hello")
# profile = client.profile(2)
# profile["motion"]["knobs"][0]["gain"] = 2.0
# client.set_profile(2, profile["config"], profile["motion"])
# snapshot = client.export_snapshot()
# client.save()  # only at the user's explicit save step
