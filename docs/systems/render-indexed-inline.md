# Inline index data

`render_draw_indexed_inline` (`0x014740a0`, 271 bytes) emits `DRAW_INDEX_2` with an address
inside its command allocation, followed by the trace/NOP pair and an inline-data NOP. Its
arguments are a `PacketBuffer *`, an independent `uint32_t` index count, a source pointer and
a `uint32_t` byte count. A zero index count still invokes the copy helper.

The packet names and count fields follow AMD's
[Linux `cikd.h`, commit `adc218676eef25575469234709c2d87185ca223a`](https://github.com/torvalds/linux/blob/adc218676eef25575469234709c2d87185ca223a/drivers/gpu/drm/amd/amdgpu/cikd.h).
The executable supplies the arithmetic, store order and fixed NOP payload `0x68752000`;
the payload's meaning is not established.

## Allocation and publication

Rounding adds three to the byte count at 32 bits before dividing by four. The reservation
requests that many words plus ten: six draw words, two trace/NOP words and two inline NOP
words. After a successful reservation, the writer saves the data destination at the admitted
cursor plus 40 bytes. A callback may redirect command storage; its low result byte admits the
new cursor without another capacity check. Capacity compares the low DWORD of available words.

The draw stores the independent index count in MAX_SIZE and INDEX_COUNT. Its address clears
bit zero of the saved data destination and keeps the complete high DWORD. Initial draw
publication adds 24 to the live cursor field, including any aliasing packet stores.

The original user-PA query's low byte chooses the next pair. A nonzero byte reloads flags and
cursor, writes EVENT_WRITE/THREAD_TRACE_MARKER, then reads the cursor again after those stores.
The zero route captures its tail cursor before writing a NOP and retains that captured value.
The common inline NOP starts eight bytes after the chosen tail. Its count field contains the
rounded inline word count and its first payload word is the fixed tag above.

The final cursor is published before clearing or copying. A nonzero index count clears the
DWORD immediately before the final cursor; a zero count skips that clear. The writer then
calls the shared `copy_bytes` import with the saved destination, original source pointer and
the zero-extended byte count, even when either count is zero. Copy length, clear gate and
index count remain independent. The shared import returns `void *`; the drawing operation
does not expose that residual helper result as its logical return.

## Return and evidence limits

The complete bounded decode agrees with the unwind extent. The retained direct caller at
`0x015fbb90` does not consume the residual copy result and forwards through `0x015f9d10` to
`0x01478220`. The complete final 72-byte target overwrites the incoming result before a
meaningful use. Its initial PUSH only fills an allocation slot: that slot is neither read nor
passed by address before being discarded. This known call chain supports a logical `void`
replacement. Other callers, indirect calls and tail-call entry paths were not exhaustively
examined. This is bounded ELF/unwind/Capstone evidence, not REA evidence.

## Renderer observation and synthetic cases

The canonical bridge prerequisite supplies the existing raw indexed-draw event. The writer
captures begin and byte offset after reservation but before stores. After final publication,
clearing and copying, it observes the final draw header, MAX_SIZE, both address words,
INDEX_COUNT and DRAW_INITIATOR. It never reloads source data to reconstruct an event and never
masks the recorded address a second time. An absent host skips observer-only reads.

The 83 synthetic cases vary zero and independent counts, odd byte lengths, 32-bit rounding
wrap, callback/query low bytes, DWORD capacity, source aliases and publication aliases. Large
opaque-copy cases check the caller's arithmetic and helper arguments; the scripted helper
does not perform huge copies. Owned-storage tests provide real bounded copies and query
changes without mapping or executing the original image. An owned mapping above 40 bits
distinguishes the high address DWORD; the differential harness's fixed arena is below that
boundary, so that particular compiled control is reported only as an owned supplement.

Original/replacement comparisons, native compiled-control rejection and genuine recordings
remain separate evidence requirements. Host checks alone do not establish edge verification.
