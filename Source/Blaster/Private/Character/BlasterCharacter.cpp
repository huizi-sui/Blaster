// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterCharacter.h"

#include "BlasterComponents/CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"


ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetMesh());
	CameraBoom->TargetArmLength = 600.f;
	// 当按下移动按键时，旋转控制器和相机臂。
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	// FollowCamera已经附加到CameraBoom上，跟随CameraBoom移动，所以FollowCamera不需要在跟随PawnControllerRotation
	FollowCamera->bUsePawnControlRotation = false;

	// 不希望角色与控制器一起旋转
	bUseControllerRotationYaw = false;
	// 使用运动方向控制视觉方向
	GetCharacterMovement()->bOrientRotationToMovement = true;

	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	// 对于ActorComponent来说，不需要在GetLifetimeReplicatedProps中注册，只需要设置为可复制。
	Combat->SetIsReplicated(true);

	// 允许玩家下蹲
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AimOffset(DeltaTime);
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 后面重写该函数
	PlayerInputComponent->BindAction(FName(TEXT("Jump")), IE_Pressed, this, &ThisClass::Jump);

	PlayerInputComponent->BindAxis(FName(TEXT("MoveForward")), this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis(FName(TEXT("MoveRight")), this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis(FName(TEXT("Turn")), this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis(FName(TEXT("Lookup")), this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction(FName(TEXT("Equip")), IE_Pressed, this, &ThisClass::EquipButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Crouch")), IE_Pressed, this, &ThisClass::CrouchButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Aim")), IE_Pressed, this, &ThisClass::AimButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Aim")), IE_Released, this, &ThisClass::AimButtonReleased);
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::MoveForward(float Value)
{
	if (Controller != nullptr && Value != 0.f)
	{
		// GetActorForwardVector()：得到的是根组件的前向向量，即胶囊
		// 但是这里要改变的是控制器的旋转
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		// 得到前向向量
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::MoveRight(float Value)
{
	if (Controller != nullptr && Value != 0.f)
	{
		const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
		// 得到向右向量
		const FVector Direction(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y));
		AddMovementInput(Direction, Value);
	}
}

void ABlasterCharacter::Turn(float Value)
{
	// 鼠标左右移动，控制转向
	AddControllerYawInput(Value);
}

void ABlasterCharacter::LookUp(float Value)
{
	// 鼠标前后移动，控制向上看或向下看
	AddControllerPitchInput(Value);
}

void ABlasterCharacter::EquipButtonPressed()
{
	// 装备武器之类的事情应该由服务器完成，服务器有权限，它应该负责
	if (Combat)
	{
		// ServerEquipButtonPressed();
		// 如果是服务器直接装备武器，如果是客户端，则通过RPC通知服务器装备武器
		if (HasAuthority())
		{
			Combat->EquipWeapon(OverlappingWeapon);
		}
		else
		{
			ServerEquipButtonPressed();
		}
	}
}

void ABlasterCharacter::CrouchButtonPressed()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
}

void ABlasterCharacter::AimButtonPressed()
{
	if (Combat)
	{
		Combat->SetAiming(true);
	}
}

void ABlasterCharacter::AimButtonReleased()
{
	if (Combat)
	{
		Combat->SetAiming(false);
	}
}

void ABlasterCharacter::AimOffset(float DeltaTime)
{
	// 仅在装备武器时，使用瞄准偏移
	if (Combat && Combat->EquippedWeapon == nullptr) return;
	
	FVector Velocity = GetVelocity();
	Velocity.Z = 0;
	float Speed = Velocity.Size();
	bool bIsAir = GetCharacterMovement()->IsFalling();

	// standing still, not jumping
	if (Speed == 0.f && !bIsAir)
	{
		const FRotator CurrentAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		// 计算偏移量
		const FRotator DeltaAimRotation = UKismetMathLibrary::NormalizedDeltaRotator(CurrentAimRotation, StartingAimRotation);
		AO_Yaw = DeltaAimRotation.Yaw;
		if (TurningInPlace == ETurningInPlace::ETIP_NotTurning)
		{
			InterpAO_Yaw = AO_Yaw;
		}
		bUseControllerRotationYaw = true;
		// 根据偏移量判断角色是否需要向左或向右转动
		TurnInPlace(DeltaTime);
	}
	// running or jumping
	if (Speed || bIsAir)
	{
		// 设置起始目标旋转变量
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		// 正在跑或跳时不需要转身
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	// 在客户端向下看，Pitch < 0，但是在对应的Server中的Pawn上，Pitch是270-360之间
	// 这是因为Pitch数据在网络传递时（从客户端到传递到服务器端），压缩为无符号数
	AO_Pitch = GetBaseAimRotation().Pitch;
	if (AO_Pitch > 90.f && !IsLocallyControlled())
	{
		// map pitch from [270, 360) to [-90, 0)
		const FVector2D InRange(270.f, 360.f);
		const FVector2D OutRange(-90.f, 0.f);
		AO_Pitch = FMath::GetMappedRangeValueClamped(InRange, OutRange, AO_Pitch);
	}
}

void ABlasterCharacter::TurnInPlace(float DeltaTime)
{
	if (AO_Yaw > 90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Right;
	}
	else if (AO_Yaw < -90.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_Left;
	}
	if (TurningInPlace != ETurningInPlace::ETIP_NotTurning)
	{
		InterpAO_Yaw = FMath::FInterpTo(InterpAO_Yaw, 0.f, DeltaTime, 4.f);
		AO_Yaw = InterpAO_Yaw;
		if (FMath::Abs(AO_Yaw) < 15.f)
		{
			TurningInPlace = ETurningInPlace::ETIP_NotTurning;
			StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		} 
	}
}

void ABlasterCharacter::ServerEquipButtonPressed_Implementation()
{
	if (Combat)
	{
		Combat->EquipWeapon(OverlappingWeapon);
	}
}

// 仅仅发生在客户端
void ABlasterCharacter::OnRep_OverlappingWeapon(const AWeapon* LastWeapon)
{
	if (OverlappingWeapon)
	{
		OverlappingWeapon->ShowPickupWidget(true);
	}
	if (LastWeapon)
	{
		LastWeapon->ShowPickupWidget(false);
	}
}

// 仅仅发生在服务器端
void ABlasterCharacter::SetOverlappingWeapon(AWeapon* Weapon)
{
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(false);
		}
	}
	// 这是发生在Server，如果该Pawn由Server操控，则显示Pickup Widget
	OverlappingWeapon = Weapon;
	if (IsLocallyControlled())
	{
		if (OverlappingWeapon)
		{
			OverlappingWeapon->ShowPickupWidget(true);
		}
	}
}

bool ABlasterCharacter::IsWeaponEquipped() const
{
	return Combat && Combat->EquippedWeapon;
}

bool ABlasterCharacter::IsAiming() const
{
	return Combat && Combat->bAiming;
}

AWeapon* ABlasterCharacter::GetWeapon() const
{
	if (Combat == nullptr) return nullptr;
	return Combat->EquippedWeapon;
}

