#include "rules/weapons.h"

namespace rules {

Mastery MasteryForNwnBaseItem(int base_item) {
  switch (base_item) {
    case 0:    // shortsword
    case 11:   // shortbow
    case 31:   // dart
    case 38:   // handaxe
    case 51:   // rapier
    case 59:   // shuriken (NWN only: like the dart)
    case 63:   // throwing axe (NWN only: like the handaxe)
      return Mastery::kVex;
    case 1:    // longsword
    case 3:    // bastard sword (NWN only: like the longsword)
    case 4:    // light flail (flail)
    case 9:    // light mace (mace)
    case 41:   // katana (NWN only: like the longsword)
    case 47:   // morningstar
    case 58:   // short spear (spear)
      return Mastery::kSap;
    case 2:    // battleaxe
    case 35:   // heavy flail (NWN only: like the maul)
    case 45:   // magic staff (quarterstaff)
    case 50:   // quarterstaff
    case 92:   // lance
    case 95:   // trident
    case 108:  // dwarven waraxe (NWN only: like the battleaxe)
      return Mastery::kTopple;
    case 5:    // warhammer
    case 6:    // heavy crossbow
      return Mastery::kPush;
    case 7:    // light crossbow
    case 8:    // longbow
    case 28:   // club
    case 61:   // sling
    case 111:  // whip
      return Mastery::kSlow;
    case 10:   // halberd
    case 12:   // two-bladed sword (NWN only)
    case 18:   // greataxe
    case 32:   // dire mace (NWN only)
    case 33:   // double axe (NWN only)
      return Mastery::kCleave;
    case 13:   // greatsword
    case 55:   // scythe (NWN only: like the glaive)
      return Mastery::kGraze;
    case 22:   // dagger
    case 37:   // light hammer
    case 40:   // kama (NWN only: like the sickle)
    case 42:   // kukri (NWN only: like the dagger)
    case 53:   // scimitar
    case 60:   // sickle
      return Mastery::kNick;
    default:
      return Mastery::kNone;
  }
}

bool HasWeaponMastery(CharacterClass character_class) {
  switch (character_class) {
    case CharacterClass::kBarbarian:
    case CharacterClass::kFighter:
    case CharacterClass::kPaladin:
    case CharacterClass::kRanger:
    case CharacterClass::kRogue:
      return true;
    default:
      return false;
  }
}

int OffHandDamageAbilityModifier(int ability_modifier, bool two_weapon_fighting_style) {
  if (ability_modifier < 0 || two_weapon_fighting_style) return ability_modifier;
  return 0;
}

bool IsLightNwnBaseItem(int base_item) {
  switch (base_item) {
    case 0:   // shortsword
    case 22:  // dagger
    case 28:  // club
    case 37:  // light hammer
    case 38:  // handaxe
    case 40:  // kama
    case 42:  // kukri
    case 53:  // scimitar
    case 60:  // sickle
      return true;
    default:
      return false;
  }
}

int OffHandAttacks(int nwn_attacks, bool double_weapon, bool dual_wielder, bool main_hand_light,
                   bool off_hand_light) {
  if (nwn_attacks <= 0) return 0;
  if (double_weapon || dual_wielder) return 1;
  return main_hand_light && off_hand_light ? 1 : 0;
}

int NwnOffHandStrengthDamage(int strength_modifier) {
  return strength_modifier > 0 ? strength_modifier / 2 : strength_modifier;
}

}  // namespace rules
