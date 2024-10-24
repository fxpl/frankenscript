a = {}
b = {}
a["f"] = b
c = a.f
region(a)

drop b
drop c
drop a
