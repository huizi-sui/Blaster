
#include "Weapon/ProjectileBullet.h"

#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

void AProjectileBullet::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& HitResult)
{
	// 在ProjectileWeapon中生成子弹时，设置了它的Owner为Weapon的Owner，即持有该武器的玩家
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (AController* OwnerController = OwnerCharacter->GetController())
		{
			// 这里所做的知识触发一个伤害事件，所以需要创建回调函数来响应伤害
			UGameplayStatics::ApplyDamage(OtherActor, Damage, OwnerController, this, UDamageType::StaticClass());
		}
	}
	// Super will call Destroy(), so call it last.
	Super::OnHit(HitComp, OtherActor, OtherComp, NormalImpulse, HitResult);
}
