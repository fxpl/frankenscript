# Build three regions
a = {}
a["self"] = a
b = {}
a["b"] = b
region a 

c = {}
c["self"] = c
d = {}
c["d"] = d
region c


e = {}
e["self"] = e
f = {}
e["f"] = f
region e 


# connect first region to second in two ways
a["c"] = c
b["d"] = d
# Connect first region to third with single entry point
b["e"] = f

# Now freeze part of the first region, and the reachable parts of the 
# second and third regions
freeze b


# Drop all the references
drop a
drop d
drop f
drop b 
drop c
drop e
