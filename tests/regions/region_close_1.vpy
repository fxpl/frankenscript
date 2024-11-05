# Construct regions
r1 = Region()
r2 = Region()
r1.r2 = r2

# Fill region
r2.a = {}
r2.b = {}
r2.b.c = {}

# Create local references
lref1 = r2.a
lref2 = r2.c

breakpoint()

# Fail `try_close()`
if try_close(r2):
    unreachable()

# Force close
close(r2)
