
#include "HUD/BlasterHUD.h"

#include "HUD/CharacterOverlay.h"

void ABlasterHUD::BeginPlay()
{
	Super::BeginPlay();

	AddCharacterOverlay();
}

void ABlasterHUD::AddCharacterOverlay()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController && CharacterOverlayClass)
	{
		CharacterOverlay = CreateWidget<UCharacterOverlay>(PlayerController, CharacterOverlayClass);
		CharacterOverlay->AddToViewport();
	}
}

void ABlasterHUD::DrawHUD()
{
	Super::DrawHUD();
	
	if (GEngine)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		const FVector2D ViewportCenter(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);

		const float SpreadScaled = CrosshairSpreadMax * HUDPackage.CrosshairSpread;
		
		if (HUDPackage.CrosshairsCenter)
		{
			const FVector2d Spread(0.f, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsCenter, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsLeft)
		{
			const FVector2d Spread(-SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsLeft, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsRight)
		{
			const FVector2d Spread(SpreadScaled, 0.f);
			DrawCrosshair(HUDPackage.CrosshairsRight, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsTop)
		{
			const FVector2d Spread(0.f, -SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsTop, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
		if (HUDPackage.CrosshairsBootom)
		{
			const FVector2d Spread(0.f, SpreadScaled);
			DrawCrosshair(HUDPackage.CrosshairsBootom, ViewportCenter, Spread, HUDPackage.CrosshairsColor);
		}
	}

}

void ABlasterHUD::DrawCrosshair(UTexture2D* Texture, FVector2D ViewportCenter, FVector2D Spread, FLinearColor CrosshairsColor)
{
	const float TextureWidget = Texture->GetSizeX();
	const float TextureHeight = Texture->GetSizeY();
	const FVector2D TextureDrawPoint(
		ViewportCenter.X - TextureWidget * 0.5 + Spread.X,
		ViewportCenter.Y - TextureHeight * 0.5 + Spread.Y);
	
	DrawTexture(
		Texture,
		TextureDrawPoint.X,
		TextureDrawPoint.Y,
		TextureWidget,
		TextureHeight,
		0.f,
		0.f,
		1.f,
		1.f,
		CrosshairsColor);
}
