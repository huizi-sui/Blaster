// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BlasterTypes/TurningInPlace.h"
#include "Components/TimelineComponent.h"
#include "GameFramework/Character.h"
#include "Interfaces/InteractWithCrosshairsInterface.h"
#include "Weapon/Weapon.h"
#include "BlasterCharacter.generated.h"

class ABlasterPlayerState;
class USoundCue;
class ABlasterPlayerController;

UCLASS()
class BLASTER_API ABlasterCharacter : public ACharacter, public IInteractWithCrosshairsInterface
{
	GENERATED_BODY()

public:

	ABlasterCharacter();
	
	virtual void Tick(float DeltaTime) override;

	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PostInitializeComponents() override;

	void PlayFireMontage(bool bAiming) const;
	void PlayElimMontage() const;
	
	virtual void OnRep_ReplicatedMovement() override;

	// 这个仅仅在Server上调用
	void Elim();
	UFUNCTION(NetMulticast, Reliable)
	void MulticastElim();

	virtual void Destroyed() override;

protected:

	virtual void BeginPlay() override;

	void MoveForward(float Value);
	void MoveRight(float Value);
	void Turn(float Value);
	void LookUp(float Value);
	void EquipButtonPressed();
	void CrouchButtonPressed();
	void AimButtonPressed();
	void AimButtonReleased();
	void CalculateAO_Pitch();
	void AimOffset(float DeltaTime);
	// 由于AimOffset，导致旋转根骨骼，所以在模拟代理端会出现抖动现象
	void SimProxiesTurn();
	void FireButtonPressed();
	void FireButtonReleased();
	virtual void Jump() override;
	
	void PlayHitReactMontage();

	UFUNCTION()
	void ReceiveDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatorController, AActor* DamageCauser);

	void UpdateHUDHealth();

	// Poll for any relevant classes and initialize our HUD
	void PollInit();

private:

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	class UCameraComponent* FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UWidgetComponent* OverheadWidget;

	UPROPERTY(ReplicatedUsing = OnRep_OverlappingWeapon)
	class AWeapon* OverlappingWeapon;

	UFUNCTION()
	void OnRep_OverlappingWeapon(const AWeapon* LastWeapon);

	UPROPERTY(VisibleAnywhere)
	class UCombatComponent* Combat;

	// Reliable保证数据到达，而UnReliable可能会导致RPC丢失
	// 信息以数据包的形式发送， 数据包是通过网络发送的信息单元
	// 如果是Reliable，客户端将在服务器收到RPC时收到确认，如果它不发送确认，RPC将再次发送
	UFUNCTION(Server, Reliable)
	void ServerEquipButtonPressed();

	float AO_Yaw;
	float InterpAO_Yaw;
	float AO_Pitch;
	FRotator StartingAimRotation;

	ETurningInPlace TurningInPlace;
	void TurnInPlace(float DeltaTime);

	UPROPERTY(EditDefaultsOnly, Category = Combat)
	UAnimMontage* FireWeaponMontage;

	UPROPERTY(EditDefaultsOnly, Category = Combat)
	UAnimMontage* HitReactMontage;

	UPROPERTY(EditDefaultsOnly, Category = Combat)
	UAnimMontage* ElimMontage;
	
	void HideCameraIfCharacterClose();

	UPROPERTY(EditDefaultsOnly)
	float CameraThreshold = 200.f;

	bool bRotateRootBone;
	float TurnThreshold = 0.5f;
	FRotator ProxyRotationLastFrame;
	FRotator ProxyRotation;
	float ProxyYaw;
	float TimeSinceLastMovementReplication;
	float CalculateSpeed() const;

	/**
	 * Player health
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Player Stats")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Health, Category = "Player Stats")
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health();

	ABlasterPlayerController* BlasterPlayerController;

	bool bElimmed = false;

	// 该计时器用于玩家重生
	FTimerHandle ElimTimer;

	UPROPERTY(EditDefaultsOnly)
	float ElimDelay = 3.f;

	void ElimTimerFinished();

	/**
	 * Dissolve effect
	 */
	// 这是在时间线中使用的轨道
	FOnTimelineFloat DissolveTrack;

	UPROPERTY(VisibleAnywhere)
	UTimelineComponent* DissolveTimeline;

	UPROPERTY(EditAnywhere)
	UCurveFloat* DissolveCurve;

	UFUNCTION()
	void UpdateDissolveMaterial(float DissolveValue);
	void StartDissolve();

	// Dynamic instance that we can change at runtime
	UPROPERTY(VisibleAnywhere, Category = Elim)
	UMaterialInstanceDynamic* DynamicDissolveMaterialInstance;

	// Material instance set on the Blueprint, used with the dynamic material instance
	UPROPERTY(EditDefaultsOnly, Category = Elim)
	UMaterialInstance* DissolveMaterialInstance;

	/**
	 * Elim bot
	 */
	UPROPERTY(EditDefaultsOnly)
	UParticleSystem* ElimBotEffect;

	UPROPERTY(VisibleAnywhere)
	UParticleSystemComponent* ElimBotComponent;

	UPROPERTY(EditDefaultsOnly)
	USoundCue* ElimBotSound;

	ABlasterPlayerState* BlasterPlayerState;
	
public:

	void SetOverlappingWeapon(AWeapon* Weapon);

	bool IsWeaponEquipped() const;
	bool IsAiming() const;

	FORCEINLINE float GetAO_Yaw() const { return AO_Yaw; }
	FORCEINLINE float GetAO_Pitch() const { return AO_Pitch; }
	AWeapon* GetWeapon() const;
	FORCEINLINE ETurningInPlace GetTurningInPlace() const { return TurningInPlace; }
	FVector GetHitTarget() const;
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE bool ShouldRotateRootBone() const { return bRotateRootBone; }
	FORCEINLINE bool IsElimmed() const { return bElimmed; }
	FORCEINLINE float GetHealth() const { return Health; }
	FORCEINLINE float GetMaxHealth() const { return MaxHealth; }
};
