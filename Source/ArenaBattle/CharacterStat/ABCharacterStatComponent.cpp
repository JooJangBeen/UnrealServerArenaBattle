// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterStat/ABCharacterStatComponent.h"
#include "GameData/ABGameSingleton.h"
#include "Net/UnrealNetwork.h"
#include "ArenaBattle.h"

// Sets default values for this component's properties
UABCharacterStatComponent::UABCharacterStatComponent()
{
	CurrentLevel = 1;
	AttackRadius = 50.0f;

	bWantsInitializeComponent = true;

	//컴포넌트 리플리케이션 활성화.
	//SetIsReplicated(true);
}

void UABCharacterStatComponent::InitializeComponent()
{
	Super::InitializeComponent();
	
	ResetStat();

	//스탯 변경 이벤트에 최대 체력 설정 함수 등록.
	OnStatChanged.AddUObject(this, &UABCharacterStatComponent::SetNewMaxHp);

	//컴포넌트 리플리케이션 활성화.
	SetIsReplicated(true);
}

void UABCharacterStatComponent::BeginPlay()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	Super::BeginPlay();
}

void UABCharacterStatComponent::ReadyForReplication()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	Super::ReadyForReplication();
}

void UABCharacterStatComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//프로퍼티를 리플리케이션에 등록.

	//HP 프로퍼티는 모든 클라이언트에게 전송.
	DOREPLIFETIME(UABCharacterStatComponent, CurrentHp);
	DOREPLIFETIME(UABCharacterStatComponent, MaxHp);

	//캐릭터를 소유한 클라이언트만 전송하도록 설정.
	DOREPLIFETIME_CONDITION(UABCharacterStatComponent, BaseStat, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UABCharacterStatComponent, ModifierStat, COND_OwnerOnly);
}

void UABCharacterStatComponent::OnRep_CurrentHp()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	//서버로부터 받은 변경된 CurrentHP 정보를 델리게이트를 통해 알림.
	OnHpChanged.Broadcast(CurrentHp, MaxHp);

	//죽었는지 확인.
	if (CurrentHp <= KINDA_SMALL_NUMBER)
	{
		OnHpZero.Broadcast();
	}
}

void UABCharacterStatComponent::OnRep_MaxHp()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));
	
	//서버로부터 받은 변경된 CurrentHP 정보를 델리게이트를 통해 알림.
	OnHpChanged.Broadcast(CurrentHp, MaxHp);
}

void UABCharacterStatComponent::OnRep_BaseStat()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));

	//스탯 변경 이벤트 발행.
	OnStatChanged.Broadcast(BaseStat, ModifierStat);
}

void UABCharacterStatComponent::OnRep_ModifierStat()
{
	AB_SUBLOG(LogABNetwork, Log, TEXT("%s"), TEXT("Begin"));

	//스탯 변경 이벤트 발행.
	OnStatChanged.Broadcast(BaseStat, ModifierStat);
}

void UABCharacterStatComponent::SetLevelStat(int32 InNewLevel)
{
	CurrentLevel = FMath::Clamp(InNewLevel, 1, UABGameSingleton::Get().CharacterMaxLevel);
	SetBaseStat(UABGameSingleton::Get().GetCharacterStat(CurrentLevel));
	check(BaseStat.MaxHp > 0.0f);
}

float UABCharacterStatComponent::ApplyDamage(float InDamage)
{
	const float PrevHp = CurrentHp;
	const float ActualDamage = FMath::Clamp<float>(InDamage, 0, InDamage);

	SetHp(PrevHp - ActualDamage);
	if (CurrentHp <= KINDA_SMALL_NUMBER)
	{
		OnHpZero.Broadcast();
	}

	return ActualDamage;
}

void UABCharacterStatComponent::SetNewMaxHp(const FABCharacterStat& InBaseStat,
	const FABCharacterStat& InModifierStat)
{
	//MaxHp 값이 변경됐는지 검증.
	float PrevMaxHp = MaxHp;
	MaxHp = GetTotalStat().MaxHp;

	if (PrevMaxHp != MaxHp)
	{
		OnHpChanged.Broadcast(PrevMaxHp, MaxHp);
	}
}

void UABCharacterStatComponent::SetHp(float NewHp)
{
	CurrentHp = FMath::Clamp<float>(NewHp, 0.0f, MaxHp);
	
	OnHpChanged.Broadcast(CurrentHp, MaxHp);
}

void UABCharacterStatComponent::ResetStat()
{
	//현재 레벨을 불러와 레벨 데이터 설정.
	SetLevelStat(CurrentLevel);

	//설정된 스탯으로 초기화.
	MaxHp = BaseStat.MaxHp;
	SetHp(MaxHp);
}

