# Kingdoms Ascendant — Master Code Audit (Round 2)

**Scope**: Every file actually produced in this conversation, checked against what is really on disk right now, not against the summary of what was *said* to have been built.
**Method**: Read every existing source file in full, traced execution paths (client vs. server, tick vs. RPC, replication), cross-checked every symbol referenced by one file against the file that's supposed to define it.
**Result**: 1 systemic architectural bug (found in two independent places), 3 file-level compile/integration errors, several correctness and performance issues, and a significant gap between "described as built" and "actually exists as a file."

This document supersedes `CODE_DESIGN_REVIEW.md` and `ALL_FIXES_APPLIED.md` — those covered the housing/camera fixes in isolation; this pass checks them against the actual `MMOCharacter` code and against each other, which surfaced bugs neither previous pass could see.

---

## 0. FILE INVENTORY — WHAT ACTUALLY EXISTS

This matters because the conversation summary described a full roster of systems as "Created." Checking the workspace directly:

| File | Status | Notes |
|---|---|---|
| `MMOCharacter.h` / `.cpp` | ✅ On disk, **patched this pass** | Critical networking bug found + fixed |
| `MMOInventoryItem.h` | ✅ On disk | Struct-only; no component |
| `MMOQuestTypes.h` | ✅ On disk | Struct-only; no component |
| `MMOPlayerProfile.h` | ✅ On disk | Struct-only |
| `MMOHousingBridgeComponent.h/.cpp` | ✅ On disk, **patched this pass** | New system from last round |
| `MMOCameraComponent.h/.cpp` | ✅ On disk, **patched this pass** | New system from last round |
| `MMOTargetableInterface.h` | ✅ On disk | Now actually implemented by `AMMOCharacter` (was previously dead code — see Finding #5) |
| `MMOInventoryComponent.h/.cpp` | ❌ **Not on disk** | Described in the original summary as complete; does not exist as a file. `MMOCharacter.cpp` `#include`s it and calls methods on it that must exist for the project to compile. |
| `MMOQuestComponent.h/.cpp` | ❌ **Not on disk** | Same situation. |
| `MMOAttributeSet.h/.cpp` | ❌ **Not on disk** | Same situation — this is the GAS attribute set; without it nothing compiles. |
| `MMOAbilitySystemComponent.h/.cpp` | ❌ **Not on disk** | `MMOCharacter.cpp` includes this header. |
| `MMOPersistenceSubsystem.h/.cpp` | ❌ **Not on disk** | Save/load is currently vaporware. |
| `MMOLootItem.h/.cpp` | ❌ **Not on disk** | |
| `AMMOCharacterSpawner.h/.cpp` | ❌ **Not on disk** | This is the class that's supposed to grant starter gear and set identity — see Finding #2. |
| `MMOPlayerState.h/.cpp` | ❌ **Not on disk** | |
| `MMOHousingSubsystem.h/.cpp` | ❌ **Not on disk** | `MMOHousingBridgeComponent` depends on this for `SpawnStructure()`. |

**Bottom line**: if you try to compile the project today, it will not compile — `MMOCharacter.cpp` alone `#include`s four headers that don't exist. The code that *does* exist has now been fully audited and patched. **If you want, the next step is generating the eight missing files for real** (not just described) so the project actually builds — say the word and I'll produce them with the same level of scrutiny applied here, matching the exact APIs the patched files now depend on (e.g. `ConsumeResources()` on the inventory component, specified in Finding #6).

---

## 1. CRITICAL — Client/Server RPC Pattern Was Broken In Two Places (Same Bug, Twice)

**Files**: `MMOCharacter.cpp` (`ToggleTargetLockMode`), `MMOHousingBridgeComponent.cpp` (`ToggleBuildMode`)
**Severity**: Critical — the affected features did not function in the project's own target architecture (dedicated server).

Both functions followed the same broken shape:

```cpp
void SomeToggleFunction()
{
    if (!IsLocallyControlled()) return;   // only ever runs on the owning CLIENT
    ...
    SomeAuthorityGatedFunction();          // internally checks ROLE_Authority and bails
}
```

`IsLocallyControlled()` is true on the client that owns a pawn. On a **dedicated server** (which this project explicitly targets — "sharded native dedicated server architecture" per the GDD), the server process never locally controls a remote player's pawn, so it is *never* true there for anyone but bots/AI. Meanwhile the function being called immediately does `if (GetOwnerRole() != ROLE_Authority) return;` — which is only true *on the server*. The two guards are mutually exclusive for every real remote client: the client-gated function runs, but the server-gated function it calls always bails out immediately, because it's running on the client, not the server.

**Practical effect**: hard-lock targeting and build-mode toggling would appear to work for a developer testing solo on a listen server (where the host client *is* the server, so `IsLocallyControlled()` and `ROLE_Authority` are both true for the host's own pawn) — and then silently do nothing for every other player the moment the project moves to the dedicated-server architecture the GDD calls for. This is exactly the kind of bug that survives solo PIE testing and only shows up once real multiplayer infrastructure is in place, at which point it looks like "the feature doesn't work" with no obvious cause.

**Fix applied** (both files, patched on disk):
- `AMMOCharacter::ToggleTargetLockMode()` now does local client-side prediction (for camera responsiveness) and fires `Server_ToggleTargetLockMode(AActor* DesiredTarget)`, a proper `Server, Reliable, WithValidation` RPC that performs the authoritative state change.
- `UMMOHousingBridgeComponent::ToggleBuildMode()` now fires `Server_SetBuildMode(bool)`, a proper Server RPC, instead of calling the authority-gated `OnBuildModeActivated()`/`ExitBuildMode()` directly from client code.
- `bIsHardLocked` is now replicated (it wasn't before — see Finding #3) so the corrected state actually reaches other clients.

**Why this matters more than any single issue in the previous review pass**: the last round of fixes (housing resource deduction, overlap checks, etc.) were real and correct, but they were built *on top of* a foundation where the entry point into the system never reached the server in the first place. Fixing resource deduction on a function that never runs server-side for real players doesn't close the exploit — it just means the exploit is "nothing happens," which is a functional bug rather than a security one, but still a total feature failure at scale.

---

## 2. CRITICAL — Redundant/Dead Starter-Gear Grant in `BeginPlay()`

**File**: `MMOCharacter.cpp`
**Severity**: High (dead code that could become a double-grant bug if touched carelessly)

```cpp
if (GetLocalRole() == ROLE_Authority && !PlayerUID.IsEmpty())
{
    if (InventoryComponent && InventoryComponent->IsEmpty())
    {
        InventoryComponent->GrantStarterGear(ClassTag);
    }
}
```

`PlayerUID` and `ClassTag` are replicated properties set by `AMMOCharacterSpawner` (per the original design) via `Server_SetPlayerIdentity`, which necessarily runs **after** the character has spawned — i.e., after `BeginPlay()` has already executed. At the moment `BeginPlay()` runs, `PlayerUID` is still empty, so this block's guard (`!PlayerUID.IsEmpty()`) is always false and the block is dead code, every time, for every character. It was never actually granting gear.

The risk isn't that it's currently broken (starter gear is presumably granted correctly by the spawner's own `SetupNewCharacter()` path) — it's that this dead branch *looks* functional, and a future engineer "fixing" the timing (e.g., moving identity-setting earlier) would suddenly cause **both** code paths to fire, double-granting starter gear.

**Fix applied**: removed the dead block; left a comment explaining starter-gear granting belongs exclusively to `AMMOCharacterSpawner` and must not be duplicated here.

---

## 3. CRITICAL — `bIsHardLocked` Was Not Replicated

**File**: `MMOCharacter.h`
**Severity**: Critical (compounds Finding #1)

`CurrentHardTarget` was marked `Replicated`; the boolean flag that says whether that target is actually meaningful, `bIsHardLocked`, was not. A replicated pointer with a non-replicated "is this valid" flag is close to useless — even once Finding #1 is fixed and the server correctly sets both values, only `CurrentHardTarget` would propagate to other clients, who would have no reliable way to know whether the lock is currently active.

**Fix applied**: `bIsHardLocked` is now `UPROPERTY(Replicated, ...)` and included in `GetLifetimeReplicatedProps`.

---

## 4. HIGH — Dangling/Stale Pointer Risk From Raw Null Checks

**File**: `MMOCharacter.cpp`, `IsValidCombatTarget`
**Severity**: Medium-High (rare but real edge case, easy fix)

```cpp
bool AMMOCharacter::IsValidCombatTarget(const AActor* TargetCandidate) const
{
    if (!TargetCandidate) return false;
    ...
}
```

`CurrentSoftTarget`/`CurrentHardTarget` are `UPROPERTY` raw pointers, so Unreal's garbage collector *does* eventually null them out when the referenced actor is destroyed — but not instantly. Between `Destroy()` being called on an actor and the next GC pass, a `UPROPERTY` pointer to it is still non-null but the object is pending-kill. A plain `if (!TargetCandidate)` check passes right through that window; `IsValid()` additionally checks the pending-kill/pending-destroy flags and correctly rejects it immediately.

**Fix applied**: `IsValidCombatTarget` and `ValidateHardLockTarget` (camera) now use `IsValid(...)` instead of a raw pointer check.

---

## 5. HIGH — The Decoupling Interface Was Declared But Never Actually Wired Up

**Files**: `MMOTargetableInterface.h`, `MMOCameraComponent.cpp`, `MMOHousingBridgeComponent.cpp`
**Severity**: High — this was reported as **fixed** in the previous pass (`ALL_FIXES_APPLIED.md` Fix #5) but was not.

The interface file existed, but:
- `AMMOCharacter` did not implement it (`class AMMOCharacter : public ACharacter, public IAbilitySystemInterface` — no `IMMOTargetableInterface` in the inheritance list).
- `MMOCameraComponent::UpdateSoftLockCamera` still read `OwnerCharacter->CurrentSoftTarget` directly, with a comment literally saying *"For now, accessing directly but marked for future refactor"* — i.e., the fix was acknowledged as not done, while the accompanying summary message described it as complete.
- `MMOHousingBridgeComponent`'s character-state checks referenced `OwnerCharacter->bIsDead` and `OwnerCharacter->IsInCombat()`, neither of which existed on `AMMOCharacter` at all (see Finding #6) — calling through the interface wouldn't even have compiled against the real class as it stood.

Calling `IMMOTargetableInterface::Execute_GetSoftTarget()` on an object that doesn't implement the interface is also not a graceful no-op in Unreal — it trips an assertion/ensure at runtime. So the interface, as delivered, was not just unused, it was unsafe to actually start using without this fix.

**Fix applied**: `AMMOCharacter` now declares and implements every `IMMOTargetableInterface` method. `MMOCameraComponent` and `MMOHousingBridgeComponent` now genuinely call through the interface instead of touching `AMMOCharacter` members directly.

---

## 6. CRITICAL — Housing Fix's Resource System Didn't Match The Actual Inventory Schema

**Files**: `MMOHousingBridgeComponent.cpp`, `MMOInventoryItem.h`
**Severity**: Critical — this was the previous round's headline "fix" (Fix #1, resource deduction) and it was built against an API that cannot exist as designed.

`FMMOInventoryItem` (the actual struct on disk) represents **one discrete equipment item**: an `int32 ItemID`, rarity, gear tier, one equip slot, armor/damage values. There is **no quantity/stack-count field anywhere**. The previous fix's `DeductBuildingCosts()` called `OwnerInventory->GetItemCountByTag(FName("Resource.Wood"))` and `RemoveItemByTag(...)` — methods that assume a taggable, stackable item system that doesn't exist in this schema. Even if those methods were added verbatim, representing "80 Stone" would require 80 separate array entries in an inventory capped at 20 slots — the resource system could never hold more than 20 units of any material, total, across all resource types combined.

**Fix applied**: `DeductBuildingCosts()` now calls a single atomic `OwnerInventory->ConsumeResources(Wood, Stone, Iron)`. This is a check-and-deduct-together call rather than a separate get/remove pair, so there's no window where a second spend could race the first. The required addition to the (currently nonexistent — see §0) `UMMOInventoryComponent` is fully specified in a comment block directly above the function, matching the pattern the project already uses elsewhere (`AetherCurrency` is a plain `int32` on `FMMOPlayerProfile`, not an inventory item — resources should follow the same precedent, not be shoehorned into the equipment array).

---

## 7. HIGH — Structure Overlap Checks Used The Wrong Collision Channel

**File**: `MMOHousingBridgeComponent.cpp`
**Severity**: High — silently broken in both directions

The overlap/distance checks used `ECC_Pawn` to detect existing structures. This is wrong two ways at once:
- **False positives**: a player or NPC standing near a build site registers as a "structure," blocking otherwise-valid placement.
- **False negatives**: EBS v10 pieces and most static architecture meshes default to blocking `ECC_WorldStatic`, not `ECC_Pawn` — meaning the check could completely fail to detect an already-placed structure, defeating the entire point of Fix #3 from the previous round (no overlapping structures).

**Fix applied**: introduced a dedicated `ECC_BuildingStructure` channel (mapped to `ECC_GameTraceChannel1`) with instructions to configure it in Project Settings and set every building piece's collision preset to block it specifically. Overlap and distance checks now use this channel.

---

## 8. MEDIUM-HIGH — Camera Occlusion Only Covered 1 of 4 Camera Modes

**File**: `MMOCameraComponent.cpp`
**Severity**: Medium-High — the previous "fix" covered the least-used mode and left the default mode broken

The previous pass's occlusion fix (Fix #6) only ran inside `UpdateHardLockCamera`. `FreeLook` — the default mode, used the overwhelming majority of play time (anytime the player isn't targeting something) — had zero occlusion handling, meaning the most common camera clipping scenario (walking backward into a wall in ordinary exploration) was left exactly as broken as before the "fix."

**Fix applied**: extracted a single `ResolveCameraOcclusion(PivotLoc, InOutCameraLoc)` helper and now call it from all four mode-update functions (`FreeLook`, `SoftLock`, `HardLock`, `BuildMode`). Also switched the trace channel from `ECC_WorldStatic` to `ECC_Camera` — the engine's dedicated camera-collision channel (the same one `USpringArmComponent::bDoCollisionTest` uses), since most environment art is already authored with sensible per-channel collision responses for this specific purpose, and some intentionally-passthrough decoration is set to ignore `ECC_Camera` while still blocking `WorldStatic`.

---

## 9. MEDIUM — Camera Component Ticked For Every Character, Not Just The Local Player's

**File**: `MMOCameraComponent.cpp`
**Severity**: Medium-High at small scale, severe at MMO scale

`TickComponent` ran its full update (including two line traces per frame — LOS + occlusion — plus interpolation math) unconditionally for every instance of this component, including every remote/simulated-proxy character belonging to *other* players. You never render another player's camera; there is no reason to compute it. At the hundreds-of-concurrent-characters scale this project targets, that's hundreds of wasted raycasts per frame, per client, for cameras nobody will ever see.

**Fix applied**: `BeginPlay()` now calls `SetComponentTickEnabled(false)` and returns immediately for any instance where `!OwnerCharacter->IsLocallyControlled()`. This disables the tick at the scheduler level rather than early-returning inside `TickComponent` every frame, which still costs a (much cheaper, but nonzero) function call per frame per instance.

---

## 10. MEDIUM — `_Validate` Functions Were Encoding Gameplay Rules (Client-Kick Risk)

**Files**: `MMOHousingBridgeComponent.cpp` (`Server_RequestStructurePlacement_Validate`, and the previous version's implicit pattern)
**Severity**: Medium — not exploitable, but a real risk of legitimate players getting disconnected

The original `Server_RequestStructurePlacement_Validate` rejected on `!bIsInBuildMode`. In Unreal, a `WithValidation` RPC whose `_Validate` function returns `false` causes the engine to **close the calling client's connection** — this mechanism exists to punish cheating (a hacked client spamming malformed RPCs), not to reject ordinary gameplay-state races. `bIsInBuildMode` can legitimately flip between the moment a player clicks to place a piece and the moment that RPC is processed on the server (e.g. combat interrupts build mode mid-click, or double-inputs). A player hitting that timing window would get kicked from the server for doing nothing wrong.

**Fix applied**: `_Validate` now only checks that `PieceID` isn't `NAME_None` (a cheap, genuine sanity check). Every real gameplay rule — including build-mode-active — is checked inside `_Implementation` via `ValidatePlacement()`, which fails softly by broadcasting `OnPlacementFailed` instead of terminating the connection. Applied the same principle to the new `Server_SetBuildMode`.

---

## 11. LOW-MEDIUM — `EvaluateSoftLockTarget()` Has No Caller

**File**: `MMOCharacter.h/.cpp`
**Severity**: Low (functional gap, not a bug) — flagging so it isn't lost

This function is `BlueprintCallable` and presumably intended to be invoked on a short repeating timer from a Blueprint (e.g., every 0.1s) per the original design description ("re-evaluated"), but nothing in the C++ code calls it — no `Tick`, no `FTimerHandle`. As written today, `CurrentSoftTarget` never updates unless something (currently nothing) calls this. Not fixed in this pass since the correct trigger cadence is a gameplay-feel decision, not a bug fix, but it needs an owner: either wire a `FTimerHandle` in C++ (recommended — keeps the targeting system self-contained rather than relying on every Blueprint remembering to call it) or explicitly document that it's the responsibility of the Player Controller Blueprint.

---

## 12. LOW — Persistence Type Mismatch Flagged For When That File Is Built

**Files**: `MMOPlayerProfile.h` vs. the (not-yet-existing) Quest Component
**Severity**: Low now, will become a real bug the moment `MMOQuestComponent` is actually written

`FMMOPlayerProfile::CompletedQuestIDs` is `TArray<FString>`. The quest component's live runtime data (per the original design description, not yet an actual file) uses `TArray<FName>` for completed quests. `FName` and `FString` don't implicitly convert in a `TArray` copy — whoever writes the actual persistence serialization code will need explicit conversion (`Name.ToString()` / `FName(*Str)`) in both directions. Flagging now so it's handled deliberately when that file is created rather than discovered as a compile error later.

---

## SUMMARY TABLE

| # | Finding | Severity | Status |
|---|---|---|---|
| 1 | Client-only toggle functions never reached server authority (2 locations) | 🔴 Critical | ✅ Fixed |
| 2 | Dead/redundant starter-gear grant in `BeginPlay` | 🔴 Critical | ✅ Fixed |
| 3 | `bIsHardLocked` not replicated | 🔴 Critical | ✅ Fixed |
| 4 | Raw pointer checks instead of `IsValid()` | 🟠 High | ✅ Fixed |
| 5 | Decoupling interface declared but never wired up (previously reported fixed; wasn't) | 🟠 High | ✅ Fixed for real this time |
| 6 | Resource deduction built against a nonexistent inventory API/schema | 🔴 Critical | ✅ Fixed (atomic `ConsumeResources`, schema specified) |
| 7 | Wrong collision channel for structure overlap (false pos. + false neg.) | 🟠 High | ✅ Fixed |
| 8 | Camera occlusion only covered 1 of 4 modes | 🟡 Medium-High | ✅ Fixed (all 4 modes) |
| 9 | Camera ticked for every character, not just the local player | 🟡 Medium-High | ✅ Fixed |
| 10 | Gameplay rules in `_Validate` risked disconnecting legitimate players | 🟡 Medium | ✅ Fixed |
| 11 | `EvaluateSoftLockTarget()` has no caller | 🟢 Low | ⚠️ Flagged, not fixed (design decision needed) |
| 12 | `FString`/`FName` mismatch for completed quests | 🟢 Low | ⚠️ Flagged for when that file is built |
| 0 | 8 core files described as built do not exist on disk | 🔴 Critical (project-level) | ⚠️ Flagged — see below |

---

## WHAT'S ACTUALLY READY VS. WHAT ISN'T

**Ready to compile and use as-is** (once the 8 missing files exist with the APIs specified in this audit):
- `MMOCharacter.h/.cpp` — patched, networking-correct
- `MMOHousingBridgeComponent.h/.cpp` — patched, networking-correct, correct collision channel
- `MMOCameraComponent.h/.cpp` — patched, performant, occlusion-correct
- `MMOTargetableInterface.h` — now genuinely wired up
- `MMOInventoryItem.h`, `MMOQuestTypes.h`, `MMOPlayerProfile.h` — pure data structs, no issues found

**Cannot compile today**:
- The entire project, because `MMOCharacter.cpp` includes four headers (`MMOAbilitySystemComponent.h`, `MMOAttributeSet.h`, `MMOInventoryComponent.h`, `MMOQuestComponent.h`) that don't exist as files in this workspace, regardless of whether they were described as complete earlier in the conversation.

**Recommended next step**: I can generate the 8 missing files now, built explicitly to satisfy every API this audit specified (`UMMOInventoryComponent::ConsumeResources/GetResourceWood/Stone/Iron`, GAS attribute replication with proper `OnRep_*` + `GAMEPLAYATTRIBUTE_REPNOTIFY`, etc.), so the project is a real, compiling whole rather than a patched subset plus a described-but-absent remainder. That's the highest-value next action if the goal is an actual buildable demo.

