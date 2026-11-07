# Indexed draw packets

The GPU library offers a draw with its own index address and a draw relative to a separately set
`INDEX_BASE`:

| Replacement | Original address and extent | Arguments after the command buffer |
|---|---|---|
| `render_draw_indexed` | `0x01474360`, 189 bytes | `uint32_t count`, `uint64_t address` |
| `render_draw_indexed_offset` | `0x01474420`, 176 bytes | `uint32_t offset`, `uint32_t count` |

Both extents match complete bounded decodes and their unwind entries. Validated direct callers
corroborate the logical argument widths and the offset-before-count order. This is independent
ELF/unwind/Capstone analysis of the owned original; it is not REA evidence.

Public packet names and opcode/count fields follow AMD's
[Linux `cikd.h`, commit `adc218676eef25575469234709c2d87185ca223a`](https://github.com/torvalds/linux/blob/adc218676eef25575469234709c2d87185ca223a/drivers/gpu/drm/amd/amdgpu/cikd.h).
Original packing and alias behavior come from the bounded executable analysis.

## Payload and publication

`DRAW_INDEX_2` writes a header, MAX_SIZE, BASE_LO, BASE_HI, INDEX_COUNT and DRAW_INITIATOR.
Both count fields initially receive the supplied count. BASE_LO clears input address bit zero;
BASE_HI keeps the full upper DWORD. This differs from the separate INDEX_BASE setter, which
keeps only the low 40 address bits. DRAW_INITIATOR initially selects DMA indices with zero.

`DRAW_INDEX_OFFSET_2` writes a header, MAX_SIZE, INDEX_OFFSET, INDEX_COUNT and DRAW_INITIATOR.
The offset remains independent from the count. The command prefix's predicate bit enters the
header for both forms.

Reservation requests eight words for an addressed draw and seven for an offset draw, including
the trailing packet. The canonical reservation helper compares the low DWORD of available
words and admits a callback's nonzero low byte. An accepted callback can replace storage and
flags without another capacity check.

After the first draw packet, both functions add to the live cursor field: 24 bytes or 20 bytes.
They call the original user-PA wrapper and test its low byte. The true route reloads flags and
cursor, writes EVENT_WRITE with THREAD_TRACE_MARKER, then adds eight to the live cursor. The
false route writes a two-word NOP and publishes the captured tail pointer plus eight instead.
These distinctions matter when a packet overlaps the prefix.

## Renderer observation

The shared optional bridge snapshots begin and the admitted byte offset after reservation,
before any packet stores. After the draw, query, tail and final cursor publication, it reads all
final draw fields from that saved output address. Events retain the raw header, MAX_SIZE,
full reconstructed address or index offset, INDEX_COUNT and DRAW_INITIATOR. Reconstructing the
address does not apply a second mask: a later alias can alter an initially cleared bit.

The backend must reject predicated or unsupported packet fields explicitly. Recording a packet
does not infer an index width, read guest indices or execute an indexed draw. The Vulkan executor
currently rejects indexed draw events by name. The renderer's index type, base and buffer size
are separate state events.

Synthetic native fixtures include independent flag bits and operands, callback widths and
redirects, DWORD-capacity truncation and safe aliases for first and tail publications. Query
scripts have no fabricated pointer arguments. Owned host tests separately bind the query to an
owned function and change flags, cursor or draw payload through test-owned storage. Those
checks establish the observer boundary and postquery reloads; they are not original/replacement
comparisons. Compiled wrong versions and restored correct checks retain that distinction.
