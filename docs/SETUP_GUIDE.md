# Kingdoms Ascendant Demo - Setup Guide

This guide walks you through setting up the complete Kingdoms Ascendant demo from scratch in Unreal Engine 5.8.

---

## 🚀 Phase 1: Project Creation (15 minutes)

### Step 1: Create New UE5.8 Project

1. Open **Epic Games Launcher**
2. Click **Create** → **Games**
3. Select **Blank** template
4. Configure:
   - **Engine Version**: Unreal Engine 5.8
   - **Project Type**: C++
   - **Project Name**: KingdomsAscendant
   - **Location**: Choose your development folder
5. Click **Create Project**

Wait for initial project generation (5-10 minutes)

### Step 2: Enable Gameplay Ability System Plugin

1. In Unreal Editor: **Edit** → **Plugins**
2. Search for "Gameplay Ability System"
3. Enable the checkbox
4. Restart Unreal Editor

---

## 📁 Phase 2: Copy Source Files (10 minutes)

### Directory Structure

Create these folders in `Source/KingdomsAscendant/`:

```
Public/
├── Characters/
├── Player/
├── Inventory/
├── Quest/
├── AbilitySystem/
├── Persistence/
├── Loot/
├── World/
└── Combat/

Private/
├── Characters/
├── Player/
├── Inventory/
├── Quest/
├── AbilitySystem/
├── Persistence/
├── Loot/
├── World/
└── Combat/
```

### Copy Header Files (.h)

Create these files in the `Public/` folders:

**Characters/**
- `MMOCharacter.h`

**Player/**
- `MMOPlayerState.h`

**Inventory/**
- `MMOInventoryItem.h`
- `MMOInventoryComponent.h`

**Quest/**
- `MMOQuestTypes.h`
- `MMOQuestComponent.h`

**AbilitySystem/**
- `MMOAttributeSet.h`

**Persistence/**
- `MMOPersistenceSubsystem.h`
- `MMOPlayerProfile.h`

**Loot/**
- `MMOLootItem.h`

**World/**
- `AMMOCharacterSpawner.h`

### Copy Implementation Files (.cpp)

Create these files in the `Private/` folders (same structure as .h files):

- `MMOCharacter.cpp`
- `MMOPlayerState.cpp`
- `MMOInventoryComponent.cpp`
- `MMOQuestComponent.cpp`
- `MMOAttributeSet.cpp`
- `MMOPersistenceSubsystem.cpp`
- `MMOLootItem.cpp`
- `AMMOCharacterSpawner.cpp`

---

## 📊 Phase 3: Create DataTables (10 minutes)

DataTables store quest and loot data in Unreal Editor.

### Create Quest DataTable

1. **Right-click** in Content Browser
2. **Miscellaneous** → **Data Table**
3. Select **Row Structure**: Choose `FQuestObjectiveDefinition`
4. Name it: `DT_Quests_Level1_10`
5. **Double-click** to open
6. **Add Row** buttons and fill in data:

| Row Name | QuestID | QuestTitle | ObjectiveType | ObjectiveTag | RequiredCount | MinimumLevel | MaximumLevel | RewardXP | RewardAether | RewardGearTier |
|----------|---------|-----------|---------------|--------------|---------------|--------------|--------------|----------|--------------|----------------|
| Quest_01_BeachBoars | Quest_01_BeachBoars | Clear the Coastline | KillTarget | Quest.Target.BeachBoar | 8 | 1 | 10 | 1200 | 100 | 1 |
| Quest_02_HarvestTimber | Quest_02_HarvestTimber | Gather Ancient Timber | GatherItem | Quest.Target.AncientOak | 5 | 2 | 10 | 800 | 75 | 1 |
| ... | ... | ... | ... | ... | ... | ... | ... | ... | ... | ... |

**See `DT_Quests_Level1_10.json` for complete data**

### Create Loot DataTable

1. **Right-click** in Content Browser
2. **Miscellaneous** → **Data Table**
3. Select **Row Structure**: Create new `FLootTableRow` struct OR use `FTableRowBase`
4. Name it: `DT_Loot_Dungeon`
5. **Add Row** and fill in loot data:

| Row Name | ItemID | ItemName | DropWeight | GearTier | ItemRarity |
|----------|--------|----------|------------|----------|------------|
| Loot_001 | 301 | Forged Vanguard Greatshield | 15 | 3 | Uncommon |
| Loot_002 | 302 | Forged Berserker Cleaver | 20 | 3 | Uncommon |
| ... | ... | ... | ... | ... | ... |

**See `DT_Loot_Dungeon.json` for complete data**

---

## 🎮 Phase 4: Create Game Blueprint Classes (15 minutes)

### Character Blueprint

1. **Right-click** Content Browser → **Blueprint Class**
2. Search for `AMMOCharacter`
3. Name it: `BP_Player_Guardian`
4. Set defaults:
   - **Mesh**: Assign a skeletal mesh (or leave default)
   - **Class Tag**: Set to `Class.Guardian`
   - **Spec Tag**: Set to `Class.Guardian.Vanguard`

Create another for Wizard:
- Name: `BP_Player_Wizard`
- **Class Tag**: `Class.Wizard`
- **Spec Tag**: `Class.Wizard.Elementalist`

### Character Spawner Blueprint

1. **Right-click** Content Browser → **Blueprint Class**
2. Search for `AMMOCharacterSpawner`
3. Name it: `BP_CharacterSpawner`
4. Set defaults:
   - **Default Character Class**: `BP_Player_Guardian` (or create generic base)
   - **New Character Spawn Location**: `(0, 0, 100)` (adjust for your map)
   - **Quest DataTable**: `DT_Quests_Level1_10`

### Game Mode Blueprint

1. **Right-click** Content Browser → **Blueprint Class**
2. Search for `AGameModeBase`
3. Name it: `BP_GameMode_Default`
4. Set defaults:
   - **Default Pawn Class**: `BP_Player_Guardian`
   - **Player State Class**: Keep default (or custom `AMMOPlayerState`)
   - **Spawn Actor List**: Add `BP_CharacterSpawner`

---

## 🗺️ Phase 5: Create Levels (10 minutes)

### Starting Zone Level

1. **File** → **New Level**
2. Select **Blank Level**
3. **Save As**: `L_StartingZone`
4. **Place** → Add floor geometry or landscape
5. **Place** `BP_CharacterSpawner` at origin (0, 0, 0)
6. **World Settings**:
   - **Game Mode**: `BP_GameMode_Default`
   - **Default Spawn Location**: `(0, 0, 100)`

### Set as Default Level

1. **Edit** → **Project Settings**
2. Search: "Default Map"
3. Set **Editor Startup Map**: `L_StartingZone`
4. Set **Game Default Map**: `L_StartingZone`

---

## ⚙️ Phase 6: Configure Gameplay Tags (5 minutes)

### Add Gameplay Tags

1. **Edit** → **Project Settings**
2. Search: "Gameplay Tags"
3. Click **Import Tags From Config**
4. Or manually add tags in **DefaultGameplayTags.ini**:

```ini
[/Script/GameplayTags.GameplayTagsManager]
CommonlyReplicatedTags=(Tag="Class.Guardian",Replicated=true)
CommonlyReplicatedTags=(Tag="Class.Guardian.Vanguard",Replicated=true)
CommonlyReplicatedTags=(Tag="Class.Guardian.Berserker",Replicated=true)
CommonlyReplicatedTags=(Tag="Class.Wizard",Replicated=true)
CommonlyReplicatedTags=(Tag="Class.Wizard.Elementalist",Replicated=true)
CommonlyReplicatedTags=(Tag="Class.Wizard.BattleMage",Replicated=true)
CommonlyReplicatedTags=(Tag="Quest.Target.BeachBoar",Replicated=false)
CommonlyReplicatedTags=(Tag="Quest.Target.AncientOak",Replicated=false)
```

---

## 🔧 Phase 7: Compile & Build (10 minutes)

### Compile C++ Code

1. **Tools** → **Compile** (or Ctrl+Shift+B)
2. Wait for compilation to complete
3. Check Output Log for errors

### Fix Compilation Errors (if any)

Common issues:
- **Missing includes**: Check `#include` paths match your folder structure
- **Undefined classes**: Ensure forward declarations are correct
- **Linking errors**: Rebuild entire solution in Visual Studio

---

## ✅ Phase 8: Testing (15 minutes)

### Run in Editor

1. Click **Play** (Alt+P)
2. Verify:
   - ✓ Character spawns
   - ✓ Can move around
   - ✓ Inventory component visible
   - ✓ Quest component initialized

### Test Quest System

1. In the game, trigger a quest event:
   ```cpp
   // In character class or console command
   GetWorld()->GetFirstPlayerController()->GetPawn()->GetQuestComponent()->Server_NotifyGameplayEvent(
       FGameplayTag::RequestGameplayTag(FName("Quest.Target.BeachBoar")), 
       8
   );
   ```
2. Verify quest completes and awards rewards

### Test Persistence

1. Close PIE
2. Open saved profile in: `Saved/Profiles/`
3. Verify `.json` file contains character data

---

## 📦 Phase 9: Package for Distribution (20 minutes)

### Create Shipping Build

1. **File** → **Package Project** → **Windows** (or target platform)
2. Choose output folder
3. Wait for packaging (2-5 minutes)

### Test Packaged Build

```bash
# Run packaged game
./KingdomsAscendant/Binaries/Win64/KingdomsAscendant.exe
```

Verify all systems work in packaged build

---

## 🎓 Phase 10: Git Setup (5 minutes)

### Initialize Git Repository

```bash
cd KingdomsAscendant_Demo
git init
git add .
git commit -m "Initial commit: Kingdoms Ascendant Demo vertical slice"
git branch -M main
git remote add origin https://github.com/yourusername/KingdomsAscendant_Demo.git
git push -u origin main
```

---

## 📋 Complete Setup Checklist

- [ ] UE5.8 project created
- [ ] GAS plugin enabled
- [ ] All source files copied
- [ ] Folder structure created
- [ ] Code compiles without errors
- [ ] DataTables created (Quests & Loot)
- [ ] Blueprint classes created
- [ ] Levels created and configured
- [ ] Gameplay tags added
- [ ] Game runs in PIE
- [ ] Quest system working
- [ ] Persistence saving/loading
- [ ] Packaged build tested
- [ ] Git repository initialized

---

## 🚨 Troubleshooting

| Problem | Solution |
|---------|----------|
| **Compile errors** | Check #include paths, rebuild solution in Visual Studio |
| **Missing blueprint class** | Regenerate Visual Studio project, recompile |
| **Character doesn't spawn** | Check BP_GameMode_Default is set as Game Mode Override |
| **Quest won't complete** | Verify DataTable is assigned in AMMOCharacterSpawner |
| **Save file not found** | Check Saved/Profiles/ folder exists and has write permissions |
| **Can't package** | Ensure all dependencies are properly linked |

---

## 📞 Next Steps

1. Review code architecture in `ARCHITECTURE.md`
2. Explore each system's header files for APIs
3. Implement custom systems (UI, audio, VFX)
4. Build content (maps, characters, items)
5. Test multiplayer functionality
6. Optimize performance

---

**Total Setup Time**: ~90 minutes
**Difficulty Level**: Intermediate-Advanced
**Prerequisites**: UE5 experience, C++ knowledge

Good luck, adventurer! 🎮
