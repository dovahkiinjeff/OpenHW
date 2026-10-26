#ifndef OPENHW_SHIP_MOTION_IDENTITY_H
#define OPENHW_SHIP_MOTION_IDENTITY_H

#include <stdint.h>
#include <string.h>

/* Renderer-only lifetime state. Nothing here is part of Ship, Asteroid, Derelict or a
   save file. Multiple live objects can have the same saved/network number.
   Key by BOTH owner and number so one draw cannot evict a different live
   object's identity. Explicit creation still distinguishes address reuse. */
typedef struct ShipMotionIdentitySlot
{
    const void *owner;
    uint64_t token;
    unsigned int number;
} ShipMotionIdentitySlot;

typedef struct ShipMotionIdentityRegistry
{
    ShipMotionIdentitySlot slots[65536];
    ShipMotionIdentitySlot asteroidSlots[65536];
    ShipMotionIdentitySlot derelictSlots[65536];
    uint64_t sequence;
} ShipMotionIdentityRegistry;

static ShipMotionIdentitySlot *shipMotionIdentityFindSlot(
    ShipMotionIdentitySlot *slots, const void *owner, unsigned int number, int insert)
{
    uintptr_t value;
    unsigned int index, probe;
    ShipMotionIdentitySlot *retired = NULL;
    if (owner == NULL || number >= 65536u) return NULL;
    value = ((uintptr_t)owner >> 4) ^ ((uintptr_t)number * (uintptr_t)2654435761u);
    value ^= value >> 16;
    index = (unsigned int)value & 65535u;
    for (probe = 0; probe < 65536u; ++probe)
    {
        ShipMotionIdentitySlot *slot = &slots[(index + probe) & 65535u];
        if (slot->owner == owner && slot->number == number) return slot;
        if (slot->owner == NULL)
        {
            /* NULL + nonzero token is a tombstone. Preserve its probe chain. */
            if (slot->token == 0) return insert ? (retired != NULL ? retired : slot) : NULL;
            if (retired == NULL) retired = slot;
        }
    }
    return insert ? retired : NULL;
}

static uint64_t shipMotionIdentityRegisterSlot(ShipMotionIdentityRegistry *registry,
                                              ShipMotionIdentitySlot *slot,
                                              const void *owner, unsigned int number)
{
    if (slot == NULL) return 0; /* Bounded table exhaustion never aliases. */
    slot->owner = owner;
    slot->number = number;
    /* Exhaustion must disable history, never alias an earlier lifetime. */
    slot->token = registry->sequence == UINT64_MAX ? 0 : ++registry->sequence;
    return slot->token;
}

/* Explicit creation/loading always starts a new lifetime, even if an allocator
   and a 16-bit network ID both happen to reuse the previous values. */
static uint64_t shipMotionIdentityRegister(ShipMotionIdentityRegistry *registry,
                                          const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return 0;
    slot = shipMotionIdentityFindSlot(registry->slots, owner, number, 1);
    return shipMotionIdentityRegisterSlot(registry, slot, owner, number);
}

static uint64_t shipMotionIdentityAcquire(ShipMotionIdentityRegistry *registry,
                                         const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return 0;
    slot = shipMotionIdentityFindSlot(registry->slots, owner, number, 0);
    return slot != NULL ? slot->token : shipMotionIdentityRegister(registry, owner, number);
}

static void shipMotionIdentityRetire(ShipMotionIdentityRegistry *registry,
                                     const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return;
    slot = shipMotionIdentityFindSlot(registry->slots, owner, number, 0);
    /* A stale retirement must not erase a replacement using the same ID. */
    if (slot != NULL)
    {
        slot->owner = NULL;
        slot->token = 1; /* Tombstone, not a reusable lifetime token. */
    }
}

static void shipMotionIdentityClear(ShipMotionIdentityRegistry *registry)
{
    memset(registry->slots, 0, sizeof(registry->slots));
    memset(registry->asteroidSlots, 0, sizeof(registry->asteroidSlots));
    memset(registry->derelictSlots, 0, sizeof(registry->derelictSlots));
    /* Preserve sequence across renderer/level resets. */
}

/* Resource IDs share the ship ID range, so asteroids must never use ship slots.
   Allocating from the same sequence also keeps exact owner keys disjoint. */
static uint64_t asteroidMotionIdentityRegister(ShipMotionIdentityRegistry *registry,
                                              const void *owner, unsigned int number)
{
    if (owner == NULL || number >= 65536u) return 0;
    return shipMotionIdentityRegisterSlot(registry,
        shipMotionIdentityFindSlot(registry->asteroidSlots, owner, number, 1), owner, number);
}

static uint64_t asteroidMotionIdentityAcquire(ShipMotionIdentityRegistry *registry,
                                             const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return 0;
    slot = shipMotionIdentityFindSlot(registry->asteroidSlots, owner, number, 0);
    return slot != NULL ? slot->token : asteroidMotionIdentityRegister(registry, owner, number);
}

static void asteroidMotionIdentityRetire(ShipMotionIdentityRegistry *registry,
                                        const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return;
    slot = shipMotionIdentityFindSlot(registry->asteroidSlots, owner, number, 0);
    if (slot != NULL)
    {
        slot->owner = NULL;
        slot->token = 1;
    }
}

/* Derelict numbers overlap both fleet and resource numbers. A graveyard full
   of repeated HODs must match exact lifetimes, never nearest mesh instances. */
static uint64_t derelictMotionIdentityRegister(ShipMotionIdentityRegistry *registry,
                                              const void *owner, unsigned int number)
{
    if (owner == NULL || number >= 65536u) return 0;
    return shipMotionIdentityRegisterSlot(registry,
        shipMotionIdentityFindSlot(registry->derelictSlots, owner, number, 1), owner, number);
}

static uint64_t derelictMotionIdentityAcquire(ShipMotionIdentityRegistry *registry,
                                             const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return 0;
    slot = shipMotionIdentityFindSlot(registry->derelictSlots, owner, number, 0);
    return slot != NULL ? slot->token : derelictMotionIdentityRegister(registry, owner, number);
}

static void derelictMotionIdentityRetire(ShipMotionIdentityRegistry *registry,
                                        const void *owner, unsigned int number)
{
    ShipMotionIdentitySlot *slot;
    if (owner == NULL || number >= 65536u) return;
    slot = shipMotionIdentityFindSlot(registry->derelictSlots, owner, number, 0);
    if (slot != NULL)
    {
        slot->owner = NULL;
        slot->token = 1;
    }
}

#endif
