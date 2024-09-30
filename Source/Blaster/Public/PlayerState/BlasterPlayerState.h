// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "BlasterPlayerState.generated.h"

class ABlasterPlayerController;
class ABlasterCharacter;
/**
 * 
 */
UCLASS()
class BLASTER_API ABlasterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:

	// 属性复制函数，只会发生在客户端，不会发生在服务器端
	virtual void OnRep_Score() override;

	void AddToScore(float ScoreAmount);

private:

	// Player Controller内置了访问Player State的函数，但是Player State并没有直接访问Player Controller的函数
	ABlasterCharacter* Character;
	ABlasterPlayerController* Controller;
};
