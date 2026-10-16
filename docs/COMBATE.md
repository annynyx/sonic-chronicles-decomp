# Combat from the inside

How Sonic Chronicles decides who acts first, who hits, how much damage comes out, and how POW
moves, status effects, items and Chao enter the calculation. Everything here came from two
sources:

- **the ARM9 code** (Ghidra pseudo-C; small functions run in isolation in
  [Unicorn](https://www.unicorn-engine.org/) to check what they do);
- **the game's tables** (GDA and the `.ITM`/`.SPL` effect files), read with
  `sonic-mod unpack`.

> **No game data here beyond what is needed to explain.** The complete tables (every move,
> effect, item and creature) are generated from your copy:
> ```
> sonic-mod unpack rom.nds my_mod
> python3 analise/tools/combate_tabelas.py my_mod      # writes my_mod/combate/*.md
> ```

## How to read this document

Each statement has a confidence level:

| Level | Means |
|---|---|
| **High** | I read the code and it matches the data (and, when possible, I ran the function in Unicorn) |
| **Medium** | I read the code, but one step of the interpretation is a deduction (the name of a field, the unit of a value) |
| **Low** | deduced from the data, the names or the game's texts; the code hasn't been read yet |

Addresses are from the ARM9 of the US version (`YWSE`). "Rule N" is row N of the
`CombatRules` table (section 15). `1dN` is an N-sided die (`Rules_RollDice`, 0x02008550). The
function names are in [`analise/symbols_manual.txt`](../analise/symbols_manual.txt).

## Contents
1. [A battle, from start to end](#1-a-battle-from-start-to-end)
2. [Attributes](#2-attributes)
3. [Turn order (initiative)](#3-turn-order-initiative)
4. [Ambush](#4-ambush)
5. [Basic attack](#5-basic-attack)
6. [Applying damage, elements and resistances](#6-applying-damage-elements-and-resistances)
7. [Defend](#7-defend)
8. [POW moves](#8-pow-moves)
9. [The effect system (.SPL and .ITM)](#9-the-effect-system-spl-and-itm)
10. [Status effects (buffs and debuffs)](#10-status-effects-buffs-and-debuffs)
11. [Special abilities](#11-special-abilities)
12. [Items](#12-items)
13. [Enemies](#13-enemies)
14. [Dynamic difficulty](#14-dynamic-difficulty)
15. [CombatRules: the 74 rules](#15-combatrules-the-74-rules)
16. [What we don't know yet](#16-what-we-dont-know-yet)
17. [How to check](#17-how-to-check)

---

## 1. A battle, from start to end

```
encounter (exploration) ─► GameModeCombat vfunc02 (0x02039af4)
   ├─ adjusts the dynamic difficulty level ............... section 14
   ├─ decides the ambush (AmbushValue, Chao) .............. section 4
   └─ round loop
        ├─ Combat_BuildTurnQueue (0x020448c0): queue by initiative ... section 3
        ├─ each action: Attack / Defend / POW / Item / Flee
        │     ├─ Combat_ResolveAttack (0x0200f804) → hit ............. section 5
        │     ├─ Combat_BasicAttackDamage / Combat_PowDamage ......... sections 5 and 8
        │     └─ Combat_ApplyDamage (0x02011ac4) + resistances ....... section 6
        └─ per-round effects (poison, regeneration, PP) .............. sections 9–11
end: XP and items (squads, rewards, RandItems); difficulty adjustment
```

## 2. Attributes

Attributes are numbers in a per-creature table. The code doesn't use the column directly: it
calls `StatsRedirect_Map` (0x020083d0), which converts **logical attribute** *n* into column
*n + 1* of `StatRedirect.gda`, and then `Stats_GetInt` (0x02007ab0), whose third argument
selects the **base** value (1) or the **current** one, with bonuses (0). **Confidence: high.**

| No. | Attribute | What it is for |
|---|---|---|
| 0 | HP | current health |
| 21 | Speed | initiative (section 3), fleeing and chasing |
| 22 | Attack | chance to hit |
| 23 | Defense | chance to be missed |
| 24 | max HP | |
| 25 | Power | damage ("damage rating"; the game doesn't show it) |
| 26 | Grit | armor: subtracted from damage (the game doesn't show it) |
| 27 | Luck | critical hits, ambush |
| 28 | PP | "Fatigue" in the tables: the points for POW moves |
| 36 | max PP | |
| 39–54 | status marks | attribute 39 + status ID (section 10) |
| 70 | Class | 0 and 4 = Power, 1 and 3 = Support, 2 = Shifter (`classes.gda`) |
| 75–80 | Fire, Water, Earth, Wind, Lightning, Ice resistance | in %, section 6 |
| 90–121 | elemental damage | > 0 turns the element on in the attack; 107–112 = the 6 elements |
| 122 | POW cost discount | Economizer item |
| 123 | actions per round | `NumActions` in `creatures.gda` |

The starting values are in `creatures.gda` (columns `HitPoints`, `Speed`, `Attack`,
`Defense`, `Health`, `Power`, `Grit`, `Luck`, `Fatigue`, `MaxFatigue`, `NumActions`) and grow
along the `Adv_<character>.gda` curves (30 rows, one per level: the cumulative `XP` to reach
the level, and the attributes). Example, Sonic: level 1 has Speed 7, Attack 8, Defense 14, HP
33, Power 8, Grit 2, Luck 3, PP 7; level 30 has 65, 64, 57, 570, 57, 31, 38, 22. The
`PlayerChoice` and `Combat` columns of the curves don't have a confirmed meaning yet
(**low**: they look like points to distribute and POW points).

## 3. Turn order (initiative)

`Combat_BuildTurnQueue` (0x020448c0). **Confidence: high.**

At the start of each round, each living combatant (up to 4 per side) gets **one queue entry
per action** it has (attribute 123, actions per round). The "time" of each entry is:

```
1st action:  t₁ = max(0, R1 − Speed × R3) + 1dR2
action n+1:  tₙ₊₁ = tₙ + max(R1 / R33, R1 − Speed × R3) + 1dR2
             R1 = rule 1 = 60   R2 = rule 2 = 2   R3 = rule 3 = 1   R33 = rule 33 = 4
```

The queue is sorted by time, smallest to largest (`ActionQueue_InsertByTime`, 0x0204516c).
Since the player's party is inserted first and a tie goes in **after** the equal ones, **the
party wins ties**.

What this means in practice:

- Low Speed spaces the actions out: Sonic at level 1 (Speed 7, 3 actions) acts at ~54, ~107,
  ~160.
- The gap between actions is never smaller than 60/4 = 15: with Speed ≥ 45 all actions are
  15 apart, and a fast character with several actions acts several times before a slow one.
- In the ambush round (section 4), only the side that ambushed acts, and with **at most 1
  action**.

## 4. Ambush

In `GameModeCombat` vfunc02 (0x02039af4). **Confidence: medium** (the formula is high; where
the player party's value comes from hasn't been found yet).

Each squad has an ambush value (`vfunc 0x12c`). For enemies it comes from the `AmbushValue`
column of `squads.gda`, which can also force the result:

| AmbushValue | Result |
|---|---|
| −1 | nobody ambushes (6 squads) |
| −2 | the player ambushes |
| −3 | the enemies ambush (3 squads) |
| other | the roll below |

The roll: the side with the higher value (*a*) has a chance to ambush the other (*b*):

```
a' = a × (100 + ambush bonus of side a) / 100 × (100 − protection of side b) / 100
ambushes if  rand(0..R47) ≤ a' − b        R47 = rule 47 = 10
```

The bonus and the protection are special abilities 21 and 22 (section 11), summed over the
squad's members (`Squad_SumAbility`, 0x02033df4); they come from the "Ambush" Chao. The
game's journal says Luck also influences the ambush; that must be in the party's value *a*,
which I haven't traced yet.

## 5. Basic attack

### 5.1 Hitting
`Combat_ResolveAttack` (0x0200f804) and `Combat_RollToHit` (0x02013740). **High.**

```
hits if  Defense_target ≤ Attack_attacker + 1d20
```
(with no target set, it uses Defense = Attack + 10.) Before the roll, the attack **always
misses** if the target has:

- **Phased** (ability 25): not even "Can't miss" attacks hit;
- **Agile** (ability 26), unless the attack is **Inescapable** ("Can't miss").

An Inescapable attack also ignores a failed hit roll. When the attack misses and the target
has the **counter-attack** ability (2), it strikes back (text "Counter").

### 5.2 Damage
`Combat_BasicAttackDamage` (0x0200fcdc). **High.**

```
P = attacker's Power       A = target's Grit
k = 110 (rule 44) for the player's party, 60 (rule 45) for enemies

normal:    damage = 0.9 × P  +  k/100 × (3dP / 3)  −  A
critical:  damage = P × (90 + k) / 100  −  A          if 1d100 < attacker's Luck
```

- The `3dP / 3` term is the average of three *P*-sided dice: it ranges from 1 to *P*, with a
  mean ≈ (P + 1)/2. On average, a party attack does ≈ **1.45 P − A** and an enemy's
  ≈ **1.2 P − A**. The critical uses the maximum: **2 P − A** for the party, **1.5 P − A**
  for enemies.
- **Armor Piercing**: if A < 500, A = 0.
- Party attacks only: if 0 < damage < P/2, add P/4 (a floor for weak attacks).
- **Minimum damage 1.**
- **Instant KO**: if the attacker's sum of ability 1 ≥ 1d100, the damage becomes the target's
  whole HP (text "KO"). It comes from the "instant KO" Chao (5%, 7%, 10%).
- **Blast**: the target's neighbors take `(damage − (A_neighbor − A_target)) / 2`, minimum 1.
- **Leech**: the attacker recovers health (section 6.1).
- **Status on hit** (ability 13): up to 4 effects stored in the value (one per byte); one is
  drawn and applied.

Enemies also get the dynamic difficulty adjustments to Attack, Defense, Power and Grit
(section 14).

## 6. Applying damage, elements and resistances

### 6.1 After the formula
`Combat_ApplyDamage` (0x02011ac4). **High**, except where noted.

1. **Target's reduction** (ability 14, sum in %): `damage × (1 − sum/100)`, rounded. If the
   target has this ability and living allies, the remaining damage is **split evenly**
   between it and the allies (each ally takes `damage / (allies + 1)`; the target takes the
   rest). It matches the ability's description: "takes less damage ... and spreads the
   remaining damage evenly among other team members".
2. **Leech**: the attacker recovers `% × the attacker's max HP` (that is what the code does:
   the base is the max HP, not the damage dealt).
3. **PP drain** (target's ability 28): if the attacker is in the player's party, it loses
   `sum% × its max PP` (at least 1).
4. The damage becomes an "HP − damage" **effect** (EffectId 1, section 9) with the attack's
   element mask. Resistance is applied to that effect.

### 6.2 Elements
The six elements are Fire, Water, Earth, Wind, Lightning and Ice. **High.**

- **Attack**: the code builds a 32-bit mask from attributes 90–121 greater than zero (bit *i*
  = attribute 90 + *i*). The six elements use bits 17–22 (attributes 107–112). A character
  gains an element by equipping a ring or a Chao (+1 to the attribute); a POW brings it in
  the `ElementalDamage` column (e.g. `1048576` = bit 20 = Wind).
- **Defense** (`Effect_ElementResistance`, 0x0200e4d0): for each bit of the mask, it reads
  the matching resistance (bits 0–15 → attributes 3–18; bits 16–31 → attributes 74–89; the
  elements fall in 75–80) and computes the **average** of the resistances involved:

```
R = average(resistances) / 100
final damage = damage × (1 − R)        (EffectFn_ModifyAttributeResist, 0x020065bc)
```

A **negative** resistance is a **weakness**: −75 multiplies the damage by 1.75; +25 by 0.75.
The interface marks the hit as weak/resisted by the sign (field `+0x159`: 1, 2 or 3). For
other effect types, there is a mode in which R is the **chance to resist entirely**
(`Effect_ApplyResistance`, 0x0200e640).

### 6.3 Enemy types
Enemy resistances come from the permanent `Spell_Int_*` effects (spells 121–129, 148–149).
The values, in %:

| Type | Fire | Water | Earth | Wind | Lightning | Ice |
|---|---|---|---|---|---|---|
| Flying robot | +25 | **−75** | +10 | −25 | +10 | +25 |
| Ground robot | +25 | **−75** | −25 | +10 | +10 | +25 |
| Flying animal | −25 | +50 | +25 | −10 | −10 | −10 |
| Ground animal | −25 | +50 | −10 | +25 | −10 | −10 |
| Kron | +25 | −25 | **+75** | −25 | −10 | −10 |
| N'rrgal | −25 | +50 | −10 | −10 | +50 | −25 |
| Zoah | −10 | −10 | −25 | +25 | +50 | +10 |
| Voxai | −25 | +10 | −25 | +50 | −10 | +25 |

(The game's table says "Wind attacks are more deadly to flying enemies"; in the data, Wind is
−25 against flying robots and −10 against flying animals, but +10/+25 against the ground
ones.)

## 7. Defend

`GameAction_Defend` vfunc04 (0x02073590). **High** for the arithmetic; **medium** for the
duration (the code stores how much it added, to undo it later, but I haven't read where it
undoes it).

```
Defense += Defense × R7 / 10      R7 = rule 7 = 7   → +70%
Grit    += Grit × R8 / 10         R8 = rule 8 = 20  → +200% (Grit triples)
if rule 11 > 0 and PP < max PP:   recovers PP
```

How much PP comes back depends on the **class** (attribute 70), for party characters:

| Class | Characters | PP per Defend |
|---|---|---|
| 1 and 3 (Support) | Tails, Big, Cream | 3 |
| 2 (Shifter) | Amy, Rouge, Eggman | 2 |
| 0 and 4 (Power) and enemies | Sonic, Knuckles, Omega, Shadow | 1 |

The game's journal confirms: "You can regain PP by defending".

## 8. POW moves

The moves are in `combo.gda` (155 rows; the first 60 belong to the player's party). The link
from each character to its moves is in `creatures.gda` (`Combo1`…`Combo10`). The owner and
the partners are creature IDs; creature 10 is an empty slot the moves use for **Shade**
(member 10 of `party.gda`, whose real creature is 27).

### 8.1 `combo.gda` columns
Several columns don't have their original name (only the CRC32); the meaning below comes from
the code (`Combat_PowDamage`, 0x02010810, which reads the columns by position) and the data.

| Position | Column | Meaning | Confidence |
|---|---|---|---|
| 1 | `col_a66f4be0` | number of participants (1–4) | medium |
| 2 | `col_64834397` | owner (creature ID; 10 = Shade) | high (matches the names) |
| 3–5 | `col_4fae1054`, `col_56b52115`, `col_19f4b7d2` | partners in a team move | high |
| 11 | `Cost` | PP cost | high |
| 12–14 | `Damage1`…`Damage3` | damage in % per move level | high |
| 24–26 | `RewardItem1`…`3` | stolen items (Plunder), two IDs per value | high |
| 27–29 | `Spell1`…`Spell3` | effect applied per level (row of `spells.gda`) | high |
| 30–32 | `col_a9e4c4fc`, `GUITypeAggressive`, `col_9bd2a67e` | effect chance per level (0.3 = 30%) | high |
| 45 | `ArmorPiercing` | ignores Grit | high |
| 46 | `col_8b96e6f9` | instant KO chance (0.5 = 50%) | high |
| 47 | `Inescapable` | "Can't miss" | high |
| 48 | `Blast` | hits the neighbors | high |
| 49 | `Scatter` | "Full Auto": many small hits | medium |
| 50 | `Leech` | % of health returned | high |
| 51 | `ElementalDamage` | element mask (section 6.2) | high |
| 52 | `col_46eddb98` | hits the whole side | high |
| 54 | `col_6f47ed9a` | written to `GameModeCombat+0x7c8` (probably the minigame's number of targets) | low |
| 55 | `EarlyFailure` | the minigame can fail at the start | low |

`GUITypeAggressive` is the name xoreos' dictionary gives to that CRC32, but the content (0.5
in the level 2 chances) shows it is the status chance: the name is a collision or was reused.

### 8.2 POW damage
`Combat_PowDamage` (0x02010810). **High.**

```
damage = P × Damage%/100 × (90 + k/2)/100 × (1 ± performance) − A
         k = 110 (party) or 60 (enemy)   →   factor 1.45 (party) or 1.2 (enemy)
```

- **Performance** is the result of the touch minigame (0 to 1). In a player move it **adds**
  (`1 + performance`); in an enemy move, the performance is the player's defense and it
  **subtracts** (`1 − performance`).
- An enemy move against a perfect defense (performance 1.0) **misses**, unless it is
  Inescapable.
- Minimum damage 1 (0 if `Damage%` is 0, as in support moves).
- Compared with section 5: a POW with `Damage%` = 100 does the same as an average basic
  attack, before the minigame bonus.

**The move level** (1, 2 or 3) picks the damage column, the effect and the chance.
`GameAction_Combo` vfunc04 (0x020702d8) also picks rules 71/72/73 (0.90 / 0.95 / 1.00) by
level and passes them to the minigame's clock (**medium**: it looks like the minigame's time
scale; level 1 would be slower).

**POW status**: a player move only applies the effect with a **perfect minigame and**
`roll < chance`; an enemy move applies it if the defense was **not** perfect. **High.** A
negative chance (−1) is never reached: Echidna Rush has effects registered that are never
applied.

**Cost**: attribute 122 (Economizer) reduces the cost (**medium**: I read the use, not the
arithmetic).

### 8.3 The party's moves
Damage in % per level; "effect" = status (level) and chance per move level.

| Move | Owner | Partners | PP | Damage % | Effect | Flags |
|---|---|---|---|---|---|---|
| Axe Kick | Sonic | | 4 | 75/88/100 | | |
| Whirlwind | Sonic | | 6 | 50/55/63 | | Inescapable, all, Wind |
| Blue Bomber | Sonic | Tails | 4 | 100/113/125 | Weakened 1/2/3 (30/50/80%) | |
| Fastball | Sonic | Amy | 5 | 88/100/113 | Sluggish 1/2/3 (30/50/80%) | |
| Triple Tornado | Sonic | Knuckles, Tails | 5 | 75/88/113 | Sluggish 1/2/3 (30/50/80%) | Inescapable, all, Wind |
| Hail Storm | Sonic | Knuckles, Tails, Amy | 6 | 88/100/113 | Stunned (20/30/40%) | |
| Uppercut | Knuckles | | 4 | 68/75/88 | | Armor Piercing |
| Quake Punch | Knuckles | | 6 | 33/40/50 | Stunned (30/40/50%) | Inescapable, all, Earth |
| Revolver Slam | Knuckles | Sonic | 4 | 100/113/125 | Vulnerable 1/2/3 (30/50/80%) | Armor Piercing |
| Knuckles Express | Knuckles | Shadow | 5 | 75/113/125 | Distracted 1/2/3 (20/30/40%) | all |
| Knuckles Sandwich | Knuckles | Sonic, Amy | 5 | 63/75/88 | Stunned (30/50/80%) | |
| Hard Line | Knuckles | Shadow, Shade | 5 | 150/163/175 | Distracted 1/2/3 (50/60/70%) | |
| Chaos Spear | Shadow | | 4 | 50/63/75 | | Armor Piercing, Scatter |
| Chaos Rift | Shadow | | 8 | 0 | Distracted 1 (40/50/60%) | KO 50% |
| Chaos Blast | Shadow | | 6 | 75/80/85 | Weakened 1 (20/30/40%) | Blast |
| Atomic Strike | Shadow | Sonic | 4 | 100/113/125 | | Inescapable, all, Lightning |
| Focus Field | Shadow | Omega | 4 | 113/125/138 | Vulnerable 1 (30/40/50%) | |
| Metal Storm | Shadow | Rouge, Omega | 5 | 150/163/175 | Stunned (20/30/40%) | |
| Scan | Tails | | 4 | 0 | Grit −25/−50/−75% (always) | |
| Tinker | Tails | | 4 | 0 | Tinkered 1/2/3 (always) | |
| Medi Bot | Tails | | 5 | 0 | heals for 3 rounds (always) | |
| Shield Bot | Tails | | 5 | 0 | Fortified 1/2/3 (always) | |
| Flash Bang | Tails | | 5 | 0 | Sluggish + Cursed 1/2/3 (always) | |
| Adrenaline Rush | Tails | | 5 | 0 | Hyper 1/2/3 and +1 action (always) | |
| Refresh | Cream | | 8 | 0 | PP +5/+10/+15 on everyone | |
| Demoralize | Cream | | 5 | 0 | Distracted 1/2/3 on all enemies | |
| Cure | Cream | | 4 | 0 | removes everyone's debuffs | |
| Revive | Cream | | 5 | 0 | revives with 1 HP / 50% / 100% | |
| Heal | Cream | | 6 | 0 | HP +50/+100/+150 on everyone | |
| Tough | Cream | | 5 | 0 | Fortified 1/2/3 | |
| Lure Whip | Big | | 4 | 25/38/50 | Stunned (60/70/80%) | |
| Battering Ram | Big | | 5 | 70/80/90 | Distracted 1 (30/50/80%) | |
| Taunt | Big | | 6 | 0 | draws the attacks + Fortified 1/2/3 | |
| Froggy Poison | Big | | 5 | 25/38/50 | Poisoned (always) | |
| Feel No Pain | Big | | 5 | 0 | regenerates 30/40/50% of HP per round | |
| Froggy Rain | Big | | 6 | 60/70/80 | | Inescapable, all |
| Flamethrower | Omega | | 4 | 50/65/75 | | all, Fire |
| Beam Cannon | Omega | | 5 | 125/138/150 | Vulnerable 1 (50/70/100%) | Armor Piercing |
| Blazing Tornado | Omega | Rouge | 4 | 100/113/125 | | Blast, Fire |
| Wrecking Ball | Omega | Big | 5 | 100/113/125 | Distracted 1 (30/50/80%) | Blast |
| Temporal Field | Omega | | 5 | 0 | Hyper + Phased for 2/3/4 rounds | |
| Machine Gunner | Omega | | 6 | 13/25/38 | | Scatter |
| Tornado Kick | Rouge | | 4 | 63/68/73 | | Wind |
| Jewel Storm | Rouge | | 5 | 38/50/63 | | Armor Piercing, all |
| Rising Knuckle | Rouge | Knuckles | 5 | 88/100/113 | Stunned (20/30/50%) | |
| Plunder | Rouge | | 4 | 0 | steals items (section 12.5) | |
| Distract | Rouge | | 4 | 0 | Distracted 1/2/3 | |
| Shriek | Rouge | | 4 | 0 | Sluggish 1/2/3 | |
| Low Blow | Amy | | 4 | 63/75/88 | Vulnerable 1 (30/50/80%) | |
| Spin Cycle | Amy | Cream | 4 | 75/88/100 | | Inescapable, Blast |
| Tantrum | Amy | | 5 | 50/63/75 | Sluggish 1 (20/30/40%) | |
| Blow Kiss | Amy | | 4 | 0 | Empowered 1/2/3 on an ally | |
| Tarot Draw | Amy | | 4 | 0 | Cursed 1/2/3 | |
| Flower Power | Amy | Big, Cream | 5 | 150/163/175 | Stunned (40/50/60%) | |
| Cloak | Shade | | 4 | 0 | Hyper 1/2/3 on herself | |
| Blade Rush | Shade | | 5 | 75/88/100 | | Leech 10% |
| Echidna Rush | Shade | Knuckles | 5 | 100/113/125 | Distracted (chance −1: never) | Armor Piercing |
| Blade Drop | Shade | Sonic | 5 | 100/113/125 | | Armor Piercing, Leech 15% |
| Bombardment | Eggman | | 5 | 100/113/125 | Sluggish 1 (50%) | Inescapable, all |
| Sabotage | Eggman | Tails | 6 | 0 | HP −100% (80/90/100%) | |

In support moves (damage 0), the effect is always applied (chance 1.0). Moves 60 and up
belong to enemies: a single level, cost 0, damage up to 400%.

## 9. The effect system (.SPL and .ITM)

Moves, items, status effects and abilities use the same mechanism: an **effect file** in 2DA
text (`.SPL` for those in `spells.gda`, `.ITM` for those in `Items.gda`, column `BaseItem1`).
Each row has `label, ID, EffectId, Data, SData1, SData2, SData3, Pulse`.

### 9.1 Header
- **`SpellData`**: that row's `EffectId` is the **target**. **High** (matches every item).

  | Value | Target |
  |---|---|
  | 0 | one enemy |
  | 1 | one ally |
  | 2 | all enemies |
  | 3 | all allies |
  | 5 | the user |
  | 6 | a knocked-out ally |

- **`CollectionData`** (in items, `CollectionData_Equip` and `CollectionData_Use`): `Data` =
  **duration**, `SData1` = **category**, `SData2` = time.

  | Data | Duration | Confidence |
  |---|---|---|
  | 0 | instant | high |
  | 1 | permanent | high |
  | 2 | while equipped | medium |
  | 3 | temporary, `SData2` thousandths of a round (3000 = 3 rounds) | medium |

  The **category** is a mask: bit *n* = status *n* − 1 for debuffs (Poisoned = 8, Weakened =
  16, Vulnerable = 32, Distracted = 64, Sluggish = 128, Cursed = 256, Stunned = 512, Tinkered
  = 4) and bit = status for buffs (Empowered = 1024, Fortified = 2048, Focused = 4096, Hyper =
  8192, Lucky = 16384). It is what "remove status" looks for. **High** (Iron Tonic removes 32
  = Vulnerable, Rock Salt 16 = Weakened, Prune Juice 64 = Distracted, Psychic Water 128 =
  Sluggish, the pairs in the game's journal).

### 9.2 Effect types (EffectId)
The code has a table with 11 types (`CGameEffectFunctions`, 0x02005cec), each with apply,
remove, repeat and resist functions.

| EffectId | What it does | Fields | Confidence |
|---|---|---|---|
| 0 | collection data (header) | | high |
| 1 | changes an attribute | `Data` = amount, `SData1` = attribute, `SData2` = flags, `Pulse` 1000 = repeats every round | high |
| 5 | visual effect | `Data` = VFX | high |
| 6 | revive | | high |
| 7 | removes status | `Data` = category mask (1022 = all debuffs; 131070 = all) | high |
| 8 | draws the attacks (Taunt) | | medium |
| 9 | applies another effect | `Data` = row of `spells.gda` | high |
| 10 | grants a special ability | `Data` = code (section 11) | medium |

### 9.3 The flags of EffectId 1
`EffectFn_ModifyAttribute` (0x02005fcc). **High.**

```
bit 0      : 0 = add to the current value;  1 = set (replace)
bits 1–2   : base of the amount
             0 = absolute value
             1 = % of the attribute's own BASE value
             2 = % of max HP
             3 = % of max PP
```

| SData2 | Reads as | Example |
|---|---|---|
| 0 | absolute add | Weakened: Power −10 |
| 2 | add % of the base value | Scan: Grit −25% |
| 3 | set to % of the base value | Heal1: Attack, Defense, Power, Grit, Luck = 100% of base (undoes debuffs) |
| 4 | add % of max HP | Poisoned: HP −10% of max per round |
| 5 | set to % of max HP | Revival Ring: HP = 10% of max |

## 10. Status effects (buffs and debuffs)

`Statuses.gda` has 16 statuses. The `CanAct` = 0 column prevents acting; `Priority` orders the
icons. An active status is attribute `39 + ID` > 0, and the effects that cause it add +1 to
it. The internal names (`ATKdown`…) differ from the names in the game's journal; the values
are from `Spell_<Status>.spl`, `_2` and `_3`. **High.**

| ID | Internal | In-game name | Effect (level 1 / 2 / 3) | Duration | Cure |
|---|---|---|---|---|---|
| 0 | Dead | K.O. | can't act | | Revival Ring, Ring of Life, Revive |
| 1 | Tinkered | (Tinker) | Power and Defense −5 / −10 / −15 | 3 | Antidote |
| 2 | Poisoned | Poisoned | HP −10% of max per round | 3 | Antidote |
| 3 | ATKdown | Weakened | Power −10 / −15 / −20 | 3 | Rock Salt, Antidote |
| 4 | DEFdown | Vulnerable | Grit −5 / −10 / −15 | 3 | Iron Tonic, Antidote |
| 5 | ACCdown | Distracted | Attack −10 / −15 / −20 | 3 | Prune Juice, Antidote |
| 6 | EVAdown | Sluggish | Defense −10 / −15 / −20 | 3 | Psychic Water, Antidote |
| 7 | LCKdown | Cursed | Luck and Attack −10 / −15 / −20 | 3 | Antidote (the journal cites Clover Juice) |
| 8 | Paralyzed | Stunned | **can't act**; Defense −20 | 2 | Antidote |
| 9 | (no name) | ? | given by the Crazy Beans item | | |
| 10 | ATKup | Empowered | Power +10 / +15 / +20 | 3 | |
| 11 | DEFup | Fortified | Grit +10 / +15 / +20 | 3 | |
| 12 | ACCup | Focused | Attack +10 / +15 | 3 | |
| 13 | EVAup | Hyper | Speed and Defense +10 / +15 / +20 | 3 | |
| 14 | LCKup | Lucky | Luck +10 / +15 / +20 | 3 | |
| 15 | (no name) | Bug Spray | Grit −10, Defense −10, Speed −6, Power −3 | | |

The numbers are absolute (not %): at level 1 a character has Power ~10 and at 30 ~60, so the
same Weakened weighs much more at the start of the game. Detail: the internal names are
"swapped" relative to what the game shows because, in the code, **Attack** is accuracy (the
"ACC") and **Power** is damage (the "ATK").

## 11. Special abilities

Effects that are not numbers in an attribute: the code asks "how much of this ability does
the creature have?" (`vfunc 0xbc`, summing every active effect of that type). They come from
two columns of `Items.gda` (`col_ab33ab1a` = code, `col_dee49bb0` = value; and
`col_801ef8d9`/`col_f5c9c873` for a second one) and from EffectId 10. The meanings come from
the item descriptions (Chao and equipment) and, where marked, from the code. **Medium**,
unless noted.

| Code | Ability | Value | In the code |
|---|---|---|---|
| 0 | POW always perfect (Chao 38) | | |
| 1 | instant KO on the basic attack (Chao 40) | % | **high**: sum ≥ 1d100 |
| 2 | counter-attack (Chao 213) | | **high**: strikes back when missed |
| 3 | revives once per battle (Angel Amulet, Chao 39) | % of HP | |
| 4 | avoided by enemies, unless it is the last one (Chao 214) | | |
| 5 | lures an enemy (Chao 35) | no. of enemies | |
| 6 | dodges some basic attacks per round (Chao 215) | | |
| 7 | regenerates the team's HP at the start of the round (Chao 21) | % | |
| 9 | regenerates HP (Replenisher, Chao 1) | % | |
| 10 | recovers PP at the end of the round (Refresher) | | |
| 11 | team luck (Cheese) | | |
| 12 | extra XP (Chao 37) | % | |
| 13 | random status on hit | up to 4 effects, 1 per byte | **high** |
| 14 | reduces damage taken and shares it with the team | % | **high** (section 6.1) |
| 15 | fleeing/chasing (Nrrgal Module, Chao 33) | | |
| 18 | enemies flee more (Spooky Charm, Chao 29) | | |
| 19 | rare Chao in the eggs (Chao 34) | | |
| 21 | chance to ambush (Chao 30) | % | **high** (section 4) |
| 22 | reduces the chance of being ambushed (Chao 31) | % | **high** (section 4) |
| 23 | extra item in the reward (Chao 32) | 2 item IDs | |
| 25 | **Phased**: immune to damage | | **high** (section 5.1) |
| 26 | **Agile** ("Evading") | | **high** (section 5.1) |
| 27 | self-repair (enemies) | % | |
| 28 | drains PP from attackers (enemies) | % | **high** (section 6.1) |
| 29 | regenerates the team's PP (Chao 22) | | |
| 30 | regenerates PP (Chao 2) | | |
| 31 | immunity (enemies) | | |

Items 211–230 and 277–285 don't appear in the inventory: they are the enemies' abilities,
equipped on them.

## 12. Items

`Items.gda` (288 rows) + one `.ITM` per item. **High**, unless noted.

### 12.1 Types and slots

| Type | Kind | EquipSlot |
|---|---|---|
| 0 | consumable (`SubType` 0 HP, 1 PP, 2 revive, 3 cure status, 4 special) | −1 |
| 1 | equipment | 0 = feet, 1 = hands, 2 = accessory |
| 2 | story item (emeralds, quest objects) | −1 |
| 3 | Chao | 4 |

**Who can equip** is the `AllowEquip` mask: bit *i* = character *i* of `creatures.gda` (0
Sonic, 1 Knuckles, 2 Tails, 3 Amy, 4 Shadow, 5 Rouge, 6 Big, 7 Cream, 8 Omega, 9 Eggman).
Examples: `21` (bits 0, 2, 4) = sneakers for Sonic, Tails and Shadow; `200` = shoes for Amy,
Big and Cream; `256` = Omega only (hydraulics and claws); `136` = dresses for Amy and Cream.
Bit 27, present in several values, has no meaning yet (**low**).

### 12.2 Consumables

| Item | Target | Effect |
|---|---|---|
| Health Seed / Leaf / Root | ally | HP +50 / +100 / +250 |
| Med Emitter | all allies | HP +250 |
| POW Candy / Gum / Drink | ally | PP +5 / +10 / +50 |
| Refresher Wave | all allies | PP +50 |
| Revival Ring / Ring of Life | knocked out | revives with 10% / 100% of HP and clears the statuses |
| Antidote / Cure All Spray / Immunity Booster | ally / all | removes the debuffs (mask 1022) |
| Iron Tonic, Rock Salt, Prune Juice, Psychic Water | ally | buff +10 and removes the opposite debuff |
| Clover Juice | ally | Lucky (+10 Luck); the journal says it cures Cursed, but the `.ITM` has no removal row |
| Speed Bar | ally | Speed +10 |
| Bug Spray | all enemies | status 15: Grit −10, Defense −10, Speed −6, Power −3 |

### 12.3 Equipment
The bonuses are small and absolute (+1 to +6), always "while equipped":

- **Feet** (shoes, sneakers, boots, hydraulics): Defense and Grit, sometimes Speed or Power.
- **Hands** (gloves, claws): Attack and Power. The "Cursed" ones give Power +6 with Attack and
  Defense −2.
- **Accessories**: elemental rings (+1 elemental damage, section 6.2), Economizer (attribute
  122), Kron Hammer (Power +5), Zoah Shield (Grit +4), Nocturne Blade (Attack +4), Voxai
  Teleporter (Defense +4), and the ability ones (Refresher, Replenisher, Nrrgal Module, Angel
  Amulet, Spooky Charm, Immunity Idol).
- Item 82 (no name) gives +999 to everything: a test item left in the game.

### 12.4 Chao
Equipped in slot 4. Those from 1 to 20 give attributes (max HP +10%, max PP +2, +1 to Attack,
Defense, Power, Grit or Luck, an element in the attack, or resistance +50 to an element);
those from 21 on give special abilities (section 11). The same Chao appears in three grades
(IDs 91–130, 131–170, 171–210) with larger values: for example, Chao 39 revives with 10%, 50%
and 100% of HP.

### 12.5 Random items, stealing and shops
- **Random items**: a `Random` column > 0 turns the item into a draw on the matching row of
  `RandItems.gda` (up to 8 weight/item pairs). Items 258–276 and 287 are these "envelopes"; a
  draw can land on another envelope (item 275 appears on row 2). `Random` = −2 on some common
  items (Health Root, Speed Bar, ...): meaning unknown (**low**).
- **Plunder** (Rouge): `RewardItem1..3` stores two IDs per value (16 bits each): level 1 = `3`
  (POW Candy or Health Seed), level 2 = `0x40007` (POW Gum or Health Leaf), level 3 =
  `0x50008` (Health Root or POW Drink). It matches the game's journal. The same format appears
  in the "extra item" Chao (code 23).
- **Shops**: `stores.gda` points to the 5 tables `Store1`…`Store5`, with 3 unnamed columns:
  item, buy price and sell price (**medium**: Health Seed costs 6 and sells for 3, the same as
  `MinimumCost`).
- **Rewards**: `rewards.gda` (70 rows: missions) gives `XP` and up to 6 items; the `Copper`,
  `Silver`, `Gold` columns haven't been tied to the code yet (**low**).

## 13. Enemies

- **`creatures.gda`** (128 rows; 0–9 are the party): base attributes, `Type`, `Level`,
  `Combo1..10`, `FleeProbability` (0.05 for almost all), and the basic attack flags
  (`AttackArmorPiercing`, `AttackInescapable`, `AttackBlast`, `AttackScatter`, `AttackLeech`),
  which go into the same formulas as section 5.
- **`squads.gda`** (332 squads): up to 4 members (`Member1..4`), `XPValue`, `Level`,
  `RewardItem`, `AmbushValue` (section 4), `CanFlee` (89 can flee), `FleeTable`/`ChaseTable`
  (flee and chase minigames), `CombatRoundLimit` (2 battles with a round limit),
  `NoticeDistance`.
- **`CombatAI.gda`** (14 profiles): weights, summing to 100, for Attack, Defend and
  `Combo1..10`. E.g.: profile 6 = 85% attack and 15% defense; profile 11 = only defends.
  **Medium**: the link from a profile to each creature hasn't been traced yet.
- **Weaknesses**: section 6.3.

## 14. Dynamic difficulty

The game adjusts the enemies as the player does well or badly. **Medium**: the arithmetic is
high; what exactly counts as "doing well" is a deduction.

- The adjustment level is in `DAT_02160e54` and goes from **−4 to +6** (rules 64 and 63). It
  only turns on under certain conditions (plot 0x1cc == 1 and a squad counter ≥ 4).
- At the end of the battle (`GameModeCombat+0x13c0`, the round counter): a battle of 6 rounds
  or more → level −1; a short battle (3 to 5 rounds) → level +1.
- Each enemy attribute gets `rule × (level ÷ divisor)`, with integer division (levels ±1
  change nothing) and a floor of 0:

| Attribute | Function | Level > 0 (÷ rule 62 = 2) | Level < 0 (÷ rule 65 = 2) |
|---|---|---|---|
| Power | 0x02019414 | +3 (rule 58) | +5 (rule 67) |
| Defense | 0x02019468 | +0 (rule 57) | +2 (rule 66) |
| Attack | 0x020194bc | +1 (rule 59) | +2 (rule 68) |
| Grit | 0x02019510 | +0 (rule 60) | +5 (rule 69) |
| max HP | 0x02019564 | +20 (rule 61) | +0 (rule 70) |

  At level +6: Power +9, Attack +3, max HP +60. At level −4: Power −10, Defense −4, Attack −4,
  Grit −10.

## 15. CombatRules: the 74 rules

`CRules_LoadCombatRules` (0x0201f270) copies each row into a global variable. The complete
list, with values, comes from `combate_tabelas.py` (`regras.md`). The known ones:

| Rules | Use | Where |
|---|---|---|
| 0 | not used in the code (90) | |
| 1, 2, 3, 33 | initiative (60, 1d2, ×1, ÷4) | 0x020448c0 |
| 4 | 0x020451c4 (not analyzed) | |
| 5, 6, 15–32, 48–50 | POW touch minigame and camera | 0x02047a14, 0x02047f70, 0x02039af4 |
| 7, 8, 11 | Defend (+70% Defense, +200% Grit, PP) | 0x02073590 |
| 10, 12, 13, 14 | 0x02043ea8, 0x020436e4, 0x020441cc, 0x02042034 (not analyzed) | |
| 34, 53 | exploration (starting combat from the map) | GameModeExplore |
| 35, 38 | real-time combat interface | CombatRealTimeGui |
| 36, 37 | 0x02069d88 (not analyzed) | |
| 39 | basic damage (100) | 0x0200fcdc |
| 40–42 | POW minigame performance | 0x02070928 |
| 43, 44, 45 | damage: fixed 90, k = 110 (party), k = 60 (enemy) | 0x0200fcdc, 0x02010810 |
| 47 | ambush die (10) | 0x02039af4 |
| 55, 56 | 0x0203d274 (not analyzed) | |
| 57–70 | dynamic difficulty | section 14 |
| 71–73 | minigame scale by move level (0.90 / 0.95 / 1.00) | 0x020702d8 |

## 16. What we don't know yet

- **Fleeing and chasing**: the formula that uses `FleeProbability`, Speed and abilities 15
  and 18; the `Flee*`/`Chase*` minigames.
- **Where the party's ambush value comes from** (and where Luck goes in).
- **XP**: how `XPValue` is divided and where Chao 37's bonus goes in.
- **The touch minigame**: the exact meaning of rules 15–32 and 48–50 and of the
  `realtime.gda` table (65 touch elements), and how the performance (0 to 1) is computed.
- **When Defend's effects are undone** and the order in which the per-round effects (poison,
  regeneration) are applied.
- The link from `CombatAI.gda` to the creatures; status 9; bit 27 of `AllowEquip`;
  `Random` = −2.

## 17. How to check

| To check | Do |
|---|---|
| a formula | read the function in the pseudo-C (`analise/run_all.sh` generates it in `work/`) by the address cited |
| a helper function | run it in isolation with Unicorn: load `arm9.bin` at 0x02000000, put the arguments in r0–r3 and a return address in lr (Thumb functions: address \| 1) |
| a table value | `sonic-mod unpack` and open the spreadsheet; `combate_tabelas.py` decodes the `.SPL`/`.ITM` files |
| in the game | change a value (e.g. rule 44, the party's k) with `sonic-mod pack` and compare the damage in the emulator |

The story of how each part was found is in the [journal](DIARIO.md#11-combat).
