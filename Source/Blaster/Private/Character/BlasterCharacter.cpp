// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterCharacter.h"

#include "BlasterComponents/CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/Weapon.h"


ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

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
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
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

