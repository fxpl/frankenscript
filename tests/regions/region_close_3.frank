# Construct regions
r1 = Region()
r2 = Region()
r1.r2 = r2

# Fill region
r2.a = {}
r2.b = {}
r2.b.c = {}

# Make sure nodes are inter-connected to mark the LRC as dirty
r2.a.c = r2.b.c

# Create local references
lref1 = r2.b
lref2 = r2.b.c

# r2.LRC is now 3, from: r2, lref1, lref2

# Freeze items to mark the LRC as dirty
freeze(r2.b)
r2 = None

# Succeed `try_close()` after LRC cleaning
if try_close(r1.r2):
    pass()
else:
    unreachable()

# Force close
close(r1.r2)
