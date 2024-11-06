# Construct regions
r1 = Region()
r1.r2 = Region()

# Fill region
r1.r2.a = {}
r1.r2.b = {}

# r2.LRC is now 0
# r2.LRC will be 1, from the value on the stack

# Succeed `try_close()`
if try_close(r1.r2):
    pass()
else:
    unreachable()

# Succeed `close()`
close(r1.r2)
