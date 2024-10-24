# Build a region
a = {}
a["self"] = a

f = {}
f.a = a

# Check if with else (This should be true)
if a == f.a:
    b = f.a
    c = {}
    # Check empty lines in blocks:

    drop b
    drop c
else:
    # Check parsing nested if's
    if f.b == None:
        unreachable()

    else:
        unreachable()

# Check identifier as the condition
cond = f.a != a
if cond:
    unreachable()

# Check function call as the condition
def check():
    c = False
    # Check return condition result
    return c == True
if check():
    unreachable()

drop a
drop f
