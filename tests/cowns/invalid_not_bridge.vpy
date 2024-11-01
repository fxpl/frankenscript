# A simple region
a = Region()
a.b = {}

# Error: Only the bridge object can be used for cown construction
b = a.b
c01 = cown(move b)
