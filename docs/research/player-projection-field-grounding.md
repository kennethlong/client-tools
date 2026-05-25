# Player Projection Field Grounding

**Date:** 2026-05-23  
**Codebases reviewed:**
- SWG Source `D:\Code\swg-main` @ `91f03571ab442a88988ddc77a64f590fae65a239`
- Core3 `D:\Code\Core3` @ `71d4ac284adc2b0e70282425bd92b2c4885190d4`

Grounding for Unified Admin API `Player` projection fields left `OPEN:` in the projection-field-mapping pass.

---

## 1a — SWG Source `lots_used`

**Answer.** `player_objects.num_lots` is **lots used** (account-wide count of housing lots already consumed on this cluster), not lots remaining. It maps to `PlayerObject::m_accountNumLots` / `getAccountNumLots()`.

**Evidence.**

Schema column:

```13:13:D:\Code\swg-main\src\game\server\database\schema\player_objects.tab
	num_lots int,
```

In-memory field declaration and comment:

```508:508:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\object\PlayerObject.h
	Archive::AutoDeltaVariable<int> m_accountNumLots;   ///< Number of lots used by all the characters on this account (on this server cluster only)
```

```813:815:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\object\PlayerObject.h
inline int PlayerObject::getAccountNumLots() const
{
	return m_accountNumLots.get();
```

**Write path (increment on structure add, decrement on remove):**

Structure placement calls `adjustLotCount(+struct_lots)`:

```812:839:D:\Code\swg-main\dsrc\sku.0\sys.server\compiled\game\script\library\player_structure.java
                int struct_lots = (getNumberOfLots(fp_template) / 4) - dataTableGetInt(PLAYER_STRUCTURE_DATATABLE, idx, DATATABLE_COL_LOT_REDUCTION);
                ...
                adjustLotCount(getPlayerObject(player), struct_lots);
```

Structure removal calls `adjustLotCount(-struct_lots)`:

```891:897:D:\Code\swg-main\dsrc\sku.0\sys.server\compiled\game\script\library\player_structure.java
                int struct_lots = (getNumberOfLots(fp_template) / 4) - dataTableGetInt(PLAYER_STRUCTURE_DATATABLE, idx, DATATABLE_COL_LOT_REDUCTION);
                ...
                adjustLotCount(getPlayerObject(player), -struct_lots);
```

C++ implementation explicitly treats sub-zero as invalid and documents “used lots”:

```3722:3726:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\object\PlayerObject.cpp
			m_accountNumLots = m_accountNumLots.get() + adjustment;
			// make sure the number of used lot doesn't go below 0, because
			// it means we would be giving the player extra lots
			if (m_accountNumLots.get() < 0)
				m_accountNumLots = 0;
```

**Read path (player-visible “not enough lots” checks):**

Placement pre-check reads `getAccountNumLots()` as current consumption:

```1164:1178:D:\Code\swg-main\dsrc\sku.0\sys.server\compiled\game\script\library\player_structure.java
            int lots = getAccountNumLots(getPlayerObject(player));
            int lots_needed = (getNumberOfLots(template) / 4) - dataTableGetInt(PLAYER_STRUCTURE_DATATABLE, idx, DATATABLE_COL_LOT_REDUCTION);
            ...
            if ((lots + lots_needed) > MAX_LOTS)
            {
                ...
                sendSystemMessageProse(player, prose.getPackage(new string_id(STF_FILE, "not_enough_lots"), lots_needed));
```

Character sheet / CS stats compute “available” as `getMaxNumberOfLots() - getAccountNumLots()` (used subtracted from a max):

```6127:6134:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\command\CommandCppFuncs.cpp
	int lots = creatureActor->getMaxNumberOfLots();
	...
		int lotsUsed = player->getAccountNumLots();
		lots -= lotsUsed;
```

**Confidence.** Confirmed.

**Notes.**
- Value is **account-scoped**, not per-character, though stored on each `player_objects` row (`num_lots` migrated from `accounts.num_lots`; see `hotfix_134_a.sql`).
- Offline projection: read `player_objects.num_lots` for the character’s `object_id` (same account value on all sibling characters).
- Do **not** interpret as lots remaining.

---

## 1b — SWG Source `lots_max`

**Answer.** Account lot **maximum** is **`ConfigServerGame::getMaxLotsPerAccount()` (default 165) + `accounts.max_lots_adjustment`**. There is no `lots_max` column on `player_objects`. The live adjustment field is `PlayerObject::m_accountMaxLotsAdjustment`, persisted to `accounts.max_lots_adjustment` via `station_id`.

**Evidence.**

Server config default:

```222:222:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\core\ConfigServerGame.cpp
	KEY_INT     (maxLotsPerAccount, 165);
```

```2179:2181:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\core\ConfigServerGame.h
inline const int ConfigServerGame::getMaxLotsPerAccount()
{
	return data->maxLotsPerAccount;
```

Hard cap enforced when incrementing used lots:

```3713:3717:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\object\PlayerObject.cpp
		if (adjustment > 0 &&
			(m_accountNumLots.get() + adjustment) > (ConfigServerGame::getMaxLotsPerAccount() + m_accountMaxLotsAdjustment.get()))
		{
			DEBUG_REPORT_LOG(true,("Account %lu is not allowed to use any more lots.  (Already has %i, limit is %i, tried to use %i more.)\n",
								   m_stationId.get(), m_accountNumLots.get(), ConfigServerGame::getMaxLotsPerAccount() + m_accountMaxLotsAdjustment.get(), adjustment));
```

Per-account adjustment storage:

```510:510:D:\Code\swg-main\src\engine\server\library\serverGame\src\shared\object\PlayerObject.h
	Archive::AutoDeltaVariable<int> m_accountMaxLotsAdjustment;   ///< Extra lots available to this account (or possibly penalty lots taken away)
```

```7:7:D:\Code\swg-main\src\game\server\database\schema\accounts.tab
	max_lots_adjustment int,
```

Loader join (offline read pattern):

```629:632:D:\Code\swg-main\src\game\server\database\packages\loader.plsql
 				t.num_lots,
 				a.is_outcast,
 				a.cheater_level,
 				a.max_lots_adjustment,
```

Persister writes adjustment to `accounts`:

```1460:1467:D:\Code\swg-main\src\game\server\database\packages\persister.plsql
		update accounts
		set
			is_outcast = nvl(p_account_is_outcast(i), is_outcast),
			cheater_level = nvl(p_account_cheater_level(i), cheater_level),
			max_lots_adjustment = nvl(p_account_max_lots_adjustment(i), max_lots_adjustment),
			house_id = nvl(p_house_id(i), house_id)
		where
			station_id = (select station_id from player_objects where object_id = p_object_id(i));
```

**Derivation for projection:**

```
lots_max = ConfigServerGame.getMaxLotsPerAccount() + accounts.max_lots_adjustment
```

Offline SQL (join via `player_objects.station_id`):

```sql
SELECT po.object_id,
       :configured_max_lots_per_account + NVL(a.max_lots_adjustment, 0) AS lots_max
FROM player_objects po
JOIN accounts a ON a.station_id = po.station_id
WHERE po.object_id = :character_object_id;
```

(`:configured_max_lots_per_account` is deployment config; code default is 165.)

**Confidence.** Confirmed for authoritative account cap.

**Notes — related but different constants (do not use for `lots_max`):**
- `CreatureObject::getMaxNumberOfLots()` returns hard-coded `HOUSING_MAX_LOTS = 10` (`CreatureObject.cpp:197`, `:11916-11918`). Used for character-sheet display and over-limit spam, **not** the 165 account cap.
- Java `player_structure.MAX_LOTS = 10` (`player_structure.java:119`) gates script-layer placement messaging/grandfathering; C++ `adjustLotCount` still enforces 165+adjustment.
- Not derived from skills in code reviewed. Adjustment is an account-level admin/CS field.

---

## 1c — Core3 `lots_used`

**Answer.** Core3 has **no persisted “lots used” counter**. Consumption is **derived at runtime** by subtracting the sum of owned structure lot sizes from `maximumLots`, via `PlayerObject::getLotsRemaining()`. Equivalently: **`lots_used = getMaximumLots() - getLotsRemaining()`**, or sum `StructureObject::getLotSize()` over `ownedStructures`.

**Evidence.**

Remaining lots computation:

```3142:3157:D:\Code\Core3\MMOCoreORB\src\server\zone\objects\player\PlayerObjectImplementation.cpp
int PlayerObjectImplementation::getLotsRemaining() {
	Locker locker(asPlayerObject());

	int lotsRemaining = maximumLots;

	for (int i = 0; i < ownedStructures.size(); ++i) {
		auto oid = ownedStructures.get(i);

		Reference<StructureObject*> structure = getZoneServer()->getObject(oid).castTo<StructureObject*>();

		if (structure != nullptr) {
			lotsRemaining = lotsRemaining - structure->getLotSize();
		}
	}

	return lotsRemaining;
}
```

Placement check uses remaining lots:

```452:456:D:\Code\Core3\MMOCoreORB\src\server\zone\managers\structure\StructureManager.cpp
		int lots = serverTemplate->getLotSize();

		if (!ghost->hasLotsRemaining(lots)) {
			StringIdChatParameter param("@player_structure:not_enough_lots");
			param.setDI(lots);
```

**Update path (structure place/remove updates ownership list, not a used counter):**

Place:

```544:547:D:\Code\Core3\MMOCoreORB\src\server\zone\managers\structure\StructureManager.cpp
	ManagedReference<PlayerObject*> ghost = creature->getPlayerObject();
	if (ghost != nullptr) {
		ghost->addOwnedStructure(structureObject);
	}
```

Remove:

```115:116:D:\Code\Core3\MMOCoreORB\src\server\zone\managers\structure\tasks\DestroyStructureTask.h
				PlayerObject* playerObject = cast<PlayerObject*>(ghost.get());
				playerObject->removeOwnedStructure(structureObject);
```

`addOwnedStructure` / `removeOwnedStructure` only mutate `ownedStructures` (`PlayerObject.idl:493-504`).

**Confidence.** Confirmed.

**Notes.**
- SWG Source tracks **account-wide** used lots; Core3 tracks **per-character** ownership in `ownedStructures` against **per-character** `maximumLots`.
- Offline projection requires either:
  1. Deserializing `PlayerObject` from the object database and summing lot sizes for OIDs in `ownedStructures`, or
  2. Joining `ownedStructures` OIDs to persisted `StructureObject` records and summing template lot sizes.
- There is no single SQL column equivalent to SWG Source `num_lots`. Core3 object state lives in Berkeley DB object databases (e.g. `sceneobjects`), not relational lot columns.

---

## 1d — Core3 `lots_max`

**Answer.** **`PlayerObject::getMaximumLots()`** returns the persisted byte field **`maximumLots`**. Default at construction is **10**. It is **per-character**, not account-wide. The only code path found that modifies it is the admin **`/adjustLotCount`** command, which adds to the maximum (despite the name).

**Evidence.**

Field, default, getter, setter:

```113:113:D:\Code\Core3\MMOCoreORB\src\server\zone\objects\player\PlayerObject.idl
	protected byte maximumLots;
```

```387:387:D:\Code\Core3\MMOCoreORB\src\server\zone\objects\player\PlayerObject.idl
		maximumLots = 10;
```

```1746:1752:D:\Code\Core3\MMOCoreORB\src\server\zone\objects\player\PlayerObject.idl
	public void setMaximumLots(byte lots) {
		maximumLots = lots;
	}

	public byte getMaximumLots() {
		return maximumLots;
```

Admin modification:

```51:51:D:\Code\Core3\MMOCoreORB\src\server\zone\objects\creature\commands\AdjustLotCountCommand.h
		ghost->setMaximumLots(ghost->getMaximumLots() + lotCount);
```

No other `setMaximumLots` call sites were found under `MMOCoreORB/src/server/zone`.

**Confidence.** Confirmed for field location and default. **Likely** that 10 is the normal production cap unless an admin has run `/adjustLotCount`.

**Notes.**
- Unlike SWG Source, Core3 does **not** use a `maxLotsPerAccount` config wired to `PlayerObject` in the server code searched (`MMOCoreORB/src/server` has no `maxLotsPerAccount` references).
- `maximumLots` serializes with the `PlayerObject` as part of the creature’s ghost object in the object database (`sceneobjects`); there is no separate relational column.
- `/adjustLotCount` adjusts the **max**, not used count — naming mirrors SWG Source Java API but behavior differs from SWG Source `adjustLotCount` (which adjusts used lots).

---

## 2 — SWG Source persisted `Player.skills`

**Answer.** Offline skill list is stored in **`property_lists`**, with **`list_id = 11`** (`PropertyListBuffer::LI_Skills`), one row per skill. **`value`** is the **plain skill name string** (e.g. skill template name). Live runtime uses `CreatureObject::m_skills` (`std::set<const SkillObject*>`), serialized to/from those strings via `Archive::put/get` on skill name.

**Evidence.**

List ID constant:

```56:56:D:\Code\swg-main\src\game\server\application\SwgDatabaseServer\src\shared\buffers\PropertyListBuffer.h
		LI_Skills=11,
```

Table schema:

```1:7:D:\Code\swg-main\src\game\server\database\schema\property_lists.tab
create table property_lists
(
	object_id number(20), -- BIND_AS(DB::BindableNetworkId)
	list_id int,
	value varchar2(500),
	constraint pk_property_lists primary key (object_id, list_id, value) 
);
```

Historical migration from dedicated `skills` table (confirms semantics):

```15:18:D:\Code\swg-main\src\game\server\database\updates\78.sql
insert into property_lists select object_id,11,sequence_number,skill from skills;
...
drop table skills;
```

(Modern rows use skill **name** in `value`; `sequence_number` from old table is not part of current PK.)

**Save path (live → persisted):**

1. `CreatureObject::m_skills` is an `Archive::AutoDeltaSet<const SkillObject*>` (`CreatureObject.h:914`, `Packager.cpp:126`).
2. On object save, creature encoder writes skills:

```435:436:D:\Code\swg-main\src\game\server\application\SwgDatabaseServer\src\shared\generated\Encoder_cpp.template
		encodeAttributes(objectId,data,9);
		encodePropertyList(objectId,PropertyListBuffer::LI_Skills,data);
```

3. `encodePropertyList` reads string set from property-list buffer and packs AutoDeltaSet (`SwgSnapshot.cpp:431-437`).
4. Skill pointers serialize as **skill names**:

```36:41:D:\Code\swg-main\src\engine\shared\library\sharedSkillSystem\src\shared\SkillObjectArchive.cpp
	void put(ByteStream & target, const SkillObject * const & source)
	{
		if (source)
			Archive::put (target, source->getSkillName ());
		else
			Archive::put (target, std::string ());
```

5. DB flush via batch property-list persister:

```104:106:D:\Code\swg-main\src\game\server\application\SwgDatabaseServer\src\shared\queries\PropertyListQuery.cpp
void PropertyListQuery::getSQL(std::string &sql)
{
	sql=std::string("begin ")+DatabaseProcess::getInstance().getSchemaQualifier()+"persister.update_property_list_batch (:object_id, :list_id, :operation, :value, :chunk_size, :enable_db_logging); end;";
```

**Load path (persisted → live on login):**

1. Loader cursor:

```230:238:D:\Code\swg-main\src\game\server\database\packages\loader.plsql
	function load_property_lists return cursortype
	...
			select /*+ ORDERED USE_NL(T) */
				t.object_id, t.list_id, t.value
			from object_list l, property_lists t
			where t.object_id = l.object_id;
```

2. On creature baseline decode, index 3 hydrates skills:

```558:561:D:\Code\swg-main\src\game\server\application\SwgDatabaseServer\src\shared\generated\Decoder_cpp.template
		case 3:
		{
			decodePropertyList(objectId,PropertyListBuffer::LI_Skills,data,isBaseline);
```

3. `decodePropertyList` inserts/deletes/clears rows in property-list buffer (`SwgSnapshot.cpp:403-426`).
4. Archive unpack resolves name → `SkillObject*`:

```21:31:D:\Code\swg-main\src\engine\shared\library\sharedSkillSystem\src\shared\SkillObjectArchive.cpp
	void get(ReadIterator & source, const SkillObject *& target)
	{
		std::string name;
		Archive::get (source, name);
		...
			target = SkillManager::getInstance ().getSkill (name);
```

5. After load, `CreatureObject::setupSkillData()` rebuilds commands/mods from `m_skills` (`CreatureObject.cpp:2602+`).

**Format.** Plain **skill name strings** (one per row), not CRC-encoded, not comma-separated in a single column. Example query:

```sql
SELECT value AS skill_name
FROM property_lists
WHERE object_id = :creature_object_id
  AND list_id = 11
ORDER BY value;
```

**Confidence.** Confirmed.

**Notes.**
- Persisted `object_id` is the **creature** object id (skills live on `CreatureObject`, not `PlayerObject`).
- Related but separate persisted fields on `player_objects`: `skill_template`, `working_skill`, `skill_title` — these are not the full skill list.
- `property_lists.list_id = 11` is also used by **`decodeComponents`** for integer component IDs stored as decimal strings (`SwgSnapshot.cpp:980-1007`). That is a **different** AutoDeltaSet on other object types; for player creatures, list 11 in offline DB rows is the skill list per migration and `LI_Skills` encoder/decoder path above.
- Legacy `persister.update_skill` / `SkillQuery` still exist but the active creature skill list path is `property_lists` + `LI_Skills`.

---

## Cross-server summary

| Projection field | SWG Source | Core3 |
|---|---|---|
| `lots_used` | `player_objects.num_lots` (= `getAccountNumLots()`) | Derived: `getMaximumLots() - getLotsRemaining()`; no dedicated column |
| `lots_max` | `ConfigServerGame.getMaxLotsPerAccount()` + `accounts.max_lots_adjustment` | `PlayerObject.maximumLots` via `getMaximumLots()` (default 10) |
| `skills` (SWG only) | `property_lists` where `list_id=11`, `value`=skill name | *(not in scope)* |
