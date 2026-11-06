#include "FPSHUD.h"
#include "FPSAttributeSet.h"
#include "FPSGameplayAbility.h"
#include "FPSGameplayTags.h"
#include "FPSHeroCharacter.h"
#include "FPSWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

void AFPSHUD::DrawHUD()
{
	Super::DrawHUD();

	const AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(GetOwningPawn());
	if (!Canvas || !Hero || !PlayerOwner)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float DeltaSeconds = FMath::Clamp(Now - LastDrawTime, 0.f, 0.1f);
	LastDrawTime = Now;

	DrawBars(Hero, DeltaSeconds);

	if (Hero->IsDead())
	{
		return;
	}

	DrawAbilities(Hero);
	if (const UFPSWeaponComponent* Weapon = Hero->FindComponentByClass<UFPSWeaponComponent>())
	{
		DrawCrosshair(Weapon);
	}

	if (Hero->IsStunned())
	{
		DrawText(TEXT("STUNNED"), FLinearColor::Yellow, Canvas->ClipX * 0.5f - 40.f, Canvas->ClipY * 0.5f + 40.f, GEngine->GetMediumFont(), 1.2f);
	}
}

// ---------------------------------------------------------------- Crosshair

void AFPSHUD::DrawCrosshair(const UFPSWeaponComponent* Weapon)
{
	if (!PlayerOwner->PlayerCameraManager)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	// Spread angle -> pixels: where a bullet at the cone edge lands on screen.
	const float HalfFovRad = FMath::DegreesToRadians(PlayerOwner->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float SpreadRad = FMath::DegreesToRadians(Weapon->GetCurrentSpread());
	const float Gap = MinGap + FMath::Tan(SpreadRad) / FMath::Tan(HalfFovRad) * CenterX;

	DrawLine(CenterX - Gap - LineLength, CenterY, CenterX - Gap, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + LineLength, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY - Gap - LineLength, CenterX, CenterY - Gap, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + LineLength, CrosshairColor, LineThickness);
	DrawRect(CrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);

	// Hit marker: only for hits the SERVER confirmed.
	if (GetWorld()->GetTimeSeconds() - Weapon->GetLastHitTime() < HitMarkerDuration)
	{
		const FLinearColor MarkerColor = Weapon->WasLastHitHeadshot() ? FLinearColor::Red : FLinearColor::White;
		const float In = Gap + 2.f;
		const float Out = In + HitMarkerSize;
		DrawLine(CenterX - Out, CenterY - Out, CenterX - In, CenterY - In, MarkerColor, LineThickness);
		DrawLine(CenterX + In, CenterY - In, CenterX + Out, CenterY - Out, MarkerColor, LineThickness);
		DrawLine(CenterX - Out, CenterY + Out, CenterX - In, CenterY + In, MarkerColor, LineThickness);
		DrawLine(CenterX + In, CenterY + In, CenterX + Out, CenterY + Out, MarkerColor, LineThickness);
	}
}

// ---------------------------------------------------------------- Health / shield

void AFPSHUD::DrawBar(float X, float Y, float W, float H, float Fill, float Ghost, const FLinearColor& Color)
{
	DrawRect(BackColor, X, Y, W, H);
	if (Ghost > Fill)
	{
		DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.6f), X, Y, W * Ghost, H); // damage just taken
	}
	DrawRect(Color, X, Y, W * Fill, H);
}

void AFPSHUD::DrawBars(const AFPSHeroCharacter* Hero, float DeltaSeconds)
{
	const UFPSAttributeSet* AS = Hero->GetAttributeSet();
	if (!AS)
	{
		return;
	}

	const float TargetHealth = AS->GetMaxHealth() > 0.f ? AS->GetHealth() / AS->GetMaxHealth() : 0.f;
	const float TargetShield = AS->GetMaxShield() > 0.f ? AS->GetShield() / AS->GetMaxShield() : 0.f;

	// Bar slides fast, ghost slides slow -> you see the chunk you lost.
	ShownHealth = FMath::FInterpTo(ShownHealth, TargetHealth, DeltaSeconds, BarSmoothSpeed);
	ShownShield = FMath::FInterpTo(ShownShield, TargetShield, DeltaSeconds, BarSmoothSpeed);
	GhostHealth = TargetHealth > GhostHealth ? TargetHealth : FMath::FInterpTo(GhostHealth, TargetHealth, DeltaSeconds, GhostSpeed);
	GhostShield = TargetShield > GhostShield ? TargetShield : FMath::FInterpTo(GhostShield, TargetShield, DeltaSeconds, GhostSpeed);

	const float Scale = Canvas->ClipY / 1080.f;
	const float X = 40.f * Scale;
	const float W = 360.f * Scale;
	const float ShieldH = 10.f * Scale;
	const float HealthH = 18.f * Scale;
	const float HealthY = Canvas->ClipY - 60.f * Scale;
	const float ShieldY = HealthY - ShieldH - 4.f * Scale;

	DrawBar(X, ShieldY, W, ShieldH, ShownShield, GhostShield, ShieldColor);

	// Health turns red when low.
	const FLinearColor Health = TargetHealth < 0.3f ? FLinearColor(1.f, 0.25f, 0.2f, 0.95f) : HealthColor;
	DrawBar(X, HealthY, W, HealthH, ShownHealth, GhostHealth, Health);

	const FString Text = FString::Printf(TEXT("%.0f  |  %.0f"), AS->GetHealth(), AS->GetShield());
	DrawText(Text, FLinearColor::White, X + W + 12.f * Scale, ShieldY, GEngine->GetMediumFont(), Scale * 1.2f);
}

// ---------------------------------------------------------------- Abilities

void AFPSHUD::DrawAbilities(const AFPSHeroCharacter* Hero)
{
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float Scale = Canvas->ClipY / 1080.f;
	const float Size = 64.f * Scale;
	const float Spacing = 12.f * Scale;
	float X = Canvas->ClipX - 40.f * Scale - Size;
	const float Y = Canvas->ClipY - 60.f * Scale - Size + 18.f * Scale;
	UFont* Font = GEngine->GetMediumFont();

	// Right to left: area attack, then dash.
	const FGameplayTag Order[] = { FPSTags::InputTag_Ability_Area, FPSTags::InputTag_Ability_Dash };
	for (const FGameplayTag& Tag : Order)
	{
		const UFPSGameplayAbility* Ability = nullptr;
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			const UFPSGameplayAbility* Candidate = Cast<UFPSGameplayAbility>(Spec.Ability);
			if (Candidate && Candidate->InputTag.MatchesTagExact(Tag))
			{
				Ability = Candidate;
				break;
			}
		}
		if (!Ability)
		{
			continue;
		}

		float Remaining = 0.f;
		float Duration = 0.f;
		int32 ChargesReady = 0;
		Ability->GetCooldownState(ASC, Remaining, Duration, ChargesReady);
		const bool bReady = ChargesReady > 0;

		// Box + cooldown fill rising from the bottom.
		DrawRect(BackColor, X, Y, Size, Size);
		if (Remaining > 0.f && Duration > 0.f)
		{
			const float Progress = 1.f - Remaining / Duration;
			const float FillH = Size * Progress;
			DrawRect(bReady ? AbilityReadyColor * 0.6f : AbilityCooldownColor, X, Y + Size - FillH, Size, FillH);
		}
		else
		{
			DrawRect(AbilityReadyColor * FLinearColor(1.f, 1.f, 1.f, 0.35f), X, Y, Size, Size);
		}

		// Border: bright when usable.
		const FLinearColor Border = bReady ? AbilityReadyColor : AbilityCooldownColor;
		DrawLine(X, Y, X + Size, Y, Border, 2.f);
		DrawLine(X, Y + Size, X + Size, Y + Size, Border, 2.f);
		DrawLine(X, Y, X, Y + Size, Border, 2.f);
		DrawLine(X + Size, Y, X + Size, Y + Size, Border, 2.f);

		// Key on top, seconds left in the middle (only when nothing is ready).
		DrawText(Ability->HUDKey.ToString(), FLinearColor::White, X + 4.f * Scale, Y + 2.f * Scale, Font, Scale);
		if (!bReady && Remaining > 0.f)
		{
			DrawText(FString::Printf(TEXT("%.1f"), Remaining), FLinearColor::White, X + Size * 0.3f, Y + Size * 0.35f, Font, Scale * 1.4f);
		}

		// Charge pips under the box.
		if (Ability->MaxCharges > 1)
		{
			const float Pip = 10.f * Scale;
			for (int32 i = 0; i < Ability->MaxCharges; ++i)
			{
				const FLinearColor PipColor = i < ChargesReady ? AbilityReadyColor : AbilityCooldownColor;
				DrawRect(PipColor, X + i * (Pip + 4.f * Scale), Y + Size + 4.f * Scale, Pip, Pip * 0.5f);
			}
		}

		DrawText(Ability->HUDName.ToString(), FLinearColor(1.f, 1.f, 1.f, 0.8f), X, Y - 22.f * Scale, Font, Scale * 0.9f);

		X -= Size + Spacing;
	}
}
