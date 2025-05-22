// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ABCharacterMovementComponent.h"

#include "ArenaBattle.h"
#include "GameFramework/Character.h"


UABCharacterMovementComponent::UABCharacterMovementComponent()
{
	//텔레포트 이동 거리 값(6미터).
	TeleportOffset = 600.0f;

	//텔레포트 쿨타임 3초.
	TeleportCoolTime = 3.0f;

	//Bool 값은 false로 초기화.
	bPressedTeleport = false;
	bDidTeleport = false;
}

void UABCharacterMovementComponent::SetTeleportCommand()
{
	//텔레포트 입력이 들어오면, True로 설정.
	bPressedTeleport = true;
}

void UABCharacterMovementComponent::ABTeleport()
{
	//텔레포트 이동 처리.
	if (CharacterOwner)
	{
		//로그 출력.
		AB_SUBLOG(LogABTeleport, Log, TEXT("%s"), TEXT("Teleport Begin"));

		//텔레포트 위치.
		FVector TargetLocation = CharacterOwner->GetActorLocation() + CharacterOwner->GetActorForwardVector() * TeleportOffset;
		
		//텔레포트 처리.
		CharacterOwner->TeleportTo(TargetLocation, CharacterOwner->GetActorRotation());

		//쿨타임 적용을 위한 설정.
		bDidTeleport = true;

		//쿨타임 진행을 위한 타이머 설정.
		FTimerHandle TeleportTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(TeleportTimerHandle, FTimerDelegate::CreateLambda([&]()
		{
			//쿨타임 끝.
			bDidTeleport = false;
			
			AB_SUBLOG(LogABTeleport, Log, TEXT("%s"), TEXT("Teleport End"));
			
		}), TeleportCoolTime, false);
	}
}

void UABCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation,
	const FVector& OldVelocity)
{
	//안에서 하는 일이 없다.
	//Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	//텔레포트 실행 조건을 확인한 후 텔레포트 처리.
	if (bPressedTeleport && !bDidTeleport)
	{
		ABTeleport();
	}

	if (bPressedTeleport)
	{
		bPressedTeleport = false;
	}
}

void UABCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bPressedTeleport = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
	bDidTeleport = ((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);

	//조건 확인 후에 텔레포트 처리.
	//서버측.
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		if (bPressedTeleport && !bDidTeleport)
		{
			AB_SUBLOG(LogABTeleport, Log, TEXT("%s"), TEXT("Begin"));

			//텔레포트 실행.
			ABTeleport();
		}
	}
	
}

void FABSavedMove_Character::Clear()
{
	Super::Clear();

	//초기화.
	bPressedTeleport = false;
	bDidTeleport = false;
}

void FABSavedMove_Character::SetInitialPosition(ACharacter* Character)
{
	Super::SetInitialPosition(Character);

	UABCharacterMovementComponent* ABMovement = Cast<UABCharacterMovementComponent>(Character->GetCharacterMovement());

	if (ABMovement)
	{
		//현재 캐릭터 무브먼트 컴포넌트가 텔레포트를 처리하기 위해 관리하는 값을 그대로 설정.
		//이후에 서버 RPC를 통해 서버로 전달됨.
		bPressedTeleport = ABMovement->bPressedTeleport;
		bDidTeleport = ABMovement->bDidTeleport;
	}
}

uint8 FABSavedMove_Character::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	//서버 RPC를 통해서 데이터를 전달할 때 압축해서 전달함.
	//특히 특수 움직임의 경우 플래그 값을 바이트로 압축함.
	if (bPressedTeleport)
	{
		Result |= FLAG_Custom_0;
	}

	if (bDidTeleport)
	{
		Result |= FLAG_Custom_1;
	}
	
	return Result;
}

class FNetworkPredictionData_Client* UABCharacterMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UABCharacterMovementComponent* MutableThis = const_cast<UABCharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FABNetworkPredictionData_Client_Character(*this);
	}

	return ClientPredictionData;
}

FABNetworkPredictionData_Client_Character::FABNetworkPredictionData_Client_Character(
	const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
{
	
	
}

FSavedMovePtr FABNetworkPredictionData_Client_Character::AllocateNewMove()
{
	return FSavedMovePtr(new FABSavedMove_Character());;
}
