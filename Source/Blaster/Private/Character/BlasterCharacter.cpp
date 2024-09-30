// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/BlasterCharacter.h"

#include "Blaster/Blaster.h"
#include "BlasterComponents/CombatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameMode/BlasterGameMode.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystemComponent.h"
#include "PlayerController/BlasterPlayerController.h"
#include "PlayerState/BlasterPlayerState.h"
#include "Sound/SoundCue.h"
#include "Weapon/Weapon.h"


ABlasterCharacter::ABlasterCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;
	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

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
	GetCharacterMovement()->RotationRate = FRotator(0.f, 850.f, 0.f);
	OverheadWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadWidget"));
	OverheadWidget->SetupAttachment(RootComponent);

	Combat = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
	// 对于ActorComponent来说，不需要在GetLifetimeReplicatedProps中注册，只需要设置为可复制。
	Combat->SetIsReplicated(true);

	// 允许玩家下蹲
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetMesh()->SetCollisionObjectType(ECC_SkeletalMesh);
	
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	NetUpdateFrequency = 66.f;
	MinNetUpdateFrequency = 33.f;

	DissolveTimeline = CreateDefaultSubobject<UTimelineComponent>(TEXT("DissolveTimelineComponent"));
}

void ABlasterCharacter::Destroyed()
{
	Super::Destroyed();
	if (ElimBotComponent)
	{
		ElimBotComponent->DestroyComponent();
	}
}

void ABlasterCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateHUDHealth();
	if (HasAuthority())
	{
		OnTakeAnyDamage.AddDynamic(this, &ThisClass::ReceiveDamage);
	}
}

void ABlasterCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 只有自主代理端和服务器端可以旋转根骨骼
	if (GetLocalRole() > ROLE_SimulatedProxy && IsLocallyControlled())
	{
		AimOffset(DeltaTime);
	}
	else
	{
		TimeSinceLastMovementReplication += DeltaTime;
		if (TimeSinceLastMovementReplication > 0.25f)
		{
			OnRep_ReplicatedMovement();
		}
		CalculateAO_Pitch();
	}
	HideCameraIfCharacterClose();
	PollInit();
}

void ABlasterCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerInputComponent->BindAction(FName(TEXT("Jump")), IE_Pressed, this, &ThisClass::Jump);

	PlayerInputComponent->BindAxis(FName(TEXT("MoveForward")), this, &ThisClass::MoveForward);
	PlayerInputComponent->BindAxis(FName(TEXT("MoveRight")), this, &ThisClass::MoveRight);
	PlayerInputComponent->BindAxis(FName(TEXT("Turn")), this, &ThisClass::Turn);
	PlayerInputComponent->BindAxis(FName(TEXT("Lookup")), this, &ABlasterCharacter::LookUp);

	PlayerInputComponent->BindAction(FName(TEXT("Equip")), IE_Pressed, this, &ThisClass::EquipButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Crouch")), IE_Pressed, this, &ThisClass::CrouchButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Aim")), IE_Pressed, this, &ThisClass::AimButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Aim")), IE_Released, this, &ThisClass::AimButtonReleased);
	PlayerInputComponent->BindAction(FName(TEXT("Fire")), IE_Pressed, this, &ThisClass::FireButtonPressed);
	PlayerInputComponent->BindAction(FName(TEXT("Fire")), IE_Released, this, &ThisClass::FireButtonReleased);
}

void ABlasterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABlasterCharacter, OverlappingWeapon, COND_OwnerOnly);
	DOREPLIFETIME(ABlasterCharacter, Health);
}

void ABlasterCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	if (Combat)
	{
		Combat->Character = this;
	}
}

void ABlasterCharacter::OnRep_ReplicatedMovement()
{
	Super::OnRep_ReplicatedMovement();
	SimProxiesTurn();
	TimeSinceLastMovementReplication = 0.f;
}

void ABlasterCharacter::PlayFireMontage(bool bAiming) const
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	// 获取动画实例
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && FireWeaponMontage)
	{
		AnimInstance->Montage_Play(FireWeaponMontage);
		const FName SectionName = bAiming ? FName(TEXT("RifleAim")) : FName(TEXT("RifleHip"));
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::PlayElimMontage() const
{
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && ElimMontage)
	{
		AnimInstance->Montage_Play(ElimMontage);
	}
}

void ABlasterCharacter::PlayHitReactMontage()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;

	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
	if (AnimInstance && HitReactMontage)
	{
		AnimInstance->Montage_Play(HitReactMontage);
		const FName SectionName("FromFront");
		AnimInstance->Montage_JumpToSection(SectionName);
	}
}

void ABlasterCharacter::ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser)
{
	Health = FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	// 只有服务器端调用该函数，在服务器端播放受到攻击的动画，然后属性复制后，在客户端的OnRep函数中调用播放动画。
	UpdateHUDHealth();
	PlayHitReactMontage();

	if (Health == 0.f)
	{
		if (ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>())
		{
			BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
			ABlasterPlayerController* AttackerPlayerController = Cast<ABlasterPlayerController>(InstigatorController);
			BlasterGameMode->PlayerEliminated(this, BlasterPlayerController, AttackerPlayerController);
		}
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
	
	const float Speed = CalculateSpeed();
	const bool bIsAir = GetCharacterMovement()->IsFalling();

	// standing still, not jumping
	if (Speed == 0.f && !bIsAir)
	{
		bRotateRootBone = true;
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
		bRotateRootBone = false;
		// 设置起始目标旋转变量
		StartingAimRotation = FRotator(0.f, GetBaseAimRotation().Yaw, 0.f);
		AO_Yaw = 0.f;
		bUseControllerRotationYaw = true;
		// 正在跑或跳时不需要转身
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
	CalculateAO_Pitch();
}

void ABlasterCharacter::CalculateAO_Pitch()
{
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

void ABlasterCharacter::SimProxiesTurn()
{
	if (Combat == nullptr || Combat->EquippedWeapon == nullptr) return;
	bRotateRootBone = false;
	
	if (CalculateSpeed() > 0.f)
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
		return;
	}
	
	ProxyRotationLastFrame = ProxyRotation;
	ProxyRotation = GetActorRotation();
	// 计算模拟代理端与上一帧的旋转差异
	ProxyYaw = UKismetMathLibrary::NormalizedDeltaRotator(ProxyRotation, ProxyRotationLastFrame).Yaw;

	if (FMath::Abs(ProxyYaw) > TurnThreshold)
	{
		if (ProxyYaw > TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Right;
		}
		else if (ProxyYaw < -TurnThreshold)
		{
			TurningInPlace = ETurningInPlace::ETIP_Left;
		}
	}
	else
	{
		TurningInPlace = ETurningInPlace::ETIP_NotTurning;
	}
}

void ABlasterCharacter::FireButtonPressed()
{
	if (Combat)
	{
		Combat->FireButtonPressed(true);
	}
}

void ABlasterCharacter::FireButtonReleased()
{
	if (Combat)
	{
		Combat->FireButtonPressed(false);
	}
}

void ABlasterCharacter::Jump()
{
	if (bIsCrouched)
	{
		UnCrouch();
	}
	else
	{
		Super::Jump();
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

void ABlasterCharacter::HideCameraIfCharacterClose()
{
	// 当卡到墙边或墙角时，隐藏自身和武器， 这只是在本地
	if (!IsLocallyControlled()) return;

	if ((FollowCamera->GetComponentLocation() - GetActorLocation()).Size() < CameraThreshold)
	{
		// 本地隐藏网格体
		GetMesh()->SetVisibility(false);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = true;
		}
	}
	else
	{
		GetMesh()->SetVisibility(true);
		if (Combat && Combat->EquippedWeapon && Combat->EquippedWeapon->GetWeaponMesh())
		{
			Combat->EquippedWeapon->GetWeaponMesh()->bOwnerNoSee = false;
		}
	}
}

float ABlasterCharacter::CalculateSpeed() const
{
	FVector Velocity = GetVelocity();
	Velocity.Z = 0.f;
	return Velocity.Size();
}

void ABlasterCharacter::OnRep_Health()
{
	// 属性复制比RPC更快，更有效
	UpdateHUDHealth();
	PlayHitReactMontage();
}

void ABlasterCharacter::UpdateHUDHealth()
{
	BlasterPlayerController = BlasterPlayerController == nullptr ? Cast<ABlasterPlayerController>(Controller) : BlasterPlayerController;
	if (BlasterPlayerController)
	{
		BlasterPlayerController->SetHUDHealth(Health, MaxHealth);
	}
}

void ABlasterCharacter::PollInit()
{
	if (BlasterPlayerState == nullptr)
	{
		// 在游戏开始第一帧时，这会返回nullptr，所以在Tick函数中轮询
		BlasterPlayerState = GetPlayerState<ABlasterPlayerState>();
		if (BlasterPlayerState)
		{
			// 更新HUD
			BlasterPlayerState->AddToScore(0.f);
		}
	}
}

void ABlasterCharacter::Elim()
{
	if (Combat && Combat->EquippedWeapon)
	{
		Combat->EquippedWeapon->Dropped();
	}
	// GameMode中角色死亡需要调用该函数，而GameMode仅存在Server
	MulticastElim();
	// 启动定时器，当定时器结束时，调用回调函数
	GetWorldTimerManager().SetTimer(ElimTimer, this, &ThisClass::ElimTimerFinished, ElimDelay);
}

void ABlasterCharacter::MulticastElim_Implementation()
{
	bElimmed = true;
	PlayElimMontage();

	// Start dissolve effect
	if (DissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicDissolveMaterialInstance);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), 0.55f);
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Glow"), 200.f);
	}
	StartDissolve();

	// Disable character movement
	// DisableMovement()仅仅使得玩家不能移动，但是玩家可以通过鼠标旋转角色
	GetCharacterMovement()->DisableMovement();
	// 该函数可以禁止旋转
	GetCharacterMovement()->StopMovementImmediately();
	// 如果装备武器，禁止输入开火
	if (BlasterPlayerController)
	{
		DisableInput(BlasterPlayerController);
	}
	// 禁用碰撞，可以禁止再次攻击已经死亡的角色触发定时器等
	// Disable collision
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Spawn elim bot, 在NetMulticast中做，所以本地播放
	if (ElimBotEffect)
	{
		// 在死亡玩家头顶200左右播放该效果
		FVector ElimBotSpawnPoint(GetActorLocation().X, GetActorLocation().Y, GetActorLocation().Z + 200.0f);
		ElimBotComponent = UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ElimBotEffect, ElimBotSpawnPoint, GetActorRotation());
	}
	if (ElimBotSound)
	{
		UGameplayStatics::SpawnSoundAtLocation(this, ElimBotSound, GetActorLocation());
	}
}

void ABlasterCharacter::ElimTimerFinished()
{
	// 这只会在Server上调用, 定时器结束后调用该函数，重生玩家
	if (ABlasterGameMode* BlasterGameMode = GetWorld()->GetAuthGameMode<ABlasterGameMode>())
	{
		BlasterGameMode->RequestRespawn(this, Controller);
	}
}

void ABlasterCharacter::UpdateDissolveMaterial(float DissolveValue)
{
	if (DynamicDissolveMaterialInstance)
	{
		DynamicDissolveMaterialInstance->SetScalarParameterValue(TEXT("Dissolve"), DissolveValue);
	}
}

void ABlasterCharacter::StartDissolve()
{
	DissolveTrack.BindDynamic(this, &ThisClass::UpdateDissolveMaterial);
	if (DissolveCurve && DissolveTimeline)
	{
		DissolveTimeline->AddInterpFloat(DissolveCurve, DissolveTrack);
		DissolveTimeline->Play();
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

FVector ABlasterCharacter::GetHitTarget() const
{
	if (Combat == nullptr) return FVector();
	return Combat->HitTarget;
}

