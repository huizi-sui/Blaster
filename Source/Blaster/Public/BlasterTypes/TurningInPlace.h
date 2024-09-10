
#pragma once

/**
 * 由于使用瞄准偏移，枪将指向我们正在看的方向
 * 但是如果角色在静止时一直向一个方向转动，则AimOffset将会从正值切换为负值，枪的动画就会切换导致出现错误
 *
 * 当转动角度达到某个角度时，角色应该在原地转动向右或向左。使用下面的enum表示
 */
UENUM(BlueprintType)
enum class ETurningInPlace : uint8
{
	ETIP_Left UMETA(DisplayName = "Turning Left"),
	ETIP_Right UMETA(DisplayName = "Turning Right"),
	ETIP_NotTurning UMETA(DisplayName = "Not Turning"),

	ETIP_MAX UMETA(DisplayName = "DefaultMAX")
};