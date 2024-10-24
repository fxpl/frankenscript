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
    if f.b == None:
        e = {}
        drop e

    else:
        d = {}
        drop d

cond = f.a != a
if cond:
    dummy = "created"

def check():
    c = False
    return c == True
if check():
    this = "is false"

drop a
drop f
