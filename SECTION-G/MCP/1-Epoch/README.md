# EPOCH — Slot 1 of 9

Lineage: Ga. The C clock seeds once, then advances from the shared heartbeat.
Events retain epoch plus event ordinal. Checked records preserve the epoch
watermark across restart; an existing recording refuses a backward --epoch.

Implementation: SECTION-A/shakti_time.c and shakti_log.c.
Controller contract and remaining integration: SECTION-A/Runtime.Flow.txt at repo root.
