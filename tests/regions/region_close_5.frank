# Helper to close the region while a ref is on the stack
def close_and_get(r1):
    # Check that the region is currently open
    if try_close(r1.r2):
        unreachable()
    
    # Force close it
    close(r1.r2)

    # r1 has to be returned, since r1.r2 would open the region again
    return r1

def check_stack_invalidation(lref, reg):
    # Check the stack value was invalidated
    if lref != None:
        unreachable()

# Construct regions
r1 = Region()
r1.r2 = Region()
r1.r2.a = {}

check_stack_invalidation(r1.r2.a, close_and_get(r1))
