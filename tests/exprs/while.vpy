lst = {}
lst.next = {}
lst.next.next = {}
lst.next.next.next = {}

obj = lst
while obj != null:
    obj = obj.next

# Identifiers
cond = False
while cond == True:
    never = "mind"
while cond:
    never = "mind"

# Fields
x = {}
x.bool = False
while x.bool == True:
    never = "mind"
while x.bool:
    never = "mind"

# Function
def bool(self):
    return False
while bool({}) == True:
    never = "mind"
while bool({}):
    never = "mind"

# Method condition
x.bool = bool
while x.bool() == True:
    never = "mind"
while x.bool():
    never = "mind"
