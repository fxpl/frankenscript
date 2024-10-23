a = {}
a.var = "hey"

temp = take a.var
a.var = take temp

a.var = take a.var
