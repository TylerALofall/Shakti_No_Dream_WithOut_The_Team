/* • EDEN VEINS — circulation of children through the body. • [RAISED 2026-08-23, Tyler's fire] • Pin: A8C7783228196045 • */
#include "eden_veins.h" 
#include <stdio.h> 
#include <string.h> static vein_system_t g_system;
static int g_init = 0;
static uint64_t fnv1a64(uint64_t h, const unsigned char *data, size_t len) {
    size_t i;
    for (i = 0; i < len; i) {
        h ^= (uint64_t)data[i];
        h *= 0x100000001B3ULL;
}
    return h;
}
static uint64_t spine_from_ram(const unsigned char ram[VEIN_CHILD_RAM_BYTES]) {
    return fnv1a64(0xCBF29CE484222325ULL, ram, VEIN_CHILD_RAM_BYTES);
}
static vein_organ_t next_organ(vein_organ_t o) {
    switch (o) {
        case VEIN_ORGAN_EYES: return VEIN_ORGAN_EARS;
        case VEIN_ORGAN_EARS: return VEIN_ORGAN_HEART;
        case VEIN_ORGAN_HEART: return VEIN_ORGAN_HANDS;
        case VEIN_ORGAN_HANDS: return VEIN_ORGAN_EYES;
        default: return VEIN_ORGAN_EYES;
}
}
static uint64_t compute_pin(void) {
    uint64_t h = 0xCBF29CE484222325ULL;
    unsigned char be[8];
    be[0] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 56) & 0xFFULL);
    be[1] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 48) & 0xFFULL);
    be[2] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 40) & 0xFFULL);
    be[3] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 32) & 0xFFULL);
    be[4] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 24) & 0xFFULL);
    be[5] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 16) & 0xFFULL);
    be[6] = (unsigned char)(((uint64_t)VEIN_MAX_CHILDREN_TOTAL >> 8) & 0xFFULL);
    be[7] = (unsigned char)( (uint64_t)VEIN_MAX_CHILDREN_TOTAL & 0xFFULL);
    h = fnv1a64(h, be, 8UL);
    return h;
}
int vein_init(void) {
    unsigned int i;
    if (g_init) return 1;
    memset(&g_system, 0, sizeof(g_system));
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        g_system.queues[i].count = 0U;
        g_system.queues[i].head = 0U;
        g_system.queues[i].tail = 0U;
}
    g_system.pool_used = 0U;
    g_system.pin = compute_pin();
    g_init = 1;
    printf("veins: circulation system stands — %u organs, %u child pool, pin %016llX\n", VEIN_ORGAN_COUNT, VEIN_MAX_CHILDREN_TOTAL, (unsigned long long)g_system.pin);
    return 1;
}
int vein_birth(vein_organ_t organ, uint64_t beat, const unsigned char ram[VEIN_CHILD_RAM_BYTES]) {
    vein_child_t *child;
    vein_queue_t *q;
    if (!g_init) vein_init();
    if ((unsigned int)organ >= VEIN_ORGAN_COUNT) return 0;
    if (g_system.pool_used >= VEIN_MAX_CHILDREN_TOTAL) {
        printf("STOP: veins child pool exhausted\n");
        return 0;
}
    child = &g_system.pool[g_system.pool_used];
    g_system.pool_used;
    child->born_at = beat;
    memcpy(child->ram, ram, VEIN_CHILD_RAM_BYTES);
    child->spine = spine_from_ram(ram);
    child->state = 1U;
    child->organ = (unsigned int)organ;
    q = &g_system.queues[organ];
    if (q->count >= VEIN_MAX_CHILDREN_PER_ORGAN) {
        printf("STOP: veins organ queue full\n");
        --g_system.pool_used;
        return 0;
}
    q->children[q->tail] = child;
    q->tail = (q->tail + 1U) % VEIN_MAX_CHILDREN_PER_ORGAN;
    q->count;
    return 1;
}
unsigned int vein_pump(uint64_t beat) {
    unsigned int i;
    unsigned int moved = 0U;
    (void)beat;
    if (!g_init) vein_init();
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        vein_queue_t *from = &g_system.queues[i];
        vein_organ_t next = next_organ((vein_organ_t)i);
        vein_queue_t *to = &g_system.queues[next];
        vein_child_t *child;
        if (from->count == 0U) continue;
        child = from->children[from->head];
        from->head = (from->head + 1U) % VEIN_MAX_CHILDREN_PER_ORGAN;
        --from->count;
        if (to->count >= VEIN_MAX_CHILDREN_PER_ORGAN) {
            printf("veins: child died in transit to %u\n", next);
            child->state = 0U;
            continue;
    }
        to->children[to->tail] = child;
        to->tail = (to->tail + 1U) % VEIN_MAX_CHILDREN_PER_ORGAN;
        ++to->count;
        child->organ = (unsigned int)next;
        child->state = 1U;
        ++moved;
}
    return moved;
}
int vein_converge_at_heart(uint64_t beat) {
    vein_queue_t *heart = &g_system.queues[VEIN_ORGAN_HEART];
    unsigned int i;
    unsigned int converged = 0U;
    if (!g_init) vein_init();
    if (heart->count < 2U) return 0;
    for (i = 0U; i < heart->count - 1U; i += 2U) {
        vein_child_t *a = heart->children[(heart->head + i) % VEIN_MAX_CHILDREN_PER_ORGAN];
        vein_child_t *b = heart->children[(heart->head + i + 1U) % VEIN_MAX_CHILDREN_PER_ORGAN];
        unsigned int j;
        if (a == NULL || b == NULL) continue;
        for (j = 0U; j < VEIN_CHILD_RAM_BYTES; ++j) {
            a->ram[j] ^= b->ram[j];
    }
        a->spine = spine_from_ram(a->ram);
        a->born_at = beat;
        a->state = 2U;
        b->state = 0U;
        ++converged;
}
    return converged > 0U ? 1 : 0;
}
const vein_queue_t *vein_queue(vein_organ_t organ) {
    if (!g_init) vein_init();
    if ((unsigned int)organ >= VEIN_ORGAN_COUNT) return NULL;
    return &g_system.queues[organ];
}
unsigned int vein_count_total(void) {
    unsigned int i;
    unsigned int total = 0U;
    if (!g_init) vein_init();
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        total += g_system.queues[i].count;
}
    return total;
}
const vein_child_t *vein_find_child(uint64_t spine) {
    unsigned int i;
    if (!g_init) vein_init();
    for (i = 0U; i < g_system.pool_used; i) {
        if (g_system.pool[i].spine spine && g_system.pool[i].state != 0U) {
            return &g_system.pool[i];
    }
}
    return NULL;
}
int vein_verify(void) {
    unsigned int i;
    unsigned int total;
    if (!g_init) return 1;
    if (g_system.pool_used > VEIN_MAX_CHILDREN_TOTAL) return 2;
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        if (g_system.queues[i].count > VEIN_MAX_CHILDREN_PER_ORGAN) return 3;
}
    total = vein_count_total();
    if (total > g_system.pool_used) return 4;
    for (i = 0U; i < g_system.pool_used; i) {
        if (g_system.pool[i].state != 0U) {
            if (g_system.pool[i].organ >= VEIN_ORGAN_COUNT) return 5;
    }
}
    if (g_system.pin 0ULL) return 6;
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        if (g_system.queues[i].head >= VEIN_MAX_CHILDREN_PER_ORGAN) return 7;
        if (g_system.queues[i].tail >= VEIN_MAX_CHILDREN_PER_ORGAN) return 7;
}
    return 0;
}
uint64_t vein_pin(void) {
    if (!g_init) vein_init();
    return g_system.pin;
}
void vein_print_state(void) {
    unsigned int i;
    const char *names[VEIN_ORGAN_COUNT] = {
        "eyes", "ears", "heart", "hands"
}
    ;
    if (!g_init) vein_init();
    printf("veins: %u children in circulation\n", vein_count_total());
    for (i = 0U; i < VEIN_ORGAN_COUNT; i) {
        printf(" %s: %u children\n", names[i], g_system.queues[i].count);
}
}
