#include "FPSGameplayTags.h"

namespace FPSTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Fire, "InputTag.Fire", "Primary fire input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Dash, "InputTag.Ability.Dash", "Dash ability input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability_Area, "InputTag.Ability.Area", "Area attack input");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Hero is dead");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Can't move, shoot or use abilities");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Dash, "Cooldown.Dash", "Dash recharging");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Shockwave, "Cooldown.Shockwave", "Shockwave on cooldown");
}
