global = {}

# A simple cown
a = Region()
a.b = {}


c01 = cown(move a)

# Store the cown in a global
global.cown = move c01

mermaid_show_tainted(global)

# Freeze global with a cown
freeze(global)

drop global
