
# Construct a
a = {}
a.set = {}
a.set["1"] = "A"
a.set["2"] = "B"
a.set["3"] = "C"

# `entry` will be dropped at the end of the loop.
for key, value in a.set:
    l = value
    drop l

drop a
