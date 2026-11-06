#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

class AFPSHeroCharacter;
class UFPSWeaponComponent;

/**
 * Canvas HUD (no assets):
 *  - Crosshair: gap = weapon's real spread on screen. Hit marker on server-confirmed hits.
 *  - Bottom left: shield + health bars (smoothly animated, white "damage taken" ghost).
 *  - Bottom right: abilities (key, name, charges, cooldown fill + seconds).
 * Set as HUD Class on the GameMode.
 */
UCLASS()
class FPS_API AFPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

protected:
	// ---- Crosshair ----
	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	FLinearColor CrosshairColor = FLinearColor(1.f, 1.f, 1.f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineLength = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float LineThickness = 2.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float MinGap = 4.f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair", meta = (Units = "s"))
	float HitMarkerDuration = 0.15f;

	UPROPERTY(EditDefaultsOnly, Category = "Crosshair")
	float HitMarkerSize = 10.f;

	// ---- Bars ----
	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	FLinearColor HealthColor = FLinearColor(0.2f, 0.9f, 0.4f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	FLinearColor ShieldColor = FLinearColor(0.3f, 0.7f, 1.f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	FLinearColor BackColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

	/** How fast the bar slides to the new value (higher = snappier). */
	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	float BarSmoothSpeed = 12.f;

	/** How fast the white "damage taken" ghost catches up. */
	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	float GhostSpeed = 2.5f;

	// ---- Abilities ----
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	FLinearColor AbilityReadyColor = FLinearColor(1.f, 0.8f, 0.2f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	FLinearColor AbilityCooldownColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.8f);

private:
	void DrawCrosshair(const UFPSWeaponComponent* Weapon);
	void DrawBars(const AFPSHeroCharacter* Hero, float DeltaSeconds);
	void DrawAbilities(const AFPSHeroCharacter* Hero);
	void DrawBar(float X, float Y, float W, float H, float Fill, float Ghost, const FLinearColor& Color);

	// Smoothed display values (0-1).
	float ShownHealth = 1.f;
	float ShownShield = 1.f;
	float GhostHealth = 1.f;
	float GhostShield = 1.f;
	float LastDrawTime = 0.f;
};
