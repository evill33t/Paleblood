# Index state before indexed draws

The GPU library sets three pieces of state independently before an indexed draw:

| Replacement | Original address | Input | Emitted packet |
|---|---|---|---|
| `render_set_index_type` | `0x01475270` | raw `uint32_t` | `INDEX_TYPE`, two words |
| `render_set_index_base` | `0x014752c0` | address `uint64_t` | `INDEX_BASE`, three words |
| `render_set_index_buffer_size` | `0x01475320` | raw `uint32_t` | `INDEX_BUFFER_SIZE`, two words |

The type and size words are copied without filtering. The base packet clears address bit zero
and keeps address bits 1 through 39. This is the setter's behavior; the separate direct-draw
packet has its own address fields. A caller must not assume those two operations share an
address mask.

Packet names and type-3 opcode/count fields follow the AMD-authored
[Linux `cikd.h`, pinned to commit `adc218676eef25575469234709c2d87185ca223a`](https://github.com/torvalds/linux/blob/adc218676eef25575469234709c2d87185ca223a/drivers/gpu/drm/amd/amdgpu/cikd.h).
The setters' packing, capacity checks, callbacks and publication order come from the original
executable, rather than from the public header. Their complete bounded extents agree with their
unwind entries: 76, 92 and 76 bytes respectively. Direct caller evidence confirms the type
word and base address widths. No direct call to the size setter was found in the bounded
rel32-call candidate search; indirect and tail-call use remains unexamined.

## Command storage and callbacks

The writers reuse `render::PacketBuffer`, the byte-copy `Field` template and `reserve_words`.
They subtract limit minus cursor with wrapping address arithmetic, divide by four, then compare
the low 32-bit word count. A shortage requests two, three or two words respectively. Only the
callback's low result byte decides whether to proceed. An accepted callback supplies the cursor
used for packet stores without a second capacity check. Final publication adds the packet size
to the live cursor, which can itself overlap the output packet.

The replacements return `void`: no deliberate scalar result is produced, and the available
caller continuations do not consume a result before it is overwritten. The size setter has the
caller-coverage gap noted above. Synthetic callback traces observe the buffer pointer and opaque argument at full width and requested words
as `uint32_t`; unused upper bits are excluded explicitly with `argi` and `argu32` selectors.

## Optional renderer observation

The canonical runtime bridge is available when the host exports its recorder provider. Immediately
after successful reservation, the writer saves the command buffer's begin address and the
admitted cursor's byte offset before packet stores can overwrite either field. After all packet
stores and cursor publication, it reads the final payload from the saved output address. The
initial INDEX_BASE stores apply the 40-bit alignment mask above; observation preserves the final
raw words, including any later alias overwrite. An available host rejects offsets before begin
or beyond `UINT32_MAX`, which the recording interface cannot represent.

A declined reservation records no event. An absent host returns before reading begin or any
observer-only payload. The game library resolves the canonical `rt_observe_*` functions through
its runtime dependency; that bridge looks up host recorder providers once. It forwards errors
through the existing refusal path. The boot executable's selective recorder exports and static
record retention are supplied by the focused observer foundation prerequisite.

Owned-storage tests exercise the actual loaded writers with present and absent host providers,
callback redirects, output/prefix aliases, inaccessible begin storage, and invalid offsets.
Generated fixtures are synthetic edge cases, not recordings from a game run. Native
original/replacement comparisons and compiled mutation rejection remain separate evidence
requirements.
