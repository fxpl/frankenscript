def no_return():
    x = "text"
    drop x

def no_args():
    a = "string value"
    return a

def three_args(a, b, c):
    d = a
    drop d
    return

def b_literal():
    return "B"


# Store the return value
value = no_args()
drop value

# Ignore the return value
no_args()

# The function has no return
no_return()

# Function call in function arguments
three_args("A", b_literal(), "C")
