#include "FPSHUD.h"
#include "FPSWeaponComponent.h"
#include "FPSHeroCharacter.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void AFPSHUD::DrawHUD()
{
	Super::DrawHUD();

	const AFPSHeroCharacter* Hero = Cast<AFPSHeroCharacter>(GetOwningPawn());
	const UFPSWeaponComponent* Weapon = Hero ? Hero->FindComponentByClass<UFPSWeaponComponent>() : nullptr;
	if (!Canvas || !Weapon || Hero->IsDead() || !PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}

	const float CenterX = Canvas->ClipX * 0.5f;
	const float CenterY = Canvas->ClipY * 0.5f;

	// Spread angle -> pixels: how far from center a bullet at the cone edge lands on screen.
	const float HalfFovRad = FMath::DegreesToRadians(PlayerOwner->PlayerCameraManager->GetFOVAngle() * 0.5f);
	const float SpreadRad = FMath::DegreesToRadians(Weapon->GetCurrentSpread());
	const float Gap = MinGap + FMath::Tan(SpreadRad) / FMath::Tan(HalfFovRad) * CenterX;

	// 4 lines + center dot.
	DrawLine(CenterX - Gap - LineLength, CenterY, CenterX - Gap, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX + Gap, CenterY, CenterX + Gap + LineLength, CenterY, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY - Gap - LineLength, CenterX, CenterY - Gap, CrosshairColor, LineThickness);
	DrawLine(CenterX, CenterY + Gap, CenterX, CenterY + Gap + LineLength, CrosshairColor, LineThickness);
	DrawRect(CrosshairColor, CenterX - 1.f, CenterY - 1.f, 2.f, 2.f);

	// Hit marker: diagonal X, only for hits the SERVER confirmed.
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
