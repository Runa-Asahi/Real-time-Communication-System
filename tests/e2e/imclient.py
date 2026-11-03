import socket, struct, json, time, itertools
_seq = itertools.count(1)
def frame(type_, service="QQ", sender="", recipient="", text="", scope="direct", requestId=""):
    v = {"version": 2, "service": service, "sender": sender, "recipient": recipient, "sequence": 0,
         "text": text, "type": type_, "requestId": requestId, "scope": scope, "messageId": 0, "conversationSeq": 0}
    p = json.dumps(v, ensure_ascii=False).encode()
    return struct.pack(">I", len(p)) + p
class Conn:
    def __init__(self, port, host="127.0.0.1"):
        self.s = socket.create_connection((host, port), timeout=10); self.buf = b""; self.inbox = []
    def send(self, **kw): self.s.sendall(frame(**kw))
    def recv(self, timeout=8):
        self.s.settimeout(timeout); end = time.time() + timeout
        while True:
            if len(self.buf) >= 4:
                n = struct.unpack(">I", self.buf[:4])[0]
                if len(self.buf) >= 4 + n:
                    m = json.loads(self.buf[4:4+n]); self.buf = self.buf[4+n:]; return m
            if time.time() > end: return None
            try: d = self.s.recv(65536)
            except socket.timeout: return None
            if not d: return None
            self.buf += d
    def wait(self, pred, timeout=8):
        for i, m in enumerate(self.inbox):
            if pred(m): return self.inbox.pop(i)
        end = time.time() + timeout
        while time.time() < end:
            m = self.recv(max(0.1, end - time.time()))
            if m is None: continue
            if pred(m): return m
            self.inbox.append(m)
        return None
    def close(self): self.s.close()
def oneshot(port, **kw):
    c = Conn(port); c.send(**kw); m = c.recv(); c.close(); return m
def group(port, service, self_, pw, **params):
    body = dict(params); body.update({"self": self_, "password": pw})
    m = oneshot(port, type_="GROUP", service=service, sender=self_, text=json.dumps(body, ensure_ascii=False))
    if m["type"] == "GROUP_RESULT": return True, json.loads(m["text"])
    return False, m["text"]
class Chat:
    def __init__(self, port, service, account, pw):
        self.c = Conn(port); self.account = account; self.service = service
        self.c.send(type_="LOGIN", service=service, sender=account, text=pw)
        m = self.c.wait(lambda m: m["type"] in ("LOGIN_OK", "ERROR"))
        assert m and m["type"] == "LOGIN_OK", m
        self.c.send(type_="DIRECTORY", service=service, sender=account)
    def directory(self, timeout=8):
        m = self.c.wait(lambda m: m["type"] == "DIRECTORY_LIST", timeout)
        return json.loads(m["text"]) if m else None
    def latest_directory(self, timeout=3):
        d = None
        while True:
            x = self.directory(timeout if d is None else 0.8)
            if x is None: return d
            d = x
    def notice(self, timeout=8):
        return self.c.wait(lambda m: m["type"] == "GROUP_NOTICE", timeout)
    def send_group(self, gid, text):
        rid = "r%d" % next(_seq)
        self.c.send(type_="SEND", service=self.service, sender=self.account, recipient=gid, scope="group", text=text, requestId=rid)
        return self.c.wait(lambda m: (m["type"] in ("SEND_ACK", "ERROR")) and m.get("requestId") == rid)
    def deliver(self, pred=lambda m: True, timeout=8):
        return self.c.wait(lambda m: m["type"] == "DELIVER" and pred(m), timeout)
