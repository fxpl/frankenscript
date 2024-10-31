# Root a
a = {}
region(a)

# Root b
b = {}
region(b)

# Child region
c = {}
region(c)
a.child = c

# Error
b.child = c
