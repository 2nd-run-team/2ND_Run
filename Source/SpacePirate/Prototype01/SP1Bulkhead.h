// 작성자 : 임진혁
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SP1Bulkhead.generated.h"
class UStaticMeshComponent;
class UBoxComponent;
class USP1InteractionComponent;
class ASP1Bulkhead;
UENUM(BlueprintType)
enum class ESP1PanelKind : uint8 { Left, Right, Bypass };

/** 월드 패널은 RPC를 받지 않는다. 소유 Pawn의 기존 E 시도를 격벽에 연결한다. */
UCLASS()
class SPACEPIRATE_API ASP1Panel : public AActor
{
    GENERATED_BODY()
public:
    ASP1Panel();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") TObjectPtr<ASP1Bulkhead> Bulkhead;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") ESP1PanelKind Kind = ESP1PanelKind::Left;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName PanelId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FVector InteractionOffset = FVector(0,0,30);
    UFUNCTION(BlueprintPure) FVector GetInteractionPoint() const;
};

UCLASS()
class SPACEPIRATE_API ASP1Bulkhead : public AActor
{
    GENERATED_BODY()
public:
    ASP1Bulkhead();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UBoxComponent> Barrier;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> DoorRight;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") FName DeviceId = TEXT("P01_C03_Bulkhead");
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float TogetherSeconds = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Prototype01") float BypassSeconds = 12;
    UPROPERTY(ReplicatedUsing=OnRep_Open, BlueprintReadOnly) bool bOpen = false;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<APawn> LeftHolder;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<APawn> RightHolder;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<APawn> BypassHolder;
    UPROPERTY(Replicated, BlueprintReadOnly) double ProgressStartedAt = -1;
    FName TryClaim(USP1InteractionComponent* Source, const ASP1Panel* Panel);
    void Release(USP1InteractionComponent* Source);
    void EvaluateOnServer(double Time);
    void ResetForRun();
    UFUNCTION(BlueprintPure) float GetProgress(double Time) const;
private:
    UFUNCTION() void OnRep_Open();
    USP1InteractionComponent* ValidHolder(APawn* Pawn, double Time, bool bStrict) const;
};
