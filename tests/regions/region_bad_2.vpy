# Fails: can't add anything but the bridge object to a region
r = Region()
r2 = Region()

a = {}

# Put b inside r2
b = {}
r2.b = b

# Make a reference b, which is fine while local object
a.b = b
# Fails: can't add anything but the bridge object to a region
r.a = a