// Fill out your copyright notice in the Description page of Project Settings.


#include "GameMode/LobbyGameMode.h"

#include "GameFramework/GameStateBase.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// PostLogin是第一个可以安全访问刚刚加入的PlayerController的地方。
	const int32 NumberOfPlayers = GameState->PlayerArray.Num();
	// 如果玩家数量达到一定值，则服务器端将会带着所有客户端一起travel到一个新的Level
	if (NumberOfPlayers == 2)
	{
		// GameMode仅仅存在于服务器上
		if (UWorld* World = GetWorld())
		{
			// 开启无缝传输，客户端不需要断开连接后重连
			bUseSeamlessTravel = true;
			World->ServerTravel(FString(TEXT("/Game/Maps/BlasterMap?listen")));
		}
	}
}
