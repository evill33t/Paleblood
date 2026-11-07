# Shader user-data observation

The descriptor, sampler, address and word replacements report their final packets through
the canonical runtime observer. A registered replacement suppresses the corresponding
original-function observer hook by address, so each replacement owns its single record.

After reserve admission, `packet_observation.h` saves the buffer begin and packet offset.
Only after all packet stores and cursor publication does the observer read the emitted index
and payload. The final index is resolved against the existing immutable stage-register table
in the runtime's compute/PS/VS/GS/ES/HS/LS order. Exactly one window must contain the complete
one-, two-, four- or eight-word span. Unknown or ambiguous windows and overflowing spans fail
explicitly; the observer does not substitute the requested stage or slot for the final one.

Payloads come from the saved output address. Borrowed descriptors may overlap those writes,
and the final cursor publication can change payload words or their register selection. Hosts
without the canonical record return before reading begin, index or payload for observation.
The game-aware owned tests use a separate initialized stage-table mapping and never modify the
original executable's read-only table.

The admitted command-mode writer reports its final `VGT_SHADER_STAGES_EN` value through the
generic register observer. Its index must remain in the context-register window before adding
the context origin. Rejected modes and failed reserve callbacks create no record. Packet marker
words retain their original values; the observer does not infer resource meaning from them.

The VS/PS wrappers retain their existing GNM calls and do not add a second observation. The
current runtime binds effective shader register words and the VS modifier through its shader
binding/placed-call path. Device shader and register support remains a backend concern; owned
observer assertions are separate from original/replacement comparisons and device execution.
