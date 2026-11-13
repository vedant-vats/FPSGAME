#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

class AFPSHeroCharacter;
class UUserWidget;
class UFPSWeaponComponent;
class UFont;

/** One line of the trash-talking objective panel. */
USTRUCT(BlueprintType)
struct FFPSObjectiveLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Objective")
	FText Objective;

	/** The mockery under it. */
	UPROPERTY(EditAnywhere, Category = "Objective")
	FText Taunt;

	FFPSObjectiveLine() = default;
	FFPSObjectiveLine(const FText& InObjective, const FText& InTaunt) : Objective(InObjective), Taunt(InTaunt) {}
};

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
	AFPSHUD();
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;

protected:
	/** UMG wave banner (binds to FPSGameState events). Empty = canvas text fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "Waves")
	TSubclassOf<UUserWidget> WaveWidgetClass;

	// ---- Size / text ----
	/** Multiplies the whole HUD (bars, boxes, text, heat bar). 1 = original size. */
	UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = 0.5, ClampMax = 3))
	float HUDScale = 1.35f;

	/** Extra multiplier for text only, on top of HUDScale. */
	UPROPERTY(EditDefaultsOnly, Category = "Style", meta = (ClampMin = 0.5, ClampMax = 3))
	float TextScale = 1.3f;

	/** Optional. A Font asset with Font Cache Type = Runtime stays sharp at any size. Empty = engine fonts. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	TObjectPtr<UFont> HUDFont;

	/** Heavy black outline on text. Off = soft 1 px shadow only (cleaner look). */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	bool bTextOutline = false;

	// Palette: one meaning per colour. No orange.
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor TextColor = FLinearColor(1.f, 1.f, 1.f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor MutedColor = FLinearColor(0.7f, 0.7f, 0.72f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor LineColor = FLinearColor(1.f, 1.f, 1.f, 0.3f);

	/** Danger: damage, low health, overheat, boss. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor DangerColor = FLinearColor(1.f, 0.27f, 0.27f, 1.f);

	/** Stun / shield. */
	UPROPERTY(EditDefaultsOnly, Category = "Style")
	FLinearColor StunColor = FLinearColor(0.3f, 0.85f, 0.85f, 1.f);

	// ---- Objective panel (left side, mocks the player; NOT hidden by F10, it's game HUD) ----
	/** One per wave (index 0 = wave 1). Waves past the end use the last entry. */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	TArray<FFPSObjectiveLine> WaveObjectives;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine BeforeFirstWave;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine BetweenWaves;

	/** Shown while health is under LowHealthFraction (beats the wave line). */
	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine LowHealth;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine Overheated;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine Dead;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine Victory;

	UPROPERTY(EditDefaultsOnly, Category = "Objective")
	FFPSObjectiveLine Defeat;

	UPROPERTY(EditDefaultsOnly, Category = "Objective", meta = (ClampMin = 0, ClampMax = 1))
	float LowHealthFraction = 0.3f;

	// ---- Ability help screen (hold F1 / H) ----
	UPROPERTY(EditDefaultsOnly, Category = "Help")
	FText HelpTitle;

	UPROPERTY(EditDefaultsOnly, Category = "Help")
	FText RifleName;

	UPROPERTY(EditDefaultsOnly, Category = "Help")
	FText RifleKey;

	UPROPERTY(EditDefaultsOnly, Category = "Help", meta = (MultiLine = true))
	FText RifleDescription;

	UPROPERTY(EditDefaultsOnly, Category = "Help")
	FText RifleStats;

	/** Tip block under the cards (left border, like a "passive" block). */
	UPROPERTY(EditDefaultsOnly, Category = "Help")
	FText TanksTitle;

	/** Supports colour tags: <w> white, <r> red, <t> teal, </> closes. */
	UPROPERTY(EditDefaultsOnly, Category = "Help", meta = (MultiLine = true))
	FText TanksText;

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
	FLinearColor HealthColor = FLinearColor(0.9f, 0.9f, 0.92f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	FLinearColor ShieldColor = FLinearColor(0.3f, 0.85f, 0.85f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	FLinearColor BackColor = FLinearColor(0.f, 0.f, 0.f, 0.5f);

	/** How fast the bar slides to the new value (higher = snappier). */
	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	float BarSmoothSpeed = 12.f;

	/** How fast the white "damage taken" ghost catches up. */
	UPROPERTY(EditDefaultsOnly, Category = "Bars")
	float GhostSpeed = 2.5f;

	// ---- Enemy bars ----
	/** Bars only for enemies closer than this. */
	UPROPERTY(EditDefaultsOnly, Category = "Enemy Bars", meta = (Units = "cm"))
	float EnemyBarMaxDistance = 4000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy Bars")
	float EnemyBarWidth = 70.f;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy Bars")
	FLinearColor EnemyHealthColor = FLinearColor(0.9f, 0.2f, 0.2f, 0.95f);

	// ---- Abilities ----
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	FLinearColor AbilityReadyColor = FLinearColor(1.f, 1.f, 1.f, 0.95f);

	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	FLinearColor AbilityCooldownColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.8f);

private:
	void DrawCrosshair(const UFPSWeaponComponent* Weapon);
	void DrawBars(const AFPSHeroCharacter* Hero, float DeltaSeconds);
	void DrawAbilities(const AFPSHeroCharacter* Hero);
	void DrawMatchInfo(const AFPSHeroCharacter* Hero);
	void DrawEnemyBars();
	void DrawHeat(const UFPSWeaponComponent* Weapon);
	void DrawObjective(const AFPSHeroCharacter* Hero);
	void DrawAbilityHelp(const AFPSHeroCharacter* Hero);
	void DrawHelpCard(float X, float Y, float W, float H, const FString& Key, const FString& Name, const FString& Description, const FString& Stats);
	/**
	 * Word-wrapped text with colour tags: <w> white, <r> red, <t> teal, </> back to BaseColor.
	 * Unknown tags are dropped, unclosed tags run to the end. Returns the height used.
	 * bDraw = false only measures. Lines that would pass MaxY are not drawn (clip).
	 */
	float DrawRichText(const FString& Markup, float X, float Y, float MaxWidth, UFont* Font, float Scale,
		const FLinearColor& BaseColor, bool bDraw = true, float MaxY = TNumericLimits<float>::Max());

	// Style primitives
	/** 1 px border, top-left + top-right corners cut by Cut. */
	void DrawFrame(float X, float Y, float W, float H, float Cut, const FLinearColor& Color);
	/** Fill with the same cut corners as DrawFrame. */
	void DrawCutRect(float X, float Y, float W, float H, float Cut, const FLinearColor& Color);
	/** Square rotated 45 degrees: filled + 1 px outline. Size = corner to corner. */
	void DrawDiamond(float CX, float CY, float Size, const FLinearColor& Fill, const FLinearColor& Line);
	/** White box, black key text, centred on (CX, CY). Returns its size. */
	FVector2D DrawKeyBadge(const FString& Key, float CX, float CY, UFont* Font, float Scale);
	FVector2D MeasureKeyBadge(const FString& Key, UFont* Font, float Scale);
	float LineThickness1px() const;
	/** Like AHUD::DrawLine but alpha-blended (DrawLine draws opaque, so LineColor's 0.3 alpha was ignored). */
	void DrawSoftLine(float X1, float Y1, float X2, float Y2, const FLinearColor& Color);
	const FFPSObjectiveLine* PickObjective(const AFPSHeroCharacter* Hero);
	void DrawCenteredText(const FString& Text, float Y, const FLinearColor& Color, float Scale);

	/** 1080p-relative scale * HUDScale. Use for every size on screen. */
	float GetUIScale() const;
	UFont* GetHUDFont(UFont* EngineFallback) const;
	/** All HUD text goes through here: applies TextScale + outline/shadow. */
	void DrawHUDText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale);
	void MeasureHUDText(const FString& Text, UFont* Font, float Scale, float& OutW, float& OutH);
	/** Shrinks Scale so Text fits in MaxWidth (never grows it). Stops labels overlapping whatever the font. */
	float FitTextScale(const FString& Text, UFont* Font, float Scale, float MaxWidth);
	void DrawBar(float X, float Y, float W, float H, float Fill, float Ghost, const FLinearColor& Color);

	UPROPERTY()
	TObjectPtr<UUserWidget> WaveWidget;

	// Smoothed display values (0-1).
	float ShownHealth = 1.f;
	float ShownShield = 1.f;
	float GhostHealth = 1.f;
	float GhostShield = 1.f;
	float LastDrawTime = 0.f;

	// Objective panel: fade in when the line changes; low-HP uses hysteresis so it doesn't flicker.
	const FFPSObjectiveLine* ShownObjective = nullptr;
	float ObjectiveChangedTime = 0.f;
	bool bLowHealthShown = false;
};
