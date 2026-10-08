#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SPStealthTypes.h"
#include "SPStealthActivityComponent.generated.h"

class ASPRestrictedArea;

/** Pawn이 현재 수행 중인 범죄 작업을 서버에서 등록한다. 신원 기억은 PlayerState가 소유한다.
 * 작업을 시작했다고 즉시 발각되지는 않는다. Observer가 이 등록을 읽고 실제 목격을 확인한다.
 */
UCLASS(ClassGroup=(SpacePirate), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USPStealthActivityComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USPStealthActivityComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    /** 직접 만든 지속 작업은 반환 핸들을 보관하고 완료/취소 때 EndCrime으로 정리한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Crime") FGuid BeginCrime(ESPCrimeKind Kind, UObject* Source);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Crime") void EndCrime(FGuid Handle);
    /** 실제 효과가 확정된 서버 시점에 호출한다. 애니메이션 종료 등 나중 시점의 시야로 대체하지 않는다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Crime") FGuid ReportInstantCrime(ESPCrimeKind Kind, UObject* Source);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Crime") void CancelAllActivity();
    /** PlayerCharacter::HandleLifeStateChanged가 서버 다운/구조 전환을 전달한다.
     * true이면 현재 작업을 취소한다. 체력·대미지·구조 규칙과 신원 초기화는 여기서 처리하지 않는다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Crime") void SetIncapacitated(bool bValue);
    UFUNCTION(BlueprintPure, Category="Stealth|Crime") bool IsIncapacitated() const { return bIncapacitated; }
    UFUNCTION(BlueprintPure, Category="Stealth|Crime") TArray<ESPCrimeKind> GetActiveCrimes() const;
    UFUNCTION(BlueprintPure, Category="Stealth|Area") ASPRestrictedArea* GetCurrentArea() const { return CurrentArea; }
    bool HasContinuousCrime(ESPCrimeKind& OutKind) const;
    uint32 GetCrimeRevision() const { return CrimeRevision; }
    void ResetForStage();
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    struct FRegistration { ESPCrimeKind Kind; TWeakObjectPtr<UObject> Source; FGuid StageId; };
    // 서버 관찰용 임시 등록이다. UI용으로 복제하는 구역/다운 상태와 수명을 혼동하지 않는다.
    TMap<FGuid,FRegistration> Registrations;
    bool bCancelling=false;
    uint32 CrimeRevision=0;
    bool CanRegister(UObject* Source) const;
    bool IsRegistrationValid(const FRegistration& Registration) const;
    UPROPERTY(Replicated) bool bIncapacitated = false;
    UPROPERTY(Replicated) TObjectPtr<ASPRestrictedArea> CurrentArea;
};
