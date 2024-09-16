
# Build a region
a = {}
a["self"] = a

f = {}
f.a = a

if a == f.a:
    b = f.a
    c = {}

    drop b
    drop c
else:
    if f.a == False:
        e = {}
        drop e

    else:
        d = {}
        drop d

drop a
drop f
