#pragma once

// 역할: 특수 금고(MVP안 10장). 기획: Docs/인벤토리_아이템_구현안_2026-10-08.md 4장.
// 직접 해제는 부모의 Interactable(누적 30초, 진행률 보존, 조작 잠금)이고, 드릴 설치는 DrillInstall(3초)이다.
// 드릴 가방을 멘 사람이 E를 누르면 설치, 아니면 직접 해제가 골라진다. 금고 전체에서 한 번에 한 명만 조작한다.
// 설치가 끝나면 가방을 소모하고, 직접 해제 진행률을 이어받아 남은 비율만큼 자동으로 작업한 뒤 연다.
// 열면 안에 미리 배치한 전리품 묶음을 포장할 수 있다(닫힌 문짝이 E 트레이스를 막는다).
// NOTICE [WORK-NOISE]: 작업 소리(ReportWorkNoise)는 소리 범위(회의 안건)와 신고 간격이 정해지면 연결한다.
// NOTICE [INDIRECT-CLUE]: 작동 중인 드릴을 간접 단서로 볼지는 10/6본에서 확인 필요(인수인계 2장). IsDrilling()으로 읽는다.

#include "CoreMinimal.h"
#include "SPOpenable.h"
#include "SPVault.generated.h"

class APawn;
class UStaticMeshComponent;
class USPInteractableComponent;

UCLASS()
class SPACEPIRATE_API ASPVault : public ASPOpenable
{
    GENERATED_BODY()

public:
    ASPVault();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void Tick(float DeltaSeconds) override;

    /** 드릴이 설치되어 자동 작업 중인지. 열리면 거짓이다. */
    UFUNCTION(BlueprintPure, Category = "Vault")
    bool IsDrilling() const { return DrillEndTime > 0.0 && !IsOpen(); }

    USPInteractableComponent* GetDrillInstall() const { return DrillInstall; }

protected:
    virtual void BeginPlay() override;
    virtual bool CanOpen(APawn* User) const override;

    /** 설치가 끝나면 보인다. */
    UPROPERTY(VisibleAnywhere, Category = "Vault")
    TObjectPtr<UStaticMeshComponent> DrillMesh;

    /** 드릴 가방을 멘 채 E를 길게 눌러 설치한다. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault")
    TObjectPtr<USPInteractableComponent> DrillInstall;

    /** 직접 해제를 하지 않은 금고를 드릴이 여는 데 걸리는 시간(초). */
    UPROPERTY(EditAnywhere, Category = "Vault", meta = (ClampMin = "0.0", Units = "s"))
    float DrillSeconds = 30.0f;

private:
    bool CanInstallDrill(APawn* User) const;
    void HandleDrillInstalled(APawn* User);
    FText GetDirectBlockedPrompt(APawn* User) const;
    double GetSyncedTime() const;

    UFUNCTION()
    void OnRep_DrillEndTime();

    /** 드릴 작업이 끝나는 서버 시각. 0이면 설치 전이다. 클라이언트는 남은 시간 안내에만 쓴다. */
    UPROPERTY(ReplicatedUsing = OnRep_DrillEndTime)
    double DrillEndTime = 0.0;

    /** 서버 전용. 남은 드릴 작업 시간(초). 완료 판정은 이 값으로 한다. */
    float DrillRemainingSeconds = 0.0f;
};
