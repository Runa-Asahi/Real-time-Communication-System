import sys, json, time
from imclient import *
P = int(sys.argv[1]); PW = "123"
results = []
def check(name, cond, detail=""):
    results.append((name, bool(cond))); print(("PASS " if cond else "FAIL ") + name + ("  " + str(detail) if detail else "")); sys.stdout.flush()

# online sessions: U1 owner, U2 admin, U3 member, U4 outsider, U5 outsider
c1 = Chat(P, "QQ", "2026001", PW); c2 = Chat(P, "QQ", "2026002", PW); c3 = Chat(P, "QQ", "2026003", PW)
c4 = Chat(P, "QQ", "2026004", PW); c5 = Chat(P, "QQ", "2026005", PW)
for c in (c1, c2, c3, c4, c5): c.latest_directory(3)

# 1. invite owner / existing member rejected
ok, r = group(P, "QQ", "2026003", PW, op="invite", group="1001", target="2026001")
check("1a 邀请群主被拒", not ok and "群主" in r, r)
ok, r = group(P, "QQ", "2026001", PW, op="invite", group="1001", target="2026002")
check("1b 邀请已在群成员被拒", not ok and "成员" in r, r)
ok, r = group(P, "QQ", "2026001", PW, op="invite", group="1001", target="2026001")
check("1c 邀请自己被拒", not ok, r)
ok, r = group(P, "QQ", "2026001", PW, op="invite", group="1001", target="2026004")
check("1d 重复邀请（已有待审批申请）被拒", not ok and "待审批" in r, r)
ok, r = group(P, "QQ", "2026001", PW, op="invite", group="1001", target="9999999")
check("1e 邀请不存在账号被拒", not ok and "没有账号" in r, r)

# 2. approval shared: U1 owner & U2 admin both see U4 pending; U2 approves; U1 sees Approved; double approve rejected
ok, d1 = group(P, "QQ", "2026001", PW, op="detail", group="1001")
st = [x for x in d1["pending"] if x["account"] == "2026004" and x["kind"] == "application"]
check("2a 群主看到 2026004 待审批", ok and st and st[0]["pending"], st)
ok, r = group(P, "QQ", "2026002", PW, op="approve", group="1001", target="2026004")
check("2b 管理员批准成功", ok, r)
n1 = c1.notice(); n4 = c4.notice()
check("2c 群主实时收到群动态推送", n1 is not None, n1 and n1["text"])
check("2d 被批准者实时收到推送", n4 is not None)
ok, d1 = group(P, "QQ", "2026001", PW, op="detail", group="1001")
st = [x for x in d1["pending"] if x["account"] == "2026004" and x["kind"] == "application"]
check("2e 群主那边状态变为 Approved（非 pending）", st and st[0]["status"] == "Approved" and not st[0]["pending"], st)
ok, r = group(P, "QQ", "2026001", PW, op="approve", group="1001", target="2026004")
check("2f 第二个管理员重复审批被拒", not ok and "已被处理" in r, r)
dir4 = c4.latest_directory(3)
check("2g 被批准者聊天首页出现群 1001", dir4 and "1001" in dir4["groups"], dir4 and dir4["groups"])

# 3. rename persisted & broadcast
ok, r = group(P, "QQ", "2026001", PW, op="rename", group="1001", name="答辩演示群")
check("3a 群主改名成功", ok, r)
d3 = c3.latest_directory(3)
check("3b 成员实时收到新群名", d3 and d3.get("names", {}).get("1001") == "答辩演示群", d3 and d3.get("names"))
ok, l3 = group(P, "QQ", "2026003", PW, op="list", group="")
check("3c 成员刷新后群名一致", ok and any(g["id"] == "1001" and g["name"] == "答辩演示群" for g in l3["groups"]))
ok, r = group(P, "QQ", "2026003", PW, op="rename", group="1001", name="x")
check("3d 普通成员改名被拒", not ok, r)
# profile nickname
m = oneshot(P, type_="PROFILE", service="QQ", sender="2026002", text=json.dumps({"password": PW, "nickname": "新昵称二号", "location": "", "newPassword": ""}, ensure_ascii=False))
check("3e 昵称同步到服务端", m["type"] == "PROFILE_OK", m["text"])
d1 = c1.latest_directory(3)
check("3f 好友实时看到新昵称", d1 and d1.get("names", {}).get("2026002") == "新昵称二号", d1 and d1.get("names"))

# 4. create group: appears for creator immediately, and after invite/approve for member
ok, r = group(P, "QQ", "2026005", PW, op="create", group="2077")
check("4a 新建群成功", ok, r)
d5 = c5.latest_directory(3)
check("4b 创建者聊天首页立即出现新群", d5 and "2077" in d5["groups"], d5 and d5["groups"])
ok, r = group(P, "QQ", "2026005", PW, op="create", group="2077")
check("4c 重复群号被拒", not ok, r)
ok, r = group(P, "QQ", "2026005", PW, op="invite", group="2077", target="2026003")
ok2, r2 = group(P, "QQ", "2026005", PW, op="approve", group="2077", target="2026003")
check("4d 邀请+审批 成功", ok and ok2, (r, r2))
d3 = c3.latest_directory(3)
check("4e 新成员聊天首页实时出现新群", d3 and "2077" in d3["groups"], d3 and d3["groups"])
a = c5.send_group("2077", "大家好")
check("4f 新群可发消息", a and a["type"] == "SEND_ACK", a)
dv = c3.deliver(lambda m: m["recipient"] == "2077")
check("4g 新成员收到新群消息", dv is not None)

# 5. subgroup in chat home + chat
d3 = c3.latest_directory(1) or c3.latest_directory(3)
ok, r = group(P, "QQ", "2026003", PW, op="list", group="")
dir3 = Chat(P, "QQ", "2026006", PW)  # unrelated
dd = dir3.latest_directory(3)
check("5a 非成员看不到子群", dd and not any("#" in g for g in dd["groups"]))
ok, r = group(P, "QQ", "2026001", PW, op="disc_create", group="1001", discussion="D2")
check("5b 创建子群 D2", ok, r)
ok, r = group(P, "QQ", "2026001", PW, op="disc_join", group="1001", discussion="D2", target="2026004")
check("5c 拉 2026004 进子群", ok, r)
d4 = c4.latest_directory(3)
check("5d 子群出现在成员聊天首页", d4 and "1001#D2" in d4["groups"], d4 and d4["groups"])
check("5e 子群有显示名", d4 and "子群" in d4.get("names", {}).get("1001#D2", ""), d4 and d4.get("names"))
d3 = c3.latest_directory(3)
check("5f 旧子群 D1 也在成员 2026003 首页", d3 and "1001#D1" in d3["groups"], d3 and d3["groups"])
check("5g 非子群成员 2026003 看不到 D2", d3 and "1001#D2" not in d3["groups"])
a = c4.send_group("1001#D2", "子群消息")
check("5h 子群可发消息", a and a["type"] == "SEND_ACK", a)
dv = c1.deliver(lambda m: m["recipient"] == "1001#D2")
check("5i 子群成员收到子群消息", dv is not None)
a = c3.send_group("1001#D2", "越权")
check("5j 非子群成员发子群消息被拒", a and a["type"] == "ERROR", a)
ok, r = group(P, "QQ", "2026001", PW, op="disc_join", group="1001", discussion="D2", target="2026004")
check("5k 重复拉人被拒", not ok, r)

# extra: kick / leave / dissolve / permissions
ok, r = group(P, "QQ", "2026003", PW, op="kick", group="1001", target="2026004")
check("6a 普通成员踢人被拒", not ok, r)
ok, r = group(P, "QQ", "2026002", PW, op="kick", group="1001", target="2026001")
check("6b 管理员踢群主被拒", not ok, r)
ok, r = group(P, "QQ", "2026002", PW, op="kick", group="1001", target="2026004")
check("6c 管理员踢普通成员成功", ok, r)
d4 = c4.latest_directory(3)
check("6d 被踢者首页群与子群消失", d4 and "1001" not in d4["groups"] and "1001#D2" not in d4["groups"], d4 and d4["groups"])
a = c4.send_group("1001", "我还在吗")
check("6e 被踢者发群消息被拒", a and a["type"] == "ERROR", a)
ok, r = group(P, "QQ", "2026001", PW, op="leave", group="1001")
check("6f 群主不能直接退群", not ok, r)
ok, r = group(P, "QQ", "2026004", PW, op="apply", group="1001")
ok2, r2 = group(P, "QQ", "2026004", PW, op="apply", group="1001")
check("6g 重复申请被拒", ok and not ok2, (r, r2))
ok, r = group(P, "QQ", "2026001", PW, op="reject", group="1001", target="2026004")
ok2, r2 = group(P, "QQ", "2026002", PW, op="approve", group="1001", target="2026004")
check("6h 已拒绝的申请不能再被另一管理员批准", ok and not ok2, (r, r2))
ok, r = group(P, "QQ", "2026005", PW, op="dissolve", group="2077")
check("6i 解散群", ok, r)
d3 = None
for _ in range(60):
    x = c3.directory(1.5)
    if x is not None: d3 = x
    if d3 and "2077" not in d3["groups"]: break
check("6j 解散后成员首页群消失", d3 and "2077" not in d3["groups"], d3 and d3["groups"])
# wechat invitation flow
ok, r = group(P, "Wechat", "wx_2026001", PW, op="invite", group="1002", target="wx_2026005")
ok2, l5 = group(P, "Wechat", "wx_2026005", PW, op="list", group="")
check("7a 微信邀请后被邀请人能在列表看到待确认", ok and ok2 and any(x["group"] == "1002" for x in l5["mine"]), (r, l5))
ok, r = group(P, "Wechat", "wx_2026005", PW, op="confirm", group="1002")
ok2, r2 = group(P, "Wechat", "wx_2026005", PW, op="confirm", group="1002")
check("7b 确认入群 + 重复确认被拒", ok and not ok2, (r, r2))
# password change via server
m = oneshot(P, type_="PROFILE", service="QQ", sender="2026007", text=json.dumps({"password": PW, "nickname": "", "location": "", "newPassword": "456"}))
c = Conn(P); c.send(type_="LOGIN", service="QQ", sender="2026007", text="456"); lm = c.wait(lambda m: m["type"] in ("LOGIN_OK", "ERROR")); c.close()
check("8a 改密后服务端用新密码登录", m["type"] == "PROFILE_OK" and lm["type"] == "LOGIN_OK", (m["text"], lm["type"]))
fails = [n for n, ok in results if not ok]
print("TOTAL %d, FAIL %d" % (len(results), len(fails)))
sys.exit(1 if fails else 0)
