// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class ABlasterHUD;
class ABlasterPlayerController;
class AWeapon;

#define TRACE_LENGTH 80000.f

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BLASTER_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCombatComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 角色和武器组件结合非常紧密。
	// 使角色类成为该组件的友元类，所以角色将可以完全访问该组件的所有变量和函数
	friend class ABlasterCharacter;

	void EquipWeapon(AWeapon* WeaponToEquip);
	
protected:
	virtual void BeginPlay() override;

	void SetAiming(bool bIsAiming);

	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bIsAiming);

	UFUNCTION()
	void OnRep_EquippedWeapon();

	void FireButtonPressed(bool bPressed);

	// 使用FVector_NetQuantize代替FVector，丢失了信息，但是更有效的通过网络发送数据
	// 有助于降低带宽
	UFUNCTION(Server, Reliable)
	void ServerFire(const FVector_NetQuantize& TraceHitTarget);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFire(const FVector_NetQuantize& TraceHitTarget);

	void TraceUnderCrosshairs(FHitResult& TraceHitResult);

	void SetHUDCrosshairs(float DeltaTime);

private:

	class ABlasterCharacter* Character = nullptr;

	UPROPERTY(ReplicatedUsing=OnRep_EquippedWeapon)
	AWeapon* EquippedWeapon = nullptr;

	UPROPERTY(Replicated)
	bool bAiming = false;

	UPROPERTY(EditAnywhere)
	float BaseWalkSpeed;
	
	UPROPERTY(EditAnywhere)
	float AimWalkSpeed;

	bool bFireButtonPressed;

	ABlasterPlayerController* Controller;
	ABlasterHUD* HUD;

	/**
	 * HUD and crosshairs
	 */
	float CrosshairVelocityFactor;
	float CrosshairInAirFactor;
	float CrosshairAimFactor;
	float CrosshairShootingFactor;

	FVector HitTarget;

	/**
	 * Aiming and FOV
	 */
	
	// Field of view when not aiming, set to the camera's base FOV in BeginPlay
	float DefaultFOV;

	UPROPERTY(EditDefaultsOnly, Category = Combat)
	float ZoomedFOV = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = Combat)
	float ZoomedInterpSpeed = 20.f;

	float CurrentFOV;

	void InterpFOV(float DeltaTime);
	
public:	

};
