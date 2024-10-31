global = {}

x = "x"
freeze_proto("string")
mermaid_hide(x["__proto__"])
drop x

def add_cown(obj, name):
    a = Region()
    a.name = name
    obj[name] = cown(move a)

add_cown(global, "c1")
add_cown(global, "c2")
add_cown(global, "c3")
add_cown(global, "c4")

mermaid_show_cown_region()
mermaid_hide_cown_region()

drop global
