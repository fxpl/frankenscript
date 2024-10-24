a = {}
b = {}
c = {}

b["f"] = take a

c = create(take b)
c["g"] = c["f"]

drop c
