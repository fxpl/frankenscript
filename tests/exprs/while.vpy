lst = {}
lst.next = {}
lst.next.next = {}
lst.next.next.next = {}

obj = lst
while obj != null:
    obj = obj.next

# Always false
x = {}
x.bool = False
while x.bool == True:
    never = "mind"

# Function
def bool(self):
    return False
while bool({}) == True:
    never = "mind"

# Method condition
x.bool = bool
while x.bool() == True:
    never = "mind"
