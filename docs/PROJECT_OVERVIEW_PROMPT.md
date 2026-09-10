# Kingdoms Ascendant — Master Project Overview Prompt

*Use this as the standing context when handing this project to another AI coding tool (e.g. Codex) or a new engineer. It replaces re-explaining the project from scratch. Paste it as the system/context prompt, then attach the actual source files listed in §4.*

---

## 1. WHAT THIS IS

**Kingdoms Ascendant** is a class-based fantasy MMORPG built in **Unreal Engine 5.8** (C++, Gameplay Ability System, server-authoritative networking, sharded dedicated-server architecture for scale). The full vision (per the Game Design Document, v2.2) is Level 1-50, 8 playable classes, 3 raid tiers, and a "living endgame" system where retired player-characters persist as AI-controlled figures in the world.

**What's currently being built is an intentional vertical slice**, not the full game: Level 1-20, 2 of the 8 classes (Guardian — tank/melee, Wizard — ranged caster), no endgame/raid content. This scope was explicitly confirmed by the project owner as deliberate, not a gap to apologize for — the slice exists to prove the core loop (combat targeting, quests, loot, progression, housing) before scaling to the full class/level range.

**Third-party integrations the full design calls for**: Flexible Combat System (FCS, a UE marketplace asset, as the base combat framework), Easy Building System v10 (EBS, free UE5.8 asset, for the housing system), Polyart Studios + Aleksandr Ivanov environment/architecture asset packs blended via PCG and a shared RVT color-map system. **None of these third-party plugins are imported into the project yet** — the code written so far integrates against them at defined seams (see §3) but does not depend on their actual classes compiling, so the project is buildable without them present, with clearly marked integration points for when they are.

---

## 2. CORE SYSTEMS & DESIGN PARAMETERS

**Classes/Specs (vertical slice)**:
- Guardian / Vanguard — tank, highest Health+Armor+ThreatModifier of the two Guardian specs, lowest damage.
- Guardian / Berserker — off-tank/DPS, less Health/Armor/Threat than Vanguard, more AttackPower.
- Wizard / Elementalist — ranged burst caster, lowest Health/Armor, highest Mana.
- Wizard / BattleMage — melee-range caster/bruiser, more Health/Armor than Elementalist, highest AttackPower of the two Wizard specs, less Mana.

**Level scaling formula**: `LevelMultiplier = 1.0 + (Level - 1) × 0.173`, producing 1.0× at Level 1 scaling to 4.0× at Level 20. Applied to base Health/Mana/AttackPower/ArmorValue per class/spec; `ThreatModifier` is a flat per-spec multiplier, NOT level-scaled.

**Combat targeting**: hybrid soft-lock/hard-lock/free-aim. Soft-lock is a client-only cosmetic scan (120° forward cone, 800u radius sphere overlap, scored by `(dot×2) − (distance/radius)` with a +0.4 stickiness bonus to reduce flicker) — never needs to replicate, re-evaluated locally per client. Hard-lock is server-authoritative (breaks on target death, >1200u range, or loss of line-of-sight) and DOES replicate (`bIsHardLocked` + `CurrentHardTarget`).

**Currency/resources**: `AetherCurrency` (quest/loot reward currency) and building resources (`ResourceWood`/`ResourceStone`/`ResourceIron`) are both modeled as plain replicated `int32` counters, NOT as inventory item stacks — the equipment inventory (`FMMOInventoryItem`) has no quantity field and is capped at 20 slots, so stackable currency-like values live outside it.

**Quests**: DataTable-driven (`FQuestObjectiveDefinition` rows), objective types KillTarget/GatherItem/ExploreArea/TalkToNPC, level-gated, reward XP + Aether (+ a gear tier that is intentionally NOT auto-rolled into loot by the quest system itself — that's a per-quest content decision, see §5).

**Housing**: player-placed structures via a build-mode toggle that swaps FCS combat state for placement-preview mode, server-validates placement (plot boundary, ground, no-overlap, minimum spacing), deducts resources atomically, then spawns a replicated structure actor.

---

## 3. ARCHITECTURE & HARD-WON PATTERNS (do not regress on these)

This project went through two full audit passes that found and fixed real, non-obvious bugs. The patterns below exist *because* of those bugs — follow them for any new code, don't rediscover the failure mode.

**Pattern 1 — Client-triggered mutation of replicated state MUST go through a real Server RPC.** The single most damaging bug found in this project was two separate functions (`ToggleTargetLockMode`, `ToggleBuildMode`) that were gated on `IsLocallyControlled()` at the call site and `ROLE_Authority` inside the function they called — with **no RPC macro bridging the two**. This works by accident on a listen server (host is both client and server) and does *nothing* for every other player on a dedicated server, which is this project's actual target architecture. It's the exact kind of bug that survives solo playtesting. Every new client-initiated action that mutates authoritative state must be a real `UFUNCTION(Server, Reliable, WithValidation)`.

**Pattern 2 — Keep gameplay rules OUT of `_Validate` functions.** A failed `_Validate` closes the calling client's connection — that mechanism is for rejecting cheating/malformed input, not ordinary gameplay-state races (e.g. build mode toggling off between a click and its RPC arriving). `_Validate` should only check for obviously malformed input (empty names, negative indices); real rules belong in `_Implementation` and fail softly (broadcast a reason, do nothing) instead of disconnecting a legitimate player.

**Pattern 3 — `IsValid()`, not a raw null check**, for any pointer to a UObject/AActor that could have been `Destroy()`'d — a `UPROPERTY` pointer isn't nulled until the next GC pass, so a plain `if (!Ptr)` can pass through a window where the object is already pending-kill.

**Pattern 4 — GAS attributes need the FULL replication pattern, every time**: `ReplicatedUsing = OnRep_X` on the `FGameplayAttributeData`, `DOREPLIFETIME_CONDITION_NOTIFY(.., REPNOTIFY_Always)` in `GetLifetimeReplicatedProps`, and `GAMEPLAYATTRIBUTE_REPNOTIFY` inside `OnRep_X`. Missing any one piece compiles fine and silently breaks for remote clients only.

**Pattern 5 — Decouple cross-cutting concerns (targeting, death/combat state) via `IMMOTargetableInterface`**, not direct casts/property access into `AMMOCharacter`. `AMMOCharacter` implements it; camera, housing, and any future combat/AI code should query through it so those systems don't hard-depend on `AMMOCharacter`'s concrete layout.

**Pattern 6 — Disable Tick entirely (`SetComponentTickEnabled(false)`) for any per-character component whose logic only matters for the locally-controlled player** (camera, input-driven prediction). At MMO scale, ticking full update logic (especially anything with a line trace) for every remote/simulated-proxy character is a real, measurable cost for zero benefit — you never render another player's camera.

**Pattern 7 — One canonical source per piece of data.** Piece definitions live on `MMOHousingBridgeComponent::AvailablePieces`; the housing subsystem takes the resolved definition as a parameter rather than maintaining its own copy. Don't let two systems each hold their own copy of the same content data.

**Pattern 8 — Sanitize any player-supplied string before it touches a filesystem path.** `PlayerUID` is client-influenced (travels through a Server RPC from character creation) and is used to build save-file paths — always strip path-traversal characters (`..`, `/`, `\`, `:`) before building a path from user-influenced input.

---

## 4. CURRENT FILE INVENTORY (all present, should compile as a coherent whole)

**Characters & Player**: `MMOCharacter.h/.cpp` (pawn, targeting, GAS interface), `MMOPlayerState.h/.cpp` (identity + live XP/level/Aether), `AMMOCharacterSpawner.h/.cpp` (new-character and load-character orchestration — the *only* place starter gear is granted).

**GAS**: `MMOAbilitySystemComponent.h/.cpp`, `MMOAttributeSet.h/.cpp` (Health/Mana/AttackPower/ArmorValue/ThreatModifier, full replication, `InitializeAttributesForLevel`).

**Inventory**: `MMOInventoryItem.h` (item/equipment structs), `MMOInventoryComponent.h/.cpp` (equip/unequip, starter gear, Wood/Stone/Iron resource counters with atomic `ConsumeResources`).

**Quests**: `MMOQuestTypes.h` (structs/enums), `MMOQuestComponent.h/.cpp` (accept/abandon/progress/completion, DataTable-driven).

**Loot**: `MMOLootItem.h/.cpp` (sphere-pickup, 5-min despawn, rarity-tinted material).

**Housing**: `MMOHousingBridgeComponent.h/.cpp` (build mode, placement validation, cost deduction), `MMOHousingSubsystem.h/.cpp` (structure spawning/tracking), `AMMOPlacedStructure.h/.cpp` (placeholder structure actor — see §5 for what replaces it).

**Camera**: `MMOCameraComponent.h/.cpp` (FreeLook/SoftLock/HardLock/BuildMode, occlusion-aware, tick-gated to local player only).

**Interfaces**: `MMOTargetableInterface.h` (belongs at `Public/Interfaces/`).

**Persistence**: `MMOPlayerProfile.h` (save-file struct), `MMOPersistenceSubsystem.h/.cpp` (JSON save/load, versioned, sanitized paths).

**Data**: `DT_Quests_Level1_10.json`, `DT_Loot_Dungeon.json` (import into UE DataTable assets per `SETUP_GUIDE.md`).

**Docs in the project bundle**: `README.md`, `SETUP_GUIDE.md` (90-minute UE5.8 setup walkthrough), `FILE_MANIFEST.md`, `PROFESSIONAL_ASSESSMENT.md` (strategic/production review), `MASTER_CODE_AUDIT.md` + `CODE_DESIGN_REVIEW.md` + `ALL_FIXES_APPLIED.md` (the two audit passes and their fixes — read these before touching networking/camera/housing code, they document *why* the code looks the way it does), `NEW_CODE_ADDED.md` (what was added to close the compile-gap), `IMPLEMENTATION_GUIDE_EBS_HARDLOCK.md` (EBS v10 + hard-lock integration architecture).

---

## 5. WHAT IS DELIBERATELY NOT BUILT YET (don't re-flag these as bugs — they're scoped out)

- **Equipment persistence** — save/load covers level/XP/Aether/completed quests, not the equipped-item array.
- **Real `GameplayAbility` classes** — `MMOAbilitySystemComponent::GrantStartingAbilities` is a logged no-op; no ability kit has been designed yet.
- **Auto-rolled gear-tier loot on quest completion** — quests grant XP/Aether directly; tying a specific quest's gear reward to a specific loot roll is a per-quest content decision, not generic component logic.
- **Real EBS v10 piece actors** — `AMMOPlacedStructure` is an explicitly-documented placeholder so housing is testable now; swap `PieceClass` per piece to real EBS actors once that plugin is imported, per `IMPLEMENTATION_GUIDE_EBS_HARDLOCK.md`.
- **Compilation has not been verified inside an actual UE5.8 toolchain** (this dev environment has none) — every symbol/include was traced by hand to resolve, but the compiler hasn't had the final word yet. Treat the codebase as "should compile clean," verify for real in-editor first.
- **Classes 3-8, Level 21-50, raid tiers, the living-endgame AI-avatar system** — full-game scope, not vertical-slice scope, per the project owner's explicit confirmation.

---

## 6. HOW TO USE THIS PROMPT

If you're an AI picking this project up: read §3 before writing or reviewing any networking, GAS, or per-character-component code — those patterns exist because their absence already caused real bugs once. Check §5 before "fixing" something that's actually scoped out on purpose. §4 tells you what already exists so you don't recreate it. The audit documents (`MASTER_CODE_AUDIT.md` etc.) have the full before/after reasoning if you need to understand *why* a piece of code is shaped the way it is, not just that it is.
