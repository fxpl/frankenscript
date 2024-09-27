def three_args(a, b, c):
    d = a
    drop d

def no_args():
    a = "string value"
    return a


no_args()

three_args("A", "B", "C")

drop three_args
drop no_args
