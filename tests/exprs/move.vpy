a = {}
a.var = "hey"

temp = move a.var
a.var = move temp

a.var = move a.var
