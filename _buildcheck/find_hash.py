import hashlib
target = "33bcff0bdb2ff633ced1972a6b3b4790f26e2abe0f5bf1a7fbf02888a944da13"
phone = "13800000000"
candidates = [
    ("无盐", phone),
    ("neusoft+phone", "neusoft" + phone),
    ("phone+neusoft", phone + "neusoft"),
    ("neusoft-charging+phone", "neusoft-charging" + phone),
    ("phone+neusoft-charging", phone + "neusoft-charging"),
    ("neusoft-charging-platform-2026+phone", "neusoft-charging-platform-2026" + phone),
    ("phone+neusoft-charging-platform-2026", phone + "neusoft-charging-platform-2026"),
    ("charging+phone", "charging" + phone),
    ("phone+charging", phone + "charging"),
    ("2026+phone", "2026" + phone),
    ("phone+2026", phone + "2026"),
    ("neusoft2026+phone", "neusoft2026" + phone),
    ("phone+neusoft2026", phone + "neusoft2026"),
]
print(f"目标哈希: {target}")
print(f"手机号: {phone}")
print()
for name, data in candidates:
    h = hashlib.sha256(data.encode()).hexdigest()
    match = "  <== 匹配!" if h == target else ""
    print(f"  {name:45s}: {h[:20]}...{match}")
# 也试双重哈希
print()
h1 = hashlib.sha256(phone.encode()).hexdigest()
h2 = hashlib.sha256(h1.encode()).hexdigest()
print(f"  双重SHA256(无盐):                            {h2[:20]}... {'<== 匹配!' if h2==target else ''}")
