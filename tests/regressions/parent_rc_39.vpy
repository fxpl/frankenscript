# This test checks for the regression introduced in #39
# and fixed in #43.
# The issue was two fold:
#  a. The region was not deallocated when the parent pointer was removed
#  b. The LRC was not correctly updated when a removed subregion had a non-zero
#     LRC.
#
# There is no current way to test for (b).  So we just test for (a).

# Create a region with a self reference
a = Region()
a.self = a

# Create a subregion with a self reference
b = Region()
b.self = b

# Nest the regions
a.b = b

# Drop handle on nested region
drop b

# Drop nesting.
a.b = None

# Tidy frame
drop a
