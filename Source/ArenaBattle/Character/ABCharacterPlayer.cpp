// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/ABCharacterPlayer.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputMappingContext.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ABCharacterControlData.h"
#include "ArenaBattle.h"
#include "UI/ABHUDWidget.h"
#include "CharacterStat/ABCharacterStatComponent.h"
#include "Interface/ABGameInterface.h"
#include "Components/CapsuleComponent.h"
#include "Physics/ABCollision.h"
#include "Engine/DamageEvents.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "EngineUtils.h"

#include "ABCharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/AssetManager.h"
#include "GameFramework/PlayerState.h"

AABCharacterPlayer::AABCharacterPlayer(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer.SetDefaultSubobjectClass<UABCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Camera
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Input
	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionJumpRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_Jump.IA_Jump'"));
	if (nullptr != InputActionJumpRef.Object)
	{
		JumpAction = InputActionJumpRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputChangeActionControlRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_ChangeControl.IA_ChangeControl'"));
	if (nullptr != InputChangeActionControlRef.Object)
	{
		ChangeControlAction = InputChangeActionControlRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionShoulderMoveRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_ShoulderMove.IA_ShoulderMove'"));
	if (nullptr != InputActionShoulderMoveRef.Object)
	{
		ShoulderMoveAction = InputActionShoulderMoveRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionShoulderLookRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_ShoulderLook.IA_ShoulderLook'"));
	if (nullptr != InputActionShoulderLookRef.Object)
	{
		ShoulderLookAction = InputActionShoulderLookRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionQuaterMoveRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_QuaterMove.IA_QuaterMove'"));
	if (nullptr != InputActionQuaterMoveRef.Object)
	{
		QuaterMoveAction = InputActionQuaterMoveRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionAttackRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_Attack.IA_Attack'"));
	if (nullptr != InputActionAttackRef.Object)
	{
		AttackAction = InputActionAttackRef.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction> InputActionTeleportRef(TEXT("/Script/EnhancedInput.InputAction'/Game/ArenaBattle/Input/Actions/IA_Teleport.IA_Teleport'"));
	if (nullptr != InputActionTeleportRef.Object)
	{
		TeleportAction = InputActionTeleportRef.Object;
	}

	

	CurrentCharacterControlType = ECharacterControlType::Quater;

	//시작할때는 공격이 가능 하도록 설정.
	bCanAttack = true;
	
	bReplicates = true;
}

void AABCharacterPlayer::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController)
	{
		EnableInput(PlayerController);
	}

	SetCharacterControl(CurrentCharacterControlType);
}

void AABCharacterPlayer::SetDead()
{
	Super::SetDead();

	GetWorld()->GetTimerManager().SetTimer(
		DeadTimerHandle,
		this,
		&AABCharacterPlayer::ResetPlayer,
		5.0f,
		false
		);

	// APlayerController* PlayerController = Cast<APlayerController>(GetController());
	// if (PlayerController)
	// {
	// 	DisableInput(PlayerController);
	// }
}

void AABCharacterPlayer::PossessedBy(AController* NewController)//Owner가 빙의 되는 함수.(클라이언트에서는 호출이 안된다.)
{
	//상위 로직에서 PlayerState 값이 컨트롤러부터 설정됨.
	Super::PossessedBy(NewController);

	//서버의 경우 로컬 플레이어를 PossessedBy를 통해서 진행.
	UpdateMeshFromPlayerState();
}

void AABCharacterPlayer::OnRep_Owner()
{
	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	Super::OnRep_Owner();

	//Super::OnRep_Owner() 함수 호출 후 오너 확인.
	AActor* OwnerActor = GetOwner();
	if (OwnerActor)
	{
		AB_LOG(LogABNetwork, Log, TEXT("Owner: %s"), *OwnerActor->GetName());
	}
	else
	{
		AB_LOG(LogABNetwork, Log, TEXT("No Owner"));
	}

	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("End"));
}

void AABCharacterPlayer::PostNetInit()
{
	AB_LOG(LogABNetwork, Log, TEXT("%s %s"), TEXT("Begin"), *GetName());
	
	Super::PostNetInit();
	
	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("End"));
}

float AABCharacterPlayer::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
	class AController* EventInstigator, AActor* DamageCauser)
{
	
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	//HP를 모두 소진하면 게임 모드에 알리기.
	if (Stat->GetCurrentHp() == 0.0f)
	{
		//게임 모드를 인터페이스로 접근.
		IABGameInterface* ABGameMode = Cast<IABGameInterface>(GetWorld()->GetAuthGameMode());

		if (ABGameMode)
		{
			ABGameMode->OnPlayerKilled(EventInstigator, GetController(), this);
		}
	}

	return ActualDamage;
}

void AABCharacterPlayer::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	UpdateMeshFromPlayerState();
}

void AABCharacterPlayer::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent);

	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ACharacter::Jump);
	EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	EnhancedInputComponent->BindAction(ChangeControlAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::ChangeCharacterControl);
	EnhancedInputComponent->BindAction(ShoulderMoveAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::ShoulderMove);
	EnhancedInputComponent->BindAction(ShoulderLookAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::ShoulderLook);
	EnhancedInputComponent->BindAction(QuaterMoveAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::QuaterMove);
	EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::Attack);
	EnhancedInputComponent->BindAction(TeleportAction, ETriggerEvent::Triggered, this, &AABCharacterPlayer::Teleport);
}

void AABCharacterPlayer::ChangeCharacterControl()
{
	if (CurrentCharacterControlType == ECharacterControlType::Quater)
	{
		SetCharacterControl(ECharacterControlType::Shoulder);
	}
	else if (CurrentCharacterControlType == ECharacterControlType::Shoulder)
	{
		SetCharacterControl(ECharacterControlType::Quater);
	}
}

void AABCharacterPlayer::SetCharacterControl(ECharacterControlType NewCharacterControlType)
{
	if (!IsLocallyControlled())
	{
		return;
	}

	UABCharacterControlData* NewCharacterControl = CharacterControlManager[NewCharacterControlType];
	check(NewCharacterControl);

	SetCharacterControlData(NewCharacterControl);

	APlayerController* PlayerController = CastChecked<APlayerController>(GetController());
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		Subsystem->ClearAllMappings();
		UInputMappingContext* NewMappingContext = NewCharacterControl->InputMappingContext;
		if (NewMappingContext)
		{
			Subsystem->AddMappingContext(NewMappingContext, 0);
		}
	}

	CurrentCharacterControlType = NewCharacterControlType;
}

void AABCharacterPlayer::SetCharacterControlData(const UABCharacterControlData* CharacterControlData)
{
	Super::SetCharacterControlData(CharacterControlData);

	CameraBoom->TargetArmLength = CharacterControlData->TargetArmLength;
	CameraBoom->SetRelativeRotation(CharacterControlData->RelativeRotation);
	CameraBoom->bUsePawnControlRotation = CharacterControlData->bUsePawnControlRotation;
	CameraBoom->bInheritPitch = CharacterControlData->bInheritPitch;
	CameraBoom->bInheritYaw = CharacterControlData->bInheritYaw;
	CameraBoom->bInheritRoll = CharacterControlData->bInheritRoll;
	CameraBoom->bDoCollisionTest = CharacterControlData->bDoCollisionTest;
}

void AABCharacterPlayer::ShoulderMove(const FInputActionValue& Value)
{
	if (!bCanAttack)
	{
		return;
	}
	
	FVector2D MovementVector = Value.Get<FVector2D>();

	const FRotator Rotation = Controller->GetControlRotation();
	const FRotator YawRotation(0, Rotation.Yaw, 0);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, MovementVector.X);
	AddMovementInput(RightDirection, MovementVector.Y);
}

void AABCharacterPlayer::ShoulderLook(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AABCharacterPlayer::QuaterMove(const FInputActionValue& Value)
{
	//공격이 불가능할 때는 이동을 못하도록 막기.
	if (!bCanAttack)
	{
		return;
	}
	
	FVector2D MovementVector = Value.Get<FVector2D>();

	float InputSizeSquared = MovementVector.SquaredLength();
	float MovementVectorSize = 1.0f;
	float MovementVectorSizeSquared = MovementVector.SquaredLength();
	if (MovementVectorSizeSquared > 1.0f)
	{
		MovementVector.Normalize();
		MovementVectorSizeSquared = 1.0f;
	}
	else
	{
		MovementVectorSize = FMath::Sqrt(MovementVectorSizeSquared);
	}

	FVector MoveDirection = FVector(MovementVector.X, MovementVector.Y, 0.0f);
	GetController()->SetControlRotation(FRotationMatrix::MakeFromX(MoveDirection).Rotator());
	AddMovementInput(MoveDirection, MovementVectorSize);
}

void AABCharacterPlayer::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//프로퍼티를 리플리케이션에 등록.
	DOREPLIFETIME(AABCharacterPlayer, bCanAttack);
}

void AABCharacterPlayer::Attack()
{
	//ProcessComboCommand();

	if (bCanAttack)
	{
		//클라이언트 로직.
		if (!HasAuthority())
		{
			//공격 다시 못하게 설정.
			bCanAttack = false;

			//bCanAttack 값이 변경되지 않아, 리플리케이션이 되지 않기때문에
			//OnRep_함수 호출을 기대하지 않고, 직접 모드 변경.
			GetCharacterMovement()->SetMovementMode(MOVE_None);

			//공격 종료를 위한 타이머도 클라이언트에서 설정.
			//고려 해야 할것: 캐릭터 무브먼트 설정이나 공격 시간 등은 클라이언트가 악의적으로 변경 할 수 있다는 사실을 가정해야한다.
			// GetWorldTimerManager().SetTimer(
			// 	Handle,
			// 	FTimerDelegate::CreateLambda([&]()
			// 	{
			// 		//공격이 끝나면 처리.
			// 		bCanAttack = true;
			// 		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			// 		
			// 	}), AttackTime, false);
			GetWorld()->GetTimerManager().SetTimer(
			AttackTimerHandle,
			this,
			&AABCharacterPlayer::ResetAttack,
			AttackTime,
			false
		 );

			//애니메이션 재생.
			PlayAttackAnimation();
		}
		
		//Server.
		//공격 입력이 들어오면 Server RPC를 호출.
		//이제는 공격을 서버에 알릴때 클라이언트가 요청한 시간을 서버에 전송함.
		//GetWorld()->GetTimeSeconds()함수는 현재 월드의 시간을 반환.
		//서버의 시간을 기준으로 해야함.
		ServerRPCAttack(GetWorld()->GetGameState()->GetServerWorldTimeSeconds());
	}
}

void AABCharacterPlayer::Teleport()
{
	AB_LOG(LogABTeleport, Log, TEXT("%s"), TEXT("Begin"));

	UABCharacterMovementComponent* ABMovement = Cast<UABCharacterMovementComponent>(GetCharacterMovement());

	if (ABMovement)
	{
		ABMovement->SetTeleportCommand();
	}
}

void AABCharacterPlayer::ResetPlayer()
{
	//애니메이션 정리.
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance)
	{
		AnimInstance->StopAllMontages(0.0f);
	}

	//현재 레벨을 초기값으로 되돌리기.
	Stat->SetLevelStat(1);

	//스탯 초기화.
	Stat->ResetStat();

	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	SetActorEnableCollision(true);

	HpBar->SetHiddenInGame(false);

	//서버 로직. 플레이어의 리스폰 위치를 설정.
	if (HasAuthority())
	{
		IABGameInterface* ABGameMode = Cast<IABGameInterface>(GetWorld()->GetGameState());
		if (ABGameMode)
		{
			FTransform NewTransform = ABGameMode->GetRandomStartTransform();
			TeleportTo(NewTransform.GetLocation(), NewTransform.GetRotation().Rotator());
		}
	}
}

void AABCharacterPlayer::ResetAttack()
{
	//공격이 끝나면 처리.
	bCanAttack = true;
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void AABCharacterPlayer::UpdateMeshFromPlayerState()
{
	//int32 RandIndex = FMath::RandRange(0, NPCMeshes.Num() - 1);

	//PlayerID를 활용해서 인덱스 값 설정.
	int32 MeshIndex = FMath::Clamp(GetPlayerState()->GetPlayerId() % PlayerMeshes.Num(), 0, PlayerMeshes.Num() - 1);
	
	
	MeshHandle = UAssetManager::Get().GetStreamableManager().RequestAsyncLoad(PlayerMeshes[MeshIndex], FStreamableDelegate::CreateUObject(this, &AABCharacterBase::MeshLoadCompleted));
}

void AABCharacterPlayer::PlayAttackAnimation()
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	//기존에 재생 중인 몽타주는 중지.
	AnimInstance->StopAllMontages(0.0f);
	//몽타주 애셋 재생.
	AnimInstance->Montage_Play(ComboActionMontage);
}

void AABCharacterPlayer::AttackHitCheck()
{
	//공격 판정은 서버에서만 실행.
	//if (!HasAuthority()) return;
	if (!IsLocallyControlled()) return;
	
	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	FHitResult OutHitResult;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(Attack), false, this);

	const float AttackRange = Stat->GetTotalStat().AttackRange;
	const float AttackRadius = Stat->GetAttackRadius();
	const float AttackDamage = Stat->GetTotalStat().Attack;
	const FVector Forward = GetActorForwardVector();
	const FVector Start = GetActorLocation() + Forward * GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector End = Start + Forward * AttackRange;

	bool HitDetected = GetWorld()->SweepSingleByChannel(OutHitResult, Start, End, FQuat::Identity, CCHANNEL_ABACTION, FCollisionShape::MakeSphere(AttackRadius), Params);

	//공격 판정 타이밍을 저장.
	float HitCheckTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();

	//클라이언트.
	if (!HasAuthority())
	{
		//판정을 보낼때 RPC에 여러 정보를 함께 보낸다.
		//이때 판정 시간도 보낸다.
		if (HitDetected)
		{
			//충돌 검출이 된 경우, 서버에 충돌이 발생했다고 알림.
			//이때 서버 기준 판정 시간도 함께 전송.
			ServerRPCNotifyHit(OutHitResult, HitCheckTime);
		}
		else
		{
			//충돌 검출이 안된 경우에도 서버에 전송.
			ServerRPCNotifyMiss(Start, End, Forward, HitCheckTime);
		}
	}
	//서버
	else
	{
		//디버그로 공격 범위 그려주기.
		FColor DebugColor = HitDetected ? FColor::Red : FColor::Green;
		DrawDebugAttackRange(DebugColor, Start, End, Forward);
		
		//서버인 경우에는 판정하지 않고 그대로 데미지 전달 로직 수행.
		if (HitDetected)
		{
			AttackHitConfirm(OutHitResult.GetActor());
		}
	}
}

void AABCharacterPlayer::DrawDebugAttackRange(const FColor& DrawColor, FVector TraceStart, FVector TraceEnd, FVector Forward)
{
#if ENABLE_DRAW_DEBUG

	const float AttackRange = Stat->GetTotalStat().AttackRange;
	const float AttackRadius = Stat->GetAttackRadius();

	FVector CapsuleOrigin = TraceStart + (TraceEnd - TraceStart) * 0.5f;
	float CapsuleHalfHeight = AttackRange * 0.5f;
	//FColor DrawColor = HitDetected ? FColor::Green : FColor::Red;

	DrawDebugCapsule(
		GetWorld(),
		CapsuleOrigin,
		CapsuleHalfHeight,
		AttackRadius,
		FRotationMatrix::MakeFromZ(Forward).ToQuat(),
		DrawColor,
		false,
		5.0f
	);
#endif
}

void AABCharacterPlayer::ServerRPCAttack_Implementation(float AttackStartTime)
{
	//서버에서 처리 할 일.
	//공격 중이라는 의미로 플래그 설정.
	bCanAttack = false;
	
	//공격중에는 이동 중지.
	OnRep_CanAttack();

	//서버-클라이언트의 시간 차이 기록.
	AttackTimeDifference = GetWorld()->GetTimeSeconds() - AttackStartTime;
	
	AB_LOG(LogABNetwork, Log, TEXT("LagTimeL %f"), AttackTimeDifference);

	//예외처리.
	//네트워크 상태가 너무 안좋은 경우, AttackTimeDifference 값이 너무 크게되면,
	//아래의 타이머가 동작하지 않을수 있기 때문에 최소한의 시간 값을 보정.
	AttackTimeDifference = FMath::Clamp(AttackTimeDifference, 0.0f, AttackTime - 0.01f);

	//타이머 설정.
	//FTimerHandle Handle;
	// GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([&]()
	// {
	// 	bCanAttack = true;
	// 	OnRep_CanAttack();
	// 	
	// }),AttackTime - AttackTimeDifference, false);
	GetWorld()->GetTimerManager().SetTimer(
		AttackTimerHandle,
		this,
		&AABCharacterPlayer::ResetAttack,
		AttackTime - AttackTimeDifference,
		false
		);

	//클라이언트가 공격 요청을 한 시간 값 저장.
	LastAttackStartTime = AttackStartTime;

	//서버도 애니메이션 재생.
	PlayAttackAnimation();
	
	//클라이언트가 공격 입력을 해 서버로 호출 요청 했을때 실행됨.
	//리슨 서버의 경우 로컬에서 실행되기 때문에 곧바로 실행됨.
	//그 외의 다른 클라이언트는 네트워크로부터 신호를 받아 실행됨.
	//MulticastRPCAttack();

	//기존에 Multicast > Client RPC로 변경.
	for (auto* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		//서버에 있는 플레이어 컨트롤러 거르기.
		if (PlayerController && GetController() != PlayerController)
		{
			//클라이언트 중에서 본인이 아닌지 확인.
			if (!PlayerController->IsLocalController())
			{
				//여기로 넘어온 플레이어 컨트롤러는 서버도 아니고, 본인 클라이언트도 아님.
				AABCharacterPlayer* OtherPlayer = Cast<AABCharacterPlayer>(PlayerController->GetPawn());

				if (OtherPlayer)
				{
					//Client RPC를 전송.
					OtherPlayer->ClientRPCPlayAnimation(this);
				}
			}
		}
	}
}

bool AABCharacterPlayer::ServerRPCAttack_Validate(float AttackStartTime)
{
	//공격 타이밍에 대한 검증 추가.
	//이전 공격 타이밍 값이 없으면 검증 안하고 통과.
	if (LastAttackStartTime == 0.0f)
	{
		return true;	
	}

	//이전에 기록된 공격 시간과 이번에 요청한 공격 시간과의 차이가 공격 애니메이션 길이보다 큰지 확인.
	//이 값이 공격 애니메이션 길이보다 작다면, 클라이언트를 의심해볼 수 있는 상황.
	return (AttackStartTime - LastAttackStartTime) > (AttackTime - 0.4f);
}

void AABCharacterPlayer::ClientRPCPlayAnimation_Implementation(AABCharacterPlayer* CharacterPlayer)
{
	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	if (CharacterPlayer)
	{
		CharacterPlayer->PlayAttackAnimation();
	}
}

void AABCharacterPlayer::AttackHitConfirm(AActor* HitActor)
{
	AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));

	//서버에서만 실행.
	if (HasAuthority())
	{
		//공격 데미지는 스탯에서.
		const float AttackDamage = Stat->GetTotalStat().Attack;

		FDamageEvent DamageEvent;
		
		HitActor->TakeDamage(AttackDamage, DamageEvent, GetController(),this);
	}
}

// void AABCharacterPlayer::ServerRPCAttack_Implementation(float AttackStartTime)
// {
// 	//서버에서 처리 할 일.
// 	//공격 중이라는 의미로 플래그 설정.
// 	bCanAttack = false;
// 	
// 	//공격중에는 이동 중지.
// 	OnRep_CanAttack();
//
// 	//서버-클라이언트의 시간 차이 기록.
// 	AttackTimeDifference = GetWorld()->GetTimeSeconds() - AttackStartTime;
// 	
// 	AB_LOG(LogABNetwork, Log, TEXT("LagTimeL %f"), AttackTimeDifference);
//
// 	//예외처리.
// 	//네트워크 상태가 너무 안좋은 경우, AttackTimeDifference 값이 너무 크게되면,
// 	//아래의 타이머가 동작하지 않을수 있기 때문에 최소한의 시간 값을 보정.
// 	AttackTimeDifference = FMath::Clamp(AttackTimeDifference, 0.0f, AttackTime - 0.01f);
//
// 	//타이머 설정.
// 	FTimerHandle Handle;
// 	GetWorld()->GetTimerManager().SetTimer(Handle,FTimerDelegate::CreateLambda([&]()
// 	{
// 		bCanAttack = true;
// 		OnRep_CanAttack();
// 		
// 	}),AttackTime - AttackTimeDifference, false);
//
// 	//클라이언트가 공격 요청을 한 시간 값 저장.
// 	LastAttackStartTime = AttackStartTime;
//
// 	//서버도 애니메이션 재생.
// 	PlayAttackAnimation();
// 	
// 	//클라이언트가 공격 입력을 해 서버로 호출 요청 했을때 실행됨.
// 	//리슨 서버의 경우 로컬에서 실행되기 때문에 곧바로 실행됨.
// 	//그 외의 다른 클라이언트는 네트워크로부터 신호를 받아 실행됨.
// 	//MulticastRPCAttack();
//
// 	//기존에 Multicast > Client RPC로 변경.
// 	for (auto* PlayerController : TActorRange<APlayerController>(GetWorld()))
// 	{
// 		//서버에 있는 플레이어 컨트롤러 거르기.
// 		if (PlayerController && GetController() != PlayerController)
// 		{
// 			//클라이언트 중에서 본인이 아닌지 확인.
// 			if (!PlayerController->IsLocalController())
// 			{
// 				//여기로 넘어온 플레이어 컨트롤러는 서버도 아니고, 본인 클라이언트도 아님.
// 				AABCharacterPlayer* OtherPlayer = Cast<AABCharacterPlayer>(PlayerController->GetPawn());
//
// 				if (OtherPlayer)
// 				{
// 					//Client RPC를 전송.
// 					OtherPlayer->ClientRPCPlayAnimation(this);
// 				}
// 			}
// 		}
// 	}
// }
//
// bool AABCharacterPlayer::ServerRPCAttack_Validate(float AttackStartTime)
// {
// 	//공격 타이밍에 대한 검증 추가.
// 	//이전 공격 타이밍 값이 없으면 검증 안하고 통과.
// 	if (LastAttackStartTime == 0.0f)
// 	{
// 		return true;	
// 	}
//
// 	//이전에 기록된 공격 시간과 이번에 요청한 공격 시간과의 차이가 공격 애니메이션 길이보다 큰지 확인.
// 	//이 값이 공격 애니메이션 길이보다 작다면, 클라이언트를 의심해볼 수 있는 상황.
// 	return (AttackStartTime - LastAttackStartTime) > AttackTime;
// }

void AABCharacterPlayer::MulticastRPCAttack_Implementation()
{
	//서버와 본인(입력 전달한) 클라이언트는 이미 공격 상태값과 무브먼트 모드 값을 변경함.
	//여기에서는 나머지 클라이언트에 대한 처리를 진행.
	//공격 상태 값과 무브먼트 모드 값은 프로퍼티 리플리케이션으로 전달되어 처리가 될것.
	//하지만, 공격 애니메이션은 재생하지 않았으니 재생해야함.
	//나머지 클라이언트인지 파악.
	if (!IsLocallyControlled())
	{
		PlayAttackAnimation();
	}
}

void AABCharacterPlayer::ServerRPCNotifyHit_Implementation(const FHitResult& HitResult, float HitCheckTime)
{
	//맞은 액터.
	AActor* HitActor = HitResult.GetActor();

	if (IsValid(HitActor))
	{
		//맞은 곳의 정보를 사용해 검증 진행.
		//맞은 위치 값.
		const FVector HitLocation = HitResult.Location;

		//맞은 캐릭터의 바운딩 박스 영역 가져오기.
		const FBox HitBox = HitActor->GetComponentsBoundingBox();

		//중심 위치.
		const FVector ActorBoxCenter = HitBox.GetCenter();

		//클라이언트가 보낸 정보를 기반으로,
		//맞은 위치와 맞은 액터 사이의 거리가 공격 가능 거리보다 작은지 비교.
		if (FVector::DistSquared(HitLocation, ActorBoxCenter) <= AttackCheckDistance * AttackCheckDistance)
		{
			//데미지 전달 기능 구현.
			AttackHitConfirm(HitActor);
			
		}
		else
		{
			AB_LOG(LogABNetwork, Log, TEXT("%s"), TEXT("HitResult Reject!"));
		}

		//Debug Draw로 정보 보여주기.
#if ENABLE_DRAW_DEBUG
		//맞은 액터의 위치를 하늘색으로 표시.(점으로)
		DrawDebugPoint(GetWorld(), ActorBoxCenter, 30.0f, FColor::Cyan, false, 5.0f);

		DrawDebugPoint(GetWorld(), HitLocation, 30.0f, FColor::Magenta, false, 5.0f);
#endif

		//디버그 드로우로 공격 영역 보여주기
		DrawDebugAttackRange(FColor::Red, HitResult.TraceStart, HitResult.TraceEnd, HitActor->GetActorForwardVector());

		
	}
}

bool AABCharacterPlayer::ServerRPCNotifyHit_Validate(const FHitResult& HitResult, float HitCheckTime)
{
	//클라이언트로부터 공격 관련 시간 값이 전달 안됬으면, 검증 안함.
	if (LastAttackStartTime == 0.0f)
		return true;

	//공격을 시작한 후 공격을 판정한 시간에 걸린 시간이 공격 판정 취소 기준을 넘어가는지 확인.
	return (HitCheckTime - LastAttackStartTime) > AcceptMinCheckTime;
}

bool AABCharacterPlayer::ServerRPCNotifyMiss_Validate(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal TraceDir,
	float HitCheckTime)
{
	//클라이언트로부터 공격 관련 시간 값이 전달 안됬으면, 검증 안함.
	if (LastAttackStartTime == 0.0f)
		return true;

	//공격을 시작한 후 공격을 판정한 시간에 걸린 시간이 공격 판정 취소 기준을 넘어가는지 확인.
	return (HitCheckTime - LastAttackStartTime) > AcceptMinCheckTime;
}

void AABCharacterPlayer::ServerRPCNotifyMiss_Implementation(FVector_NetQuantize TraceStart, FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal TraceDir,
	float HitCheckTime)
{
	DrawDebugAttackRange(FColor::Green, TraceStart, TraceEnd, TraceDir);
}

void AABCharacterPlayer::OnRep_CanAttack()
{
	if (bCanAttack)
	{
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_Walking);
	}
	else
	{
		GetCharacterMovement()->SetMovementMode(EMovementMode::MOVE_None);
	}
}


void AABCharacterPlayer::SetupHUDWidget(UABHUDWidget* InHUDWidget)
{
	if (InHUDWidget)
	{
		InHUDWidget->UpdateStat(Stat->GetBaseStat(), Stat->GetModifierStat());
		InHUDWidget->UpdateHpBar(Stat->GetCurrentHp(), Stat->GetMaxHp());

		Stat->OnStatChanged.AddUObject(InHUDWidget, &UABHUDWidget::UpdateStat);
		Stat->OnHpChanged.AddUObject(InHUDWidget, &UABHUDWidget::UpdateHpBar);
	}
}
