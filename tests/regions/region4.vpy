# Create multiple regions
r1 = Region()
r1.level = "One"

r2 = Region()
r2.level = "Two"

r3 = Region()
r3.level = "Three"

r4 = Region()
r4.level = "Four"

r5 = Region()
r5.level = "Five"

# Stack regions
r1.n = move r2
r1.n.n = r3

r4.n = move r5
r3.n = move r4
drop r3

# Move this into a cown
c = cown(move r1)

# Check the visuals
mermaid_show_cown_region()
mermaid_show_immutable_region()
