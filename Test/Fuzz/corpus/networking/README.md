# Networking fuzz corpus

Keep valid packet seeds, malformed parser boundaries, and minimized historical
crashes in this directory. Textual documentation belongs here as a README;
binary or byte-oriented inputs should be added as corpus files without being
decoded or normalized by tooling.

The `networking_reassembly` corpus exercises a structured two-fragment flow.
The target derives valid fragment boundaries from each seed, then mutates
overlap bytes and metadata to reach deeper bounded-reassembly paths.
