# Build three regions
a = {}
a["self"] = a
b = {}
a["b"] = b
region(a)


c = {}
c["self"] = c
c["d"] = {}
region(c)


e = {}
e["self"] = e
e["f"] = {}
region(e)


# connect first region to second in two ways
a.c = c
a.b.d = c.d
# Connect first region to third with single entry point
a.b.e = e.f

# Now freeze part of the first region, and the reachable parts of the 
# second and third regions
freeze(b)


# Drop all the references
drop a
drop b
drop c
drop e
