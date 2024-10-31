# For now separate
def insert(node, item):
    if node.next == None:
        node.next = {}
        node.next.data = item
        node.next.prev = node
    else:
        insert(node.next, item)

def contains(node, item):
    if node.data == item:
        return True
    if node.next == None:
        return False
    return find(node.next, item)

def remove(node, item):
    if node.data == item:
        node.prev.next = node.next
        if node.next != None:
            node.next.prev = node.prev
        return True
    if node.next == None:
        return False
    return remove(node.next, item)

def new_list():
    list = {}
    list.head = {}
    return list

list = new_list()

# Required to not leak memory
r = Region()
r.list = list

value = "x"

# Dyrona doesn't freeze shared objects automatically (yet)
proto = value["__proto__"]
freeze(proto)
drop proto

insert(list.head, {})
insert(list.head, {})
insert(list.head, value)
insert(list.head, {})
remove(list.head, value)

drop value
drop list
