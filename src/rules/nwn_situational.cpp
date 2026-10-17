#include "rules/nwn_situational.h"

namespace rules {

int NwnSituationalTotal(const NwnSituationalTerms& terms) {
  int total = 0;
  if (terms.target_blind) total += 2;
  if (terms.attacker_invisible) total += 2;
  if (terms.cant_see_target) total -= 4;
  if (terms.target_moving) total -= 2;
  if (terms.target_prone) total += terms.ranged ? -4 : 4;
  if (terms.target_stunned) total += 2;
  if (terms.target_flanked) total += 2;
  if (terms.ranged_in_melee) total -= 4;
  return total;
}

RollMode SituationalRollMode(const NwnSituationalTerms& terms) {
  int advantage = 0;
  int disadvantage = 0;
  if (terms.target_blind) ++advantage;
  if (terms.attacker_invisible) ++advantage;
  if (terms.target_stunned) ++advantage;
  if (terms.cant_see_target) ++disadvantage;
  if (terms.ranged_in_melee) ++disadvantage;
  if (terms.target_prone) ++(terms.ranged ? disadvantage : advantage);
  return ResolveRollMode(advantage, disadvantage);
}

}  // namespace rules
