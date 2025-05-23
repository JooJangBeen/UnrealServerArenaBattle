// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ABGameInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UABGameInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ARENABATTLE_API IABGameInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	//게임 모드에서 리프손 처리를 할 때 랜듬으로 스폰 위치를 가져올 때 사용할 함수.
	virtual FTransform GetRandomStartTransform() const = 0;
	
	//누가 누구를 죽였는지 알 수 있도록 함수 확장.
	virtual void OnPlayerKilled(AController* killer, AController* KilledPlayer, APawn* KilledPawn) = 0;
};
