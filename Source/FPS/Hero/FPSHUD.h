#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

/**
 * Crosshair whose gap = the weapon's real spread cone on screen.
 * Hit marker on server-confirmed hits (red = headshot).
 * Set as HUD Class on the GameMode.
 */
UCLASS()
class FPS_API AFPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineLength = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineThickness = 2.f;

	/** Gap in pixels even at zero spread. */
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float MinGap = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair", meta = (Units = "s"))
	float HitMarkerDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float HitMarkerSize = 10.f;
};
