# Root a
a = Region()

# Root b
b = Region()

# Child region
c = Region()
a.child = c

# Error
b.child = c
