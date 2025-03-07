# Built-in Functions

## Debugging

#### `breakpoint()`

Causes the interpreter to enter *interactive mode*.

#### `unreachable()`

Aborts the interpreter process. It's intended to indicate that an execution branch is not reachable.

#### `pass()`

Does nothing it can be used to fill empty blocks

## Constructors

#### `Region()`

Creates a new region object.

#### `Cown(move region)`

Creates a new `Cown` object.

The region must have a local reference count of one. The `move` keyword is used to replace the local value with `None`.

#### `create(proto)`

Creates a new object from the given prototype.

## Memory Management

#### `freeze(obj)`

Performs a deep freeze of the object and all referenced objects.

This will move the objects out of their current region into the immutable region. Cowns will stop the freeze propagation, as they can be safely shared across threads and behaviors.

#### `close(reg)`

Forces the given region to close by invalidating all local references into to region and its subregions.

This function will also correct all dirty LRCs.

#### `is_closed(reg)`

Checks if the given region can be closed. If the LRC is dirty or subreagions are open, it will correct all dirty LRCs.

Returns `True`, if the region is closed in the end, `False` otherwise.

#### `merge(source, sink)`

This merges the `source` region into the `sink` region. The bridge object of `source` looses its prototype
since it's no longer the bridge object of the region.

#### `dissolve(region)`

Dissolves the given region into the local region. The bridge object will lose the `RegionObject` prototype.

## Pragmas

#### `pragma_enable_implicit_freezing()`

Enables implicit freezing for the rest of the program.

#### `pragma_mermaid_draw_regions_nested(bool)`

Indicates if nested regions should be drawn as nested or floating boxes in the diagram.

## Mermaid

#### `mermaid_hide(obj, ..)`

Hides the given arguments from the mermaid graph. 

#### `mermaid_show(obj, ..)`

Shows the given arguments in the mermaid diagram. 

#### `mermaid_show_all()`

Makes all nodes visible in the mermaid diagram.

#### `mermaid_show_tainted(obj, ...)`

Draws a mermaid diagram with the given objects marked as tainted. This will show which objects are reachable at this point.

#### `mermaid_taint(obj, ...)`

Marks the given objects as tainted, this will highlight, which nodes are reachable from the given objects.

The tainted status will remain until `mermaid_untaint` is called.

`mermaid_show_tainted()` can be used to only taint the current snapshot.

#### `mermaid_untaint(obj, ...)`

Marks the given objects as untainted, thereby removing the highlights from the mermaid diagram.

#### `mermaid_show_cown_region()`

This explicitly shows the "Cown region" in the generated diagrams.

#### `mermaid_hide_cown_region()`

This hides the "Cown region" and cown prototype in diagram. (Default)

#### `mermaid_show_immutable_region()`

This explicitly shows the "Immutable region" in the generated diagrams

#### `mermaid_hide_immutable_region()`

This hides the "Immutable region" in diagram. (Default)

#### `mermaid_show_functions()`

Shows user defined functions in the diagram.

#### `mermaid_hide_functions()`

Hides user defined functions in the diagram. (Default)
