a = {}
b = {}
c = {}

a["self"] = a
b["f"] = a
drop a

c = create b
drop b
c["g"] = c["f"]