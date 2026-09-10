# New Code Added — Closing the Gap Found in MASTER_CODE_AUDIT.md

**Answer to "does new code need to be written": yes.** The previous audit found that 8 files referenced by `#include` or by direct call (`MMOInventoryComponent`, `MMOQuestComponent`, `MMOAttributeSet`, `MMOAbilitySystemComponent`, `MMOPersistenceSubsystem`, `MMOLootItem`, `AMMOCharacterSpawner`, `MMOPlayerState`) did not exist anywhere in the workspace, which meant the project could not compile regardless of how correct the already-patched files were. Rather than re-reading the GDD to produce another document, the correct response to that finding is to close it — so that's what this turn did.

**10 new files were written** (the 8 originally missing, plus 2 more that turned out to be required once the housing system was actually wired up for real — see §3). Every file follows the same rules established in the audit: real Server RPCs (never a client-gated call into an authority-gated function with no network bridge), `IsValid()` over raw null checks, permissive `_Validate` functions with gameplay rules living in `_Implementation`, and — critically — every API these files expose was **built to match what the already-patched files (`MMOHousingBridgeComponent`, `MMOCharacter`) actually call**, not designed in isolation and hoped to line up.

---

## 1. GAS Attribute System

**`MMOAbilitySystemComponent.h/.cpp`** — thin `UAbilitySystemComponent` subclass with an explicit, logged-not-hidden stub for granting class abilities (`GrantStartingAbilities`) - empty until real `GameplayAbility` classes exist, which is outside this pass's scope (no ability *design* was specified anywhere in this conversation to build against).

**`MMOAttributeSet.h/.cpp`** — Health/MaxHealth/Mana/MaxMana/AttackPower/ArmorValue/ThreatModifier, every single one with the full replication pattern GAS requires (`ReplicatedUsing` + `DOREPLIFETIME_CONDITION_NOTIFY(.., REPNOTIFY_Always)` + `GAMEPLAYATTRIBUTE_REPNOTIFY` in the `OnRep_*`). This is the single most commonly-botched part of a GAS project — skip either half of that pair and it compiles fine, looks correct for the host/server player, and silently breaks GAS's internal value tracking for every remote client. `PreAttributeChange`/`PostGameplayEffectExecute` clamp Health/Mana to their max both ways (direct sets and GameplayEffect-driven changes). `InitializeAttributesForLevel(Level, ClassTag, SpecTag)` implements the level-scaling formula from this conversation's earlier design work (`1.0 + (Level-1)×0.173`, 1.0→4.0 across Levels 1-20) with base values tuned so Guardian/Vanguard, Guardian/Berserker, Wizard/Elementalist and Wizard/BattleMage each land inside the Health/AttackPower/ThreatModifier ranges established earlier, without re-deriving those ranges from scratch.

---

## 2. Inventory, Equipment & Resources

**`MMOInventoryComponent.h/.cpp`** — implements exactly the API `MMOHousingBridgeComponent` (already patched) and `MMOCharacter`/`AMMOCharacterSpawner` (new this pass) actually call:
- `AddItem` / `RemoveItem` / `IsEmpty` / `IsFull` — equipment array management, 20-slot cap.
- `RequestEquipItem` (client) → `Server_EquipItem` (real RPC) — validates class requirement via `FMMOInventoryItem::CanEquip`, swaps whatever was previously in that gear slot back into the freed inventory slot rather than discarding it.
- `GrantStarterGear(ClassTag)` — Guardian gets sword/shield/cuirass, Wizard gets staff/robes/focus, matching the original design.
- `GetResourceWood/Stone/Iron`, `AddResources`, `ConsumeResources` — the resource-currency system specified as a requirement in Finding #6 of the audit, implemented exactly as specified: three plain replicated `int32`s (matching the project's existing `AetherCurrency` precedent), with `ConsumeResources` doing an atomic check-then-deduct-all-three rather than three separate calls.

---

## 3. Quest Component (and Why the Housing System Needed 2 More Files)

**`MMOQuestComponent.h/.cpp`** — `RequestAcceptQuest`/`RequestAbandonQuest` (client) → real `Server_AcceptQuest`/`Server_AbandonQuest` RPCs, with level-range validation against the quest definition. `Server_NotifyGameplayEvent` updates progress and — this is the fix for the *original* Gemini-era bug noted all the way back at the start of this conversation ("`EvaluateQuestCompletion()` defined but never called") — now actually calls `CheckQuestCompletion()` every time, not just sometimes. Rewards (XP, Aether) are granted through the new `AMMOPlayerState`, not invented on the spot.

While wiring quest rewards through `AMMOPlayerState`, tracing what `AMMOCharacterSpawner` and `MMOPersistenceSubsystem` needed turned up two more files that don't exist yet either — not part of the original 8, but required for what was already delivered to actually run:

**`AMMOPlayerState.h/.cpp`** — `PlayerUID`/`SelectedClassTag`/`SelectedSpecTag`/`PlayerLevel` (matching the original design) *plus* `ExperiencePoints`/`AetherCurrency` as the live, replicated runtime copies of what `FMMOPlayerProfile` persists to disk. `Server_SetPlayerIdentity` is a real RPC (character creation is client-initiated); `Server_AddExperience`/`Server_AddAether` are deliberately *not* RPCs — they're server-internal functions called from other already-server-side code (the quest component), and making them RPCs would just add a pointless same-process network indirection. `ProcessLevelUps()` loops (not a single `if`) so a large XP grant can carry a character through more than one level, and re-initializes attributes via `MMOAttributeSet::InitializeAttributesForLevel` on every level gained.

**`AMMOCharacterSpawner.h/.cpp`** — this is the class the earlier audit's Finding #2 said *should* own starter-gear granting exclusively (instead of the dead `BeginPlay()` code that was removed from `MMOCharacter`). `Server_SpawnNewCharacter` and `Server_LoadCharacter` both possess the pawn, set identity via `AMMOPlayerState`, and call one of two clearly-separated setup paths: `SetupNewCharacter` (Level 1, starter gear, fresh attributes) or `SetupLoadedCharacter` (restores level/attributes/completed-quest history from a `FMMOPlayerProfile`, and deliberately does *not* re-grant starter gear, which would duplicate items for a returning player).

---

## 4. Persistence

**`MMOPersistenceSubsystem.h/.cpp`** — `SavePlayerProfile`/`LoadPlayerProfile`/`ProfileExists`, JSON via `FJsonObject`, versioned (`CURRENT_SAVE_VERSION`, with `MigrateProfileIfNeeded` as an explicit no-op today rather than an absent function the next engineer has to invent). Two things worth flagging on their own:

- **The `FName`/`FString` mismatch the previous audit flagged (Finding #12) is now resolved at the boundary, not by changing either type.** `FMMOPlayerProfile::CompletedQuestIDs` stays `TArray<FString>` (stable, readable save format); `MMOQuestComponent::CompletedQuests` stays `TArray<FName>` (fast runtime comparison). The conversion happens explicitly in both directions: `SerializeProfileToJSON`/`DeserializeProfileFromJSON` handle the JSON↔FString side, `AMMOCharacterSpawner::SetupLoadedCharacter` handles the FString↔FName side when populating a loaded character's live quest component.
- **A real security issue turned up while writing this file, not previously flagged**: `PlayerUID` is used to build the save file's path on disk, and `PlayerUID` ultimately originates from client-provided data (it travels through `Server_SetPlayerIdentity`). An unsanitized UID containing `..`, `/`, or `\` could be used to read or write files outside the `Saved/Profiles/` directory. `GetProfileFilePath` now strips those characters before building the path. This is the kind of thing that's easy to miss because it never comes up in solo testing with a well-formed UID — flagging it explicitly rather than letting it pass silently.

---

## 5. Loot

**`MMOLootItem.h/.cpp`** (declared as `AMMOLootItem`, matching the original naming) — sphere-overlap pickup, 5-minute auto-despawn, rarity-tinted material (grey/green/blue/purple/orange via a dynamic material instance parameter). **Proactively applied the audit's Finding #1 lesson before it could become Finding #13**: the overlap callback checks `GetLocalRole() != ROLE_Authority` and bails before doing anything, because overlap events on a replicated actor can fire on machines other than the server, and pickup (granting inventory, destroying the actor) must only ever be decided authoritatively.

---

## 6. Housing System Completion

The previous pass fixed `MMOHousingBridgeComponent` thoroughly, but it called `HousingSubsystem->SpawnStructure(...)` against a subsystem that didn't exist, and even once it existed, `MMOHousingBridgeComponent::InitializeDefaultPieces()` never set `PieceClass` on any of its default pieces — there was nothing to actually spawn. Two files close this out:

**`MMOHousingSubsystem.h/.cpp`** — a `UWorldSubsystem` (world-scoped, appropriate for spawning world actors) whose `SpawnStructure` takes the already-resolved `FBuildingPieceDefinition` from the bridge component rather than maintaining a second, potentially-desynced copy of piece data. Falls back to a real spawnable class (below) when a piece doesn't specify a custom `PieceClass`, so every default piece is genuinely placeable today.

**`AMMOPlacedStructure.h/.cpp`** — explicitly documented as a placeholder bridge actor, not the final shipped structure class. It loads its mesh from `FBuildingPieceDefinition::EBSMeshPath` at runtime and configures its collision to block `ECC_BuildingStructure` (the dedicated channel the previous audit introduced specifically so overlap/distance checks work correctly). Once EBS v10 is actually imported per `IMPLEMENTATION_GUIDE_EBS_HARDLOCK.md`, real pieces should point `PieceClass` at EBS's own piece actors instead — this class exists so the housing feature is genuinely testable end-to-end *before* that integration happens, not to replace it.

One small surgical edit was made to the already-delivered `MMOHousingBridgeComponent_FIXED.cpp` to pass the resolved piece definition through to the new subsystem call — noted here rather than silently changing a file you already have.

---

## WHAT'S DELIBERATELY STILL NOT HERE

Consistent with this project's own stated philosophy (intentional vertical-slice scope, not "everything"):

- **Equipment persistence** — `FMMOPlayerProfile` saves level/XP/Aether/completed quests, not the equipped-item array. `AMMOCharacterSpawner::SetupLoadedCharacter` says so explicitly in a comment rather than silently dropping it.
- **Real GameplayAbility classes** — `MMOAbilitySystemComponent::GrantStartingAbilities` is a logged no-op until actual ability Blueprints/C++ classes are designed; no ability kit was specified anywhere in this conversation to build against.
- **Gear-tier loot rolls on quest completion** — `MMOQuestComponent::AwardQuestRewards` grants XP/Aether directly but does not auto-roll a `RewardGearTier` into a spawned `AMMOLootItem`; that's a per-quest content/level-design decision (what moment triggers the reward - a chest, an NPC handoff), not something a generic component should hardcode.
- **Real EBS v10 piece actors** — `AMMOPlacedStructure` is the honest placeholder described above.

---

## FILE INVENTORY — UPDATED

| File | Status |
|---|---|
| Everything listed in the previous audit as "✅ On disk, patched" | Unchanged |
| `MMOAbilitySystemComponent.h/.cpp` | ✅ New |
| `MMOAttributeSet.h/.cpp` | ✅ New |
| `MMOInventoryComponent.h/.cpp` | ✅ New |
| `MMOQuestComponent.h/.cpp` | ✅ New |
| `MMOPlayerState.h/.cpp` | ✅ New |
| `AMMOCharacterSpawner.h/.cpp` | ✅ New |
| `MMOPersistenceSubsystem.h/.cpp` | ✅ New |
| `MMOLootItem.h/.cpp` | ✅ New |
| `MMOHousingSubsystem.h/.cpp` | ✅ New (not in original 8 - required by the housing bridge) |
| `AMMOPlacedStructure.h/.cpp` | ✅ New (not in original 8 - makes housing actually testable) |

**The project is now a complete, internally-consistent set of files** — every `#include` resolves to a real file, every cross-component call matches a real method signature. This has not been compiled inside an actual Unreal Engine toolchain (this environment doesn't have one), so treat it as "should compile clean" rather than "verified compiling" — the next real step is opening it in UE5.8 and letting the compiler have the final word, per `SETUP_GUIDE.md`.

**Placement note**: `MMOTargetableInterface.h` (from the previous pass) should live at `Public/Interfaces/MMOTargetableInterface.h` — every file that includes it uses the path `"Interfaces/MMOTargetableInterface.h"`.
