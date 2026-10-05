# 기획안 1 구현 상태

계획서의 기능 목록과 실제 구현 결과를 구분한다. 아래 결과는 해당 날짜에 실행한 검사 범위에 한정한다.

최신 작업은 아래 **P5** 항목이다. 이전 단계의 미구현 표기는 당시 기록이며 현재 상태와 혼동하지 않는다.

## 2026-10-01 · P0 프로젝트 기준과 전용 맵 준비

작성자 : 임진혁

**P0 완료.** 기존 이동·시점·점프를 사용하는 1개 플레이 객차에서 Listen Server와 Client가 함께 스폰하고 서로의 캐릭터를 볼 수 있다. 창밖의 행성·배경은 움직이며 열차 지형은 고정된다. 바닥의 노란 테두리와 `EXIT / P0 / MARKER ONLY` 표지는 다음 단계의 탈출 배치 기준이다. 영역 진입으로 탈출·정산이 실행되지는 않는다. P1 이후 기능은 미구현이다.

### 작업 시작 기준과 설계 차이

- 브랜치 `feature/teststage`, HEAD `f2a6235`. 시작 시 사용자 작성 문서 `prototype-plan.md`, `asset-list.md`, `codex-prompts.md` 세 개만 미추적 상태였다. 이 문서들은 수정하지 않았다.
- `E:/UE_5.8/Engine/Build/Build.version`: **5.8.2**, Changelist **56702186**. `.uproject`의 EngineAssociation `5.8`은 유지했다.
- README, CONTRIBUTING, Plan01 계획·에셋 목록, TrainFreight README·verification.json 및 관련 실제 C++를 읽었다. 저장소와 상위 경로에서 적용되는 AGENTS.md는 발견되지 않았다.
- TrainFreight 문서의 과거 미커밋 표기는 현재 상태가 아니다. 원본 `Lvl_SPTrainFreight`는 현재 추적 파일이며 그 맵의 실제 편성은 12칸이다. `BP_SPTrainConsist`의 CDO `MaxCars=8`과 맵 인스턴스 값을 구분했다.
- P0의 승인 범위는 1칸이다. 계획서의 6칸·10칸 구성, 450cm/s 제안 속도, 자동 전송 규칙을 적용하지 않았다. 팀 Pawn 기본값인 걷기 400, 달리기 700, 무중력 최고속도 700cm/s, 가속도 500cm/s², 항력 0을 상속한다.
- UE 5.8의 IMC 실제 키 목록은 `default_key_mappings.mappings`에서 확인했다. 구형 `Mappings` 속성의 빈 배열을 입력 누락으로 판단하지 않았다.

### 실제 부모·기본값 조사와 새 에셋

아래 경로는 `/Game/SpacePirate` 기준이다. `_C`는 생성된 BP 클래스를 의미한다.

| 기존 에셋 | 실제 부모 | 확인한 주요 연결 |
|---|---|---|
| `Player/Blueprints/BP_SPPlayerCharacter` | `/Script/SpacePirate.SPPlayerCharacter` | 기존 커스텀 이동, Move/Look/Jump/Sprint/Interact/Drop/슬롯 액션 |
| `Player/Blueprints/BP_SPPlayerController` | `/Script/SpacePirate.SpacePiratePlayerController` | `Player/Input/IMC_SPPlayer` |
| `Core/Blueprints/BP_SPGameMode` | `/Script/SpacePirate.SpacePirateGameMode` | 팀 Pawn·Controller, Engine GameStateBase·PlayerState |
| `Train/Framework/BP_SPTrainPlayerController` | 팀 `BP_SPPlayerController_C` | 같은 팀 IMC 상속 |
| `Train/Framework/GameModes/BP_SPTrainGameMode` | 팀 `BP_SPGameMode_C` | 팀 Pawn, Train Controller·GameState |
| `Train/Framework/GameStates/BP_SPTrainGameState` | `/Script/Engine.GameStateBase` | MotionState, GetTravelState, SetTrainSpeed |
| `Train/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameMode` | `BP_SPTrainGameMode_C` | 팀 Pawn, PlanetTrain Controller·GameState |
| `Train/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameState` | `BP_SPTrainGameState_C` | 기존 MotionState와 SkyTransition, `DA_SPPlanetSkyCatalog` |
| `Train/Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainPlayerController` | `BP_SPTrainPlayerController_C` | 같은 팀 IMC와 기존 열차·하늘 요청 경로 |

신규 BP는 모두 `/Game/SpacePirate/Prototype01/Blueprints`에 있으며, Blueprint Description에 `작성자 : 임진혁`과 역할을 적고 Author 메타데이터도 기록했다.

| 신규 BP | 부모 / 설정 |
|---|---|
| `BP_SP1PlayerCharacter` | 팀 `BP_SPPlayerCharacter` 자식, 입력·이동 기본값 유지 |
| `BP_SP1PlayerController` | `BP_SPPlanetTrainPlayerController` 자식 |
| `BP_SP1GameState` | `BP_SPPlanetTrainGameState` 자식, 라운드 컴포넌트 부착 예정 |
| `BP_SP1GameMode` | `BP_SPPlanetTrainGameMode` 자식, 위 전용 Pawn·Controller·GameState를 지정 |

`/Game/SpacePirate/Maps/Lvl_SPPrototype01`의 **World Settings → GameMode Override**는 `BP_SP1GameMode`이다. 기존 GameState의 부모를 교체하지 않았으므로 기존 Train/PlanetTrain 캐스트, MotionState 및 하늘 상태 상속을 유지한다. 프로젝트 전역 기본 맵·GameMode는 바꾸지 않았다.

맵은 UE `LevelEditorSubsystem.new_level_from_template`으로 원본에서 새 패키지를 생성했다. 신규 BP는 `AssetTools`·`BlueprintFactory`로 생성했다. 파일명 변경으로 에셋을 복제하지 않았다.

### 배치와 확장 기준

- 편성 인스턴스 `P01_C01_TrainConsist`: `MaxCars=1`, `CarSequence=[DA_SPCar_Long]`. 기존 기관차·앞 플랫폼·후방 마감은 유지되며 플레이 화물 객차는 1개이다.
- 객차 식별자 **`P01_C01`**: 편성 및 마커의 `CarId=P01_C01` Actor Tag, PlayerStart의 `PlayerStartTag`에 기록했다. 현재는 배치 식별자이며 서버 객차 판정 시스템은 아니다.
- 객차 중심 `(-1250, 0, 210)`, 길이 2000cm, X 범위 `[-2250, -250]`. 이동 방향은 +X이다.
- PlayerStart 4개: `(-1900,-90,320)`, `(-1900,90,320)`, `(-1600,-90,320)`, `(-1600,90,320)`, Yaw 0. 이름은 `P01_C01_PlayerStart_1`부터 `_4`이다.
- 입구 TargetPoint `P01_C01_Entry=(-2130,0,320)`, 출구 TargetPoint `P01_C01_Exit=(-450,0,320)`.
- 임시 구역 `P01_C01_ExtractionPlaceholder`: 중심 `(-650,0,330)`, Box 반경 `(200,150,120)`cm. 실내 바닥 위의 4m×3m 영역이며 `P0MarkerOnly` 태그를 붙였다. 비충돌 경계 메시 4개와 출입구 TextRender 표지를 배치했다.
- 기존 `DA_SPTrainScene_FreightPlanet`과 하늘 카탈로그 참조를 그대로 사용한다. 이 프리셋은 근경 SpaceTrack과 원경 EmptySpace **배경 매니저 2개** 및 PlanetFlyby backdrop을 사용한다.

### 라운드·E 입력 통합 결정 — P1 이후 구현할 내용

1. `Source/SpacePirate/Prototype01`에 서버 권한의 복제 라운드 컴포넌트(예정 이름 `USP1RoundComponent`)를 만들고 **BP_SP1GameState에만 부착**한다. 기존 Train/PlanetTrain GameState 부모는 유지한다. 시간·가치·완료·탈출 등 영속 상태는 복제 속성으로 관리한다. P0에는 이 C++ 클래스나 라운드 상태가 없다.
2. 실제 E는 `/Game/Input/Actions/IA_Interact`의 Boolean 액션이며 액션 트리거는 비어 있다. `IMC_SPPlayer`가 E를 매핑하고, `ASPPlayerCharacter::SetupPlayerInputComponent`가 `Started`를 **private `Interact()`**에 바인딩한다. `Interact()`는 개발용 중력 스위치를 먼저 검사한 뒤 기존 직접 집기 요청을 처리한다.
3. P1에서 기존 Pawn에 기본 OFF의 작은 opt-in 연결점을 추가한다. BP_SP1PlayerCharacter에서만 켜고 상호작용 컴포넌트를 연결한다. 바인딩 시 **새 Started/Completed/Canceled 경로와 기존 Started→Interact 경로 중 하나만 선택**한다. 공용 IMC나 IA_Interact에 Hold 트리거를 추가하지 않는다. `Interact` 접근 범위를 넓히거나 입력 설정 전체를 복제할 필요가 없다.
4. 새 홀드 요청은 클라이언트가 소유하는 Pawn의 복제 컴포넌트를 통해 보낸다. 서버가 거리·시선·가림·시간과 화물 상태를 검사한다. 월드 화물의 Owner를 바꾸는 RPC 방식, 클라이언트 완료·가치 숫자 신뢰 방식은 사용하지 않는다. 자동 전송 화물은 ASPCargo와 별도 Actor로 만든다.

P0에서는 새 홀드 바인딩이 없으므로 기존 E 입력만 실행된다. ASPCargo·USPInventoryComponent·중력 코드·공용 UCLASS와 설정은 변경하지 않았다.

### 실행 및 재검사

1. UE **5.8.2**로 `E:/GitHub/2ND_Run/SpacePirate.uproject`를 연다.
2. 콘텐츠 브라우저에서 `/Game/SpacePirate/Maps/Lvl_SPPrototype01`을 연다. World Settings의 `BP_SP1GameMode`를 확인한다.
3. Play 옵션에서 **Number of Players=2**, **Net Mode=Play As Listen Server**, **Run Under One Process=ON**, **New Editor Window (PIE)**를 선택하고 실행한다. 한 창은 Server 0, 다른 창은 Client 1이다. 각 창을 클릭해 입력을 잡고 WASD·마우스·Space로 조작한다. Shift+F1로 마우스를 해제한다.
4. 반복 가능한 입력 검사는 새 PIE 세션에서 에디터 Cmd에 다음을 입력한다. 약 8초 후 `Saved/Prototype01/pie-input.json`의 `passed`를 확인한다. 시작점에서 전진하는 검사이므로 같은 세션에서 반복 실행하지 않는다.

```text
py "E:/GitHub/2ND_Run/Tools/Prototype01/verify_pie.py"
```

읽기 전용 BP·맵 검사는 별도 PowerShell에서 실행한다. 이 스크립트는 에셋을 저장하지 않는다.

```powershell
& 'E:/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' 'E:/GitHub/2ND_Run/SpacePirate.uproject' -run=pythonscript '-script=E:/GitHub/2ND_Run/Tools/Prototype01/validate_p0.py' -unattended -nullrhi -nosound -nop4
```

`Tools/Prototype01/create_p0.py`는 최초 생성용 기록이다. 이미 목적지가 있으면 중단하며 재실행으로 덮어쓰지 않는다. 재생성은 해당 신규 에셋의 LFS 락과 별도 빈 목적지 작업 환경이 필요하다.

### 수행한 검사와 한계

| 검사 | 실제 결과 |
|---|---|
| 기존 `Tools/TrainFreight/validate_assets.py` 읽기 전용 실행 | 종료 0. **110 에셋, 37 BP, 285 게임 의존 패키지**, 누락·과거/원본 프로젝트 참조·검사 오류 0 |
| 신규 생성 commandlet | 종료 0, 신규 BP 4개와 맵 생성·저장 |
| 최종 `validate_p0.py` | 종료 0, BP 4개 UpToDate, 1칸·4 PlayerStart·17 편집 월드 Actor, **277 게임 의존 패키지**, 누락·검사 오류 0. 부모·GM·입력·배경·표지 전체 위치/회전 검사 |
| Listen Server + Client, 동일 프로세스 PIE | 각 월드에 Pawn 2명, 로컬 Pawn 1명. 각 로컬 Enhanced Input에 Move/Look/Jump 액션을 주입하여 검사 통과 |
| 이동·시점·점프 수치 | 양쪽 약 294.92cm 전진, 시점 Yaw 약 75.01° 변화, 점프 높이 약 89.93cm, 최종 실내 바닥 Z 약 308.15cm |
| 복제·배경 | 종료 시 플레이어별 양쪽 위치 차이 5cm 미만. MotionState·SkyTransition 동일. 매니저 2개 모두 초기화·거리 증가, 열차와 객차 위치 고정 |
| 에디터 화면·입력 | 양쪽 창에서 상대 캐릭터 렌더를 확인. Server와 Client의 Space 점프, Client 마우스 시점 변화 확인. 전후 표지와 노란 임시 구역, 행성 창밖 배경 확인 |
| 원본 보존 | `git diff --exit-code`로 Source, Config, .uproject, Train/Player 콘텐츠, 원본 Lvl_SPTrainFreight에 변경 없음 확인 |
| C++ Development Editor Win64 빌드·프로젝트 파일 재생성 | **미실행**. C++·UCLASS·모듈 의존성 변경이 없어 이번 P0에는 필요하지 않음 |
| 기타 미실행 | 별도 PC/프로세스 접속, 지연·패킷 손실, 4인 동시 플레이, 기존 운반·중력 자동화 전체 회귀, 하늘 전환 요청 시나리오, 패키징 |

입력 자동 검사는 키보드 하드웨어 검사가 아닌 Enhanced Input 액션 주입이다. 클라이언트의 상대 캐릭터 가시성 캡처는 별도 런타임 시점 설정 후 확인했다. 초기 SkyTransition의 양쪽 일치만 검사했으며 하늘 전환 애니메이션 전체를 검증했다고 해석하지 않는다. 테스트 후 에디터 Play 설정은 기존 1인 Standalone, 창 1280×720으로 복원했다.

검사 요약은 `p0-verification.json`에 보관한다. 상세 로그·원시 샘플·스크린샷은 로컬 `Saved/Prototype01`과 `Saved/TrainFreightMigration`에 있으며 커밋 대상이 아니다.

### 재현 가능한 제한과 다음 단계의 선행 조건

- 노란 구역에 들어가도 라운드 종료·탈출은 발생하지 않는다. P0 표시 영역이며 P1 이후 서버 판정 연결이 필요하다.
- E를 눌러도 자동 전송 화물을 확보할 수 없다. 전송 Actor·홀드·가치·HUD·장치·상처·타이머·정산은 미구현이다. P1 작업 전에 위 opt-in 연결점과 라운드 컴포넌트 계약을 구현·검증해야 한다.
- 4개 시작점은 배치했지만 이번 네트워크 검사는 2인이다. 4인 합격 기록으로 사용하지 않는다.
- 검수에서 발견한 표지 뒤집힘·배치 오류는 최종 맵에서 수정하고 다시 검사했다. 최종 P0 검사 범위에서 남은 실패는 없다.
- 신규 바이너리 5개는 생성·편집 전에 LFS 락을 확보했다. 원본 맵의 작업 시작 전 락은 건드리지 않았다. 커밋·푸시·PR·팀 메시지는 실행하지 않았다.
- 신규 에셋 5개의 락은 현재 `bizet12` 소유로 유지한다. CONTRIBUTING의 푸시 후 해제 규칙에 맞춰 후속 커밋·푸시 때 해제하며, 당일 해제 규칙도 확인해야 한다. 기존 원본 맵 락과 구분한다.

주요 신규 텍스트 파일은 이 상태 문서, `p0-verification.json`, `Tools/Prototype01/create_p0.py`, `validate_p0.py`, `verify_pie.py`이다. 새 런타임 C++ 파일은 없다.

## 2026-10-01 · P1 확보부터 정산까지 한 판 연결

작성자 : 임진혁

**P1 구현 및 아래 범위의 검증 완료.** 한 객차에서 2명이 준비하고, 호스트가 시작한 뒤 E 홀드로 일반 화물을 자동 전송하고, 표시 구역의 단말을 조작해 탈출·정산·같은 맵 새 라운드를 진행할 수 있다. 상처·스태미나·위험 장치·S01·정비·포드·핑은 이번 구현에 포함하지 않았다. 음원 청취와 별도 PC 등의 미실행 항목은 아래에 남긴다.

### 기준과 통합 결정

- 브랜치 `feature/teststage`, 시작 HEAD `f2a62353815b1290279cf08d237f46a4df608312`. 시작 시 추적 파일 변경은 없었고, 사용자 보유 팩과 P0 결과를 포함한 미추적 파일 2,979개를 보존했다. 원시 시작 목록은 `Saved/Prototype01/p1-start-git-status.txt`다.
- 설치 엔진은 `E:/UE_5.8`, **5.8.2 / CL 56702186**. 엔진 버전, 프로젝트 기본 맵, 공용 입력 에셋, 기존 Train/PlanetTrain 부모는 변경하지 않았다.
- P0의 전용 자식 BP에 `USP1RoundComponent`와 `USP1InteractionComponent`를 붙였다. 새 C++ GameState/Pawn으로 부모를 교체하지 않았다. 기존 MotionState·SkyTransition·배경 연결을 재사용한다.
- E는 실제 `/Game/Input/Actions/IA_Interact`, Boolean, 트리거 없음이다. `ASPPlayerCharacter`는 전용 상호작용 컴포넌트가 있으면 새 Started/Triggered/Completed/Canceled 경로만 바인딩한다. 없으면 private `Interact()`의 즉시 집기 경로를 유지한다.
- 일반 자동 전송은 별도 `ASP1TransferCargo`이며 인벤토리 슬롯·Small/Mid/Large 운반을 사용하지 않는다. 기존 운반 제한 외에 추가한 슬롯/놓기 차단은 P1 확보 중에만 적용된다.
- 계획의 이동 450cm/s를 팀 기본값처럼 쓰지 않았다. 실제 팀 BP의 걷기 **400**, 달리기 **700**을 유지하고 확보 중 걷기는 **200cm/s**다. 기존 SavedMove에 홀드 상태를 추가해 압축 플래그, 결합, 중요 이동, 재생과 서버 보정을 함께 처리한다. 무중력은 관성을 지우지 않고 추진만 50%로 제한한다.
- 재시작은 서버의 **동일 월드 내 ResetRun**이다. 새 RunId, 화물 초기화, 참가자 초기화, Pawn 재스폰으로 새 판을 만든다. 배경 월드를 ServerTravel로 다시 로드하지 않는다.

### 구현한 행동과 서버 규칙

1. 두 플레이어가 HUD의 준비 버튼을 누르면 호스트가 임무를 시작한다. 기본 개발 시간은 120초이며 `BP_SP1GameState`의 SP1Round 컴포넌트에서 바꾼다. C++ 기본값은 본편 후보 420초다.
2. 조준 대상의 이름·가치·시간이 표시된다. E 유지 중 개인 진행률을 보여주고 해제·가림·시선 이탈·다른 대상·연결 종료로 취소한다. 이유는 3초 표시한다.
3. 서버가 시작/매 Tick/완료에 생존 Pawn, 참가자, 라운드, 거리, 각도, 벽을 검사한다. 기본값은 시작 250cm, 유지 300cm, 각도 20도, 시선 유예 0.25초, 유지 신호 0.2초/타임아웃 0.75초다. 벽과 거리에는 추가 유예를 두지 않는다. 계획의 거리 유예는 최대 0.1초 허용 범위이며 P1은 즉시 취소를 선택했다.
4. 서버가 AttemptId·시작 시간·DataAsset의 시간을 결정한다. 요청에는 RunId·입력 RequestId·대상 참조만 전달한다. 오래된 취소/ACK/이전 판 요청은 새 시도를 지우지 못한다. 월드 화물의 Owner를 바꾸지 않는다.
5. 개인별 확보 시간은 독립적이다. 서버 원장이 CargoId를 한 번만 승인하며, 화물의 복제된 Transferred 상태와 팀 미정산 가치를 함께 갱신한다. 완료·가치 숫자를 받는 클라이언트 RPC는 없다.
6. 노란 탈출 구역 안에서 단말을 E 1초 조작하면 10초 출발 카운트다운을 시작한다. 이후 E를 놓아도 계속된다. 기존 참가자의 구역 합류/이탈은 출발 순간 캡슐 중심으로 판정한다. 새 접속/재접속은 Ready에서만 참가시킨다.
7. Ready / Running / ExtractionCountdown / Succeeded / Failed / Aborted를 구분한다. 임무 마감이 완료·출발보다 우선하며, 마감 전 유효한 화물 완료를 반영한 뒤 탑승자를 판정한다. 한 명 이상 살아서 탑승하면 성공, 빈 탈출이나 연결된 생존자 없음은 실패다. 실패 최종 가치는 0이다.
8. 서버 RunId당 정산을 한 번만 확정한다. 결과 이후 이동·점프·추진을 중단하며, 호스트만 새 라운드를 요청할 수 있다. 연결된 원격 클라이언트의 재시작 요청은 거부한다.
9. 호스트 연결 중단은 클라이언트 GameInstance에 지급 없는 Aborted 스냅샷을 유지한다. UE가 기본 맵으로 이동해도 결과 HUD를 다시 붙이는 처리를 추가했다.

P1의 생존 검사는 소유 Controller/PlayerState, Pawn 유효성, 관전 여부와 이동 모드를 기준으로 한다. 상처 W=100에 따른 사망·부활 판정은 P2 이후 생존 컴포넌트와 연결할 대상이며 구현 완료로 표시하지 않는다.

### 파일과 자산

핵심 코드는 `Source/SpacePirate/Prototype01`의 `SP1InteractionComponent`, `SP1RoundComponent`, `SP1TransferCargo`, `SP1CargoDefinition`, `SP1ExtractionZone`, `SP1HUDWidget`, `SP1SessionSubsystem`, `SP1Types`, `SP1RunRules`다. `Tests/SP1RulesTests.cpp`는 중복 정산·시도 식별·종료 우선순위만 자동 검사한다. `Tests/SP1PIETestLibrary`는 PIE 밖에서 동작하지 않는 입력/시간 fixture다. `Editor/SP1EditorLibrary`는 최초 HUD 템플릿 생성용이며 지원되는 UMG 에디터 API를 사용한다.

공용 수정은 `SPPlayerCharacter.cpp`, `SPCharacterMovementComponent.h/.cpp`, `SPInventoryComponent.cpp`, `SpacePirate.Build.cs`다. Json/SlateCore 의존성과 에디터 빌드 전용 UnrealEd/UMGEditor 의존성을 추가했다. 새 C++ 파일 상단과 BP 설명에 작성자를 기록했다.

실행 맵: **`/Game/SpacePirate/Maps/Lvl_SPPrototype01`**. World Settings는 `BP_SP1GameMode`다. 전용 Pawn/Controller/GameState와 기존 PlanetTrain 계층을 유지한다.

| 기능 | 사용한 실제 원본 패키지 경로 | 프로젝트용 자산 경로 | 직접 제작·대체한 부분과 이유 | 검증과 남은 항목 |
|---|---|---|---|---|
| 보급 상자 40 / 0.8초 | `/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_4` | `/Game/SpacePirate/Prototype01/Data/DA_SP1Supply` | 낮고 넓은 형태. 별도 전송 Actor의 데이터로 참조 | 로드·경계·볼록 충돌·PIE 전송 확인 |
| 부품 케이스 100 / 1.2초 | `/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_3` | `/Game/SpacePirate/Prototype01/Data/DA_SP1Parts` | 네모난 케이스 형태 | 로드·경계·충돌·맵 표현 확인 |
| 합금 상자 160 / 2.5초 | `/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_1` | `/Game/SpacePirate/Prototype01/Data/DA_SP1Alloy` | 긴 상자 형태 | 로드·충돌·동시 전송·취소 검사 |
| 데이터 코어 300 / 2초 | `/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_8` | `/Game/SpacePirate/Prototype01/Data/DA_SP1Core` | 세로 원통 형태, 일반 화물이며 S01이 아님 | 로드·충돌·대상 전환·맵 표현 확인 |
| 전송 Actor | 위 메시와 원본 재질 참조 | `/Game/SpacePirate/Prototype01/Blueprints/BP_SP1TransferCargo` | 새 서버 판정 Actor, 물리 시뮬레이션 OFF | BP 컴파일·복제 소멸 확인 |
| 탈출 단말 | `/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01` | `/Game/SpacePirate/Prototype01/Blueprints/BP_SP1ExtractionZone` | 외부 문 BP를 가져오지 않고 단말 메시만 재사용 | 크기·바닥 위치·출발 조준·구역 판정 확인 |
| HUD/표지 | 기존 P0 열차와 TextRender 표지 | `/Game/SpacePirate/Prototype01/UI/WBP_SP1HUD`, 전용 맵 | 임시 정보 UI를 직접 구성. 위젯 배치는 BP Designer에 저장, C++는 데이터와 버튼 연결 | 준비/진행/결과 표시와 양쪽 조작 검사 |
| 확보 시작·취소·성공·실패음 | `/Game/Assets/Sound/InterfaceAndItemSounds/Cues/`의 `Futuristic_Click_01_wav_Cue`, `Back_Click_01_wav_Cue`, `Special_Musical_01_wav_Cue`, `Error_Buzz_01_wav_Cue` | `WBP_SP1HUD` 기본값 | 후보 4종 임시 배정. 개인 피드백은 이전 AudioComponent를 멈춰 1개만 유지 | 실제 로드와 원본 WAV 길이/비루프 확인. 음색 청취·믹스 평가는 미검증 |
| 전송 완료음 | `/Game/Assets/Sound/InterfaceAndItemSounds/Cues/Item_Sell_Purchase_01_wav_Cue` | 각 화물 DA와 `/Game/SpacePirate/Prototype01/Audio/ATT_SP1Cargo` | 복제 상태 변경 시 월드 음 한 번. 감쇠 반경 100cm + 감쇠 거리 1,000cm | 원본 1.0717초, 비루프 확인. 청감·호스트/클라이언트 청취 중복은 미검증 |

보유 SciFi 상자 9종과 Medical/Ammo 후보, 통신 단말을 실제 로드해 크기·재질 참조·simple collision을 조사했다. 선정한 4개는 같은 메시의 색상 변형이 아닌 서로 다른 원형이다. `/Game/Assets` 원본은 수정하지 않았다. 화려한 전송 입자는 추가하지 않았고, 대상/진행/중단 글자와 서버 확정 후 소멸로 상태를 표현한다. 카메라·레이저·은하 셰이더·가방은 P1에 배치하지 않았다.

배치 기준은 `CarId=P01_C01`. CargoId는 `P01_C01_Supply/Parts/Alloy/Core`다. 구역 중심 `(-650,0,330)`, 반범위 `(200,150,120)`, 출구 기준 `(-450,0,320)`이며 4개 PlayerStart를 유지했다. 원본 `Lvl_SPTrainFreight`는 그대로다.

### 실행과 재현

1. UE 5.8.2로 프로젝트와 `Lvl_SPPrototype01`을 연다.
2. Play: **2 Players / Play As Listen Server / Run Under One Process / New Editor Window (PIE)**.
3. 각 창에서 준비 버튼을 누르고 호스트의 임무 시작을 누른다.
4. WASD·마우스·Space·Shift는 기존 조작이다. 화물을 조준해 E를 유지한다. 전송 화물은 집어서 들지 않으며 완료 후 사라지고 팀 미정산 가치가 오른다.
5. 노란 구역에서 단말을 향해 E를 1초 유지한다. 10초 후 한 명 이상 구역에 남아 있으면 성공한다. 결과의 호스트 새 라운드 버튼으로 준비 상태로 돌아간다.
6. 개발 시간/거리/유예/유지 신호 설정은 각각 전용 GameState와 Pawn의 P1 컴포넌트에서, 화물 값/시간/메시는 DA에서, HUD 배치는 WBP Designer에서 변경한다. HUD의 `Status`, `Target`, `Progress`, `Reason`, `ReadyButton`, `StartButton`, `RestartButton` 이름은 연결 계약이다.

반복 검사 스크립트는 에디터 Cmd에서 `py "E:/GitHub/2ND_Run/Tools/Prototype01/<파일>"`로 실행한다. 입력은 Enhanced Input 액션 주입이며 OS 키보드/마우스 전체 검사를 뜻하지 않는다. 서버와 로컬 위치·시선은 fixture로 배치한다. 서버 점수·화물 완료는 직접 쓰지 않는다.

| 스크립트 | 조건과 결과 파일 |
|---|---|
| `verify_p1.py` | 2인 PIE. 두 역할의 취소/동시 확보/10초 탈출/실패/중복 요청/5회 재시작. `Saved/Prototype01/p1-pie.json` |
| `verify_p1_movement.py` | 2인 Ready. 실제 이동·달리기·점프 입력으로 50% 제한/복원 검사. `p1-movement.json` |
| `verify_p1_disconnect.py` | 2인 Ready. 클라이언트가 확보 중 실제 disconnect. 검사 후 PIE 새로 시작. `p1-disconnect.json` |
| `verify_p1_host_disconnect.py` | 2인 Ready. 호스트 disconnect 및 기본 맵 이동 뒤 Aborted HUD. 검사 후 PIE 새로 시작. `p1-host-disconnect.json` |
| `validate_p1.py` | 읽기 전용 commandlet BP/부모/입력/배치/의존 검사. `p1-assets-validation.json` |

UE 5.8.2의 에디터 Python 호출 구간은 `GAllowActorScriptExecutionInEditor` 때문에 RPC를 로컬로 실행한다. 그래서 테스트의 준비/시작/재시작/직접 BeginHold는 PIE 전용 `NextTick` helper를 거쳐 실제 게임 Tick에서 호출한다. E 홀드는 Enhanced Input이 실제 바인딩을 실행한다. 시간 변경은 PIE 전용 setter로 하고, live ActorComponent에 `set_editor_property`를 사용해 Construction Script가 재실행되는 문제를 피했다.

자동 검사 중 에디터의 배경 CPU 제한을 일시적으로 꺼 3 FPS 때문에 입력·시선 검사가 왜곡되지 않도록 했다. 검사 종료 시 이 설정과 원래 Play 설정으로 되돌린다. 로컬 에디터 자동화 연결은 루프백 한정이며 종료 시 해제한다.

### 로그와 검증 기록

서버는 `Saved/Prototype01/Runs/<RunId>.jsonl`과 `[SP1]` 로그에 RunId, CargoId, CarId, 서버 시간, AttemptId, playerId, 취소/종료 이유, 팀 가치를 남긴다. 주요 이벤트는 RunReady/RunStart/HoldStart/HoldRejected/HoldCancel/HoldCompleted/CargoTransferred/ExtractionStart/Disconnected/RunEnd다. 클라이언트 중단 로그는 별도 ClientAborted이며 서버 전송 집계에 포함하지 않는다. Saved 결과와 원본 문서 사본은 커밋 대상이 아니다.

최종 빌드·자산 검사·실행 결과는 아래 검증 갱신과 `p1-verification.json`에 기록한다. 실행하지 않은 네트워크 환경이나 청취 검사를 합격으로 표시하지 않는다.

### 최종 검증 결과 · 2026-10-01

| 검사 | 실제 결과 |
|---|---|
| UE 5.8.2 Development Editor Win64 | `SpacePirateEditor Win64 Development` 빌드 성공. 신규 UCLASS/에디터 의존성 반영 후 UBT 프로젝트 파일 생성 성공 |
| 전용 에셋 | BP **7개** 컴파일 정상, 게임 의존 패키지 **337개**, 누락·검사 오류 0. `BindWidget`으로 저장된 HUD 연결까지 컴파일 검사 |
| 원본 열차 읽기 전용 검사 | 에셋 **110개**, BP **37개**, 게임 의존 패키지 **285개**, 누락·오래된 참조·검사 오류 0 |
| C++ 자동 검사 | `SpacePirate` **7개 성공 / 실패 0**. 신규 시도 식별·중복 정산·종료 우선순위 3개는 경고도 0. 기존 운반/중력 검사 3개에는 기존 fixture의 경고가 있음 |
| 2인 네트워크 한 판 | **81개 판정 통과**. 두 역할 E 해제·시선 유예·벽 가림·다른 화물 조준·유지 신호 타임아웃, 동시 확보 비합산·한 번 전송, 탈출 합류/이탈, 빈 탈출 실패, 약 9초 남은 시점의 10초 출발 실패, 결과 불변, 원격 재시작 거부, 5회 재시작 |
| 이동 예측 | **13개 판정 통과**. 양쪽 소유자와 서버에서 확보 중 실제 속도/상한 200, 달리기·점프 차단, 해제 후 400 복원, 결과에서 이동 중단, 재시작에서 복원 |
| 클라이언트 이탈 | **5개 판정 통과**. 원격 확보 승인 직후 실제 `disconnect`, 4초 뒤 가치 0·미전송 유지, 참가자 이탈·생존자 1명 확인 |
| 호스트 종료 | **4개 판정 통과**. 실제 `disconnect` 후 클라이언트 Aborted·최종 가치 0. 기본 맵으로 이동한 뒤에도 결과 HUD **1개** 유지, 실제 화면 확인 |
| 기존 이동·열차 연결 | 최종 P1 맵에서 기존 P0 입력 검사를 재실행해 **50개 시점 표본** 확인. 두 명 이동·시점·점프·착지·위치 복제 정상, 열차 위치 고정, 양쪽 배경 매니저 2개의 진행 거리 증가, MotionState/SkyTransition 일치 |
| 실제 UI 조작 | OS 마우스로 클라이언트 준비 → 호스트 준비 → 호스트 시작 → 결과의 같은 맵 새 라운드 버튼 확인. E 유지 검사는 Enhanced Input 주입으로 수행 |
| 서버 로그 대조 | 반복 검사의 종료 판 **5개** 모두 CargoId별 전송 1회, RunEnd 1회, 전송 합계와 팀 가치 일치. 성공 160/40/40, 빈 탈출·임무 만료 최종 가치 0 |
| 보존·정리 | 추적 변경은 공용 코드/빌드 파일 5개뿐. 기존 Content/Config/uproject 추적 변경 없음. 시작 미추적 목록 2,979개 모두 남아 있음. 전용 C++ 20개 작성자 확인, Python 구문·`git diff --check` 통과 |

빌드는 설치 MSVC 14.51이 권장 14.50보다 새롭다는 경고와 설치 엔진 AIModule의 deprecated API 경고가 있었다. 엔진·툴체인을 바꾸지 않았다. 한국어 기본 에디터 시작 시 엔진 UnifiedError smoke 검사 구간의 `Condition failed` 메시지 15개가 재현된다. 최종 프로젝트 자동 검사 실행은 `-culture=en`을 명시했고 해당 시작 오류 없이 7개가 통과했다. 이 관측을 엔진 문제의 원인 확정이나 수정 완료로 해석하지 않는다.

검수 중 발견한 HUD 변수/위젯 이름 충돌은 `BindWidget`으로, 호스트 종료 후 결과 화면 소실은 GameInstance의 맵 로드 후 HUD 복원으로 수정했다. 최종 BP 컴파일·PIE 검사는 수정된 파일로 다시 실행했다. 임시 Python 검사 도구의 속성 이름/enum 변환 오류도 있었으나 최종 에디터 정리 확인은 성공했다.

검사 뒤 PIE를 종료하고 `Lvl_SPPrototype01`을 에디터에 열어 두었다. 저장되지 않은 맵/콘텐츠 패키지는 0개다. Play 설정은 시작 전의 1인 Standalone/1280×720, 백그라운드 CPU 제한은 ON으로 복원했다. 임시 Python 원격 실행은 OFF, MCP 경로는 해제해 HTTP 404를 확인했다. 프로젝트 설정에 자동화 접속을 저장하지 않았다.

### 미실행 항목·다음 단계 조건

- **음원 청취는 미검증**이다. 원본 SoundCue/WAV 로드·길이·비루프와 재생 경로만 확인했다. 청취 입력을 제공하지 않는 도구 환경이므로 음색·실제 음량·감쇠·두 창 중복 청취를 합격으로 기록하지 않았다. 에디터에서 `WBP_SP1HUD`의 Audio 기본값과 각 `DA_SP1*`의 CompleteSound/CompleteAttenuation을 열고, 위 표의 원본 Cue Preview 및 2인 PIE의 확보 시작/해제/완료/성공/실패를 직접 청취해야 한다. 원본 Cue는 편집하지 않는다.
- 별도 프로세스/별도 PC, 패킷 지연·손실, 4인, 패키징 빌드는 실행하지 않았다. OS E 키를 실제로 길게 누른 상태의 Alt-Tab 행렬도 미실행이며, Enhanced Input과 서버 유지 신호 중단 검사가 이를 전부 대체하지 않는다.
- 진행 중 새 접속자는 관전 처리하지만 전용 관전 HUD/재접속 UX는 P1 범위에서 만들지 않았다. 호스트 연결이 끊어진 뒤에는 Aborted 결과를 확인하고 PIE를 종료·다시 시작해 새 세션을 연다.
- P2에서 상처·사망을 붙일 때 `IsAlive()`를 서버 생존 상태에 연결해야 한다. 현재 이동 모드 기반의 임시 생존 판정을 상처 시스템 완성으로 간주하지 않는다. 기존 홀드 취소, 탈출 생존자 집계, 재시작 복원 계약을 함께 유지해야 한다.
- P1/P0 전용 바이너리 **13개**의 LFS 락은 `bizet12` 소유로 유지했다. 원본 열차 맵의 기존 락은 그대로다. 커밋·푸시·PR·팀 메시지를 실행하지 않았다. Saved/Intermediate/Binaries 및 원본 문서 사본을 새 커밋 대상으로 추가하지 않았다.

다음 단계는 위 P1 기반을 유지한 P2 범위의 명시적 요청 후 진행한다. 계획서의 다른 미구현 단계는 완료로 바꾸지 않았다.

## P2 — 상처·스태미나·사망·레이저 · 2026-10-01

작성자 : 임진혁. 이번 변경은 **P2만** 적용했다. 위 P0/P1 기록은 당시 결과이며 아래가 현재 생존 상태다. 시작 브랜치 `feature/teststage`, HEAD `f2a62353815b1290279cf08d237f46a4df608312`. 설치 `E:/UE_5.8/Engine/Build/Build.version`에서 5.8.2를 확인했다. 공용 코드 5개의 기존 P1 변경과 미추적 P1 코드·에셋·보유 팩을 보존했다. P1 native 20개와 전용 Pawn/GameState 컴포넌트가 실제 존재했으므로 계획서만 보고 기반을 재생성하지 않았다.

### 플레이와 판정

- 전용 Pawn의 `USP1SurvivalComponent`가 W/S/M을 관리한다. 한 게이지의 청록 부분은 S, 어두운 부분은 회복 가능 공간, 오른쪽 빨강은 W다. `M=max(0,100-W)`, `0≤S≤M`. 같은 Pawn의 상처는 감소하지 않고 새 판의 새 Pawn에서 초기화된다.
- 전용 걷기 450, 달리기 700cm/s, 소비 20/s, 중단 뒤 0.75초 대기와 25/s 회복. **공용 BP 걷기 400은 보존했다.** 소진 후에도 걷고 회복할 수 있다. Shift를 놓아야 다시 달리며, 계속 누른 채 회복/재소진을 반복하는 현상을 막는다. 달리기는 W를 바꾸지 않는다. 확보 중에는 225cm/s이고 달리기·점프 차단을 유지한다.
- 서버 피해와 이동 승인 응답의 스태미나를 사용한다. `FSP1MovementResponse`에 승인된 이동 시각의 자원 상태를 담고, 클라이언트는 미승인 SavedMove 입력만 다시 계산한다. 클라이언트가 S/W 숫자를 서버로 보내지 않는다. 속성 복제와 이동 응답의 순서가 바뀌어도 상처가 되돌아가지 않게 한다.
- 비치명 피해는 확보를 유지한다. W=100이면 서버가 한 번만 사망 처리하고 E 작업을 취소하며 숨은 인벤토리 슬롯과 Large 잡기를 해제한다. 사망 Pawn은 충돌·표시·이동을 끄고 소유 연결은 HUD/호스트 재시작용으로 유지한다. 카메라는 연결된 생존 팀원을 따라가며 이동·시점·확보·소지품·기존 중력 스위치 요청은 차단한다. 전원 사망은 `NoConnectedSurvivors` 실패다.
- PlanetTrain GameState 부모는 그대로다. Round의 서버 순서는 **임무 만료 → 유효 화물/출발 입력 완료 → 대기 피해·사망 → 생존/탑승 집계 → 출발 결과**다. Actor Tick 순서로 정산을 결정하지 않는다. 정비·부활·S01이나 P3 장치 시스템은 만들지 않았다.

### 레이저·에셋·실행

맵은 `/Game/SpacePirate/Maps/Lvl_SPPrototype01`, GameMode는 `BP_SP1GameMode`다. 고정 객차 1개, PlayerStart 4개, 전송 화물 4개와 출구를 유지했다. `CarId=P01_C01`, `HazardId=P01_C01_Laser01`. 레이저 중심 `(-950,100,330)`, 피해 Box 반범위 `(14,210,120)`cm다. 피해는 입자 충돌이 아닌 서버 Box로 판정한다.

RunId와 주기 시작 시각을 복제한다. **OFF 2초의 마지막 0.5초가 예고**이고 ON 2초와 합쳐 4초 주기다. ON 접촉 W+20, 같은 위험은 플레이어별 1초 간격. 값은 전용 BP/맵 인스턴스에서 편집한다. 네 빔과 글자·색이 위상을 표시하고, 결과에서 연출을 끄고 다음 시작에서 새 주기를 연다. 경고음은 예고 구간만 재생하며 위상 변경·EndPlay에서 정지한다.

| 길 | 위치와 조작 |
|---|---|
| 짧은 위험길 | `(-1150,100)` → `(-800,100)`. 켜진 동안 통과하면 피격 가능 |
| 안전 우회길 | `(-1150,-160) → (-1100,-275) → (-775,-275) → (-650,0)`. 흰 선과 SAFE WALK 표식을 따라 걷기로 출구에 도달 |

좌표 Z는 플레이어 중심 약 308cm다. 우회로는 점프·웅크리기에 의존하지 않는다. 실행은 P1의 **2 Players / Listen Server / Run Under One Process / New Editor Window** 절차와 같다. 양쪽 준비 → 호스트 시작 후 Shift로 소진·회복을 확인하고 레이저/우회길을 선택한다. 사망한 호스트도 결과 화면에서 새 라운드를 시작할 수 있다.

`asset-usage-instructions.md`에 따라 아래 보유 원본을 실제 로드해 사용했다.

| 원본 경로 | 적용·확인 |
|---|---|
| `/Game/Assets/VFX/FreeStylizedLaserBeamVFX/VFX/NS_Laser_01` | 레이저 네 줄. 공식 Niagara API로 `Color:LinearColor`, `LaserEnd:Vector3f` 공개 변수를 확인해 색·월드 끝점 연결 |
| `/Game/Assets/VFX/FreeStylizedLaserBeamVFX/Meshes/LaserCannon` | 양끝 발진기/수신기. bounds·재질을 조사하고 0.1 배율 적용 |
| `/Game/Assets/Sound/InterfaceAndItemSounds/Cues/Futuristic_Alarm_01_wav_Cue` | 예고음. 로드 길이 약 10.971초, 사용 구간은 Warning 동안만. P1 `ATT_SP1Cargo` 감쇠 재사용. 청취는 아래 한계 참조 |

`NS_Laser_02/03`, `BP_LaserBeam/BP_LaserBeam1`과 벽 조명 후보도 구조를 조사했다. 원본 공격 BP 대신 메시·효과만 조립했다. 길 안내에는 얇은 비충돌 표식이 적합해 엔진 Cube와 TextRender를 사용했다. 원본 팩·팀 Pawn·열차 콘텐츠는 저장하지 않았다.

### 변경 파일과 재검사 도구

- 신규 C++: `Source/SpacePirate/Prototype01/SP1SurvivalComponent.*`, `SP1SurvivalRules.h`, `SP1MovementResponse.*`, `SP1LaserHazard.*`, `Tests/SP1SurvivalTests.cpp`.
- 연결: 공용 Movement/Inventory/PlayerCharacter, P1 Round/Interaction/HUD/Editor/PIE helper. Niagara 의존성과 프로젝트 파일을 갱신했다. 공용 UCLASS 이름·부모, 엔진 버전, 기본 맵 설정은 유지했다.
- 신규 BP: `/Game/SpacePirate/Prototype01/Blueprints/BP_SP1LaserHazard`. P1 바이너리 수정은 `BP_SP1PlayerCharacter`, `UI/WBP_SP1HUD`, `Lvl_SPPrototype01` 세 개뿐이다. 나머지 P1 바이너리 10개는 시작 해시와 같다.
- `Tools/Prototype01/setup_p2.py`, `validate_p2.py`, `verify_p2.py` 추가. 기존 P1 검사에서 탈출 구역 밖 fixture만 레이저와 겹치지 않는 X=-1150으로 옮겼고 이동 기대값은 전용 Pawn 설정에서 계산한다.
- HUD Designer의 `SurvivalText`, `SurvivalCapacity`, `SurvivalStamina` 이름을 유지해야 한다. BP 설명과 전용 C++ 28개 맨 위의 작성자를 확인했다.

`verify_p2.py`는 새 2인 Ready에서 Cmd의 `py "E:/GitHub/2ND_Run/Tools/Prototype01/verify_p2.py"`로 실행한다. 위치·시선·일반 피해는 PIE fixture이며 이동/달리기/E는 실제 Enhanced Input, 인벤토리는 소유 Pawn RPC를 거친다. 레이저는 실제 서버 겹침/주기로 피해를 준다. 결과는 `Saved/Prototype01/p2-pie.json`, 자산 검사는 `p2-assets-validation.json`, 자동 검사는 `P2Automation/index.json`에 남는다. 피해·사망·레이저 위상도 기존 JSONL 로그에 기록하며 위험 식별자를 CargoId 칸에 넣고 RunId로 구분한다.

### 최종 검증 결과

세부 수치·RunId·로그·잠금 목록은 `p2-verification.json`에 기록했다.

| 검사 | 실제 결과 |
|---|---|
| UE 5.8.2 Development Editor Win64 | 최종 빌드 성공. UBT 프로젝트 파일 생성 성공. 설치 MSVC 권장 버전 및 엔진 deprecated API 경고는 남아 있음 |
| 전용 자산 | BP **8개** 컴파일 정상, 의존 패키지 **353개**, 누락/검사 오류 0. 맵 액터 28개. 공용 400 / 전용 450 / 달리기 700 확인 |
| 원본 열차 읽기 전용 검사 | 에셋 **110개**, BP **37개**, 의존 패키지 **285개**, 오류 0 |
| C++ 자동 검사 | **9개 성공 / 실패 0**. 기존 Large/LargeLeash/중력 복귀 fixture 3개는 기존 경고를 포함. 생존 예산·레이저 시계 및 P1 중복 정산/종료 순서 검사는 경고 0 |
| P2 두 역할 반복 | **77개 판정 통과**. 5초 소진, 0.75초 회복 대기, 최대치 감소, 다섯 번 상처 사망, 비치명 확보 유지/사망 취소, 숨은 슬롯 드롭 1회, 관전 시점/입력/인벤토리 RPC 차단, 전멸 실패/새 생명 초기화, 연속 레이저 피해 간격, 네 빔 ON/OFF, 걷기 우회 통과 |
| 서버와 예측 | 원격 스태미나 차이 표본 최대 **1.094 미만**. 경고 전환 차이 **약 20~25ms**. 위상 경계 ±0.15초 밖에서 서버/클라이언트 위상과 표시·빔 수 일치. 이 수치는 동일 프로세스 PIE 관측값 |
| P1 회귀 | 한 판/동시 확보/취소/탈출/정산/5회 재시작 **81개**, 확보 중 225cm/s 제한과 450 복원 **13개**, 클라이언트 이탈 **5개**, 호스트 종료 후 Aborted **4개** 통과. P2 포함 총 **180개 PIE 판정** |
| 이동·중력·배경 | 기존 중력/화물 자동 검사와 별도로 이동·시점·점프·착지·위치 복제 **50개 시점 표본** 확인. 열차 위치 고정, 배경 매니저 진행, MotionState/SkyTransition 일치 |
| 서버 로그 | P1 회귀의 종료 5판 모두 CargoId별 전송 1회, RunEnd 1회, 전송 합계=팀 가치. 빈 탈출/만료 최종 가치 0 |
| 실제 화면 | 호스트/클라이언트 레이저 ON/OFF, 안전 우회 표식, W20/S80/M80 게이지, 클라이언트 W100 관전 HUD와 생존 팀원 시점을 확인. 캡처는 `Saved/Prototype01/p2-*.png` |
| 보존 | 시작 미추적 **3,014개** 전부 유지. 기존 Content/Config/uproject의 추적 변경 0. 전용 C++ 작성자·Python 12개 구문·`git diff --check` 통과 |

검증 도중 HUD 확장에 필요한 UE 5.8 위젯 GUID 등록과 레이저 배치 인스턴스의 None 참조 오버라이드를 수정했다. 수정 후 저장된 맵 재로드, BP 컴파일, 최종 PIE 검사를 다시 수행했다. 초기 Python 검사 도구의 보호 속성/벡터 메서드 오류도 최종 검사 전에 수정했다. 최종 에디터 로그의 네트워크 종료 오류 2건은 의도한 호스트 disconnect 검사에서 발생했으며 Aborted 판정은 통과했다.

PIE를 종료하고 `Lvl_SPPrototype01`을 열어 두었다. 저장되지 않은 콘텐츠/맵 패키지는 0개다. Play 설정은 원래 1인 Standalone/1280×720으로, 백그라운드 CPU 제한은 ON으로 복원했다. 임시 Python 원격 실행은 OFF, MCP 경로는 HTTP 404를 확인했다. 전용 바이너리 **14개**의 LFS 락은 `bizet12` 소유로 유지했으며 신규 레이저 락은 `53279108`이다. 원본 열차 맵의 기존 락은 유지했다. 커밋·푸시·PR·팀 메시지는 실행하지 않았다. Saved/Intermediate/Binaries 및 원본 문서 사본은 커밋 대상에 추가하지 않았다.

### 미실행 항목과 다음 단계 조건

- **음원 청취는 미검증**이다. 에디터에서 `BP_SP1LaserHazard`의 WarningSound/Attenuation과 원본 Alarm Cue Preview를 열고, 두 PIE 창에서 0.5초 예고/ON 전환/종료를 직접 들어 음량·감쇠·중복 재생을 점검해야 한다. 청취 입력을 제공하지 않는 현재 도구로 청감 합격을 주장하지 않는다.
- 별도 프로세스/별도 PC, 지연·패킷 손실, 4인, 패키징 빌드와 실제 OS 키 유지/Alt-Tab 행렬은 실행하지 않았다. 이번 동일 프로세스 Enhanced Input 검사가 그 환경의 검증을 대체하지 않는다.
- 최종 실행한 범위에는 재현 가능한 실패가 남아 있지 않다. 다음 장치 구현은 서버 `IsAlive()`와 공통 E 시도 취소, Round의 완료→피해→출발 순서를 유지해야 한다. 정비·부활·S01·격벽·카메라 등 후속 단계는 미구현이며 완료로 표시하지 않았다.

## P3 — 여섯 객차와 장치 연결 (2026-10-01)

작성자 : 임진혁. P2의 실제 C++/저장 자산/검증 기록을 확인한 뒤 **P3만** 추가했다. 위 P2 기록의 ‘S01·격벽·카메라 미구현’은 P2 당시 상태이며, 아래 P3 결과가 현재 상태다. 정비·부활·10칸 확장·기획안 2/3은 구현하지 않았다. 상세 수치와 성공 RunId는 [p3-verification.json](p3-verification.json)에 저장했다.

### 플레이어 행동과 실제 통합

- 기존 이동·시점과 E 홀드로 여섯 칸을 전진한다. 서로 다른 두 사람이 양쪽 패널을 **함께 2초** 유지하면 격벽이 영구 개방된다. 먼저 잡은 사람은 상대를 기다릴 수 있으며 1초 시작 동기화 제한은 없다. 한쪽이 놓거나 사망하면 공동 시간이 초기화된다.
- **12초 단독 콘솔**은 마지막 생존자도 사용할 수 있다. 서버에서 먼저 시작한 경로가 점유하며, 협동 패널과 단독 콘솔의 진행을 합산하지 않는다. 열림 상태와 담당 Pawn/진행 시작 시각은 복제 속성이다.
- **S01 3초 / 350** 전송이 확정되면 연결된 `ASPGravityZone::SetGravityMode(ZeroGravity)`를 호출한다. C04만 바뀌고 통로·이웃 칸은 중력이다. 재진입해도 유지되며 새 RunId에서는 중력으로 초기화된다. 화물·선반은 고정이다. 바닥 합금을 먼저 챙기거나, 전환 뒤 높은 선반의 코어 300을 확보할 수 있다.
- 카메라는 서버 시각으로 회전한다. 800cm / 전체 60도 / 개인별 노출 1.5초 / 이탈 시 초당 0.5 감소다. 발각하면 방향을 고정해 **1초 예고**, 그 순간 C05 영역·거리·시야각·가림·생존을 다시 검사해 상처 20을 준다. 6초 재사용 대기 중에도 통과할 수 있다. 다음 칸/탈출 구역에는 피해를 주지 않는다.
- HUD에 객차와 위험 종류, 두 패널/우회 담당자와 공동 진행, S01 결과, 감시 방향·노출·예고/재사용 대기를 표시한다. 카메라의 조명·문자·보유 알람은 복제 상태를 표현하며 예고/라운드 종료 시 알람을 정지한다. 음원 청취 평가는 별도 미검증이다.

기존 `BP_SP1GameState → BP_SPPlanetTrainGameState` 부모와 열차 배경/하늘 연결을 유지했다. Round의 서버 판정 순서는 마감 → 유효 확보/장치 완료 → 레이저/카메라 피해 → 생존/탑승 → 종료다. 월드 장치에는 소유권을 주지 않고 기존 소유 Pawn의 `SP1InteractionComponent` 요청·AttemptId·유지 신호·시선 검사를 재사용했다. 일반 직접 운반 및 공용 입력·이동 파일은 P3에서 추가 수정하지 않았다.

### 맵·자산·코드

실행 맵은 **`/Game/SpacePirate/Maps/Lvl_SPPrototype01`**이다. World Settings는 기존 전용 `BP_SP1GameMode`를 사용한다. 원본 `Lvl_SPTrainFreight`와 열차 BP를 저장하지 않고 전용 맵의 Consist 배열만 Long 객차 6개로 확장했다. 열차는 고정이고 배경이 움직인다. 계획서의 10칸/다수 화물 예시는 이번 구현 완료 목록이 아니다.

| 객차 ID | 중심 X (cm), Y=0 | 실제 역할 |
|---|---:|---|
| P01_C01 | -12050 | 입구, PlayerStart 4개, 보급 40 |
| P01_C02 | -9890 | P2 레이저, 걷기 우회, 부품 100 |
| P01_C03 | -7730 | 격벽 X=-6810, 좌/우 패널과 단독 콘솔 |
| P01_C04 | -5570 | 바닥 합금 160, S01 350, 높은 선반 코어 300 |
| P01_C05 | -3410 | 카메라 1대, 두 엄폐물, 코어 300 |
| P01_C06 | -1250 | 안전 탈출 영역 (-650,0,330), 전방 단말 |

바닥 Z=210, 각 객차 판정 영역 중심 Z=510 / 반크기 (900,480,300), 객차 간격 2160cm다. S01 영역은 통로까지 늘리지 않았다. 객차 ID는 `BP_SP1CarRegion`과 각 화물/위험 ID가 기준이다. 기존 P0/P1/P2 액터 일부의 이름에는 C01이 남아 있지만 전용 배치·태그·실제 ID는 위 표를 따른다. 탈출 액터/표식은 C06 폴더로 정리했다.

- 새 C++: `Source/SpacePirate/Prototype01/SP1Bulkhead.*`(패널 포함), `SP1CarRegion.*`, `SP1GravityCargo.*`, `SP1SecurityCamera.*`, `SP1DeviceRules.h`, `Tests/SP1DeviceTests.cpp`.
- 연결 수정: 같은 폴더의 Interaction/Round/TransferCargo/HUD와 `Editor/SP1EditorLibrary.*`. 작성자 헤더와 동작 단위 주석을 추가했다. 새 모듈 의존성은 필요 없으며 기존 Engine/UMG/에디터 의존성을 사용했다. UBT 프로젝트 파일을 갱신했다.
- 새 BP: `/Game/SpacePirate/Prototype01/Blueprints/BP_SP1CarRegion`, `BP_SP1Panel`, `BP_SP1Bulkhead`, `BP_SP1GravityCargo`, `BP_SP1SecurityCamera`.
- 새 데이터/재질: `/Game/SpacePirate/Prototype01/Data/DA_SP1S01`, `/Game/SpacePirate/Prototype01/Materials/MI_SP1S01Galaxy`. 기존 전용 GameState(시험 임무 시간 **420초**)와 `UI/WBP_SP1HUD`, 전용 맵을 수정했다.
- 도구: `Tools/Prototype01/setup_p3.py`, `validate_p3.py`, `verify_p3.py`, `verify_p3_playthrough.py`. P1/P2 생성 스크립트는 현재 여섯 칸 맵에 재실행하지 않는다.

보유 원형은 실제 로드/경계/컴포넌트/재질 파라미터/BodySetup을 조사했다. `/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Door_half_01`, `SM_Comm_Terminal_01`, `SM_Sci_fi_wall_straight_01`을 문짝·패널·엄폐에 사용했다. 문틀은 기존 열차 프레임을 유지했으며 원본 자동 개폐 BP는 실행하지 않는다. 카메라는 `/Game/Assets/Object/SecurityCameras/Meshes/SM_Security_Camera` 일체형 메시만 합리적인 축으로 회전하고 CCTV 조작/화면 BP는 가져오지 않았다.

S01 받침은 `/Game/Assets/Cargo/SciFi_Props/Models/SM_Box_7`, 코어 재질은 `/Game/Assets/VFX/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/M_VFX_Lush_Galaxy_Shader`를 부모로 한 전용 MI다. 독립 링/맞는 선반 원형을 찾지 못해 링 프레임 8조각과 선반만 기본 도형으로 보완했다. 문과 엄폐의 판정도 프로젝트 인스턴스의 명시적 충돌로 분리했다. 경고음은 `/Game/Assets/Sound/InterfaceAndItemSounds/Cues/Futuristic_Alarm_01_wav_Cue`와 기존 `ATT_SP1Cargo`를 참조한다. 원본 팩은 저장하지 않았다.

### 실행과 수행한 검사

에디터에서 위 맵을 열고 Play 설정을 **Listen Server / 2명 또는 4명 / New Editor Window**로 지정한다. 각 창에서 준비를 누른 뒤 호스트가 임무 시작을 누른다. E는 대상 조준 후 유지, 무중력은 **Space 상승 / Shift 하강**이다. 관성이 있어 확보 전에 역추진/충돌 등으로 자세를 안정시키는 편이 낫다. 결과 화면의 호스트 재시작은 같은 맵에서 새 RunId와 Pawn을 만든다. 검증 종료 후 Play 설정은 원래 1인 Standalone으로 복원했다.

| 검사 | 실행 결과 |
|---|---|
| 설치 엔진/빌드 | Build.version **5.8.2, CL 56702186**. Development Editor Win64 최종 빌드 성공, UBT 프로젝트 생성 성공. 설치 MSVC 권장 버전/엔진 deprecated 경고는 남음 |
| 저장 자산 | 전용 BP **13개** 컴파일 정상, 의존 패키지 **389개**, 맵 액터 **68개**, 누락/오류 0 |
| 원본 열차 읽기 전용 검사 | 에셋 **110개**, BP **37개**, 의존 패키지 **285개**, 오류 0 |
| C++ 자동 검사 | **10개 성공 / 실패 0**. 기존 Large/LargeLeash/중력 복귀 fixture 3개는 기존 경고 포함. 개인별 노출·선형 감소 검사 추가, 기존 점수 중복/종료 우선순위·생존·중력/화물 검사 포함 |
| 2인 장치 검사 | **51개 판정 통과**. 호스트/원격 역할 교환, 지연된 패널 합류, 경로 점유 충돌, E 해제/공동 진행 초기화, 개방 복제, S01/선반/중력 경계/재진입/새 라운드 초기화, 마지막 생존자 우회, 감시 예고/피해/엄폐 |
| 4인 지연 장치 검사 | **52개 판정 통과**. 전체 장치 검사에 송신 지연 100ms / 변동 20ms / 손실 1% 적용. 출구 방향으로 카메라를 돌린 fixture에서도 다음 객차 제외 확인 |
| 감시 예고 복제 | 예고 관측 지연: 2인 약 **114~118ms**, 4인 약 **97~244ms**. 서버 예고/20 상처/엄폐/재사용 판정 통과. 동일 프로세스 PIE 관측값 |
| 실제 연속 입력 통과 | 순간이동·시간/상처/점수 fixture 없이 **2인 64.37초 / 4인 74.57초**, 각각 전원 탈출·**550 정산**. 각각 17개 판정 통과. 4인에는 위 지연/손실을 명시적으로 적용. 2인도 앞선 검사에서 유지된 동일 NetEmulation 상태의 PIE |
| 통과 중 피해 | 2인 W=0/0, 4인 W=0/20/0/0. 네 월드에서 각각 플레이어 4명 확인 |
| 연속 경계 표본 | 4인 256개 시점 관찰. 초기 전환/경계 근처 **13개 일시적 서버·클라이언트 모드 차이**가 있었음. 초기 복제 후 경계에서 100cm 이상 떨어진 **885개 비교는 일치**. 모두 경계를 통과하고 탈출했지만 순간 보정까지 없었다고 판단하지 않음 |
| 서버 로그 | 성공한 두 RunId 각각 전송 3건(40+160+350), CargoId 중복 0, 격벽 개방 1회, RunEnd 1회. RunId/CargoId/실제 CarId 포함 |

실제 화면은 `Saved/Prototype01/p3-two-player-result.png`, `p3-four-player-result.png`에, 세부 입력/위치/피해/예고 표본은 `p3-playthrough-*.json`, `p3-pie-*.json`, `p3-route-network-observation.json`에 있다. Saved 자료는 커밋 대상이 아니다. 초기 시험 도구의 무중력 조준/관성 제어와 4인 대기 좌표 충돌은 입력 경로를 보완해 재검증했다. 게임의 항력·이동 예측을 시험 편의로 바꾸지 않았다.

### 실제 플레이 관찰과 남은 조건

통과는 **자동 입력으로 실행한 실제 PIE 한 판**이며 사람의 재미 평가가 아니다. 보급 40과 바닥 합금 160을 먼저 챙긴 뒤 S01 350을 전송했다. 부품은 레이저 우회 경로를 유지하기 위해, 높은 코어는 상승·관성 제어·추가 확보 시간을 줄이기 위해, 감시 코어는 구역 안에 멈춰 노출을 쌓는 대신 통과하기 위해 남겼다. 최대 1,250 중 550을 택한 명시적 시험 정책이다. 420초 임무에 시간 여유가 있었으므로 이 결과만으로 자연스러운 위험/보상 선택이나 재미를 검증했다고 주장하지 않는다.

격벽 2초와 탈출 10초 외에 시작 시각 동기화·문 재폐쇄·감시 종료를 강제로 기다리는 과정은 없었다. 혼자 사용하는 경우 12초 우회 대기가 필요하다. 연속 통과의 0.25~1.2초 입력 안정화와 RPC 여유 시간은 자동화 도구의 대기다. 다른 플레이어가 화물을 잡는 동안 팀을 세워 둔 부분도 시험 정책이며 병렬 확보로 줄일 수 있다. 무중력→감시 경로는 2인 10.24초 / 4인 16.23초였으며 후자는 역추진·동료 통과·지연의 영향을 포함한다.

- **재현되는 성능 제한:** RTX 3060 Ti에서 동일 프로세스 PIE 창 4개를 열면 Texture Streaming/VRAM 예산 초과와 낮은 프레임률을 관측했다. `p3-four-player-ready-vram-warning.png`와 4인 결과 캡처에 남겼다. 렌더링 설정을 영구 변경하지 않았다. 별도 PC/별도 프로세스에서 실제 프레임 예산과 감시 예고 체감을 확인해야 한다.
- **경계 보정:** 위 지연·낮은 프레임 환경에서 C04 전방 경계 X=-4670 부근에 일시적인 모드 차이/재진입 보정이 관측됐다. 안정 구간과 최종 통과는 일치했다. 다음 단계 전에 실제 기기에서 같은 경계 왕복과 높은 선반의 역추진을 확인하고, 필요하면 공용 중력 이동 담당 범위로 별도 튜닝한다.
- **미실행:** 패키징, 별도 PC/프로세스 멀티플레이, 사람 조작의 재미 평가, 음원 청취. 음원은 `BP_SP1SecurityCamera`의 WarningSound/Attenuation과 원본 Alarm Cue Preview를 열고 두 PIE 창에서 1초 예고/타격/라운드 종료 시 음량·감쇠·잔류를 직접 확인한다. 기존 P1/P2의 단일 객차 고정 좌표 PIE 스크립트 전체는 여섯 칸 맵에 재실행하지 않았다.

시작 브랜치 `feature/teststage`, HEAD `f2a62353815b1290279cf08d237f46a4df608312`를 유지했다. 시작 미추적 **3,027개**가 모두 남아 있고, P3 전 기존 공용 Source 파일의 SHA-256이 동일하다. P3의 기존 바이너리 수정은 전용 맵/GameState/HUD이며 새 자산 7개는 지원 에디터 API로 생성했다. 전용 바이너리 **21개**의 `bizet12` LFS 락과 원본 열차의 기존 락을 유지했다. 강제 잠금 해제, 커밋·푸시·PR·팀 메시지는 수행하지 않았다.

최종 에디터는 전용 맵을 열어 두고 PIE 0, 미저장 패키지 0이다. 1인 Standalone/1280×720, 백그라운드 CPU 제한 ON으로 복원했고 `NetEmulation.Off`를 실행했다. 임시 Python 원격 실행 OFF, MCP 경로 HTTP 404를 확인했다. 새 단계는 이 여섯 칸 상태와 남은 성능/경계 관찰을 기준으로 시작한다.

## P4 — 10량, 정비·부활과 중간 탈출 (2026-10-01)

작성자 : 임진혁. P3 실제 코드/저장 맵과 2인·4인 통과 기록을 확인하고 P4만 확장했다. P3의 사람 재미 평가·별도 PC 성능·중력 경계 보정 관찰은 미검증/잔여 사항으로 유지한다. 계획서 5~6장과 asset-usage-instructions를 적용했다. 기획안 2/3, P5 핑, 도구와 포드는 추가하지 않았다.

### 구현과 실행

- 실행 맵은 **`/Game/SpacePirate/Maps/Lvl_SPPrototype01`**이다. 기존 전용 GameMode, `BP_SP1GameState → BP_SPPlanetTrainGameState`, 팀 Pawn/Controller·입력·배경/하늘 연결을 유지했다. 편성 원점 (0,0,0)의 고정 지형이다. **C05 정비와 C10 기관실을 포함한 10량**이며 별도 11번째 차량은 없다. 기관실은 기존 Long 원형을 사용한 시험 공간이다.
- 첫 생존자의 C05 진입은 실제 서버 시간 **30초 정비를 한 번** 시작한다. 남은 임무 시간만 고정하고 확보·이동·레이저·감시는 계속된다. 30초 만료, 생존자의 C06 이후 진입, 출발 승인 또는 라운드 종료가 정비를 닫는다. 먼저 앞칸으로 간 생존자가 있으면 후발 진입으로 시간을 추가하지 않는다. 재진입은 재활성화하지 않는다.
- 정비 패널 **E 3초**는 연결된 사망 팀원을 **W50/S50**으로 부활시킨다. 참가자별 1회 여부는 Round에 유지한다. 같은 Controller/PlayerState에 새 Pawn을 배정하여 기존 이동 예측의 단조 상처 규칙과 충돌하지 않는다. 사망 때 떨어뜨린 소지품은 회수/재생성하지 않는다. 안전 위치가 막히면 그 참가자의 횟수를 소모하지 않는다.
- 활성 정비의 C05 또는 C10 기관실에서 **E 1초 → 출발 10초**를 시작한다. 서버가 출발 장소/ID와 Phase를 함께 고정하며 한 라운드에 한 출발만 유효하다. 중간 출발은 정비를 닫고 임무 시계를 재개한다. 출발 순간 살아 있는 캡슐 중심으로 탑승을 판정한다.
- C10 최종 구역은 X=-1050~-650, Y=±200이며 앞 200cm인 X=-1250~-1050도 보호한다. Round 피해 큐와 적용 단계에서 모든 위험 피해를 제외하고, 카메라 감지 후보에서도 제외한다. 표식은 이 경계와 일치한다.
- HUD는 정비 잔여 시간/종료 이유, 부활 조건, 출발 장소/카운트다운/인원, 결과의 전송·정산 가치와 탈출·미탈출 인원을 복제 상태로 표시한다. 로그에는 RunId/CargoId/CarId, 설정 ID, 출발 ID, 탈출·미탈출 인원을 남긴다.

서버 순서는 **임무 만료·정비 종료 → 유효 확보/장치 완료 → 피해·사망 → 탑승/종료**다. 같은 시각의 정비 종료는 부활/중간 출발보다 먼저다. 늦은 프레임에도 예정된 정비 종료 시각에 남은 임무 시간을 더하므로 프레임 지연을 무료 시간으로 지급하지 않는다. 승인된 출발은 계속 진행한다.

Play를 Listen Server / New Editor Window / 2명 또는 4명으로 설정하고, 각 창에서 준비한 뒤 호스트가 시작한다. WASD/마우스와 E 홀드는 기존 방식이다. 결과 화면의 호스트 재시작은 같은 맵에 새 RunId와 Pawn을 만들고 화물·중력·격벽·정비·부활 횟수·출발 상태를 초기화한다. 일반 맵의 private Interact 즉시 집기와 공용 운반 코드는 P4에서 변경하지 않았다.

### 배열과 데이터

| 칸 / 중심 X(cm) | 역할 | 배치 | 가치 |
|---|---|---|---:|
| C01 / -20690 | 입구 | 보급 4 | 160 |
| C02 / -18530 | 레이저·왼쪽 걷기 우회 | 부품 2, 합금 2 | 520 |
| C03 / -16370 | 협동 격벽·단독 우회 | 부품 4 | 400 |
| C04 / -14210 | S01·높은 선반 | 코어 2, 부품 2, S01 1 | 1,150 |
| C05 / -12050 | 정비·부활·중간 탈출 | 없음 | 0 |
| C06 / -9890 | 감시·엄폐 | 코어 2, 합금 2 | 920 |
| C07 / -7730 | 세 작업 구역 | 부품 6, 합금 4 | 1,240 |
| C08 / -5570 | 반대쪽 S01 선반 | 코어 2, 부품 2, S01 1 | 1,150 |
| C09 / -3410 | 레이저·오른쪽 걷기 우회 | 코어 2, 합금 2 | 920 |
| C10 / -1250 | 기관실·입구 감시·최종 탈출 | 코어 2 | 600 |
| 합계 | 정비/기관실 포함 10량 | **일반 40 + S01 2** | **7,060** |

`/Game/SpacePirate/Prototype01/Data/DA_SP1Plan04`에 순서와 화물 종류/개수/특수 여부를 저장했다. 서버는 실제 CargoId·정의·객차별 수량·총합을 대조하고 불일치 시 시작을 거부한다. 읽기 전용 검사도 화물의 실제 객차 영역과 **10개 바닥/영역 중심의 일치**를 검사한다.

같은 Data 폴더의 `DA_SP1FourPlayer420`(4인 시험), `DA_SP1TwoPlayer600`(2인 시작), `DA_SP1Original600`(4인 원안 비교)을 분리했다. 기본은 연결 인원으로 2인/4인 설정을 선택한다. 원안 비교는 `Blueprints/BP_SP1GameState`의 **SP1RoundComponent → Settings Override**에 `DA_SP1Original600`을 지정하고 컴파일/저장한 뒤 새 PIE를 시작한다. 비교 후 None으로 복원한다. 편집 전 본인 LFS 락을 확인한다. 개발용 임의 시간은 Use Settings Assets를 끄고 Mission Seconds로 지정한다.

### 실측과 보유 애셋

`SM_SPLong_Floor` 실제 경계는 **2000×1000cm, 스케일 1**이다. 바닥 윗면 Z=210, 지붕 충돌 아랫면 Z=810, 문 통과 폭 500cm·높이 420cm를 확인했다. 객차 간격은 2160cm, 연결 간격은 160cm다. 자료의 20m×5m×4m를 실측값으로 쓰지 않았다. 전용 Pawn 캡슐은 반지름 34cm·반높이 96cm이며 바닥 정지 중심은 Z≈308.15다.

네 PlayerStart는 X=-21340/-21040, Y=±100, Z=320이다. 두 탈출 영역은 400×400cm다. 정비 부활 위치는 (-12700,-180/80,320), (-12480,-180/80,320)이며 서버가 캡슐 충돌 검사 후 빈 위치를 선택한다.

| 기능 | 실제 원본 패키지 | 프로젝트 적용 / 대체 이유 | 검증 |
|---|---|---|---|
| 10량/기관실 | `/Game/SpacePirate/Train/Freight/Data/DA_SPCar_Long`, `Blueprints/BP_SPFreightCar_Long` | 전용 맵 편성 10개와 C10 표지, 원본 원형/충돌 보존 | 경계/충돌 실측, 바닥/영역 정렬 |
| 정비/중간 탈출 패널 | `/Game/Assets/BackGround/Sci_fi_hallway/Models/SM_Comm_Terminal_01` | `BP_SP1MaintenanceStation`과 기존 ExtractionZone 원형. E 판정 재사용 | 메시/조준 충돌, BP 컴파일, 소유 Pawn 입력 |
| 정비 의료 소품 | `/Game/Assets/Cargo/AE_BR_Props/Models/Medical_Box_SM`, `Medical_Box_Top_SM` | C05 몸통+뚜껑을 같은 원점에 조립, 스케일 2, 장식만 사용 | 두 메시 경계·단순 충돌 조사, 원본 미저장 |
| 추가 위험/S01 | 기존 전용 GravityCargo/LaserHazard/SecurityCamera BP와 보유 팩 참조 | C08/C09/C10 인스턴스. 새로운 장치 종류 없이 재배치 | 독립 중력 참조, 데이터, 안전 범위 |
| 안전/작업 표식 | 기존 탈출 재질·Engine Cube·TextRender | 두 4m×4m 구역, 최종 앞 2m, C07 작업 구역 | 저장 좌표·크기 및 PIE |

보유 `SM_wall_lamp_C`도 조사했으나 큰 비대칭 원형이고 기존 열차 조명/문자/HUD로 상태를 표시할 수 있어 추가하지 않았다. P4 패널 시작/취소·결과음은 기존 보유 HUD 음원을 재사용한다. 의료·조명 팩의 별도 입력이나 기능은 가져오지 않았다. 음원 청취 평가는 미실행이다.

### 검증 기록

P4 구현과 아래 로컬 검증을 마쳤다. 세부 기록은 `p4-verification.json`, 원시 결과·로그·화면은 커밋에서 제외되는 `Saved/Prototype01`에 있다.

- 설치 엔진 **5.8.2 / CL 56702186**, Development Editor Win64 빌드 및 UBT 프로젝트 생성 성공.
- C++ 자동 검사 **12개 성공**(경고 없음 9 / 기존 fixture 경고 3), 실패 0. 정비 시계/종료 경계·참가자별 부활 한도 검사 추가.
- 전용 BP **14개** 컴파일 정상, 의존 **404개**, 맵 Actor **144개**, 누락/오류 0. 원본 Train 읽기 전용 검사는 에셋 **110 / BP 37 / 의존 285**, 오류 0.
- 생성 중 중간 단말 메시 누락과 편성 Actor 이동을 발견해 전용 맵에서 수정했다. 단말 조준 충돌과 모든 바닥/장치 영역의 정렬을 검사에 추가했다. 최종 앞 2m 표식도 실제 판정과 일치시켰다.
- 실제 연속 이동에서 C02/C09 걷기 우회와 C06 엄폐 우회를 좁히는 새 합금 배치를 확인해 안쪽으로 옮겼다. 수량/가치/장치 규칙은 유지했고 연속 통과를 다시 실행했다.

| 실제 PIE 검사 | 결과와 범위 |
|---|---|
| 2인 규칙 | **114개 판정 통과**, 서로 다른 RunId 9개. 호스트/원격 조작자와 부활 대상 역할 교환 |
| 4인 규칙 | **121개 판정 통과**, 서로 다른 RunId 9개. 동시 정비 진입/두 패널 조작자, 두 사망자 동시 부활, 전원 종료 이유 복제 포함 |
| 정비·부활 | 실제 30초 창, 임무만 정지, 뒤 객차 확보 40과 레이저 피해 계속, W50/S50·새 Pawn 소유/시점/이동 복구, 드롭 유지, 같은 참가자 두 번째 부활 거부 |
| 정비 경계 | 먼저 전진한 팀원, 재진입 불가, 부활/중간 출발 완료와 정비 종료의 정확한 동시 경계에서 종료 우선 |
| 출발 | 원격 중간 출발, 이탈/재탑승, 중간·최종 동시 요청 중 한 곳만 승인, 빈 출발 실패, 임무 만료/출발 동시에는 실패, 네 캡슐 탑승/최종 앞 2m·구역의 피해 차단 |
| 전체 초기화 | 매 재시작에 42개 화물·가치·출발·격벽·상처·정비·부활 횟수 초기화. 4인에서는 두 S01을 전송해 700을 만든 뒤 두 중력 영역/연결 통로 복제와 초기화를 확인 |
| 서버 원장 | 검사 RunId **18개 각각 RunEnd 1회**, 출발·정비 시작 각 최대 1회, 같은 CargoId 중복 전송 0 |
| 연속 입력 통과 | **2인 79.21초 / 4인 80.78초**, 각각 **18개 판정 통과**, 전원 최종 탈출·**40 정산**, W=20/20 및 W=0/0/0/0 |

규칙 검사는 실제 Enhanced Input과 소유 Pawn RPC를 사용하며 위치·피해·정확한 경계 시각에는 명시적인 PIE fixture를 썼다. 점수/완료/부활 결과를 직접 쓰지 않았다. 2인 결과는 단말 조준 안정화 대기 수정 후 이어서 실행했고, RunId 직렬화 오류는 실제 서버 JSONL의 9개 식별자로 재대조했다. 4인은 드롭의 물리 낙하가 안정되기 전 위치를 비교한 시험 도구 오류를 고친 뒤 **처음부터 전부 재실행**했다. 초기 운반 fixture의 서버 Actor를 원격 월드 RPC 인자로 전달한 오류도 로컬 복제 Actor 참조로 수정했다. 이때의 실패/ensure 로그를 최종 통과로 감추지 않고 Saved에 남겼다.

연속 플레이는 스폰 뒤 위치·시간·상처·점수 fixture 없이 이동/시점/E 입력만 사용했다. C02 왼쪽·C09 오른쪽 걷기 우회, 협동 격벽, C04/C08 바닥, C05 정비, C06 엄폐, C10 탈출을 통과했다. **첫 보급 40만 확보**하고 높은 코어·S01·나머지 화물을 남기는 통과성 시험 정책이다. 사람이 위험/보상 때문에 남긴 결과로 해석하지 않는다. 강제 대기는 격벽 2초·출발 10초이며 정비에서 표지 확인/합류에 2초를 썼다. 그 밖의 조준 안정화와 입력 여유 대기는 시험 도구에 포함된다. 전진하면 30초 정비 만료를 기다릴 필요가 없었다.

연속 플레이 RunId는 2인 `3016D8BE4D2A7AC02CCD26AC2123DB8A`, 4인 `544B70EA462632883A9463BD2A70DC37`다. 결과 화면은 `Saved/Prototype01/p4-two-player-result.jpg`, `p4-four-player-result.jpg`, 4인 스폰은 `p4-four-player-ready.jpg`다. P4 네트워크 에뮬레이션은 명시적으로 **OFF**였다. P3의 지연 검사 결과를 P4 재검증으로 대신 표시하지 않았다.

**재현되는 제한:** 동일 PC/프로세스에서 PIE 창 네 개를 열면 RTX 3060 Ti의 Texture Streaming/VRAM 예산 초과 경고가 다시 나타난다. 4인 결과 화면에도 남겼다. 통과 성공은 별도 기기의 프레임 성능이나 사람의 경고 인지/재미를 보장하지 않는다. P4의 추가 지연/손실, 실제 클라이언트 연결 종료 후 정비 부활, 패키징/별도 PC, 사람 조작·음원 청취는 미실행이다. 연결 종료자의 부활 제외는 C++ 규칙 검사로 확인했다. 후속 검증은 같은 맵의 C05에서 원격 사망자를 연결 종료시킨 뒤 생존자가 E 3초를 유지해 새 Pawn/부활 횟수가 생기지 않는지 확인한다. 추가 지연은 Play의 Network Emulation을 켜고 정비 종료 경계와 두 출발 요청을 반복한다.

### 변경 범위와 다음 조건

새 C++는 `SP1ScenarioDefinition.h`, `SP1MaintenanceStation.*`, `SP1RoundMaintenance.cpp`, `Tests/SP1MaintenanceTests.cpp`다. 기존 전용 Round/Types/RunRules/Extraction/Interaction/Survival/SecurityCamera/HUD와 PIE 시험 라이브러리를 연결했다. 새 모듈 의존성은 필요 없으며 새 UCLASS의 프로젝트 파일을 갱신했다.

새 자산은 MaintenanceStation BP, Plan04와 세 시간 설정 DataAsset의 **5개**다. 기존 수정 자산은 전용 맵/GameState/HUD/S01 표시 이름이다. 원본 열차 맵·원본 팩·공용 BP는 저장하지 않았다. 도구는 `setup_p4.py`, `validate_p4.py`, `verify_p4.py`, `verify_p4_playthrough.py`다. 이전 P0~P3 생성 스크립트를 현재 10량 맵에 재실행하지 않는다.

별도 PC/패키지 멀티플레이, 사람 재미 평가, 음원 청취는 자동화 결과와 구분한다. P3의 4창 VRAM/프레임 제한과 일시적인 중력 경계 보정은 해소됐다고 표시하지 않는다. P5 진행 시 실제 사람 플레이와 별도 기기 성능 확인이 필요하다.

P4 시작 브랜치 `feature/teststage`, HEAD `f2a62353815b1290279cf08d237f46a4df608312`를 유지했다. 원본 열차/보유 팩 **1,724개**, 원본 열차 맵과 공용 Source의 SHA-256은 작업 전과 같다. 시작 미추적 **3,049개**가 모두 남아 있다. `bizet12`의 기존 락 22개에 새 전용 자산 5개를 추가해 **27개**를 유지했다. 다른 사람 락 해제, 커밋·푸시·PR·팀 메시지는 하지 않았다.

최종 에디터는 전용 맵을 열어 두고 PIE 0 / 미저장 패키지 0이다. 1인 Standalone·1280×720·백그라운드 CPU 제한 ON으로 복원하고 `NetEmulation.Off`를 실행했다. 임시 Python 원격 실행 OFF, MCP 경로 HTTP 404를 확인했다.

## 2026-10-01 · P5 화면·필수 표현·핑·기록

작성자 : 임진혁

**P5 기능 구현과 아래 로컬 검증을 수행했다.** P4의 10량, 일반 40개+S01 2개, 총 7,060, 정비/중간·최종 탈출 구조를 유지한다. 사람의 청취·설명 없는 이해도 평가, 별도 PC/패키지 성능을 완료로 표시하지 않는다. P6 및 다른 기획안 기능은 추가하지 않았다.

### 실제로 가능한 행동과 실행

`/Game/SpacePirate/Maps/Lvl_SPPrototype01`을 열고 Play를 **Listen Server + 2명 또는 4명**으로 실행한다. 각 창에서 `준비 / 준비 취소`, 호스트에서 `임무 시작`을 누른다. 이동·시점·E 확보·정비·탈출은 P4의 제품 경로를 사용한다. 결과 화면의 호스트 `같은 맵 · 새 라운드`로 다시 시작한다. 연결이 끊긴 클라이언트는 `연결 중단 · 정산 없음` 화면에서 `새 로컬 호스트로 준비 화면 열기`를 눌러 로딩 후 이 맵으로 돌아온다. 이는 이전 서버 자동 재접속이나 온라인 매칭 기능이 아니다.

- 일반 보급/부품/합금/코어와 S01을 서로 다른 기존 메시와 새 아이콘으로 구분한다. 재질과 독립적인 `[E] 이름 +가치`, `[!] S01`, `[완료] 전송됨` 표식을 사용한다. 대상 패널에 가치·확보 시간·진행률·취소/완료 문구를 표시한다.
- HUD에 임무 시계, 팀 미정산 가치, 현재 칸, P1~P4 번호/이름/생존·사망·상처·부활 사용, 정비 시간/종료 이유/부활 대상 수, 출발 장소/탑승·생존·구역 밖 인원, 결과 정산을 표시한다. 결과 이후 시계를 최초 시간으로 돌려 보이지 않고 종료와 정산을 표시한다.
- **MMB**는 서버 시점에서 30m 안의 첫 가림 표면에 고정 위치 핑을 만든다. 위치/화물/위험 아이콘, 발신자 번호와 색을 함께 표시한다. 수명 8초, 인당 최대 2개(오래된 것 교체), 재사용 1초다. 좌표·대상 Actor를 클라이언트가 확정하거나 계속 추적하지 않는다. 사망/종료 후 새 요청은 거부하고 종료·재시작 시 모두 지운다.
- 60초/30초, 출발, 마지막 5~1초, S01 확보 시작, 전송 완료, 취소, 감시 경고, 성공/실패를 서로 다른 사건/큐로 연결했다. 준비·결과 화면의 `피드백음` 버튼으로 프로토타입 UI/장치음을 끌 수 있다. 문구와 아이콘은 음소거 중에도 남는다. 공용 엔진 전체 볼륨 설정을 바꾸는 버튼은 아니다.
- 전용 Pawn의 작은 입력 계층이 MMB를 처리하고 **F6~F9, 0, NumPad 0**을 소비한다. 원본 Pawn/Controller와 IMC는 수정하지 않았다. 동일 프로세스 PIE에서 다른 창이 입력 포커스를 빼앗던 문제를 고쳐 활성 뷰포트에만 입력 모드를 적용한다.

### 보유 자산과 직접 제작한 부분

P4에서 사용한 열차/플레이어/배경/문/패널/레이저/감시 메시와 재질을 그대로 참조한다. 아래 화물 메시의 원본 폴더는 `/Game/Assets/Cargo/SciFi_Props/Models/`다.

| 역할 | 기존 메시 | 전용 아이콘 | 가치 / 시간 |
|---|---|---|---|
| 보급 | `SM_Box_4` | `T_SP1Supply` 의료 상자 | 40 / 0.8초 |
| 부품 | `SM_Box_3` | `T_SP1Parts` 공구 | 100 / 1.2초 |
| 합금 | `SM_Box_1` | `T_SP1Alloy` 주괴 | 160 / 2.5초 |
| 코어 | `SM_Box_8` | `T_SP1Core` 코어 | 300 / 2초 |
| S01 | `SM_Box_7` + 기존 전용 받침·링·구체 | `T_SP1S01` 궤도 | 350 / 3초 |

아이콘 5개와 `T_SP1PingLocation`, `T_SP1PingCargo`, `T_SP1PingDanger`는 **256×256 투명 UI 텍스처**다. 보유 팩의 표면 재질을 바꿔도 의미가 유지되는 최소 기호가 필요해 `Tools/Prototype01/make_p5_icons.py`로 직접 그렸다. UE `AssetImportTask`로 `/Game/SpacePirate/Prototype01/UI/Icons`에 임포트하고 Author를 기록했다. 임포트용 PNG는 Saved에만 두며 재생성 가능하다. UI 배치는 전용 `UI/WBP_SP1HUD`의 Designer 트리에 저장한다. 새 메시·온라인 서비스·구매·외부 다운로드는 없다.

음원은 실제 SoundCue/SoundWave를 로드하여 노드, 반복 여부, 길이를 조사했다. `I`는 `/Game/Assets/Sound/InterfaceAndItemSounds/Cues/`, `S`는 `/Game/Assets/Sound/SciFiUISFX/Cues/`다. 원본 SoundCue는 저장하지 않았다.

| 사건 | 원본 SoundCue | 길이(초) |
|---|---|---:|
| 확보 시작 | I `Futuristic_Click_01_wav_Cue` | 1.85 |
| 취소 | I `Back_Click_01_wav_Cue` | 1.22 |
| 전송 성공 | I `Item_Sell_Purchase_01_wav_Cue` | 1.07 |
| S01 예고 / 전송 | I `Special_Powerup_02_wav_Cue` / `Special_Powerup_03_wav_Cue` | 1.99 / 2.04 |
| 60초 | S `Rings/Ring_Pitched_Down_Cue` | 0.69 |
| 30초 | S `Tone1/Tritone/Tone1A_TritoneUp_Cue` | 1.00 |
| 탈출 시작 | I `Special_Powerup_01_wav_Cue` | 2.97 |
| 마지막 카운트다운 | S `Clicks/High_Click_1_Cue` | 0.69 |
| 감시 경고 | I `Futuristic_Alarm_01_wav_Cue` | 원본 10.97, 예고 1초 종료 시 정지 |
| 성공 / 실패 | I `Special_Musical_01_wav_Cue` / `Error_Buzz_01_wav_Cue` | 2.11 / 1.86 |

12개 원본 모두 단일 WavePlayer, 비반복이며 분석용 PCM 결과의 해시도 서로 달랐다. **사람 또는 오디오를 해석하는 도구의 청취 검사는 하지 못했다.** 이름만으로 음색 적합성을 확정하지 않는다. 실제 컴포넌트 재생/거리 감쇠/중단은 아래 PIE에서 확인했다. 원본 팩의 취득·라이선스 증빙은 기존 보유 기록을 따르며 이번에 새 권리를 취득했다고 주장하지 않는다. asset-list의 22개 기능 큐 제안을 전부 완성한 것으로 표시하지 않는다.

### 서버 기록과 검사 도구

`Saved/Prototype01/Runs/<RunId>.jsonl`에 서버 한 곳만 **schemaVersion 2**를 기록한다. `sequence`, `buildId`(HEAD+Prototype01 소스 해시), `settingsVersion=Plan01-P05.1`, `layoutVariant=A`, `playerCount`, `settingsId`, `missionLimit`, 서버 시각, RunId/CarId/CargoId/AttemptId/플레이어 ID/사유/가치를 포함한다.

계획서의 `CarEnter/Exit`, `AcquireStart/Cancel/Complete`, `HazardHit`, `GateOpen`, `GravityChange`, `MaintenanceStart/End`, `Revive`, `ExtractionStart`, `RunEnd`를 연결했다. 이전 이름은 `legacyEvent`로 보존한다. `AcquireComplete`만 완료 원장으로 집계하며 `HoldCompleted`를 다시 더하지 않는다. 퇴실 시 객차 잔여 가치, 시작/종료 시 전체 객차 잔여 가치도 남긴다. 사망/연결 종료/라운드 종료 시 열린 방문을 닫는다.

`Tools/Prototype01/audit_runs.py`는 순번·중복 CargoId·중복 종료·설정 혼합·전송 누계·성공 지급·실패/중단 0 지급·인원·객차 예산을 대조한다. 개인 방문, 팀 점유 구간, 첫 입실~마지막 퇴실 시간, 첫 개인 퇴실/팀 전원 퇴실/종료 시 잔여 가치를 구분한다. 예:

```powershell
python Tools/Prototype01/audit_runs.py Saved/Prototype01/Runs/35A1EFC84755E33BEEDA3EBE8C823238.jsonl --out Saved/Prototype01/audit-example.json
python Tools/Prototype01/test_audit_runs.py
```

이 도구는 P5 스키마용이다. P0~P4 로그를 조용히 P5 기록으로 간주하지 않는다. 로그·PNG·WAV·테스트 산출물은 Saved에 있고 커밋 대상이 아니다.

### 수행한 검증

| 검사 | 실제 결과 |
|---|---|
| UE 기준 / 빌드 | 설치 Build.version **5.8.2 CL 56702186**, **Development Editor Win64 성공**. 마지막 빌드 22.90초. 기존 VS 14.51 비권장 버전 경고 유지 |
| 프로젝트 파일 | 설치 UBT `-projectfiles -game -engine` 성공. 추가 모듈 의존성 없음 |
| 원본 TrainFreight 읽기 전용 검사 | **110 에셋 / BP 37 / 의존 패키지 285**, 누락 없음 |
| P5 자산 검사 | **BP 14 컴파일 / 의존 패키지 424 / 액터 144**, 누락 없음. 10량/42개/7,060 및 원본 부모·공용 걷기 400/전용 450 보존 |
| 기존 C++ 규칙 자동화 | **12개: 9 성공+3 기존 경고, 실패 0**. 점수 중복·종료 우선순위·생존·장치·정비 규칙 포함 |
| 새 로그 검사 회귀 테스트 | **7개 통과**. 중복 완료/종료, 잘못된 실패 지급, 비종료 정산·완료 ID 누락, 설정/누계 오류, 방문 잔여 값 구분 |
| P5 PIE | **2인 46개 / 4인 46개 통과**, 각각 성공 390 정산과 다섯 번 실패→재시작 포함. 6개 서로 다른 완료 RunId씩 |
| 핑 / 상태 | 각 소유자의 고정 좌표·종류·수명 복제 일치, 재사용/인당 수 제한, 벽 첫 표면, 30m 무표면 거부, 사망 거부, 종료·재시작 제거 |
| 화면 / 소리 | 60/30초 문구·큐 1회, 출발 1회·마지막 5틱·결과 1회, 음소거 중 경고 문구, 같은 판의 UI 1개, 재시작 후 이력 초기화 |
| 표현 추가 검사 | **13개 통과**. 모든 화물 재질을 PIE에서 교체한 뒤 S01 표시·350 전송 유지, 근거리 전송/감시 AudioComponent 각 1개·다른 객차 감쇠, 경고 도중 음소거 즉시 정지, 정비/부활 팀 UI, 빈 탈출 설명/0 지급, 재시작 후 재생 중인 UI/월드 음원 0 |
| 실제 키·버튼 | 준비·시작 버튼 클릭, 호스트/원격 MMB 각각 자기 PlayerId로 요청. F6~F9/0/NumPad0 차단 문구와 MotionState/SkyTransition 불변 확인 |
| 연결 복구 | 실제 호스트 `disconnect` 후 0 정산, 기본 맵으로 넘어가도 연결 중단 HUD 1개, 로딩, 전용 맵의 새 권한 호스트/Ready·가치/핑 초기화 검사 통과 |

기능 검사는 동일 PC의 Listen Server와 로컬 클라이언트, 실제 Enhanced Input/소유 Pawn RPC를 썼다. 위치·피해·시각에는 명시적인 PIE fixture를 사용했으며 점수·완료 결과를 직접 대입하지 않았다. 2/4인 주 검사와 표현 검사 후 마지막으로 준비 설정명 한글화와 연결 중단 시 소유 Pawn 없는 빈 생존 게이지 제거를 적용하고 재빌드·전체 BP 검사·연결 복구 화면을 다시 확인했다. 서로 다른 빌드의 실행 식별자는 JSONL에 그대로 남는다.

초기 UMG 템플릿 교체의 삭제된 위젯 GUID ensure를 수정했다. 검사 도구의 비공개 속성 접근, 파괴된 과거 위젯을 읽는 문제, 공간 음원이 먼 객차에서도 재생될 것이라는 잘못된 기대값도 고쳐 재실행했다. 분석용 WAV 내보내기에서 발생한 입력 디코더 오류는 제품 재생 오류와 구분하며, 불필요한 내보내기는 영구 setup 도구에서 제거했다. 초기 실패 로그를 최종 성공으로 바꿔 쓰지 않았다.

### 변경 파일·보존·남은 평가

주요 C++는 `Source/SpacePirate/Prototype01/SP1HUDWidget.*`, `SP1HUDPresentation.cpp`, `SP1RoundPresentation.cpp`, Round/Interaction/Session/Types/CargoDefinition, 화물·레이저·감시의 오디오 종료 경로, `Editor/SP1EvaluationHUDTemplate.cpp`다. 새 공용 UCLASS나 GameState 부모 교체는 없다. 기존 GameState 컴포넌트와 전용 Pawn의 E opt-in을 유지한다.

기존 전용 자산 수정은 **HUD, GameState, 화물 DataAsset 5개**다. 신규는 위 아이콘 **8개**다. 원본 열차/팩 1,724개, 공용 Source, **P4 전용 맵까지 SHA-256 불변**을 확인했다. 시작 기준 파일 169개 중 삭제 0, 기존 tracked diff도 동일하다. `bizet12`의 기존 27개 락과 신규 아이콘 8개 락을 유지한다. 강제 잠금 해제·커밋·푸시·PR·팀 메시지는 하지 않았다.

관련 도구는 `make_p5_icons.py`, `setup_p5.py`, `validate_p5.py`, `verify_p5.py`, `verify_p5_presentation.py`, `verify_p5_disconnect.py`, `audit_runs.py`, `test_audit_runs.py`다. 기존 P0~P4 setup을 현재 맵에 재실행하지 않는다.

**남은 확인과 재현 가능한 제한:**

1. 1920×1080 데스크톱에서 호스트·최대화 클라이언트 화면의 임무/팀/대상/결과/연결 중단 문구를 시각적으로 확인했다. Windows 제목줄/작업 영역으로 실제 PIE 뷰포트는 **1920×994 또는 1920×1000**이었다. 정확한 1920×1080 전체 화면과 별도 PC/패키지는 미실행이다. 기본 640×480 추가 클라이언트 창은 팀 글자가 많이 줄바꿈되고 하단 패널이 겹친다. 평가 시 Play 설정의 주/추가 클라이언트 크기를 1920×1080으로 지정하고 창을 최대화한다.
2. 동일 PC 4창의 **RTX 3060 Ti VRAM/텍스처 예산 초과**는 재현된다. 기능 검사의 일부에서 3D `r.ScreenPercentage 50`을 임시 사용했으며 UI 크기는 유지했다. 읽기 쉬운 캡처를 위해 엔진 화면 디버그 메시지를 임시 숨겼고 원래 경고 캡처/로그도 보존했다. 이를 GPU 성능 개선으로 보고하지 않는다.
3. 청취자는 `WBP_SP1HUD` Cues와 위 원본 음원을 비교하고, C04 S01·C06 감시·C10 출발에서 켜짐/음소거를 번갈아 평가해야 한다. 소리만으로 구분, 피로도, 경보 겹침, 설명 없이 종료 이유를 이해하는지에 대한 **사람 평가 미실행**이다. 최소 음원 연결·실제 재생 확인과 구분한다.
4. P5 지연/손실 네트워크 에뮬레이션, 다른 기기, 패키징, 일반 운반/중력 전체 플레이 회귀는 미실행이다. P5의 공용 소스는 불변이고 기존 네이티브 규칙 검사와 S01 전송·UI/핑/소리 리셋 연결 범위는 확인했다. 다음 평가 전에는 같은 맵에서 별도 PC 4인과 정확한 1080p 화면으로 위 항목을 확인한다.

대표 로컬 캡처는 `Saved/Prototype01/p5-material-s01.png`, `p5-2p-pings.png`, `p5-2p-silent-warning.png`, `p5-2p-success.png`, `p5-2p-failure.png`, `p5-connection-lost-final.png`다. 캡처 파일명만으로 대상 창을 추정하지 않고 실제 창 제목을 함께 확인한다.

최종 서버 원장 대조는 P5에서 시작 후 종료된 **26개 RunId 모두 통과**했다(성공 4 / 실패 18 / 연결 중단 4). 완료 CargoId 중복, 종료 중복, 전송 누계/최종 지급 불일치 0이며 시작 후 닫히지 않은 판도 0이다. 최종 빌드 `f2a6235+P5-d67d1894f0fb`에서 자동 복구 호출 없이 실제 복구 버튼을 눌러 **8개 확인**을 통과했다. 세부 근거는 [p5-verification.json](p5-verification.json)에 보존한다.

최종 에디터는 전용 맵, **PIE 0 / 미저장 맵·콘텐츠 0**이다. 1인 Standalone, 주 창 1280×720/추가 창 640×480, 백그라운드 CPU 제한 ON, NetEmulation OFF, 임시 화면 비율/디버그 표시를 복원했다. Python 원격 실행 OFF와 MCP HTTP 404를 확인했다. 커밋·푸시는 하지 않았다.
