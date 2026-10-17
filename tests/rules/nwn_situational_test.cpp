#include "check.h"
#include "rules/nwn_situational.h"

using namespace rules;

// Breaks caught: a term's 3e value or sign wrong (the hook would leave part of
// the flat modifier in, or remove one the engine never added); prone using the
// melee value for ranged attacks.
TEST(NwnSituationalTermsAddUpLikeTheEngine) {
  CHECK_EQ(NwnSituationalTotal({}), 0);
  NwnSituationalTerms blind;
  blind.target_blind = true;
  CHECK_EQ(NwnSituationalTotal(blind), 2);
  NwnSituationalTerms invisible;
  invisible.attacker_invisible = true;
  CHECK_EQ(NwnSituationalTotal(invisible), 2);
  NwnSituationalTerms unseen;
  unseen.cant_see_target = true;
  CHECK_EQ(NwnSituationalTotal(unseen), -4);
  NwnSituationalTerms prone_melee;
  prone_melee.target_prone = true;
  CHECK_EQ(NwnSituationalTotal(prone_melee), 4);
  NwnSituationalTerms prone_ranged = prone_melee;
  prone_ranged.ranged = true;
  CHECK_EQ(NwnSituationalTotal(prone_ranged), -4);
  NwnSituationalTerms stunned;
  stunned.target_stunned = true;
  CHECK_EQ(NwnSituationalTotal(stunned), 2);
  NwnSituationalTerms flanked;
  flanked.target_flanked = true;
  CHECK_EQ(NwnSituationalTotal(flanked), 2);
  // The 0.6.1 bow test, shot 2: a goblin still running up and already within
  // its reach took the shot from +4 to -2.
  NwnSituationalTerms shot2;
  shot2.ranged = true;
  shot2.target_moving = true;
  shot2.ranged_in_melee = true;
  CHECK_EQ(NwnSituationalTotal(shot2), -6);
}

// Phase 1b proposed mapping (PLAN.md). Breaks caught: a 2024 source missing
// (unseen target, prone at range, ranged in close combat -> Disadvantage;
// blinded, invisible attacker, stunned, prone in melee -> Advantage); a 3e
// term with no 2024 rule turned into one (moving target; flanking, an
// optional rule, until the owner decides); sources not cancelling.
TEST(NwnSituationsBecomeAdvantageOrDisadvantage) {
  CHECK_EQ(SituationalRollMode({}), RollMode::kNormal);
  NwnSituationalTerms close_shot;
  close_shot.ranged = true;
  close_shot.ranged_in_melee = true;
  CHECK_EQ(SituationalRollMode(close_shot), RollMode::kDisadvantage);
  NwnSituationalTerms prone_melee;
  prone_melee.target_prone = true;
  CHECK_EQ(SituationalRollMode(prone_melee), RollMode::kAdvantage);
  NwnSituationalTerms prone_ranged = prone_melee;
  prone_ranged.ranged = true;
  CHECK_EQ(SituationalRollMode(prone_ranged), RollMode::kDisadvantage);
  NwnSituationalTerms stunned;
  stunned.target_stunned = true;
  CHECK_EQ(SituationalRollMode(stunned), RollMode::kAdvantage);
  NwnSituationalTerms blind;
  blind.target_blind = true;
  CHECK_EQ(SituationalRollMode(blind), RollMode::kAdvantage);
  NwnSituationalTerms invisible;
  invisible.attacker_invisible = true;
  CHECK_EQ(SituationalRollMode(invisible), RollMode::kAdvantage);
  NwnSituationalTerms unseen;
  unseen.cant_see_target = true;
  CHECK_EQ(SituationalRollMode(unseen), RollMode::kDisadvantage);
  NwnSituationalTerms moving;
  moving.ranged = true;
  moving.target_moving = true;
  CHECK_EQ(SituationalRollMode(moving), RollMode::kNormal);
  NwnSituationalTerms flanked;
  flanked.target_flanked = true;
  CHECK_EQ(SituationalRollMode(flanked), RollMode::kNormal);
  NwnSituationalTerms invisible_archer_in_melee = close_shot;
  invisible_archer_in_melee.attacker_invisible = true;
  CHECK_EQ(SituationalRollMode(invisible_archer_in_melee), RollMode::kNormal);
}
