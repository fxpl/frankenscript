# Build three regions
a = Region()
a["self"] = a
b = {}
a["b"] = b
mermaid_show_tainted(a)

c = Region()
c["self"] = c
c["d"] = {}


e = Region()
e["self"] = e
e["f"] = {}

mermaid_taint(a)

# connect first region
a.c = c

# Connect first region to third with single entry point
a.b.e = e.f

mermaid_untaint(a)

# Now freeze part of the first region, and the reachable parts of the 
# second and third regions
freeze(b)

# Drop all the references
drop a
drop b
drop c
drop e
