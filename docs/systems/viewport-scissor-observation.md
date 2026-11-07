# Viewport and screen-scissor observation

The viewport writer emits depth and transform packets, and the screen-scissor writer emits
packed `PA_SC_SCREEN_SCISSOR_TL`/`BR` words. Their renderer records use the canonical observer
bridge, after reserve admission and all output stores, including cursor publication. The shared
`packet_observation.h` saves begin/offset before output can overwrite the command prefix.

Viewport values come from the final emitted depth and transform packets. Borrowed scale and
offset inputs may overlap output, and publishing the cursor can change a depth word. The final
packet headers and register indices must select one matching supported viewport; corrupt or
inconsistent packet shapes fail explicitly instead of producing an invented viewport record.
The observer copies float bits, preserving NaN payloads and signed zero.

The scissor observer emits two generic register records from the final packet index and packed
words. It does not reinterpret coordinates. Normally those are the screen-scissor registers;
an alias that changes the index retains its actual context-register selection. Unsupported
headers and spans extending outside the context window fail explicitly.

Declined callbacks produce no record. Hosts without the canonical renderer record gain no
observer-only begin or packet reads. Owned tests use the real canonical bridge and recorder,
including guard pages and publication aliases; they are separate from original/replacement
comparisons and GPU execution.
