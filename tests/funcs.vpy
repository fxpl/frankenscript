def three_args(a, b, c):
    d = a
    drop d

def no_args():
    a = "string value"
    return a

def b_literal():
    return "B"

# Store the return value
value = no_args()
drop value

# Ignore the return value
no_args()

three_args("A", b_literal(), "C")

a = {}
drop a
