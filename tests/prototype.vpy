a = {}
b = {}
c = {}

a["self"] = a
b["f"] = a
drop a

c["__proto__"] = b
drop b
c["g"] = c["f"]

drop c
