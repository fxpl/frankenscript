global = {}

# A simple cown
a = {}
a.b = {}
region a
c01 = cown a

# Store the cown in a global
global.cown = c01
taint global

# Freeze global with a cown
freeze global

# FIXME: traint with cowns is wrong
# FIXME: Show local region address
# FIXME: Cowns should have special rendering and out edges

drop c01
drop global
