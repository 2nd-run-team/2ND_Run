# 잠입 사건 API 계약

UE 5.8.2 / 2026-10-08. 구현은 `Source/SpacePirate/SPGuardAlertSubsystem.*` 및 `SPStealthTypes.h`.
모든 변경 함수는 **서버 내부 API**다. RPC가 아니다. 클라이언트가 종류·범인·좌표를 임의로 제출하는 일반 RPC를 추가하지 않는다. 실제 장치의 기존 서버 상호작용/목격/신고 완료에서 호출한다.

## 상태와 수명

공통 GameState 부모는 `ASPGameState`다. 열차는 `ASPGameState → BP_SPTrainGameState → BP_SPPlanetTrainGameState` 상속을 유지하고 그 인스턴스에 보안 복제 컴포넌트를 붙인다. GameMode는 사용자 지정 열차 GameState를 강제로 대체하지 않는다.

보안 API의 `RestartStage`는 체력/작전 실패를 초기화하지 않는다. 전체 게임 회차 재시작은 서버 `ASpacePirateGameMode::ResetForStage`를 사용한다. 보안 종료·작업 취소 → 작전/체력 복구 → 새 보안 회차 시작 → 팀 인원 집계 순서이며 물건/위치 재생성은 스테이지 소유자가 별도로 처리한다.

| 원본 | 저장 내용 | 읽기 API |
|---|---|---|
| 기존 PlayerState의 `SPStealthPlayerStateComponent` | StageId, KnownToScopes, FirstIdentifiedAt | `GetIdentityState`, `IsIdentified`, `IsKnownTo` |
| 기존 GameState의 `SPStealthGameStateComponent` | StageId, StageActive, GlobalAlarm, AlarmServerTime, AlarmIncidentId | `GetAlarmState` |
| 서버 `SPGuardAlertSubsystem` | 이번 스테이지의 처리 ID 집합, 최근 감사 영수증 | `GetRecentIncidents` (최근 128건, 서버만) |
| 개별 경비 | 대상/목격 위치 또는 익명 수색 위치·상태·만료 시간 | 기존 경비 상태 및 `InvestigationLocation` |

발각 여부와 전체 경보는 컴포넌트 외부에 별도 원본을 저장하지 않는다. HUD는 GS/PS를 읽는다. Director의 최초 효과 횟수는 시험용 실행 계수이며 게임 규칙에 사용하지 않는다.

`ASpacePirateGameMode::InitGameState`가 BP가 선택한 GameState를 그대로 두고 첫 스테이지를 시작한다. `GenericPlayerInitialization`이 접속 PlayerState를 등록한다. 다른 네이티브 GameMode를 사용하는 새 모드에서는 이 두 초기 연결을 명시적으로 수행해야 한다. 컴포넌트는 서버에서 동적 생성되어 모든 클라이언트에 복제된다. 컴포넌트가 아직 도착하지 않았거나 StageId가 다르면 UI는 대기 상태로 취급한다.

- `FGuid StartStage()`: 새 StageId, 경보/신원/처리 ID/경비 대응 초기화, Active=true.
- `bool EndStage()`: 현재 단계 비활성화, 경보/신원/처리 ID/경비 대응 초기화. 이후 사건은 InactiveStage.
- `FGuid RestartStage()`: 새 StageId로 StartStage와 동일한 초기화.
- `RegisterPlayer(PlayerState)`: 컴포넌트를 한 번만 생성. 현재 StageId라면 발각을 보존하며, 다른 StageId면 초기화한다. 새 플레이어에게 팀원의 발각을 복사하지 않는다.

시작/종료는 모든 경비의 확인 중 타이머·추격/익명 수색·들었던 발소리·이동 지시를 정리한다. 순찰 복귀는 신원을 지우지 않는다. 종료된 스테이지에서 경비는 순찰하지만 새 확인/발소리 판정을 수행하지 않는다. 이벤트 콜백 안에서 스테이지를 바꾸면 진행 중 처리를 무효화할 수 있으므로 수명 API는 그동안 실패한다(시작: invalid Guid, 종료: false). 전환은 콜백 반환 후 다음 서버 Tick 등에 실행한다.

다른 시스템의 타이머/금고 작업까지 초기화하지 않는다. 각 시스템이 작업을 취소하고 새 StageId로 시작해야 한다. 같은 PlayerState의 Pawn 리스폰은 발각을 유지한다. 새 PlayerState로 재접속 시 이전 계정의 신원 복원과 서버 트래블은 별도 연동 과제다.

## 제출

```cpp
USPGuardAlertSubsystem* Security = GetWorld()->GetSubsystem<USPGuardAlertSubsystem>();

// 서버에서 소리/레이저/단서가 발생할 때. 재시도 시 Context 자체를 재사용한다.
FSPStealthIncidentContext Context = Security->MakeIncidentContext(
    GetActorLocation(), TEXT("VaultGuards"));
Context.ResponseRadius = 2500; // cm, 사건 당시 위치 기준
Context.MaxResponders = 2;
const FSPStealthIncidentRecord Receipt = Security->SubmitAnonymousIncident(
    ESPStealthIncident::WorkNoise, Context);
```

```cpp
// 신고 시작 시 서버가 기억한다. Pawn이 교체되어도 PlayerState를 사용한다.
PendingReport = Security->MakeIncidentContext(WitnessedLocation, TEXT("Car1Guards"));
WitnessedPlayerState = ConfirmedActor->GetPlayerState(); // 약한 참조 등으로 수명 검사

// 서버가 신고 진행/거리/생존/방해 여부를 검사하고 완료했을 때만 호출한다.
Security->SubmitDirectIncident(ESPStealthIncident::DirectReportCompleted,
    WitnessedPlayerState.Get(), TEXT("TrainSecurity"), PendingReport);
```

위 변수는 호출자 예시이며 Subsystem에 신고 진행 상태가 있다는 뜻이 아니다. 직접 범죄의 타이머/확인 규칙은 생산자 책임이다. 현재 기존 경비는 `ConfirmSightTime`과 LOS로 지속 침입을 확인한다. 순간 범죄와 직접 신고는 후속 장치/NPC의 서버 완료 검증을 연결해야 한다.

| API | 허용 종류 |
|---|---|
| `SubmitDirectIncident(Kind, PlayerState, IdentityScope, Context)` | SustainedCrimeConfirmed, InstantCrimeWitnessed, DirectReportCompleted, IdentifiedPlayerRediscovered |
| `SubmitAnonymousIncident(Kind, Context)` | LaserContact, WorkNoise, IndirectReport, VictimReport, EscapeActivated |

직접 확인 3종은 그 플레이어와 IdentityScope만 등록하고 전체 경보를 발생시킨다. Rediscovered는 **그 범위에서 이미 알려진** 플레이어만 허용한다. 레이저/작업 소리/간접 신고는 경비 수색만 지시한다. 피해자 신고/탈출 작동은 범인 없이 경보를 올린다.

`FSPStealthIncidentContext`:

- `IncidentId`: 유효한 GUID. 동일 사건의 재전송/복수 완료 콜백은 동일한 ID를 사용한다. 독립 사건은 새 ID를 쓴다.
- `StageId`: 생산자가 시작 시 캡처한 단계. 재시도 때 현재 단계 값으로 바꾸지 않는다.
- `Location`: 서버가 확인한 사건 위치. NaN/Infinity 거부. 실시간 Pawn 좌표 대신 목격/소리 위치 스냅샷이다.
- `DispatchGroup`: 출동시킬 실제 경비의 `AlertGroup`. None은 출동 없음이며 전체 경보 자체는 발생할 수 있다.
- `ResponseRadius`: 사건 위치 기준 3D 거리(cm), 기본 5000. 음수/NaN/Infinity 거부, 0은 출동 없음.
- `MaxResponders`: 거리 순서로 지시를 수락하는 최대 인원, 기본 4. 음수 거부, 0은 출동 없음.

서버 시각은 제출 처리 시 `GameState.GetServerWorldTimeSeconds()`에서 얻는다. 클라이언트 시각 파라미터는 없다. 사건 생산 시점과 수신 완료 시점이 다를 수 있으며 현재 로그의 ServerTime은 **처리/수신 시각**이다.

## 공유 범위와 출동

Guard의 `IdentityScope`와 `AlertGroup`은 독립 설정이다. 예를 들어 경비 G1=(TrainSecurity,Car1), G2=(TrainSecurity,Car2), G3=(LocalSecurity,Car1)일 때:

- A의 직접 범죄를 IdentityScope=TrainSecurity, DispatchGroup=Car1로 제출한다.
- A는 TrainSecurity에 알려지며 G1은 A의 마지막 목격 위치로 출동한다.
- G2도 A를 재발견할 수 있지만 이번 사건으로 바로 출동하지 않는다.
- G3는 Car1 출동 그룹에 있으나 신원을 공유받지 않으므로 위치만 익명 수색한다.
- B의 신원은 바뀌지 않는다.

익명 사건은 직접 추격을 덮어쓰지 않는다. 직접 보고 역시 이미 눈앞에서 다른 범인을 추격하는 경비의 대상을 강제로 바꾸지 않는다. 영수증 `DispatchedGuards`는 대응 지시를 실제 수락한 경비 수다. 수색에 필요한 길찾기 도달 성공을 의미하지는 않는다. 경로 실패는 제한 재시도·정체·이동 예산으로 종료하며 결과는 경비의 `LastInvestigation`과 로그에 별도로 남긴다. 마지막 목격 위치로 이동할 때에는 LostTargetTimeout도 적용한다.

## 결과와 최초 효과

`FSPStealthIncidentRecord`에는 Context, Kind, ServerTime, Result, 선택적 PlayerState 약한 참조/IdentityScope, `bFirstIdentification`, `bNewIdentityScope`, `bFirstGlobalAlarm`, DispatchedGuards가 있다.

| Result | 의미 |
|---|---|
| Applied | 유효 사건 처리 완료. 이미 경보/발각 상태이면 최초 플래그는 false일 수 있음 |
| Duplicate | 이번 스테이지에 이미 적용한 IncidentId. 상태/지시/최초 효과를 반복하지 않음 |
| NotAuthority | 네이티브 API를 클라이언트에서 호출함 |
| InactiveStage / StaleStage | 단계가 종료됐거나 Context의 StageId가 현재 단계와 다름 |
| InvalidRequest | 유효하지 않은 ID·위치·행위자·공유 범위 또는 잘못된 API/종류 조합 |
| UnknownIdentity | 미발각 플레이어 또는 다른 공유 범위에 재발견 요청 |

BlueprintAuthorityOnly 호출은 스크립트 디스패치가 C++ 본체 전에 차단할 수도 있다. 이 경우 기본 반환 구조체(InvalidRequest/invalid Guid)를 받으며 상태 변경은 없다. 반환의 Applied 여부와 필요 시 현재 복제 상태를 확인한다.

이미 적용된 ID는 해당 스테이지의 같은 사건으로 취급한다. 같은 ID로 다른 payload를 보내도 다시 적용하지 않는다. 거부된 요청은 ID를 소비하지 않아 잘못된 입력을 고쳐 재제출할 수 있다. 최근 감사 128건이 넘어가도 중복 ID 집합은 스테이지가 끝날 때까지 유지한다. 이전 단계 ID는 StageId 비교에서 먼저 거부한다.

- `OnFirstPlayerIdentified` / `OnFirstPlayerIdentifiedNative`: 플레이어별로 한 스테이지에 한 번. 다른 공유 범위로 확대돼도 반복되지 않음.
- `OnFirstGlobalAlarm` / `OnFirstGlobalAlarmNative`: 해당 스테이지 전체에 한 번. 피해자/탈출로 먼저 발생한 뒤 범죄가 확인돼도 반복되지 않음.
- `OnIncidentProcessed` / Native: 적용·중복·거부된 서버 제출의 감사 이벤트. 이것을 최초 완료 효과로 취급하지 않는다.

상태와 ID 예약을 먼저 완료하고 최초 효과를 알리므로 콜백의 동일 사건 재전송도 Duplicate다. GS/PS `OnStateChanged`는 초기 복제·갱신·초기화 때도 발생하는 표시용 알림이다. 목표/지원 타이머/탈출 정산은 서버 최초 델리게이트를 구독하고, 실제 클라이언트 연출은 해당 시스템의 복제/재생 정책으로 연결한다. 180초 지원 타이머와 목표·탈출 완료 시스템은 이번 구현에 포함하지 않았다.

감사 전체는 `LogSPStealth`에 기록한다. 서버 `GetRecentIncidents()`는 최근 128건이다. 클라이언트에 전체 감사 기록/Subsystem 원본을 복제하지 않고 상태와 시험 Director의 마지막 결과만 표시한다.

## 조사 대응 (단계 3)

Context의 `ResponseRadius`(기본 5000cm)와 `MaxResponders`(기본 4명)으로 출동을 제한한다. 같은 AlertGroup에서 사건 위치와 가까운 순으로 지시하며 거절한 경비는 인원에 포함하지 않는다. 0은 출동 없음이다. IdentityScope 기억 범위는 변경하지 않는다. 새 익명 ID는 이전 익명 조사만 교체하며, 같은 ID 재시도는 시간/목적지를 갱신하지 않는다. 상태·이동 실패·영수증과 제작 설정은 [GuardInvestigation.md](GuardInvestigation.md).

## 실제 목격/작업 소리 연결 (단계 4)

`ReportWorkNoise(Context)`는 범인 인자 없이 WorkNoise를 제출하는 서버 전용 진입점이다. 기존 익명 사건과 같은 중복/StageId/범위 검증을 적용한다. 실제 드릴/금고 생산자는 작업 당시 위치로 Context를 만들며 신원은 전달하지 않는다.

Context에 `CrimeKind`와 `bRestrictedArea` 증거 메타데이터를 추가했다. 사건 정책은 여전히 Kind로 결정하며 메타데이터만으로 신원을 바꾸지 않는다. 기본 None/false라 기존 호출과 호환된다. 사건 ID/종류/위치/서버 처리 시각/결과에 더해 `LogSPStealth`의 Evidence 행에서 범죄 종류/침입 여부를 볼 수 있다.

공통 Observer의 `OnWitnessConfirmed`는 증거 출력만 한다. 경비 HandleWitness가 해당 PlayerState, IdentityScope와 함께 SustainedCrimeConfirmed/InstantCrimeWitnessed를 제출한다. 지속 작업·침입은 관찰자/Pawn별 1초 확인, 순간 행동은 실제 서버 호출 순간 시야를 사용한다. `ReportSighting`은 기존 호환 진입점 및 알려진 대상 재발견으로 유지한다. [CrimeObservation.md](CrimeObservation.md)에 장치/민간인 대응 정책 분리와 등록 수명 계약이 있다.

StartStage/EndStage/RestartStage는 이제 해당 월드의 Activity 등록과 진행 중 E, 공통 Observer의 확인/순간 ID도 정리한다. 임의 외부 장치의 별도 타이머/오디오/목표 진행을 취소하는 API는 아니며 장치 소유자가 함께 종료한다. 다운 상태는 체력 소유자의 원본이므로 잠입 초기화가 임의로 해제하지 않는다.

초기화 중에는 취소/상태 변경 콜백에서도 `IsStageActive()`가 false이며 사건 제출은 `InactiveStage`다. 콜백이 새 회차에 경보를 남기거나 회차를 재귀적으로 시작할 수 없다. 사건 처리 또는 초기화 콜백 안의 Start/Restart는 무효 ID, End는 false를 반환한다. 다음 전환이 필요하면 스테이지 소유자가 다음 프레임에 요청한다. 전환 API가 반환된 이후 새 행동을 시작한다.

## 실제 레이저 연결 (단계 5)

`SPLaserSecurityDevice`는 서버에서 실제 플레이어 캡슐의 접촉/이동을 검사한 뒤 `SubmitAnonymousIncident(LaserContact, Context)`를 호출한다. Context의 위치는 접촉자 위치가 아니라 **사건 당시 장치 액터 원점**이다. 시험 맵에서는 이 원점을 NavMesh 위 바닥에 두고 빔 끝점만 로컬 높이를 준다. `DispatchGroup/ResponseRadius/MaxResponders`는 장치 설정을 사용한다. 접촉 판정에 사용한 Pawn/PlayerState는 사건 API와 경비에 전달하지 않는다.

같은 접촉이 지속되면 같은 IncidentId를 유지하고 기본 2초 간격 이후 재제출은 Duplicate다. 수색 목적지/시간과 최초 효과는 갱신되지 않는다. 이탈 후 재진입, 꺼졌다가 안에서 켜짐, 다른 센서 접촉은 새 사건이다. 간격 안에 생긴 새 접촉은 장치별 대기열에 넣고 접촉 당시 위치/StageId를 유지한다. `DeviceId`는 장치 표시·`LogSPLaser` 감사용이며 사건의 유일 키는 GUID다. 장치별 호출 간격이 신원 공유나 다른 장치의 호출을 막지 않는다.

장치는 스테이지 ID 변경을 다음 서버 Tick에서 감지해 자체 주기/표본/대기열/계수를 초기화한다. 종료된 스테이지에서는 Off로 표시하고 제출하지 않는다. 개별 `ResetDevice`는 PS 신원이나 GS 경보를 해제하지 않는다. 자세한 설치·복제·빠른 횡단 판정·외형 교체와 실행 증거는 [LaserSecurity.md](LaserSecurity.md).
