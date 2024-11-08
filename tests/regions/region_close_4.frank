# Create nested region
r0 = Region()
r0.r1 = Region()
r0.r1.r2 = Region()
r0.r1.r2.r3 = Region()
r0.r1.r2.r3.a = {}

# Create local ref to child region
lref = r0.r1.r2.r3.a

# Fail try close, since r3 is open
if try_close(r0.r1):
    unreachable()

# Force close
close(r0.r1)

# Check lref was invalidated
if lref != None:
    unreachable()
