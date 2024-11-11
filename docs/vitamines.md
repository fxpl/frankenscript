<!--
* Start from section 4
* small introduction
* Mermaid
* legend and color coding
* Draft example code with steps
* Language constructs
* Limitations
-->

# FrankenScript

<!-- FrankenScript is a monster of our creation. -->
FrankenScript a toy language intended to explore how regions can be used in dynamic programming languages to reason about memory in concurrent programs. The semantics are inspired by Python and JavaScript. The output of FrankenScript are several diagrams that show the evolution of the memory.

This document shows how FrankenScript can be used. It's assumed that the reader has a rough understanding of reference counting, regions, and cowns, as explained in the paper.

Frankenscript has a few more capabilities than described in the paper. This comes from the nature of FrankenScript as a toy language to explore the space. This introduction will focus on the features mentioned in our proposed model in the paper.

The build and run instructions can be found in the main [`README.md`](../README.md) file of the repository.

## Quick Start

Frankenscript uses a Python like syntax. Statements are terminated with new lines and instructions are collected into blocks based on their indention.

<!-- Jump to Legend -->

### A Simple Object

Every object in FrankenScript behaves like a dictionary. Fields can be read and written to, if the current context allows. Let's take a simple example:

```python
obj = {}
obj.f1 = {}
reference = obj.f1
```

The curly brackets `{}` create new empty dictionary. Line 2 populates the field `f1` with a new empty dictionary. The final line creates a reference to the dictionary in the field. The example will generate the following diagram:

<!-- Don't mention paper, make the story clear, from safe to concurrency -->

<!-- FIXME: Add background differently here, since GH doesn't like this diff -->
<div style='background: #fff'>

```mermaid
%%{init: {'theme': 'neutral', 'themeVariables': { 'fontSize': '16px' }}}%%
graph TD
  id0(None):::immutable
id1[0x706fbf22bf60<br/>rc=1]
id1--> |#95;#95;proto#95;#95;| id2[#91;Frame#93;<br/>rc=2 #40;global#41;]
  linkStyle 0 stroke:#777777,stroke-width:2px
id1--> |reference| id3[0x706fbf216710<br/>rc=2]
  linkStyle 1 stroke:#777777,stroke-width:2px
id1--> |obj| id4[0x706fbf215d60<br/>rc=1]
  linkStyle 2 stroke:#777777,stroke-width:2px
id4--> |f1| id3
  linkStyle 3 stroke:#777777,stroke-width:2px
id4
id3
id1
subgraph LocalReg["Local region"]
  id1
  id2
  id3
  id4
end
style LocalReg fill:#eefcdd
classDef unreachable stroke-width:2px,stroke:red
classDef highlight stroke-width:4px,stroke:yellow
classDef error stroke-width:4px,stroke:red
classDef tainted fill:#8e84cc
classDef immutable fill:#94f7ff
```
</div>

This example only shows the final state of the memory. FrankenScript actually generates one diagram for each step. The `--interactive` flag can be used to step through the program line by line and inspect the memory at the different stages.

### Frozen Objects

### Adding Regions

FrankenScript uses reified regions, meaning that regions are represented as actual objects which are also visible in the generated diagrams. Regions are created by the `Region()` function. They behave like dictionaries and allow assignments to their fields, with the difference that they will also take ownership of the assigned values.

```python
r1 = Region()
r2 = Region()
a = {}

# r1 takes ownership of a
r1.a = a

# r1 takes ownership of r2
r1.r2 = r2
```
<!-- Maybe put the diagram and code side by side? -->

The styling of the arrows indicate what kind of connection the reference is. Borrowed references, i.e. references coming from the local region, are indicated by dotted lines. Connections crossing from one region into another region are highlighted in orange, while region internal connections have a default dark gray color.

Region objects use a trapezoid shape and display the number of incoming borrowed references in the *local reference count (LRC)* and the number of *open subregions (SBRC)*. A region is closed if the `LRC + SBRC` is `0` otherwise it's considered open.


### Arguing With Cowns

Concurrent Owners (Cowns) are special objects in FrankenScript used to enforce ownership and argue about memory safety. While FrankenScript doesn't support concurrency, it allows the simulation of blocking access to cowns and the highlight of the mutable state. This allows reasoning about cowns and the safety of our ownership model.



### Legend

* Colors
* Examples
* Crying

## Advanced Usage

### More Expressions

### Styling the diagram

### Methods

<!--
> P ::= b
> b ::= s | ss
> s ::= while c: b
-->

## Generating Diagrams

FrankenScript

## Built-in functions
