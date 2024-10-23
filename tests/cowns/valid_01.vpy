global = {}

# A simple cown
a = {}
a.b = {}
region a
c01 = cown(a)

# Store the cown in a global
global.cown = c01

# Freeze global with a cown
freeze global

drop c01
drop global
