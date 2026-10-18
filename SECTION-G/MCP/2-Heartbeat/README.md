# HEARTBEAT — Slot 2 of 9

Lineage: Gb. Resting pace is one beat per second. A beat carries 32 sight slots
and 64 hearing slots. /heartbeat/ selects 1, 2, 4, 8, 16, 32 or 64 beats/second;
all channels speed together and retain the 2:1 relationship.

The loop continues without input or a reply. Tool interruption holds tools.
The native API publishes PCM pulse frames; speaker playback requires the Swift
adapter. Slot counters alone do not prove real playback or a device frame rate.

Implementation: SECTION-A/shakti_loop.c and the nonblocking host in main.c.
Controller contract and remaining integration: SECTION-A/Runtime.Flow.txt at repo root.
