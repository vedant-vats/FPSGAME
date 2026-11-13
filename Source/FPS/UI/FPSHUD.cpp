#include "FPSHUD.h"
#include "GAS/FPSAttributeSet.h"
#include "GAS/FPSGameplayAbility.h"
#include "Core/FPSGameplayTags.h"
#include "Core/FPSGameState.h"
#include "Enemies/FPSEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "Hero/FPSHeroCharacter.h"
#include "Weapons/FPSWeaponComponent.h"

#include "AbilitySystemComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "CanvasItem.h"
#include "GameFramework/PlayerController.h"

#define LOCTEXT_NAMESPACE "FPSHUD"

AFPSHUD::AFPSHUD()
{
	// Defaults. All editable in BP_FPSHUD (Objective / Help categories), no code needed to rewrite them.
	// Panel voice: a dry, bored arena announcer.
	WaveObjectives = {
		{ LOCTEXT("W1", "WAVE 1"), LOCTEXT("W1T", "Warm-up. Don't embarrass yourself.") },
		{ LOCTEXT("W2", "WAVE 2"), LOCTEXT("W2T", "Tanks now. Keep moving.") },
		{ LOCTEXT("W3", "WAVE 3"), LOCTEXT("W3T", "You won't.") },
		{ LOCTEXT("W4", "WAVE 4"), LOCTEXT("W4T", "Still alive. Odd.") },
		{ LOCTEXT("W5", "KILL THE BOSS"), LOCTEXT("W5T", "Big one. Bigger than you.") },
	};
	BeforeFirstWave = { LOCTEXT("Start", "SURVIVE 5 WAVES"), LOCTEXT("StartT", "Good luck with that.") };
	BetweenWaves = { LOCTEXT("Break", "BREAK"), LOCTEXT("BreakT", "Short break. Use it.") };
	LowHealth = { LOCTEXT("Low", "LOW HEALTH"), LOCTEXT("LowT", "Called it.") };
	Overheated = { LOCTEXT("Heat", "OVERHEATED"), LOCTEXT("HeatT", "Let go of the trigger.") };
	Dead = { LOCTEXT("Dead", "DOWN"), LOCTEXT("DeadT", "As expected.") };
	Victory = { LOCTEXT("Win", "VICTORY"), LOCTEXT("WinT", "Huh. Fine.") };
	Defeat = { LOCTEXT("Lose", "DEFEAT"), LOCTEXT("LoseT", "Told you.") };

	// Help screen: plain facts. Colour tags: <w> white, <r> red, <t> teal, </> closes.
	HelpTitle = LOCTEXT("HelpTitle", "ABILITIES");
	RifleName = LOCTEXT("RifleName", "RIFLE");
	RifleKey = LOCTEXT("RifleKey", "LMB");
	RifleDescription = LOCTEXT("RifleDesc", "Hitscan. Headshots deal <r>double damage</>. Hold the trigger too long and it <r>overheats</>.");
	RifleStats = LOCTEXT("RifleStats", "Overheats after ~50 shots. Fire in bursts.");
	TanksTitle = LOCTEXT("TanksTitle", "TANKS");
	TanksText = LOCTEXT("TanksText", "A glowing Tank is about to charge in a straight line. Step sideways, or stun it.");
}

#undef LOCTEXT_NAMESPACE

void AFPSHUD::BeginPlay()
{
	Super::BeginPlay();

	if (WaveWidgetClass && PlayerOwner && PlayerOwner->IsLocalController())
	{
		WaveWidget = CreateWidget<UUserWidget>(PlayerOwner, WaveWidgetClass);
		if (WaveWidget)
		{
			WaveWidget->AddToViewport(10);
		}
	}
}

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

	DrawEnemyBars();
	DrawBars(Hero, DeltaSeconds);
	DrawMatchInfo(Hero);
	DrawObjective(Hero);

	if (Hero->IsDead())
	{
		if (Hero->IsAbilityHelpShown())
		{
			DrawAbilityHelp(Hero);
		}
		return;
	}

	DrawAbilities(Hero);
	if (const UFPSWeaponComponent* Weapon = Hero->FindComponentByClass<UFPSWeaponComponent>())
	{
		DrawCrosshair(Weapon);
		DrawHeat(Weapon);
	}

	if (Hero->IsStunned())
	{
		DrawCenteredText(TEXT("STUNNED"), Canvas->ClipY * 0.5f + 70.f * GetUIScale(), StunColor, GetUIScale() * 0.9f);
	}

	if (Hero->IsAbilityHelpShown())
	{
		DrawAbilityHelp(Hero);
	}
}

// ---------------------------------------------------------------- Style helpers

float AFPSHUD::GetUIScale() const
{
	return Canvas->ClipY / 1080.f * HUDScale;
}

UFont* AFPSHUD::GetHUDFont(UFont* EngineFallback) const
{
	return HUDFont ? HUDFont.Get() : EngineFallback;
}

void AFPSHUD::MeasureHUDText(const FString& Text, UFont* Font, float Scale, float& OutW, float& OutH)
{
	GetTextSize(Text, OutW, OutH, Font, Scale * TextScale);
}

float AFPSHUD::FitTextScale(const FString& Text, UFont* Font, float Scale, float MaxWidth)
{
	float W = 0.f;
	float H = 0.f;
	MeasureHUDText(Text, Font, Scale, W, H);
	return (W > MaxWidth && W > 0.f) ? Scale * (MaxWidth / W) : Scale;
}

void AFPSHUD::DrawHUDText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font, float Scale)
{
	FCanvasTextItem Item(FVector2D(X, Y), FText::FromString(Text), Font, Color);
	Item.Scale = FVector2D(Scale * TextScale);
	if (bTextOutline)
	{
		Item.bOutlined = true;
		Item.OutlineColor = FLinearColor(0.f, 0.f, 0.f, 0.85f);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.6f), FVector2D(2.f, 2.f));
	}
	else
	{
		// Soft shadow only: readable on bright walls without the heavy "sticker" look.
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.5f * Color.A), FVector2D(1.f, 1.f));
	}
	Canvas->DrawItem(Item);
}

float AFPSHUD::LineThickness1px() const
{
	return FMath::Max(1.f, GetUIScale());
}

void AFPSHUD::DrawSoftLine(float X1, float Y1, float X2, float Y2, const FLinearColor& Color)
{
	FCanvasLineItem Line(FVector2D(X1, Y1), FVector2D(X2, Y2));
	Line.SetColor(Color);
	Line.LineThickness = LineThickness1px();
	Line.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Line);
}

void AFPSHUD::DrawFrame(float X, float Y, float W, float H, float Cut, const FLinearColor& Color)
{
	DrawSoftLine(X + Cut, Y, X + W - Cut, Y, Color);
	DrawSoftLine(X + W - Cut, Y, X + W, Y + Cut, Color);
	DrawSoftLine(X + W, Y + Cut, X + W, Y + H, Color);
	DrawSoftLine(X + W, Y + H, X, Y + H, Color);
	DrawSoftLine(X, Y + H, X, Y + Cut, Color);
	DrawSoftLine(X, Y + Cut, X + Cut, Y, Color);
}

void AFPSHUD::DrawCutRect(float X, float Y, float W, float H, float Cut, const FLinearColor& Color)
{
	// Body below the cut + a top strip between the cuts (the two cut triangles stay empty).
	DrawRect(Color, X, Y + Cut, W, H - Cut);
	DrawRect(Color, X + Cut, Y, W - Cut * 2.f, Cut);
}

void AFPSHUD::DrawDiamond(float CX, float CY, float Size, const FLinearColor& Fill, const FLinearColor& Line)
{
	// Fill: a square tile rotated 45 degrees around its centre (no texture needed).
	const float Side = Size / UE_SQRT_2;
	FCanvasTileItem Tile(FVector2D(CX - Side * 0.5f, CY - Side * 0.5f), FVector2D(Side, Side), Fill);
	Tile.Rotation = FRotator(0.f, 45.f, 0.f);
	Tile.PivotPoint = FVector2D(0.5f, 0.5f);
	Tile.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Tile);

	const float Half = Size * 0.5f;
	DrawSoftLine(CX, CY - Half, CX + Half, CY, Line);
	DrawSoftLine(CX + Half, CY, CX, CY + Half, Line);
	DrawSoftLine(CX, CY + Half, CX - Half, CY, Line);
	DrawSoftLine(CX - Half, CY, CX, CY - Half, Line);
}

FVector2D AFPSHUD::MeasureKeyBadge(const FString& Key, UFont* Font, float Scale)
{
	float TW = 0.f;
	float TH = 0.f;
	MeasureHUDText(Key, Font, Scale, TW, TH);
	const float S = GetUIScale();
	return FVector2D(TW + 10.f * S, TH + 4.f * S);
}

FVector2D AFPSHUD::DrawKeyBadge(const FString& Key, float CX, float CY, UFont* Font, float Scale)
{
	const FVector2D Size = MeasureKeyBadge(Key, Font, Scale);
	const float X = CX - Size.X * 0.5f;
	const float Y = CY - Size.Y * 0.5f;
	DrawRect(TextColor, X, Y, Size.X, Size.Y);

	// Black text, no shadow (a shadow on white looks dirty).
	float TW = 0.f;
	float TH = 0.f;
	MeasureHUDText(Key, Font, Scale, TW, TH);
	FCanvasTextItem Item(FVector2D(CX - TW * 0.5f, CY - TH * 0.5f), FText::FromString(Key), Font, FLinearColor::Black);
	Item.Scale = FVector2D(Scale * TextScale);
	Canvas->DrawItem(Item);
	return Size;
}

// ---------------------------------------------------------------- Objective panel

const FFPSObjectiveLine* AFPSHUD::PickObjective(const AFPSHeroCharacter* Hero)
{
	const AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>();
	if (!GS)
	{
		return nullptr;
	}

	// Priority: match result > dead > overheated > low health > the phase/wave line.
	if (GS->Phase == EFPSMatchPhase::Victory) return &Victory;
	if (GS->Phase == EFPSMatchPhase::Defeat) return &Defeat;
	if (Hero->IsDead()) return &Dead;

	const UFPSWeaponComponent* Weapon = Hero->FindComponentByClass<UFPSWeaponComponent>();
	if (Weapon && Weapon->IsOverheated()) return &Overheated;

	if (const UFPSAttributeSet* AS = Hero->GetAttributeSet())
	{
		const float Health = AS->GetMaxHealth() > 0.f ? AS->GetHealth() / AS->GetMaxHealth() : 1.f;
		// Hysteresis: enter below the threshold, leave only a bit above it.
		bLowHealthShown = bLowHealthShown ? Health < LowHealthFraction + 0.05f : Health < LowHealthFraction;
		if (bLowHealthShown && GS->Phase == EFPSMatchPhase::InWave) return &LowHealth;
	}

	switch (GS->Phase)
	{
	case EFPSMatchPhase::WaitingToStart:
		return &BeforeFirstWave;
	case EFPSMatchPhase::Intermission:
		return &BetweenWaves;
	case EFPSMatchPhase::InWave:
		if (WaveObjectives.Num() > 0)
		{
			return &WaveObjectives[FMath::Clamp(GS->CurrentWave - 1, 0, WaveObjectives.Num() - 1)];
		}
		break;
	default:
		break;
	}
	return nullptr;
}

void AFPSHUD::DrawObjective(const AFPSHeroCharacter* Hero)
{
	const FFPSObjectiveLine* Line = PickObjective(Hero);
	const float Now = GetWorld()->GetTimeSeconds();
	if (Line != ShownObjective)
	{
		ShownObjective = Line;
		ObjectiveChangedTime = Now;
	}
	if (!Line)
	{
		return;
	}

	const float S = GetUIScale();
	const float Alpha = FMath::Clamp((Now - ObjectiveChangedTime) / 0.25f, 0.f, 1.f); // quick fade-in
	UFont* Font = GetHUDFont(GEngine->GetMediumFont());

	const float X = 40.f * S;
	const float Y = Canvas->ClipY * 0.38f;
	const FString Objective = Line->Objective.ToString();
	const FString Taunt = Line->Taunt.ToString();

	float OW = 0.f, OH = 0.f, TW = 0.f, TH = 0.f;
	MeasureHUDText(Objective, Font, S * 0.85f, OW, OH);
	MeasureHUDText(Taunt, Font, S * 0.65f, TW, TH);

	// Urgent states go red; everything else stays white/grey.
	const bool bUrgent = Line == &LowHealth || Line == &Overheated;

	// Thin line on the left, like a mission tracker.
	FLinearColor Accent = bUrgent ? DangerColor : LineColor;
	Accent.A *= Alpha;
	DrawRect(Accent, X - 10.f * S, Y, LineThickness1px(), OH + TH + 6.f * S);

	FLinearColor ObjColor = bUrgent ? DangerColor : TextColor;
	ObjColor.A *= Alpha;
	FLinearColor TauntCol = MutedColor;
	TauntCol.A *= Alpha;
	DrawHUDText(Objective, ObjColor, X, Y, Font, S * 0.85f);
	DrawHUDText(Taunt, TauntCol, X, Y + OH + 4.f * S, Font, S * 0.65f);
}

// ---------------------------------------------------------------- Ability help (hold F1 / H)

namespace
{
	struct FRichToken
	{
		FString Text;
		FLinearColor Color;
		bool bSpaceBefore = false;
	};
}

float AFPSHUD::DrawRichText(const FString& Markup, float X, float Y, float MaxWidth, UFont* Font, float Scale,
	const FLinearColor& BaseColor, bool bDraw, float MaxY)
{
	// 1) Tokenise: words with their colour. A tag only changes the colour; whitespace splits words.
	TArray<FRichToken> Tokens;
	FString Current;
	FLinearColor Color = BaseColor;
	bool bPendingSpace = false;
	auto Flush = [&]()
	{
		if (!Current.IsEmpty())
		{
			Tokens.Add({ Current, Color, bPendingSpace });
			Current.Reset();
			bPendingSpace = false;
		}
	};

	for (int32 i = 0; i < Markup.Len(); ++i)
	{
		const TCHAR Ch = Markup[i];
		if (Ch == TEXT('<'))
		{
			const int32 Close = Markup.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
			if (Close != INDEX_NONE)
			{
				Flush();
				const FString Tag = Markup.Mid(i + 1, Close - i - 1);
				if (Tag == TEXT("w")) Color = TextColor;
				else if (Tag == TEXT("r")) Color = DangerColor;
				else if (Tag == TEXT("t")) Color = StunColor;
				else if (Tag == TEXT("/")) Color = BaseColor;
				// Unknown tag: dropped, colour unchanged.
				i = Close;
				continue;
			}
		}
		if (FChar::IsWhitespace(Ch))
		{
			Flush();
			bPendingSpace = Tokens.Num() > 0;
			continue;
		}
		Current.AppendChar(Ch);
	}
	Flush();

	if (Tokens.Num() == 0)
	{
		return 0.f;
	}

	// 2) Lay out word by word; only break where there was a space.
	float W = 0.f;
	float LineH = 0.f;
	MeasureHUDText(TEXT("Ag"), Font, Scale, W, LineH);
	float W1 = 0.f, W2 = 0.f, Unused = 0.f;
	MeasureHUDText(TEXT("a a"), Font, Scale, W1, Unused);
	MeasureHUDText(TEXT("aa"), Font, Scale, W2, Unused);
	const float SpaceW = FMath::Max(0.f, W1 - W2);
	const float LineGap = 2.f * GetUIScale();

	float CursorX = 0.f;
	float LineY = 0.f;
	for (const FRichToken& Token : Tokens)
	{
		float TokenW = 0.f;
		float TokenH = 0.f;
		MeasureHUDText(Token.Text, Font, Scale, TokenW, TokenH);
		float Lead = (Token.bSpaceBefore && CursorX > 0.f) ? SpaceW : 0.f;
		if (CursorX > 0.f && Token.bSpaceBefore && CursorX + Lead + TokenW > MaxWidth)
		{
			CursorX = 0.f;
			LineY += LineH + LineGap;
			Lead = 0.f;
		}
		if (bDraw && Y + LineY + LineH <= MaxY)
		{
			DrawHUDText(Token.Text, Token.Color, X + CursorX + Lead, Y + LineY, Font, Scale);
		}
		CursorX += Lead + TokenW;
	}
	return LineY + LineH;
}

void AFPSHUD::DrawHelpCard(float X, float Y, float W, float H, const FString& Key, const FString& Name, const FString& Description, const FString& Stats)
{
	const float S = GetUIScale();
	const float Pad = 18.f * S;
	const float Cut = 14.f * S;
	UFont* Font = GetHUDFont(GEngine->GetMediumFont());

	// Card: barely-there fill, thin cut-corner frame, diamond on the top edge.
	DrawCutRect(X, Y, W, H, Cut, FLinearColor(0.f, 0.f, 0.f, 0.25f));
	DrawFrame(X, Y, W, H, Cut, LineColor);
	DrawDiamond(X + W * 0.5f, Y, 22.f * S, FLinearColor(0.06f, 0.06f, 0.09f, 1.f), LineColor);

	// Key badge sits on the shared line above the card.
	DrawKeyBadge(Key, X + W * 0.5f, Y - 34.f * S, Font, S * 0.55f);

	// Name, centred caps.
	float NW = 0.f, NH = 0.f;
	const float NameScale = FitTextScale(Name, Font, S * 0.85f, W - Pad * 2.f);
	MeasureHUDText(Name, Font, NameScale, NW, NH);
	const float NameY = Y + 30.f * S;
	DrawHUDText(Name, TextColor, X + (W - NW) * 0.5f, NameY, Font, NameScale);

	// Stats pinned to the bottom-left.
	float StatsTop = Y + H - Pad;
	if (!Stats.IsEmpty())
	{
		float SW = 0.f, SH = 0.f;
		const float StatScale = FitTextScale(Stats, Font, S * 0.55f, W - Pad * 2.f);
		MeasureHUDText(Stats, Font, StatScale, SW, SH);
		StatsTop = Y + H - Pad - SH;
		DrawHUDText(Stats, MutedColor, X + Pad, StatsTop, Font, StatScale);
	}

	// Description: shrink up to 3 times to fit above the stats, then clip.
	const float DescY = NameY + NH + 12.f * S;
	const float DescMaxY = StatsTop - 6.f * S;
	const float DescW = W - Pad * 2.f;
	float DescScale = S * 0.6f;
	for (int32 Try = 0; Try < 3; ++Try)
	{
		if (DescY + DrawRichText(Description, X + Pad, DescY, DescW, Font, DescScale, MutedColor, false) <= DescMaxY)
		{
			break;
		}
		DescScale *= 0.85f;
	}
	DrawRichText(Description, X + Pad, DescY, DescW, Font, DescScale, MutedColor, true, DescMaxY);
}

void AFPSHUD::DrawAbilityHelp(const AFPSHeroCharacter* Hero)
{
	const float S = GetUIScale();
	UFont* Font = GetHUDFont(GEngine->GetMediumFont());

	// Dim the game behind it (it doesn't pause: multiplayer).
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);

	// Collect the cards first so the row can be centred on however many there are.
	struct FCard { FString Key, Name, Description, Stats; };
	TArray<FCard> Cards;
	Cards.Add({ RifleKey.ToString(), RifleName.ToString(), RifleDescription.ToString(), RifleStats.ToString() });

	// Abilities: text from each ability's BP, numbers straight from the real data.
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	const FGameplayTag Order[] = { FPSTags::InputTag_Ability_Dash, FPSTags::InputTag_Ability_Area };
	for (const FGameplayTag& Tag : Order)
	{
		const UFPSGameplayAbility* Ability = nullptr;
		if (ASC)
		{
			for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
			{
				const UFPSGameplayAbility* Candidate = Cast<UFPSGameplayAbility>(Spec.Ability);
				if (Candidate && Candidate->InputTag.MatchesTagExact(Tag))
				{
					Ability = Candidate;
					break;
				}
			}
		}
		if (!Ability)
		{
			continue;
		}

		FString Stats;
		const float Cooldown = Ability->GetCooldownSeconds();
		if (Cooldown > 0.f)
		{
			Stats = FString::Printf(TEXT("Cooldown %.0f s"), Cooldown);
		}
		if (Ability->MaxCharges > 1)
		{
			Stats += FString::Printf(TEXT("%s%d charges"), Stats.IsEmpty() ? TEXT("") : TEXT("  |  "), Ability->MaxCharges);
		}
		const FString Description = Ability->HUDDescription.IsEmpty()
			? FString(TEXT("(Set HUD Description in this ability's Blueprint.)"))
			: Ability->HUDDescription.ToString();

		Cards.Add({ Ability->HUDKey.ToString(), Ability->HUDName.ToString(), Description, Stats });
	}

	const float CardW = 300.f * S;
	const float CardH = 220.f * S;
	const float Gap = 40.f * S;
	const float TotalW = CardW * Cards.Num() + Gap * (Cards.Num() - 1);
	const float Left = (Canvas->ClipX - TotalW) * 0.5f;
	const float Top = Canvas->ClipY * 0.5f - 110.f * S;

	DrawCenteredText(HelpTitle.ToString(), Top - 100.f * S, TextColor, S * 1.1f);

	// One thin line tying the cards together, dots at both ends. Badges sit on it.
	const float LineY = Top - 34.f * S;
	const float LineL = Left - 20.f * S;
	const float LineR = Left + TotalW + 20.f * S;
	const float Dot = 3.f * S;
	DrawSoftLine(LineL, LineY, LineR, LineY, LineColor);
	DrawRect(LineColor, LineL - Dot * 0.5f, LineY - Dot * 0.5f, Dot, Dot);
	DrawRect(LineColor, LineR - Dot * 0.5f, LineY - Dot * 0.5f, Dot, Dot);

	float X = Left;
	for (const FCard& Card : Cards)
	{
		DrawHelpCard(X, Top, CardW, CardH, Card.Key, Card.Name, Card.Description, Card.Stats);
		X += CardW + Gap;
	}

	// TANKS block: bottom-left, thin line on its left.
	const float BlockX = Left;
	const float BlockY = Top + CardH + 50.f * S;
	const float Inset = 14.f * S;
	float TW = 0.f, TH = 0.f;
	MeasureHUDText(TanksTitle.ToString(), Font, S * 0.8f, TW, TH);
	DrawHUDText(TanksTitle.ToString(), TextColor, BlockX + Inset, BlockY, Font, S * 0.8f);
	const float TextH = DrawRichText(TanksText.ToString(), BlockX + Inset, BlockY + TH + 6.f * S, 600.f * S, Font, S * 0.6f, MutedColor);
	DrawRect(LineColor, BlockX, BlockY, LineThickness1px(), TH + 6.f * S + TextH);
}

// ---------------------------------------------------------------- Heat

void AFPSHUD::DrawHeat(const UFPSWeaponComponent* Weapon)
{
	const float Heat = Weapon->GetHeatPercent();
	const bool bOverheated = Weapon->IsOverheated();
	if (Heat <= 0.01f && !bOverheated)
	{
		return; // cold gun: nothing on screen
	}

	const float S = GetUIScale();
	const float W = 90.f * S;
	const float H = 3.f * S;
	const float X = (Canvas->ClipX - W) * 0.5f;
	const float Y = Canvas->ClipY * 0.5f + 40.f * S;

	// White -> red as it heats up, red while locked out.
	const FLinearColor Fill = bOverheated ? DangerColor : FMath::Lerp(TextColor, DangerColor, Heat);

	DrawRect(BackColor, X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
	DrawRect(Fill, X, Y, W * Heat, H);

	if (bOverheated)
	{
		DrawCenteredText(TEXT("OVERHEATED"), Y + H + 6.f * S, DangerColor, S * 0.6f);
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

	const float Scale = GetUIScale();
	const float X = 40.f * Scale;
	const float W = 360.f * Scale;
	const float ShieldH = 10.f * Scale;
	const float HealthH = 18.f * Scale;
	const float HealthY = Canvas->ClipY - 60.f * Scale;
	const float ShieldY = HealthY - ShieldH - 4.f * Scale;

	DrawBar(X, ShieldY, W, ShieldH, ShownShield, GhostShield, ShieldColor);

	// Health turns red when low.
	const FLinearColor Health = TargetHealth < 0.3f ? DangerColor : HealthColor;
	DrawBar(X, HealthY, W, HealthH, ShownHealth, GhostHealth, Health);

	const FString Text = FString::Printf(TEXT("%.0f  |  %.0f"), AS->GetHealth(), AS->GetShield());
	UFont* BarFont = GetHUDFont(GEngine->GetMediumFont());
	float TW = 0.f;
	float TH = 0.f;
	MeasureHUDText(Text, BarFont, Scale * 0.9f, TW, TH);
	const float BarsCenterY = (ShieldY + HealthY + HealthH) * 0.5f;
	DrawHUDText(Text, TextColor, X + W + 12.f * Scale, BarsCenterY - TH * 0.5f, BarFont, Scale * 0.9f);
}

// ---------------------------------------------------------------- Abilities

void AFPSHUD::DrawAbilities(const AFPSHeroCharacter* Hero)
{
	UAbilitySystemComponent* ASC = Hero->GetAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	const float Scale = GetUIScale();
	const float Size = 64.f * Scale;
	const float Spacing = 12.f * Scale;
	float X = Canvas->ClipX - 40.f * Scale - Size;
	const float Y = Canvas->ClipY - 60.f * Scale - Size + 18.f * Scale;
	UFont* Font = GetHUDFont(GEngine->GetMediumFont());

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

		// Box: cut corners, thin frame, cooldown fill rising from the bottom.
		const float Cut = 8.f * Scale;
		DrawCutRect(X, Y, Size, Size, Cut, BackColor);
		if (Remaining > 0.f && Duration > 0.f)
		{
			const float Progress = 1.f - Remaining / Duration;
			const float FillH = (Size - Cut) * Progress;
			DrawRect(FLinearColor(1.f, 1.f, 1.f, 0.15f), X, Y + Size - FillH, Size, FillH);
		}
		else
		{
			DrawCutRect(X, Y, Size, Size, Cut, FLinearColor(1.f, 1.f, 1.f, 0.08f));
		}
		DrawFrame(X, Y, Size, Size, Cut, bReady ? AbilityReadyColor : LineColor);

		// Key badge on the top-left corner.
		const FString Key = Ability->HUDKey.ToString();
		const float KeyScale = FitTextScale(Key, Font, Scale * 0.5f, Size - 8.f * Scale);
		const FVector2D Badge = MeasureKeyBadge(Key, Font, KeyScale);
		DrawKeyBadge(Key, X + Badge.X * 0.5f, Y, Font, KeyScale);

		// Seconds left in the middle (only when nothing is ready).
		if (!bReady && Remaining > 0.f)
		{
			const FString Seconds = FString::Printf(TEXT("%.1f"), Remaining);
			float TW = 0.f;
			float TH = 0.f;
			const float SecScale = FitTextScale(Seconds, Font, Scale * 1.2f, Size * 0.8f);
			MeasureHUDText(Seconds, Font, SecScale, TW, TH);
			DrawHUDText(Seconds, TextColor, X + (Size - TW) * 0.5f, Y + (Size - TH) * 0.5f, Font, SecScale);
		}

		// Charge pips under the box.
		if (Ability->MaxCharges > 1)
		{
			const float Pip = 10.f * Scale;
			for (int32 i = 0; i < Ability->MaxCharges; ++i)
			{
				const FLinearColor PipColor = i < ChargesReady ? TextColor : LineColor;
				DrawRect(PipColor, X + i * (Pip + 4.f * Scale), Y + Size + 4.f * Scale, Pip, FMath::Max(1.f, Pip * 0.3f));
			}
		}

		float NW = 0.f;
		float NH = 0.f;
		const FString Name = Ability->HUDName.ToString();
		const float NameScale = FitTextScale(Name, Font, Scale * 0.6f, Size + Spacing * 0.5f);
		MeasureHUDText(Name, Font, NameScale, NW, NH);
		DrawHUDText(Name, MutedColor, X + (Size - NW) * 0.5f, Y - NH - Badge.Y * 0.5f - 4.f * Scale, Font, NameScale);

		X -= Size + Spacing;
	}
}

// ---------------------------------------------------------------- Waves / match

void AFPSHUD::DrawCenteredText(const FString& Text, float Y, const FLinearColor& Color, float Scale)
{
	UFont* Font = GetHUDFont(GEngine->GetLargeFont());
	float W = 0.f;
	float H = 0.f;
	MeasureHUDText(Text, Font, Scale, W, H);
	DrawHUDText(Text, Color, (Canvas->ClipX - W) * 0.5f, Y, Font, Scale);
}

void AFPSHUD::DrawMatchInfo(const AFPSHeroCharacter* Hero)
{
	const AFPSGameState* GS = GetWorld()->GetGameState<AFPSGameState>();
	if (!GS)
	{
		return;
	}

	const float Scale = GetUIScale();
	const float TopY = 30.f * Scale;
	const int32 Seconds = FMath::CeilToInt(GS->GetPhaseTimeRemaining());

	// Fallback only: the UMG wave widget takes over when it's set.
	if (!WaveWidget)
	{
	switch (GS->Phase)
	{
	case EFPSMatchPhase::WaitingToStart:
		DrawCenteredText(FString::Printf(TEXT("FIRST WAVE IN %d"), Seconds), TopY, FLinearColor::White, Scale);
		break;
	case EFPSMatchPhase::InWave:
		DrawCenteredText(FString::Printf(TEXT("WAVE %d / %d"), GS->CurrentWave, GS->TotalWaves), TopY, FLinearColor::White, Scale);
		DrawCenteredText(FString::Printf(TEXT("%d ENEMIES LEFT"), GS->EnemiesRemaining), TopY + 34.f * Scale * TextScale, MutedColor, Scale * 0.8f);
		break;
	case EFPSMatchPhase::Intermission:
		DrawCenteredText(FString::Printf(TEXT("WAVE %d CLEARED"), GS->CurrentWave), TopY, TextColor, Scale);
		DrawCenteredText(FString::Printf(TEXT("NEXT WAVE IN %d"), Seconds), TopY + 34.f * Scale * TextScale, FLinearColor::White, Scale * 0.8f);
		break;
	case EFPSMatchPhase::Victory:
		DrawCenteredText(TEXT("VICTORY"), Canvas->ClipY * 0.35f, TextColor, Scale * 2.5f);
		DrawCenteredText(FString::Printf(TEXT("Restarting in %d"), Seconds), Canvas->ClipY * 0.35f + 70.f * Scale * TextScale, FLinearColor::White, Scale);
		break;
	case EFPSMatchPhase::Defeat:
		DrawCenteredText(TEXT("DEFEAT"), Canvas->ClipY * 0.35f, DangerColor, Scale * 2.5f);
		DrawCenteredText(FString::Printf(TEXT("Restarting in %d"), Seconds), Canvas->ClipY * 0.35f + 70.f * Scale * TextScale, FLinearColor::White, Scale);
		break;
	}
	}

	// Local player respawn countdown.
	if (Hero->IsDead() && !GS->IsMatchOver())
	{
		const int32 RespawnIn = FMath::CeilToInt(FMath::Max(0.f, Hero->GetRespawnServerTime() - static_cast<float>(GS->GetServerWorldTimeSeconds())));
		DrawCenteredText(FString::Printf(TEXT("RESPAWN IN %d"), RespawnIn), Canvas->ClipY * 0.45f, DangerColor, Scale * 1.5f);
	}
}

// ---------------------------------------------------------------- Enemy health bars

void AFPSHUD::DrawEnemyBars()
{
	// One HUD pass for every enemy: no widget component per enemy (50 widgets = 50 ticks + draws).
	const APlayerCameraManager* Camera = PlayerOwner->PlayerCameraManager;
	if (!Camera)
	{
		return;
	}

	const FVector CameraLocation = Camera->GetCameraLocation();
	const FVector CameraForward = Camera->GetCameraRotation().Vector();
	const float MaxDistSq = EnemyBarMaxDistance * EnemyBarMaxDistance;
	const float UIScale = GetUIScale();
	UFont* Font = GetHUDFont(GEngine->GetSmallFont());

	for (TActorIterator<AFPSEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AFPSEnemyCharacter* Enemy = *It;
		// Only elites and the mini-boss get a bar + name (normal enemies = clutter in a horde).
		if (Enemy->IsHidden() || Enemy->IsCombatantDead() || Enemy->GetTier() == 0)
		{
			continue;
		}

		// Cheap rejects first: too far, or behind the camera.
		const FVector ToEnemy = Enemy->GetActorLocation() - CameraLocation;
		const float DistSq = ToEnemy.SizeSquared();
		if (DistSq > MaxDistSq || FVector::DotProduct(ToEnemy, CameraForward) <= 0.f)
		{
			continue;
		}

		const UFPSAttributeSet* AS = Enemy->GetAttributeSet();
		if (!AS || AS->GetMaxHealth() <= 0.f)
		{
			continue;
		}

		// Point just above the head (capsule already includes elite/boss scale).
		const FVector Above = Enemy->GetActorLocation() + FVector(0.f, 0.f, Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 25.f);
		const FVector Screen = Project(Above);
		if (Screen.Z <= 0.f || Screen.X < 0.f || Screen.X > Canvas->ClipX || Screen.Y < 0.f || Screen.Y > Canvas->ClipY)
		{
			continue;
		}

		// Shrink a bit with distance so far bars don't clutter.
		const float Dist = FMath::Sqrt(DistSq);
		const float DistScale = FMath::GetMappedRangeValueClamped(FVector2D(500.f, EnemyBarMaxDistance), FVector2D(1.f, 0.6f), Dist);
		const uint8 Tier = Enemy->GetTier();
		const float TierScale = Tier == 2 ? 2.2f : (Tier == 1 ? 1.4f : 1.f);

		const float W = EnemyBarWidth * TierScale * DistScale * UIScale;
		const float H = FMath::Max(3.f, 6.f * DistScale * UIScale * (Tier == 2 ? 1.5f : 1.f));
		const float X = Screen.X - W * 0.5f;
		const float Y = Screen.Y;
		const float Fill = FMath::Clamp(AS->GetHealth() / AS->GetMaxHealth(), 0.f, 1.f);

		DrawRect(BackColor, X - 1.f, Y - 1.f, W + 2.f, H + 2.f);
		DrawRect(EnemyHealthColor, X, Y, W * Fill, H);

		// Name: white / grey elite / red mini-boss.
		FString Name = Enemy->GetDisplayName().ToString();
		FLinearColor NameColor = FLinearColor(1.f, 1.f, 1.f, 0.85f);
		if (Tier == 1)
		{
			Name = TEXT("Elite ") + Name;
			NameColor = MutedColor;
		}
		else if (Tier == 2)
		{
			Name = TEXT("Mini-Boss ") + Name;
			NameColor = DangerColor;
		}

		const float NameScale = DistScale * UIScale * (Tier > 0 ? 1.1f : 0.9f);
		float TW = 0.f;
		float TH = 0.f;
		MeasureHUDText(Name, Font, NameScale, TW, TH);
		DrawHUDText(Name, NameColor, Screen.X - TW * 0.5f, Y - TH - 2.f, Font, NameScale);
	}
}
