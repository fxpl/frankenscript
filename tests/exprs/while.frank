lst = {}
lst.next = {}
lst.next.next = {}
lst.next.next.next = {}

obj = lst
while obj != null:
    obj = obj.next

# Check identifiers in the condition
cond = False
while cond == True:
    unreachable()
while cond:
    unreachable()

# Check field access in the condition
x = {}
x.bool = False
while x.bool == True:
    unreachable()
while x.bool:
    unreachable()

# Check function calls in the condition
def bool(self):
    return False
while bool({}) == True:
    unreachable()
while bool({}):
    unreachable()

# Check method calls in the condition
x.bool = bool
while x.bool() == True:
    unreachable()
while x.bool():
    unreachable()
