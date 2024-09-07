
#include "BlasterComponents/CombatComponent.h"

#include "Character/BlasterCharacter.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
}

void UCombatComponent::EquipWeapon(AWeapon* WeaponToEquip)
{
	if (Character == nullptr || WeaponToEquip == nullptr) return;

	EquippedWeapon = WeaponToEquip;
	// 这里需要做属性复制
	EquippedWeapon->SetWeaponState(EWeaponState::EWS_Equipped);
	// 这里也是，信息会自动传播到客户端端
	if (const USkeletalMeshSocket* HandSocket = Character->GetMesh()->GetSocketByName(FName(TEXT("RightHandSocket"))))
	{
		HandSocket->AttachActor(EquippedWeapon, Character->GetMesh());
	}
	// 设置武器的拥有者是Character，这里已经给做了属性复制
	EquippedWeapon->SetOwner(Character);
}

