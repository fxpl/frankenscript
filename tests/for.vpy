
# Construct a
a = {}
a.set = {}
a.set["1"] = "1"
a.set["2"] = "2"
a.set["3"] = "3"

# `entry` will be dropped at the end of the loop.
for entry in a.set:
    l = entry.value
    drop l

drop a
