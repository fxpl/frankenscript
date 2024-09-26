a = {}
b = {}
c = {}

b["f"] = a
drop a

c = create b
drop b
c["g"] = c["f"]

drop c