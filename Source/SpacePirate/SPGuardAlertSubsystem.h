#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SPStealthTypes.h"
#include "SPGuardAlertSubsystem.generated.h"

class APlayerState;
class ASPPlayerCharacter;
class ASPGuardCharacter;
class USPStealthGameStateComponent;
class USPStealthPlayerStateComponent;

/** 서버의 사건 접수·중복 검사·경비 출동을 담당한다.
 * 신원 원본은 PlayerState의 보안 컴포넌트, 경보 원본은 GameState의 보안 컴포넌트다.
 * 새 장치는 아래 사건 API를 사용하고, 이 Subsystem에 별도 발각/경보 변수를 추가하지 않는다.
 */
UCLASS()
class SPACEPIRATE_API USPGuardAlertSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    /** GameMode가 최초 보안 회차를 시작하고, 이후 전환은 스테이지 소유자가 요청한다.
     * 사건 처리/초기화 콜백 안의 재전환은 무효 ID 또는 false로 거부하므로 다음 프레임에 요청한다.
     * 이 API는 보안만 초기화한다. 체력·작전 실패까지 복구하려면 GameMode::ResetForStage를 사용한다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Stage") FGuid StartStage();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Stage") bool EndStage();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Stage") FGuid RestartStage();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Stage") USPStealthPlayerStateComponent* RegisterPlayer(APlayerState* Player);
    UFUNCTION(BlueprintPure, Category="Stealth") USPStealthGameStateComponent* GetSecurityState() const;
    UFUNCTION(BlueprintPure, Category="Stealth") bool IsStageActive() const;
    UFUNCTION(BlueprintPure, Category="Stealth") bool IsIdentified(FName Scope, const APlayerState* Player) const;
    /** 사건 발생 시 한 번 생성하고 같은 사건의 완료/재시도에는 그대로 전달한다.
     * 새 Context를 매번 만들면 중복 검사를 통과하는 별도 사건이 된다. 처리 시각은 서버가 기록한다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Incident")
    FSPStealthIncidentContext MakeIncidentContext(FVector Location, FName DispatchGroup) const;
    /** 레이저·작업 소리·간접 신고·피해자 신고·탈출 사건용. 범인 인수가 없어 신원을 전달하지 않는다.
     * 익명이라고 항상 무경보는 아니다. 피해자 신고/탈출은 신원 없이 전체 경보를 발생시킬 수 있다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Incident")
    FSPStealthIncidentRecord SubmitAnonymousIncident(ESPStealthIncident Kind, const FSPStealthIncidentContext& Context);
    /** 금고/드릴 소리용 익명 사건. 같은 소리의 재시도는 같은 Context를 사용하며 범인 정보는 받지 않는다. */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Incident")
    FSPStealthIncidentRecord ReportWorkNoise(const FSPStealthIncidentContext& Context);
    /** 호출자가 서버에서 목격 또는 신고 완료를 검증한 뒤 사용한다. 클라이언트 요청 RPC가 아니다.
     * IdentityScope는 신원 공유 범위, Context.DispatchGroup은 출동 그룹으로 서로 다른 설정이다.
     */
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Stealth|Incident")
    FSPStealthIncidentRecord SubmitDirectIncident(ESPStealthIncident Kind, APlayerState* Player,
        FName IdentityScope, const FSPStealthIncidentContext& Context);
    void ReportSighting(ASPGuardCharacter* Witness, ASPPlayerCharacter* Player, const FVector& Location);
    UFUNCTION(BlueprintPure, Category="Stealth|Debug") TArray<FSPStealthIncidentRecord> GetRecentIncidents() const { return RecentIncidents; }
    UPROPERTY(BlueprintAssignable, Category="Stealth|Events") FSPStealthIncidentEvent OnFirstPlayerIdentified;
    UPROPERTY(BlueprintAssignable, Category="Stealth|Events") FSPStealthIncidentEvent OnFirstGlobalAlarm;
    UPROPERTY(BlueprintAssignable, Category="Stealth|Events") FSPStealthIncidentEvent OnIncidentProcessed;
    FSPStealthIncidentNative OnFirstPlayerIdentifiedNative;
    FSPStealthIncidentNative OnFirstGlobalAlarmNative;
    FSPStealthIncidentNative OnIncidentProcessedNative;
protected:
    virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;
private:
    bool IsServer() const;
    USPStealthGameStateComponent* EnsureSecurityState();
    void ResetPlayersAndGuards(const FGuid& StageId);
    FSPStealthIncidentRecord Process(ESPStealthIncident Kind, const FSPStealthIncidentContext& Context,
        APlayerState* Player, FName Scope, bool bDirectAPI);
    FSPStealthIncidentRecord Audit(FSPStealthIncidentRecord Record);
    // 중복 ID는 회차 전체 동안 보관한다. 최근 기록 128건이 밀려나도 같은 사건을 재처리하면 안 된다.
    TSet<FGuid> ProcessedIds;
    UPROPERTY(Transient) TArray<FSPStealthIncidentRecord> RecentIncidents;
    bool bProcessing = false;
    // 초기화 재진입을 막는 실행 가드다. 회차 활성 상태의 원본은 여전히 복제 GameState에 있다.
    bool bTransitioningStage = false;
};
