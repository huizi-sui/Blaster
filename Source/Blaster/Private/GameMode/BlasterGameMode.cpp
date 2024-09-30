
#include "GameMode/BlasterGameMode.h"

#include "Character/BlasterCharacter.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "PlayerState/BlasterPlayerState.h"

void ABlasterGameMode::PlayerEliminated(ABlasterCharacter* EliminatedCharacter, ABlasterPlayerController* VictimController, ABlasterPlayerController* AttackerController)
{
	ABlasterPlayerState* AttackerPlayerState = AttackerController ? AttackerController->GetPlayerState<ABlasterPlayerState>() : nullptr;
	const ABlasterPlayerState* VictimPlayerState = VictimController ? VictimController->GetPlayerState<ABlasterPlayerState>() : nullptr;

	if (AttackerPlayerState && AttackerPlayerState != VictimPlayerState)
	{
		AttackerPlayerState->AddToScore(1.f);
	}
	
	if (EliminatedCharacter)
	{
		EliminatedCharacter->Elim();
	}
}

void ABlasterGameMode::RequestRespawn(ACharacter* ElimmedCharacter, AController* ElimmedController)
{
	if (ElimmedCharacter)
	{
		// 该函数本质上将角色和控制器分离，为Controller调用UnPossess
		ElimmedCharacter->Reset();
		ElimmedCharacter->Destroy();
	}
	if (ElimmedController)
	{
		TArray<AActor*> PlayerStarts; 
		UGameplayStatics::GetAllActorsOfClass(this, APlayerStart::StaticClass(), PlayerStarts);
		const int32 Selection = FMath::RandRange(0, PlayerStarts.Num() - 1);
		RestartPlayerAtPlayerStart(ElimmedController, PlayerStarts[Selection]);
	}
}
