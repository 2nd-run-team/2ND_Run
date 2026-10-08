#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SPStealthTypes.h"
#include "SPLaserSecurityDevice.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class UMaterialInterface;
class USoundBase;
class ASPPlayerCharacter;

UENUM(BlueprintType)
enum class ESPLaserMode : uint8 { AlwaysOn, Periodic };
UENUM(BlueprintType)
enum class ESPLaserPhase : uint8 { Off, WarningOn, On, WarningOff };
UENUM(BlueprintType)
enum class ESPLaserContactType : uint8 { Entry, Reentry, Continuous, ActivatedInside, FastCross };

/** 레벨에 배치하는 익명 보안 센서. 접촉자 신원은 사건에 넘기지 않으며, 메시 교체는 감지 판정에 영향을 주지 않는다. */
UCLASS()
class SPACEPIRATE_API ASPLaserSecurityDevice : public AActor
{
    GENERATED_BODY()
public:
    ASPLaserSecurityDevice();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool ShouldTickIfViewportsOnly() const override { return true; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser") TObjectPtr<USceneComponent> SceneRoot;
    /** 로컬 X축이 빔 방향인 서버 판정 상자. 표시용 메시의 크기와 분리되어 있다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser") TObjectPtr<UBoxComponent> DetectionVolume;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser|Visual") TObjectPtr<UStaticMeshComponent> EmitterVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser|Visual") TObjectPtr<UStaticMeshComponent> ReceiverVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser|Visual") TObjectPtr<UStaticMeshComponent> BeamVisual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Laser|Visual") TObjectPtr<UTextRenderComponent> StateIndicator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry", meta=(MakeEditWidget=true)) FVector LocalStart = FVector(0,0,100);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry", meta=(MakeEditWidget=true)) FVector LocalEnd = FVector(500,0,100);
    /** 선택적인 수신점 액터. 이동하면 감지 범위를 갱신하되, 센서 자체가 지나간 궤적은 접촉으로 처리하지 않는다. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry") TObjectPtr<AActor> ReceiverActor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry") bool bUseBeamLength = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry", meta=(ClampMin="1")) float BeamLength = 500;
    /** 감지 상자의 전체 두께(cm). 메시 크기·액터 스케일과 독립적인 월드 단위다. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Geometry", meta=(ClampMin="0.1")) float DetectionThickness = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual", meta=(ClampMin="0.1")) float VisualThickness = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing") ESPLaserMode Mode = ESPLaserMode::AlwaysOn;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing", meta=(ClampMin="0.1")) float OnSeconds = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing", meta=(ClampMin="0.1")) float OffSeconds = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing") float InitialPhaseSeconds = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing", meta=(ClampMin="0")) float WarningSeconds = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, ReplicatedUsing=OnRep_Config, Category="Laser|Timing") bool bEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Laser|Response") FName DeviceId = TEXT("Laser");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category="Laser|Response") FName DispatchGroup = TEXT("FreightGuards");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Response", meta=(ClampMin="0")) float ResponseRadius = 5000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Response", meta=(ClampMin="0")) int32 MaxResponders = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Response", meta=(ClampMin="0.01")) float CallInterval = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual") TObjectPtr<UMaterialInterface> OnMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual") TObjectPtr<UMaterialInterface> OffMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual") TObjectPtr<UMaterialInterface> WarningMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual") TObjectPtr<USoundBase> WarningSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Visual") TObjectPtr<USoundBase> ContactSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Laser|Debug") bool bDrawDetection = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_Phase, Category="Laser|State") ESPLaserPhase Phase = ESPLaserPhase::Off;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") double EpochServerTime = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") FGuid StageId;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 ContactCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 ReentryCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 FastCrossCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 ActivationCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 ContinuousCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing=OnRep_IncidentCount, Category="Laser|State") int32 IncidentCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 SubmissionCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 PendingCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") int32 DroppedContactCount = 0;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") ESPLaserContactType LastContactType = ESPLaserContactType::Entry;
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Replicated, Category="Laser|State") FSPStealthIncidentRecord LastIncident;

    UFUNCTION(BlueprintPure, Category="Laser") FVector GetBeamStart() const;
    UFUNCTION(BlueprintPure, Category="Laser") FVector GetBeamEnd() const;
    UFUNCTION(BlueprintPure, Category="Laser") bool IsBeamActive() const;
    UFUNCTION(BlueprintPure, Category="Laser") ESPLaserPhase GetPhaseAtElapsed(double Elapsed) const;
    /** 실행 중 설정을 바꾼 뒤 호출한다. 이전 표본을 지워 새 설정으로 과거 이동을 재판정하지 않게 한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Laser") void RefreshDevice();
    /** 장치를 명시적으로 초기화한다. 스테이지 변경도 서버 Tick에서 감지해 자동 초기화한다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Laser") void ResetDevice();

protected:
    virtual void BeginPlay() override;
private:
    struct FPlayerSample
    {
        FVector Location = FVector::ZeroVector;
        FQuat Rotation = FQuat::Identity;
        float Radius = 0;
        float HalfHeight = 0;
        bool bInside = false;
        bool bEverTouched = false;
        FSPStealthIncidentContext Episode;
    };
    struct FPendingContact { FSPStealthIncidentContext Context; ESPLaserContactType Type; };
    // 플레이어 참조는 센서 내부의 진입·재진입·빠른 통과 구분에만 사용한다.
    TMap<TWeakObjectPtr<ASPPlayerCharacter>, FPlayerSample> Samples;
    // 출동 요청 대기열에는 사건 당시 위치와 ID만 보관하며, 접촉자 참조는 전달하지 않는다.
    TArray<FPendingContact> Pending;
    FVector PreviousBeamStart = FVector(UE_BIG_NUMBER);
    FVector PreviousBeamEnd = FVector(UE_BIG_NUMBER);
    double PreviousSampleTime = -1;
    double NextCallTime = 0;
    bool bPreviousActive = false;
    bool bStageWasActive = false;
    bool bPresentationInitialized = false;
    int32 PresentedIncidentCount = 0;
    ESPLaserPhase PresentedPhase = ESPLaserPhase::Off;
    void UpdateGeometry();
    void UpdatePresentation(bool bAllowSound);
    void SamplePlayers(double Now, bool bGeometryChanged);
    bool SweepActiveInterval(const FPlayerSample& Previous, const FPlayerSample& Current, double From, double To) const;
    void QueueContact(FPlayerSample& Sample, ESPLaserContactType Type);
    void SubmitDueContact(double Now);
    void ClearSamples();
    void DrawDetection() const;
    UFUNCTION() void OnRep_Config();
    UFUNCTION() void OnRep_Phase();
    UFUNCTION() void OnRep_IncidentCount();
};
