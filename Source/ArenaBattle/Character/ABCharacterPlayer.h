// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ABCharacterBase.h"
#include "InputActionValue.h"
#include "Interface/ABCharacterHUDInterface.h"
#include "ABCharacterPlayer.generated.h"

/**
 * 
 */
UCLASS(Config=ArenaBattle)
class ARENABATTLE_API AABCharacterPlayer : public AABCharacterBase, public IABCharacterHUDInterface
{
	GENERATED_BODY()
	
public:
	AABCharacterPlayer(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void SetDead() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Owner() override;
	virtual void PostNetInit() override;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

	//플레이어 스테이트가 클라이언트에 동기화 될때 호출.
	virtual void OnRep_PlayerState() override;

public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

// Character Control Section
protected:
	void ChangeCharacterControl();
	void SetCharacterControl(ECharacterControlType NewCharacterControlType);
	virtual void SetCharacterControlData(const class UABCharacterControlData* CharacterControlData) override;

// Camera Section
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UCameraComponent> FollowCamera;

// Input Section
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> ChangeControlAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> ShoulderMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> ShoulderLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> QuaterMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> AttackAction;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<class UInputAction> TeleportAction;

	void ShoulderMove(const FInputActionValue& Value);
	void ShoulderLook(const FInputActionValue& Value);

	void QuaterMove(const FInputActionValue& Value);

	ECharacterControlType CurrentCharacterControlType;

	//프로퍼티 리플리케이션
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	
	void Attack();

	//공격 애니메이션을 재생하는 함수.
	void PlayAttackAnimation();
	
	virtual void AttackHitCheck() override;

	//공격 판정 확인 함수.
	void AttackHitConfirm(AActor* HitActor);

	//Debug Draw 함수.
	void DrawDebugAttackRange(const FColor& DrawColor, FVector TraceStart, FVector TraceEnd, FVector Forward);

	//공격 명령 처리를 위한 Server RPC 선언.
	UFUNCTION(Server, Unreliable, WithValidation)
	void ServerRPCAttack(float AttackStartTime);

	//클라이언트에 공격 명령 전달을 위한 멀티캐스트 RPC 선언.
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCAttack();

	UFUNCTION(Client, Unreliable)
	void ClientRPCPlayAnimation(AABCharacterPlayer* CharacterPlayer);
	

	//클라이언트에서 액터가 맞았을때 서버로 판정을 보내는 함수.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPCNotifyHit(const FHitResult& HitResult, float HitCheckTime);

	//클라이언트에서 충돌 판정 후 미스가 발생했을때 보내는 함수.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerRPCNotifyMiss(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal TraceDir, float HitCheckTime);
	
	UFUNCTION()
	void OnRep_CanAttack();

	//현재 공격 중인지를 판단할 때 사용할 변수.
	UPROPERTY(ReplicatedUsing= OnRep_CanAttack)
	uint8 bCanAttack : 1;
	
	//첫 뻔째 공격 애니메이션의 재생 길이 값.
	//타이머를 사용해서 이 시간이 지나면, 공격을 종료 하도록 처리.
	float AttackTime = 1.4667f;

	//시간 관련 변수.
	//클라이언트가 공격한 시간을 기록하기 위한 변수.
	float LastAttackStartTime = 0.0f;

	//클라이언트와 서버의 시간 차를 기록하기 위한 변수.
	float AttackTimeDifference = 0.0f;

	//공격 판정에 사용할 거리 값.
	float AttackCheckDistance = 300.0f;

	//공격을 시작한 후에 이정도 시간은 지나야 판정이 가능하다고 판단할 기준값.
	float AcceptMinCheckTime = 0.15f;

	// UI Section
protected:
	virtual void SetupHUDWidget(class UABHUDWidget* InHUDWidget) override;

	//Teleport Section.
protected:
	//텔레포트 입력이 눌렀을때 바인딩을 통해 호출될 함수.
	void Teleport();

	//PVP Section.
public:
	//캐릭터가 죽었을때 리셋하는 함수.
	void ResetPlayer();

	//공격을 해제할 때 사용할 함수.
	void ResetAttack();

	//플레이어 스테이트로부터 메시 정보를 업데이트 할때 사용할 함수.
	void UpdateMeshFromPlayerState();

	//리스폰 관련 처리를 위한 타이머 핸들.
	FTimerHandle AttackTimerHandle;
	FTimerHandle DeadTimerHandle;

	UPROPERTY(config)
	TArray<FSoftObjectPath> PlayerMeshes;
};
