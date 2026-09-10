# Kingdoms Ascendant Demo - Level 1-20 Vertical Slice

A production-ready MMORPG vertical slice built in **Unreal Engine 5.8** with complete systems for character creation, inventory management, quest progression, persistence, and combat mechanics.

---

## 📋 Project Overview

**Kingdoms Ascendant** is a Level 1-20 vertical slice demo featuring:

- ✅ **Character Creation** - Guardian (Tank/DPS) and Wizard (Ranged/Hybrid) classes
- ✅ **Dynamic Inventory System** - Equipment slots, item rarity tiers, class requirements
- ✅ **Complete Quest System** - Objectives, progress tracking, rewards
- ✅ **Attribute Scaling** - GAS-based stats that scale from Level 1-20
- ✅ **Persistence** - Save/load player profiles with JSON serialization
- ✅ **Loot Drops** - Weighted loot tables, auto-despawn mechanics
- ✅ **Network Replication** - Server-authoritative gameplay with proper client/server architecture
- ✅ **Targeting System** - Soft/hard lock targeting with line-of-sight validation

---

## 🛠️ System Requirements

### Development Environment
- **Unreal Engine 5.8** (or later)
- **Visual Studio 2022** (C++ Development Tools)
- **Git** (for version control)
- **4GB+ RAM** (recommended 16GB)
- **SSD** (for faster builds)

### Platform Support
- **Windows** (primary development platform)
- **Mac** (supported)
- **Linux** (server builds)

---

## 📦 Installation & Setup

### 1. Clone the Repository

```bash
git clone https://github.com/yourusername/KingdomsAscendant_Demo.git
cd KingdomsAscendant_Demo
```

### 2. Generate Visual Studio Project

```bash
# Windows
./GenerateProjectFiles.bat

# Mac/Linux
./GenerateProjectFiles.sh
```

### 3. Open in Unreal Engine

```bash
# Method 1: Through UE5 Project Launcher
# - Open Epic Games Launcher → Library → Projects
# - Locate KingdomsAscendant_Demo.uproject
# - Click "Open" to launch in UE5.8

# Method 2: Direct File Open
# - Right-click KingdomsAscendant_Demo.uproject
# - Select "Generate Visual Studio project files"
# - Open KingdomsAscendant.sln in Visual Studio
# - Build → Build Solution (Debug or Release)
# - Close Visual Studio and open .uproject file
```

### 4. Compile C++ Code

In **Unreal Editor**:
- Tools → Compile
- Wait for compilation to complete (2-5 minutes)

---

## 🎮 Running the Demo

### Play in Editor (PIE)

1. Open the default level: `Content/Maps/L_StartingZone`
2. Click **Play** (or press `Alt + P`)
3. Character spawns at the coastline

### Standalone Build

```bash
# Package the game
Unreal Editor → File → Package Project → Windows (or target platform)

# Run packaged game
./KingdomsAscendant/Binaries/Win64/KingdomsAscendant.exe
```

### Dedicated Server

```bash
# Cook game content
Unreal Editor → Tools → Cook Content

# Launch server
./KingdomsAscendant/Binaries/Win64/UE4Server-Win64-Shipping.exe -server -log
```

---

## 🎯 Quick Start Gameplay

### Character Creation
1. **Select Class**: Guardian (Tank/Melee) or Wizard (Ranged/Magic)
2. **Select Specialization**:
   - Guardian: Vanguard (Tank) or Berserker (DPS)
   - Wizard: Elementalist (Ranged DPS) or BattleMage (Hybrid)
3. **Spawn**: Character spawns with starter gear (Tier 1 Grey items)

### Quest Progression
1. **Accept Quest**: Quest_01_BeachBoars (Kill 8 enemies)
2. **Trigger Progress**: Kill enemies to update quest objectives
3. **Complete**: Receive XP and Aether rewards
4. **Save**: Profile auto-saves upon quest completion

### Combat
- **Soft Lock**: Look toward enemies (cone-based targeting)
- **Hard Lock**: Press Alt+Right Click to lock to target
- **Target Rotation**: Character automatically faces hard-locked target
- **Line of Sight**: Combat requires clear line of sight (no walls blocking)

### Inventory
- **View Inventory**: Press 'I' (configurable)
- **Equip Items**: Drag items to equipment slots
- **Class Restrictions**: Items can require specific classes
- **Gear Stats**: Armor and damage scale with equipped items

### Save/Load
- **Automatic**: Profile saves when exiting to main menu
- **Manual**: `Persistence/Subsystems/MMOPersistenceSubsystem.h` - Call `SavePlayerProfile()`
- **Load**: `LoadPlayerProfile(PlayerUID, OutProfile)`

---

## 📂 Project Structure

```
KingdomsAscendant_Demo/
├── Source/KingdomsAscendant/
│   ├── Public/
│   │   ├── Characters/          # Player character class
│   │   ├── Player/              # Player state and controller
│   │   ├── Inventory/           # Item and equipment systems
│   │   ├── Quest/               # Quest framework
│   │   ├── AbilitySystem/        # GAS attributes and stats
│   │   ├── Persistence/         # Save/load systems
│   │   ├── Loot/                # Loot drop mechanics
│   │   ├── World/               # Level-specific systems
│   │   └── Combat/              # Combat log and threat
│   │
│   └── Private/
│       ├── Characters/
│       ├── Player/
│       ├── Inventory/
│       ├── Quest/
│       ├── AbilitySystem/
│       ├── Persistence/
│       ├── Loot/
│       ├── World/
│       └── Combat/
│
├── Content/
│   ├── Data/
│   │   ├── DT_Quests_Level1_10.uasset
│   │   ├── DT_Quests_Level11_20.uasset
│   │   ├── DT_Loot_Dungeon.uasset
│   │   └── DT_Equipment_Tier1.uasset
│   │
│   ├── Maps/
│   │   ├── L_StartingZone.umap
│   │   ├── L_Dungeon_Crypt.umap
│   │   └── L_Volcano_Crags.umap
│   │
│   ├── Characters/
│   ├── Items/
│   ├── UI/
│   └── Assets/
│
├── Plugins/
│   └── GameFeatures/
│       └── MMOSystem/
│
├── Saved/
│   └── Profiles/                # Player save data (.json files)
│
├── KingdomsAscendant.uproject
├── README.md
├── .gitignore
└── LICENSE
```

---

## 🔑 Key Classes & Components

| Class | Purpose |
|-------|---------|
| `AMMOCharacter` | Player-controlled character pawn |
| `AMMOPlayerState` | Replicated player identity & level |
| `UMMOInventoryComponent` | Item management & equipment |
| `UMMOQuestComponent` | Quest tracking & progression |
| `UMMOAttributeSet` | GAS attributes (health, mana, damage, armor) |
| `UMMOPersistenceSubsystem` | Save/load player profiles |
| `AMMOLootItem` | Droppable loot with auto-despawn |
| `AMMOCharacterSpawner` | Character creation & spawning |

---

## 📊 Data Tables

All quest and loot data is **data-driven** using UE5 DataTables:

### Quest DataTable Format

| Column | Type | Example |
|--------|------|---------|
| QuestID | Name | Quest_01_BeachBoars |
| QuestTitle | Text | Clear the Coastline |
| ObjectiveType | Enum | KillTarget |
| ObjectiveTag | GameplayTag | Quest.Target.BeachBoar |
| RequiredCount | Int | 8 |
| MinimumLevel | Int | 1 |
| RewardExperience | Int | 1200 |
| RewardAether | Int | 100 |

### Loot Table Format

| Column | Type | Example |
|--------|------|---------|
| ItemID | Int | 301 |
| ItemName | Text | Forged Vanguard Greatshield |
| DropWeight | Float | 15.0 |
| GearTier | Int | 3 |
| ItemRarity | Enum | Uncommon |

---

## 🧪 Testing & Validation

### Compilation Test
```bash
# Full rebuild
File → "Compile" in Unreal Editor
```

### Runtime Test
```bash
# Start PIE
Play (Alt+P)

# Test character spawn
✓ Character appears at L_StartingZone

# Test quest system
✓ Accept Quest_01_BeachBoars
✓ Kill 8 enemies (trigger events)
✓ Quest completes with rewards

# Test persistence
✓ Save profile
✓ Load profile
✓ Stats restored correctly
```

### Network Test
```bash
# Launch server + client
File → Multiplayer Options → Number of Players = 2
Play (Alt+P)
✓ Verify player replication
✓ Verify inventory sync
✓ Verify quest progress sync
```

See `TESTING_CHECKLIST.txt` for complete test suite.

---

## 📝 Configuration

### Gameplay Settings

Edit in Blueprint or C++:

```cpp
// Character targeting
AMMOCharacter::SoftTargetSearchRadius = 800.0f;
AMMOCharacter::MaxConeAngle = 120.0f;

// Quest system
UMMOQuestComponent::MaxActiveQuests = 10;

// Loot
AMMOLootItem::AutoPickupDelay = 300.0f; // 5 minutes
```

### Network Settings

In `DefaultEngine.ini`:

```ini
[/Script/Engine.GameNetworkManager]
MaxClientRate=100000
MaxInternetClientRate=10000

[/Script/Engine.PlayerController]
bEnableNetworkOptimization=True
```

---

## 🐛 Troubleshooting

### Issue: "Missing Module 'KingdomsAscendant'"

**Solution**: Regenerate Visual Studio project files
```bash
./GenerateProjectFiles.bat
```

### Issue: "Compile Errors on Open"

**Solution**: Clean intermediate files
```bash
# Delete these folders
rm -rf Intermediate/
rm -rf Binaries/
rm -rf Saved/

# Regenerate project
./GenerateProjectFiles.bat
```

### Issue: "Character Doesn't Spawn"

**Solution**: Check AMMOCharacterSpawner configuration
- Verify `DefaultCharacterClass` is set in Blueprint
- Verify spawn location is accessible (no collision)
- Check console logs for error messages

### Issue: "Quests Won't Complete"

**Solution**: Verify DataTable is loaded
- Open quest DataTable: `Content/Data/DT_Quests_Level1_10`
- Confirm rows match quest IDs in code
- Verify quest events are being triggered (Server_NotifyGameplayEvent)

---

## 📚 Documentation

- **Architecture**: See `ARCHITECTURE.md` for system design
- **Code Style**: UE5 C++ Coding Standard (Google C++ style guide)
- **Networking**: Server-authoritative with minimal replication
- **Data Pipeline**: JSON serialization for persistence

---

## 🤝 Contributing

This is a **reference implementation** for educational purposes. For modifications:

1. **Branch**: Create feature branch (`git checkout -b feature/quest-improvements`)
2. **Code**: Follow UE5 C++ standards
3. **Test**: Run full test suite before commit
4. **Commit**: Use descriptive messages with [Module] prefix
   - `[Quest] Fix completion detection for multi-objective quests`
   - `[Inventory] Add item sorting by rarity`

---

## 📄 License

This project is provided as **educational reference material**. See `LICENSE` file for terms.

---

## 🎓 Learning Resources

### Unreal Engine 5.8
- [UE5 Documentation](https://docs.unrealengine.com/5.0/)
- [Gameplay Ability System Guide](https://docs.unrealengine.com/5.0/en-US/gameplay-ability-system-in-unreal-engine/)
- [Network Replication](https://docs.unrealengine.com/5.0/en-US/networking-overview-in-unreal-engine/)

### MMORPG Development
- Quest System Patterns
- Server Authority & Client Prediction
- Loot Distribution Mechanics
- Player Progression Systems

---

## 📞 Support

For issues or questions:
1. Check this README
2. Review code comments in relevant systems
3. Check `ARCHITECTURE.md` for design patterns
4. Examine existing test cases

---

## 🎯 Future Enhancements

Potential additions to the vertical slice:

- [ ] Dungeon instancing system
- [ ] PvP battlegrounds
- [ ] Housing/plot management
- [ ] Faction reputation system
- [ ] Raid encounters
- [ ] Seasonal content updates
- [ ] Cross-platform support
- [ ] Anti-cheat systems
- [ ] Cloud save integration

---

**Last Updated**: September 2026
**Engine Version**: Unreal Engine 5.8
**Status**: Production-Ready ✅
