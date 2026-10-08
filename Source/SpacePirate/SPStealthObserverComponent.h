#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "SPStealthTypes.h"
#include "SPStealthObserverComponent.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthSightSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float Distance=1200;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1",ClampMax="179")) float HorizontalAngle=90;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1",ClampMax="179")) float VerticalAngle=100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float ConfirmationSeconds=1;
};
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthObservation
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bVisible=false;
    UPROPERTY(BlueprintReadOnly) bool bRestricted=false;
    UPROPERTY(BlueprintReadOnly) bool bContinuousCrime=false;
    UPROPERTY(BlueprintReadOnly) bool bConfirmed=false;
    UPROPERTY(BlueprintReadOnly) float Progress=0;
    UPROPERTY(BlueprintReadOnly) ESPCrimeKind Crime=ESPCrimeKind::None;
};
USTRUCT(BlueprintType)
struct SPACEPIRATE_API FSPStealthWitness
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FGuid WitnessId;
    UPROPERTY(BlueprintReadOnly) FGuid ActionId;
    UPROPERTY(BlueprintReadOnly) TWeakObjectPtr<APawn> Player;
    UPROPERTY(BlueprintReadOnly) FVector Location=FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) double ServerTime=0;
    UPROPERTY(BlueprintReadOnly) ESPCrimeKind Crime=ESPCrimeKind::None;
    UPROPERTY(BlueprintReadOnly) bool bRestricted=false;
    UPROPERTY(BlueprintReadOnly) bool bInstant=false;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSPStealthWitnessEvent,const FSPStealthWitness&,Witness);
DECLARE_MULTICAST_DELEGATE_OneParam(FSPStealthWitnessNative,const FSPStealthWitness&);

/** 경비·CCTV·민간인이 공유하는 시야 및 목격 확인 기능. 발각·경보·출동 정책은 목격 이벤트 수신자가 결정한다.
 * 컴포넌트의 위치와 전방이 눈의 기준이므로, 관찰자 종류별 눈 위치는 부착 위치/회전으로 조절한다.
 */
UCLASS(ClassGroup=(SpacePirate), meta=(BlueprintSpawnableComponent))
class SPACEPIRATE_API USPStealthObserverComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    USPStealthObserverComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stealth|Sight") FSPStealthSightSettings Sight;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Stealth|Sight") bool bObservationEnabled=true;
    /** 경비는 자신의 서버 Tick에서 SamplePlayer를 호출하므로 false로 둔다. 중복 호출하면 확인 시간이 두 번 쌓인다. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stealth|Sight") bool bAutoObserve=true;
    UPROPERTY(BlueprintAssignable, Category="Stealth|Witness") FSPStealthWitnessEvent OnWitnessConfirmed;
    FSPStealthWitnessNative OnWitnessConfirmedNative;
    /** 기존 경비의 시야 설정을 유지하기 위한 연결점. 공통 판정 직전에 소유자의 설정을 반영한다. */
    FSimpleDelegate RefreshSettings;
    UFUNCTION(BlueprintPure, Category="Stealth|Sight") bool CanSeePlayer(const APawn* Player) const;
    /** 관찰자 공통 판정: 관찰 가능한 플레이어인지 확인한 뒤 캡슐 표본, 눈 기준 각도, 엄폐 Trace를 검사한다. */
    static bool TestVisibility(const AActor* Observer,const FTransform& Eye,const APawn* Player,const FSPStealthSightSettings& Settings);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Sight") FSPStealthObservation SamplePlayer(APawn* Player,float DeltaSeconds);
    void ObserveInstantCrime(APawn* Player,ESPCrimeKind Kind,const FGuid& ActionId);
    void ResetObservations();
private:
    struct FConfirmation { float Seconds=0; bool bEmitted=false; uint32 CrimeRevision=0; bool bRestricted=false; };
    // 관찰자 인스턴스 안에서 플레이어별로 따로 누적한다. 팀원이나 다른 관찰자의 확인 시간을 합치지 않는다.
    TMap<TWeakObjectPtr<APawn>,FConfirmation> Confirmations;
    TSet<FGuid> InstantActions;
    bool CanObserve() const;
    void Emit(APawn* Player,ESPCrimeKind Kind,bool bRestricted,bool bInstant,const FGuid& ActionId=FGuid());
};
