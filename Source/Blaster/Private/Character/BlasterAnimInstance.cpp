// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterAnimInstance.h"

#include "Character/BlasterCharacter.h"
#include "Weapon/Weapon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UBlasterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
}

void UBlasterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (BlasterCharacter == nullptr)
	{
		BlasterCharacter = Cast<ABlasterCharacter>(TryGetPawnOwner());
	}
	if (BlasterCharacter == nullptr) return;

	FVector Velocity = BlasterCharacter->GetVelocity();
	Velocity.Z = 0.f;
	Speed = Velocity.Size();

	bIsInAir = BlasterCharacter->GetCharacterMovement()->IsFalling();
	bIsAccelerating = BlasterCharacter->GetCharacterMovement()->GetCurrentAcceleration().Size() > 0.f;
	bWeaponEquipped = BlasterCharacter->IsWeaponEquipped();
	EquippedWeapon = BlasterCharacter->GetWeapon();
	bIsCrouched = BlasterCharacter->bIsCrouched;
	bAiming = BlasterCharacter->IsAiming();
	TurningInPlace = BlasterCharacter->GetTurningInPlace();

	// 获得偏移量，Offset Yaw for Strafing
	// 基本目标旋转
	const FRotator AimRotation = BlasterCharacter->GetBaseAimRotation();
	// 与我们的运动相对应的旋转
	const FRotator MovementRotation = UKismetMathLibrary::MakeRotFromX(BlasterCharacter->GetVelocity());
	// 为了动画更平滑，应该插值，但是如果使用混合空间自带的插值，在后退时会造成在-180和180之间插值，很难看
	const FRotator DeltaRot = UKismetMathLibrary::NormalizedDeltaRotator(MovementRotation, AimRotation);
	DeltaRotation = FMath::RInterpTo(DeltaRotation, DeltaRot, DeltaSeconds, 6.f);
	YawOffset = DeltaRotation.Yaw;
	
	// 倾斜是两帧之间角色本身旋转的增量
	CharacterRotationLastFrame = CharacterRotation;
	CharacterRotation = BlasterCharacter->GetActorRotation();
	// 得到的值会非常小
	const FRotator Delta = UKismetMathLibrary::NormalizedDeltaRotator(CharacterRotation, CharacterRotationLastFrame);
	// 帧率独立性，扩大该值，且与DeltaSeconds成正比
	const float Target = Delta.Yaw / DeltaSeconds;
	const float Interp = FMath::FInterpTo(Lean, Target, DeltaSeconds, 6.f);
	Lean = FMath::Clamp(Interp, -90.f, 90.f);

	AO_Yaw = BlasterCharacter->GetAO_Yaw();
	AO_Pitch = BlasterCharacter->GetAO_Pitch();

	if (bWeaponEquipped && EquippedWeapon && EquippedWeapon->GetWeaponMesh() && BlasterCharacter->GetMesh())
	{
		// 从装备的武器上的插槽获取插槽变换，在世界空间坐标系下
		LeftHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName(TEXT("LeftHandSocket")), RTS_World);
		// 将其转换为骨骼上的骨骼空间
		// 右手拿武器，位置不变，以它为根，获取武器插槽相对于右手的位置
		FVector OutPosition;
		FRotator OutRotation;
		BlasterCharacter->GetMesh()->TransformToBoneSpace(FName(TEXT("hand_r")), LeftHandTransform.GetLocation(), FRotator::ZeroRotator, OutPosition, OutRotation);
		// 将武器插槽相对于hand_r的位置信息(在骨骼空间下)记录在LeftHandTransform中
		LeftHandTransform.SetLocation(OutPosition);
		LeftHandTransform.SetRotation(FQuat(OutRotation));

		// 由于AttachActor函数，使得EquippedWeapon的骨骼与角色骨骼有了关联
		// 右手虽然拿武器的位置对了，但是旋转不对，所以需要对右手骨骼进行旋转
		// 只需要本地玩家调整就行，因为本地玩家的视口中枪口方向和目标方向差距最大，在其他玩家视口中，并不关心该玩家的该信息，所以不必浪费带宽做属性复制
		if (BlasterCharacter->IsLocallyControlled())
		{
			bLocallyControlled = true;
			const FTransform RightHandTransform = EquippedWeapon->GetWeaponMesh()->GetSocketTransform(FName("hand_r"), RTS_World);
			RightHandRotation = UKismetMathLibrary::FindLookAtRotation(RightHandTransform.GetLocation(), RightHandTransform.GetLocation() + (RightHandTransform.GetLocation() - BlasterCharacter->GetHitTarget()));
		}
	}
}
