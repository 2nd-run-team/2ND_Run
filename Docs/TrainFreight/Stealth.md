# 일반 경비 잠입 시스템 구현 및 유지보수 가이드

작성 기준: 2026-10-05 · `feature/item-interaction` · Unreal Engine 5.8.2.
이 문서는 팀원이 현재 기능을 실행하고, 동작을 이해하고, 수정할 파일을 찾기 위한 인수인계 자료다. 기획 전체가 아닌 **현재 소스와 저장된 시험 레벨의 구현 범위**를 설명한다.

## 구현 범위와 실행 방법

구현한 기능은 Ctrl 유지 앉기, 감속 이동과 앉은 자세, 경비 2명의 걷기 순찰·달리기 추격, 편집 가능한 순찰 경로, 직접 목격, 발소리 방향 확인, 플레이어별 신원 기억과 경보 전달, 마지막 목격 위치 수색, 느낌표와 개발용 시야 표시다.

1. C++을 `Development Editor / Win64`로 빌드한 뒤 프로젝트를 연다. 새 네이티브 클래스가 있으므로 에셋만 받아서는 충분하지 않다.
2. 콘텐츠 브라우저에서 `/Game/SpacePirate/Maps/Lvl_SPTrainFreight`를 연다. 요청 당시의 `spl_SPTrainFreight` 대신 실제 저장된 이 맵을 사용한다.
3. 아웃라이너의 `Stealth_DirectSight_SPCarBig` 폴더에서 시험 액터들을 찾는다.
4. 단일 플레이는 Play, 멀티플레이는 `Number of Players = 2`, `Play As Listen Server`로 실행한다.
5. 왼쪽 또는 오른쪽 Ctrl을 누른 채 W로 이동한다. 키를 놓으면 일어서기를 요청한다. 서서 경비 뒤로 접근하면 소리에 돌아보고, 이후 실제 시야에 들어오면 발각된다.

**앉기는 투명화가 아니다.** 발소리 판정을 피하고 낮은 자세로 이동하지만 몸통이나 상체가 시야에 노출되면 발각된다. 공격·피해, CCTV, 민간인 신고, 전체 경보 UI, 지원 병력, 범죄 행위 판정, 실제 발소리 오디오 재생은 이번 범위에 없다.

## 기능별 파일과 수정 지점

아래 경로는 저장소 루트 기준이며 파일명 링크로 원본을 확인할 수 있다. `.h`는 설정·상태 선언, `.cpp`는 실행 로직을 담당한다.

| 기능 | 파일 | 주요 수정 지점 |
|---|---|---|
| Ctrl 입력, 앉기 요청, 카메라 높이, 자세 클래스 연결 | [SPPlayerCharacter.h](../../Source/SpacePirate/SPPlayerCharacter.h), [SPPlayerCharacter.cpp](../../Source/SpacePirate/SPPlayerCharacter.cpp) | `SetupPlayerInputComponent`, `HoldCrouch`, `ReleaseCrouch`, `OnStartCrouch`, `OnEndCrouch`, `BeginPlay`, `EndPlay` |
| 앉기 허용과 이동 기본값 | [SPCharacterMovementComponent.cpp](../../Source/SpacePirate/SPCharacterMovementComponent.cpp) | 생성자의 `bCanCrouch`, `SetCrouchedHalfHeight`, `MaxWalkSpeedCrouched`. 기존 `GetMaxSpeed` 계열과 가방 감속도 확인 |
| 앉은 자세 블렌딩 | [SPCrouchAnimInstance.h](../../Source/SpacePirate/SPCrouchAnimInstance.h), [SPCrouchAnimInstance.cpp](../../Source/SpacePirate/SPCrouchAnimInstance.cpp) | `NativeUpdateAnimation`의 `CrouchAlpha` |
| 경비 감지·상태·순찰·추격·표시 | [SPGuardCharacter.h](../../Source/SpacePirate/SPGuardCharacter.h), [SPGuardCharacter.cpp](../../Source/SpacePirate/SPGuardCharacter.cpp) | `CanSeePlayer`, `CanHearPlayer`, `UpdateSight`, `UpdateHearing`, `UpdateBehavior`, `ReceiveSighting`, `DrawVision` |
| 경비 걷기·달리기 애니메이션 값 | [SPGuardAnimInstance.h](../../Source/SpacePirate/SPGuardAnimInstance.h), [SPGuardAnimInstance.cpp](../../Source/SpacePirate/SPGuardAnimInstance.cpp) | `NativeUpdateAnimation`의 속도·방향·재생 배율 계산 |
| 순찰 지점과 침입 구역 | [SPGuardPatrolRoute.h](../../Source/SpacePirate/SPGuardPatrolRoute.h), [SPGuardPatrolRoute.cpp](../../Source/SpacePirate/SPGuardPatrolRoute.cpp) | `Points`, `bLoop`, `RestrictedArea`, `GetPatrolLocation`, `ContainsLocation` |
| 플레이어별 신원 기억과 동료 호출 | [SPGuardAlertSubsystem.h](../../Source/SpacePirate/SPGuardAlertSubsystem.h), [SPGuardAlertSubsystem.cpp](../../Source/SpacePirate/SPGuardAlertSubsystem.cpp) | `IdentifiedPlayers`, `IsIdentified`, `ReportSighting` |
| 감지 회귀 검사 | [SPGuardTests.cpp](../../Source/SpacePirate/Tests/SPGuardTests.cpp) | `SpacePirate.Stealth` 자동 검사 4개 |

### 레벨과 애니메이션 에셋

| 에셋 | 역할과 연결 |
|---|---|
| [Lvl_SPTrainFreight.umap](../../Content/SpacePirate/Maps/Lvl_SPTrainFreight.umap) | 첫 Big 차량의 경비 2명, 경로, 벽, 내비게이션, PlayerStart 배치 저장 |
| [BP_SPGuardCharacter.uasset](../../Content/SpacePirate/Stealth/Blueprints/BP_SPGuardCharacter.uasset) | 부모는 네이티브 `ASPGuardCharacter`. 팀 플레이어의 메시·메시 상대 Transform을 사용하고 경비 전용 AnimBP 연결 |
| [ABP_SPGuardLocomotion.uasset](../../Content/SpacePirate/Stealth/Animation/ABP_SPGuardLocomotion.uasset) | 부모 `USPGuardAnimInstance`. 공용 `BS_Idle_Walk_Run`을 속도와 방향으로 재생 |
| [ABP_SPCrouchPostProcess.uasset](../../Content/SpacePirate/Stealth/Animation/ABP_SPCrouchPostProcess.uasset) | 부모 `USPCrouchAnimInstance`. 기존 애니메이션 결과에 Manny용 앉은 자세 적용 |

기존 에셋 중 [BP_SPPlayerCharacter](../../Content/SpacePirate/Player/Blueprints/BP_SPPlayerCharacter.uasset), [ABP_CargoCarry](../../Content/SpacePirate/Cargo/ABP_CargoCarry.uasset), [SKM_Manny_Simple](../../Content/Characters/Mannequins/Meshes/SKM_Manny_Simple.uasset), [BS_Idle_Walk_Run](../../Content/Characters/Mannequins/Anims/Unarmed/BS_Idle_Walk_Run.uasset)은 기반·참조 에셋이다. 이번 작업에서 이 바이너리들을 수정하지 않았다. 반면 **공용 플레이어와 이동 컴포넌트의 C++은 수정했으므로** 다른 맵의 플레이어에도 앉기 기능이 적용될 수 있다.

### 제작 및 검사 스크립트

| 파일 | 역할 |
|---|---|
| [setup_freight.py](../../Tools/Stealth/setup_freight.py) | 시험 액터 생성·재배치, 순찰 지점·침입 구역 설정, 경비 BP 메시 연결, PlayerStart 이동, 내비게이션 재생성 요청 |
| [build_crouch_pose.py](../../Tools/Stealth/build_crouch_pose.py) | 앉기 후처리 AnimGraph 생성·컴파일·저장 |
| [build_guard_animation.py](../../Tools/Stealth/build_guard_animation.py) | 경비 이동 AnimGraph 생성·저장, 경비 BP와 레벨 경비에 AnimClass 연결 |
| [verify_pie.py](../../Tools/Stealth/verify_pie.py) | 2인 PIE에서 시야·경보·수색·앉기 입력과 복제 검사 |
| [verify_hearing_pie.py](../../Tools/Stealth/verify_hearing_pie.py) | 새 2인 PIE에서 발소리 반응·회전·플레이어 구분·걷기/달리기 자세 변화 검사 |

이 스크립트는 **Unreal 에디터 내부 Python**에서 실행한다. 일반 Python 실행 파일로 직접 실행하는 프로그램이 아니다. 애니메이션 생성기와 PIE 검사기는 UE 5.8의 `editor_toolset`도 사용한다. 생성된 에셋은 이미 저장되어 있어 보통의 플레이에는 스크립트를 실행할 필요가 없다.

## 레벨 배치와 디테일 설정

현재 시험 대상은 첫 Big 차량이며 바닥 기준 원점은 `(-4595, 0, 210)`cm이다. 다음 로컬 좌표는 현재 배치의 해당 차량 원점을 기준으로 한다.

| 아웃라이너 이름 | 현재 배치 및 용도 |
|---|---|
| `Stealth_Big_PatrolRoute_EditPoints` | 차량 원점. 순찰 지점 4개와 침입 구역 보유 |
| `Stealth_Big_Guard_1` | 로컬 `(350,-630,98)`, Yaw 180°, 시작 지점 0 |
| `Stealth_Big_Guard_2` | 로컬 `(-350,630,98)`, Yaw 0°, 시작 지점 2 |
| `Stealth_Big_CenterCover` | 로컬 `(0,0,130)`, 크기 약 `90 × 800 × 260`cm. 이동 및 Visibility 차단 |
| `Stealth_Big_NavigationFloor` | 로컬 `(0,0,-2)`, 크기 `1070 × 1760 × 2`cm. 실제 바닥보다 상면이 1cm 낮은 숨김 내비게이션용 바닥 |
| `Stealth_Big_NavMeshBounds` | 로컬 중심 `(0,0,250)`, 범위 반크기 `(1500,1000,450)`cm |
| 기존 PlayerStart 4개 | 로컬 `(740,±120,110)`, `(920,±120,110)`. 시험 칸 앞 연결 구역에서 입구 방향 Yaw 180°로 시작 |

경로의 현재 `Points`는 `(350,-630,2) → (-350,-630,2) → (-350,630,2) → (350,630,2)`이다. 배열 값이나 뷰포트의 다이아몬드 조작점으로 수정한다. 값은 월드 좌표가 아니라 **경로 액터의 로컬 좌표**이며 편집기 화살표가 순서를 표시한다.

| 디테일 위치 | 설정 이름 | 현재 기본값과 의미 |
|---|---|---|
| 경로 / Patrol | `Points`, `Loop` | 4개 지점. Loop 켜짐은 순환, 꺼짐은 양 끝에서 반전하여 왕복 |
| 경로 / RestrictedArea 컴포넌트 | `Box Extent` | `(530,880,300)`cm. 전체 크기가 아닌 반크기 |
| 경비 / Guard / Patrol | `Patrol Route`, `Start Point Index`, `Patrol Wait Time` | 배치된 경로 참조, 0 또는 2, 지점 대기 1.2초 |
| 경비 / Guard / Movement | `Patrol Speed`, `Chase Speed` | 170 / 340cm/s |
| 경비 / Guard / Sight | `Sight Distance`, `Sight Angle`, `Vertical Sight Angle` | 1200cm, 전체 수평각 90°, 전체 수직각 100°. 좌우 반각은 각각 45° |
| 경비 / Guard / Sight | `Confirm Sight Time` | 미발각 플레이어를 연속 목격해야 하는 시간 1초 |
| 경비 / Guard / Hearing | `Hear Footsteps`, `Hearing Distance`, `Minimum Footstep Speed` | 켜짐, 650cm, 수평 이동속도 하한 80cm/s |
| 경비 / Guard / Hearing | `Listen Time`, `Hearing Turn Speed` | 마지막 소리 확인 후 2초, 회전속도 240°/s |
| 경비 / Guard / Search | `Search Time`, `Lost Target Timeout` | 현장 수색 5초, 마지막 목격 후 최대 12초 |
| 경비 / Guard / Alert | `Alert Group` | 배치 경비는 `FreightBig_DirectSight`. 네이티브 기본값은 `FreightGuards` |
| 경비 / Guard / Debug | `Show Vision` | 켜짐. 개발용 시야 표시 |
| 플레이어 / Character Movement | `Max Walk Speed Crouched`, 앉은 캡슐 높이 | 160cm/s, 캡슐 반높이 56cm |
| 플레이어 클래스 기본값 / Stealth / Animation | `Crouch Pose Class` | `ABP_SPCrouchPostProcess` |

BP 기본값과 레벨 인스턴스의 덮어쓰기 값은 다를 수 있다. C++ 기본값을 바꿔도 기존 액터의 값이 자동으로 바뀐다고 가정하지 말고, 실제 배치된 경비의 디테일을 확인한다.

## 서버 동작 순서와 상태

`ASPGuardCharacter`는 `ASPPlayerCharacter`를 상속하되 `AAIController`가 조종한다. Behavior Tree·StateTree·AI Perception 대신 C++에서 판정하며, 서버 Tick의 순서는 `UpdateSight → UpdateHearing → UpdateBehavior`다. 따라서 직접 목격 확인이나 이미 확정된 추격 대상이 발소리보다 우선한다.

| 상태 | 행동과 진입 조건 | 화면 표시 |
|---|---|---|
| `Patrol` | 경로 지점을 따라 이동. 목표와 소리·시야 의심이 없을 때 | 초록 시야, 머리 위 문자 없음 |
| `Listening` | 발소리 방향으로 정지 회전. 소리가 갱신되지 않으면 Listen Time 후 순찰 | 노랑 시야, `?` |
| `Suspicious` | 미발각 플레이어를 연속 목격하며 확인 시간 누적. 이동 정지 | 노랑 시야, `?` |
| `Pursuing` | 직접 발각 또는 동료의 목격 보고로 특정 플레이어의 마지막 목격 위치 추격 | 빨강 시야, `!` |
| `Searching` | 시야를 잃고 마지막 위치 근처에 도착하면 정지 회전 수색 | 노랑 시야, `?` |

대표 흐름은 `순찰 → 발소리 방향 확인 → 시야 확인 → 발각 및 동료 호출 → 추격 → 수색 → 순찰`이다. 소리 없이 직접 보아도 시야 확인부터 시작한다. 이미 기억한 플레이어를 다시 보면 확인 시간을 기다리지 않는다.

### 플레이어 앉기

`CrouchAction`이 지정되지 않았으면 런타임 Boolean 액션과 Mapping Context를 만들고 좌우 Ctrl을 매핑한다. `Triggered`에서 `Crouch()`, `Completed/Canceled`에서 `UnCrouch()`를 호출한다. 액션을 직접 지정한 경우에는 팀 IMC에 키 매핑도 직접 추가해야 한다. 런타임으로 추가한 Context는 `EndPlay`에서 제거한다.

엔진 CharacterMovement의 `bWantsToCrouch` SavedMove 경로로 이동 예측과 서버 처리를 연결하고, 복제되는 `bIsCrouched`를 자세와 청각 판정에 사용한다. 별도의 앉기 전용 RPC는 추가하지 않았다. 앉기 시작·종료 시 엔진 처리 후 카메라 상대 Z도 `HalfHeightAdjust × 0.5`만큼 내리거나 복원한다.

기존 기본 걷기 400cm/s, 달리기 700cm/s에 앉기 기본값 160cm/s를 추가했다. 앉기 상태의 속도 선택이 달리기보다 우선하고, 기존 가방 이동 배율은 계속 적용된다. 따라서 160은 가방 감속이 없는 기준이다. 머리 위 공간이 부족할 때 일어서기 가능 여부는 엔진 캡슐 충돌 처리에 따른다.

### 직접 목격과 엄폐

`CanSeePlayer`는 플레이어가 조종하는 유효한 캐릭터에 대해 다음을 검사한다.

1. 경비 눈 위치는 액터 위치의 Z + 64cm이다.
2. 대상 샘플은 캡슐 중심과 `중심 Z + 캡슐 반높이 × 0.65`의 두 점이다. 앉으면 실제 캡슐 높이가 반영된다.
3. 각 샘플이 거리, 전체 수평 시야각의 절반, 전체 수직 시야각의 절반 안에 있어야 한다.
4. 눈에서 샘플까지 `ECC_Visibility` Line Trace가 막히지 않아야 한다. 자신과 해당 대상은 트레이스에서 제외한다.
5. 두 샘플 중 하나라도 통과하면 보이는 것으로 판단한다.

`UpdateSight`는 여기에 신원·구역 조건을 추가한다. **미발각 플레이어는 경로의 RestrictedArea 안에 있을 때만** 확인 대상이고, 이미 같은 Alert Group에 신원이 기억된 플레이어는 구역 밖에서도 대상이다. `ContainsLocation`은 플레이어 액터 중심의 박스 내부 여부를 검사하며, Overlap 이벤트나 캡슐 전체 포함 검사가 아니다.

`ConfirmationTimes`는 플레이어 Pawn별로 시간을 따로 저장한다. 보이는 상태가 기본 1초 지속되면 발각되고, 도중에 가려지거나 시야·구역 조건을 벗어나면 그 플레이어의 시간이 제거된다. `SuspicionProgress`는 미확정 후보 중 가장 높은 진행률이며 특정 플레이어의 UI 정보로 사용하기에는 추가 구분이 필요하다.

엄폐물은 반드시 Visibility를 Block해야 한다. 메시가 눈에 보이더라도 해당 채널 충돌이 없으면 시야를 차단하지 않는다. 반대로 다른 액터가 이 채널을 막으면 그것도 가림에 영향을 줄 수 있다.

### 발소리에 뒤돌아보기

현재 발소리는 **서버에서 이동 상태로 판단하는 청각 모사**다. 사운드 파일·AnimNotify·바닥 재질별 소리나 AI Hearing 이벤트는 사용하지 않는다.

`CanHearPlayer`는 청각 활성화, 플레이어 조종, 서 있는 상태, 지상 이동, 수평속도 80cm/s 이상, 3차원 거리 650cm 이내를 모두 요구한다. 미발각 플레이어는 해당 침입 구역 안이어야 하며, 이미 신원이 기억된 플레이어는 구역 밖에서도 조건을 만족할 수 있다. 앉은 이동·정지·공중 이동은 이 판정에서 제외된다.

`UpdateHearing`는 추격 대상이나 시야 확인 중인 후보가 없을 때 0.25초 간격으로 검사한다. 이전에 듣던 플레이어가 계속 들리면 유지하고, 그렇지 않으면 가장 가까운 후보를 고른다. `HeardLocation`을 저장하고 2초 타이머를 갱신한 뒤, `Listening`에서 이동을 멈추고 그 방향으로 회전한다. 소리 위치로 걸어가지는 않는다.

청각은 시야각이나 벽 가림을 검사하지 않는다. 벽 뒤에서도 방향을 보지만 **청각만으로 신원 등록이나 경보 전파는 하지 않는다.** 돌아본 뒤 `UpdateSight`의 연속 목격을 통과해야 발각된다. 플레이어가 앉거나 멈추면 마지막 소리 위치를 잠시 보고 순찰로 복귀한다. 벽·층간 소리 감쇠를 구현하려면 `CanHearPlayer`부터 확장한다.

### 플레이어별 신원과 동료 호출

`USPGuardAlertSubsystem`은 월드 단위로 `AlertGroup → PlayerState 약한 참조 집합`을 관리한다. `ReportSighting`는 서버 목격자, 유효한 플레이어·PlayerState, 같은 월드를 확인한 다음 해당 신원을 등록하고, 같은 그룹의 모든 경비에게 `ReceiveSighting(플레이어, 목격 위치)`를 호출한다.

예를 들어 A만 발각되면 경비 1과 2의 목표는 A가 된다. 미발각 B의 확인 시간이 채워지거나 B의 신원이 자동 등록되지는 않는다. 다만 B도 별도로 침입 구역에서 연속 목격되면 B가 새로 발각되는 것은 정상이다. “A가 발각되면 모든 플레이어가 적”인 전역 Boolean은 사용하지 않는다.

동료는 직접 A를 보지 못해도 받은 위치로 출동한다. 다른 확정 대상을 눈앞에서 추격 중이면 무선 보고만으로 목표를 바꾸지 않지만, 기존 대상이 안 보이는 상황에서는 다른 확정 대상의 보고로 바뀔 수 있다. 다중 대상의 상세 위협도 정책은 구현하지 않았다.

보고 범위는 **거리 제한 없이 같은 월드의 동일 Alert Group**이다. 다른 차량을 독립 경계 구역으로 만들려면 그룹명을 구분한다. 이미 알려진 대상의 반복 보고는 경비별 약 0.4초 간격으로 제한한다.

순찰로 돌아와도 신원 기억은 지워지지 않는다. 기억은 현재 월드와 PlayerState 기준이며 세이브·접속 재개·맵 이동을 위한 영구 기록이 아니다. 같은 PlayerState를 유지하는 리스폰은 기억을 이어받을 수 있다. 현재 명시적인 신원 초기화 API는 없으며, 리스폰·재접속 정책을 추가할 때 함께 설계해야 한다.

### 순찰과 추격 및 수색

순찰은 현재 지점에서 수평거리 65cm 이내에 도착하면 1.2초 대기하고 다음 지점으로 이동한다. 경로가 없거나 지점 배열이 비어 있으면 정지한다. 복귀 시에는 기존 순찰 인덱스를 유지하며 가장 가까운 지점을 새로 찾지는 않는다.

추격은 `TargetPlayer`의 실시간 위치를 목적지로 고정하는 `MoveToActor` 대신, 직접 목격하거나 보고받은 `LastSeenLocation`을 `AAIController::MoveToLocation`에 전달한다. 길찾기와 목적지 내비게이션 투영, 부분 경로를 허용한다. 이동 요청 간격은 최소 0.35초이며, 이미 이동 중이고 목적지가 거의 같으면 재요청을 생략한다.

대상이 보이면 위치를 갱신하며 추격하고, 115cm 이내에서는 멈추어 바라본다. 시야를 잃으면 마지막 목격 위치로 이동한다. 그 위치의 100cm 이내에 도착하면 초당 50°로 회전하며 5초 수색한 뒤 순찰로 돌아간다. 시야를 잃은 상태에서 새 목격·보고 없이 12초가 지나면, 그 위치에 도달하지 못했어도 복귀한다. **계속 보이거나 새 보고가 들어오는 대상에는 이 종료 조건이 적용되지 않으며, 도달할 수 없는 위치에서도 반드시 5초 현장 수색을 하는 것은 아니다.** 새 목격·보고는 위치와 마지막 목격 시각을 갱신한다.

접근해도 공격하거나 피해를 주지 않는다. 현재 추격속도 340cm/s는 플레이어 기본 걷기 400cm/s보다 느리므로 쉽게 따돌릴 수 있다. 난이도를 바꿀 때는 거리·확인 시간뿐 아니라 이동속도도 함께 검토한다.

## 애니메이션 연결 원리

### 경비 걷기와 달리기

경비는 플레이어와 같은 캐릭터 기반이지만 AI 길찾기는 플레이어 입력 가속도와 다른 경로로 움직인다. 기존 운반 애니메이션의 가속도 조건에 의존하면 이동 중에도 대기 자세가 나올 수 있어, 별도 `USPGuardAnimInstance`와 `ABP_SPGuardLocomotion`을 연결했다.

`GroundSpeed`는 실제 수평 속도이고 `Direction`은 액터 로컬 좌표 속도의 방향이다. 공용 `BS_Idle_Walk_Run`의 X에 Direction, Y에 BlendSpeed를 넣는다. 속도 3cm/s 미만은 대기, PatrolSpeed는 블렌드 좌표 300의 걷기, ChaseSpeed는 좌표 600의 달리기에 대응시킨다. 중간 속도는 보간하고 `StridePlayRate = Clamp(GroundSpeed / Max(BlendSpeed, 1), 0.1, 2)`로 재생속도를 조절한다.

상태 이름만 보고 달리기를 강제하지 않으므로 추격 중에도 정지하면 대기하고, 감속하면 걷기로 섞인다. 클라이언트도 복제된 이동 속도로 계산한다. PatrolSpeed보다 ChaseSpeed가 크도록 조정하고, 블렌드 스페이스를 교체하면 300/600 표본 대응도 확인한다.

### 플레이어 앉은 자세

`ASPPlayerCharacter::BeginPlay`는 `CrouchPoseClass`를 로드하여 메시의 Post Process AnimBP override로 연결한다. `USPCrouchAnimInstance`는 `bIsCrouched`에 따라 CrouchAlpha를 0 또는 1로 보간한다(속도 14).

생성된 그래프는 입력 포즈를 컴포넌트 공간으로 바꾸고 골반 Z -40cm, 양 허벅지 Roll -55°, 종아리 +110°, 발 -55°를 Alpha만큼 더한 뒤 로컬 공간으로 복원한다. 기존 이동·운반 포즈에 덧붙이는 Manny용 시험 자세이며 전용 앉기 보행 클립은 아니다.

스켈레톤을 바꾸면 본 이름·축과 `CrouchPoseClass`를 함께 조정해야 한다. 이미 다른 Post Process AnimBP를 쓰는 캐릭터에 적용할 때는 현재 override 설정이 기존 후처리를 대체한다는 점을 확인하고 그래프를 통합한다. 기본 CrouchPoseClass 경로는 C++의 Soft Class 참조이므로 에셋 이름·경로를 바꿀 때 이 참조도 점검한다.

## 멀티플레이와 화면 표시

| 처리 | 실행 위치와 데이터 |
|---|---|
| 시야·청각·확인 타이머·목표 선정·길찾기 | 서버의 `HasAuthority()` 분기 |
| 신원 집합과 경보 전달 | 서버 Subsystem. `ReceiveSighting`는 RPC가 아니며 클라이언트 호출은 무시 |
| 상태 복제 | `GuardState`, `TargetPlayer`, `LastSeenLocation`, `SuspicionProgress`, `HeardLocation` |
| 경비 이동과 앉기 | 기존 Character/CharacterMovement 복제 경로 사용 |
| 머리 위 문자·시야·자세 재생 | 각 화면에서 복제된 상태와 이동을 사용. 전용 서버는 표시 생략 |

서버 내부의 플레이어별 확인 타이머, 청각 후보 참조, 신원 집합은 클라이언트로 복제하지 않는다. **시야·청각·속도 설정과 경로 Points도 Replicated 속성이 아니다.** 런타임에 서버에서만 설정을 바꾸면 클라이언트의 시야 그림이나 애니메이션 속도 대응이 달라질 수 있다. 플레이 전 맵/BP 설정을 공유하고, 실행 중 난이도 변경 기능을 추가한다면 필요한 설정의 복제를 별도로 구현한다.

머리 위 문자는 `UTextRenderComponent`이며 각 월드의 첫 로컬 플레이어 카메라를 향한다. `!`는 추격, `?`는 소리 확인·시야 확인·수색에 공통 사용한다. 분할 화면의 여러 로컬 카메라를 각각 향하는 구현은 아니다.

시야 그림은 눈높이에서 32개 구간으로 수평 트레이스한 뒤 바닥 높이에 투영한다. 실제 판정의 수직 범위와 두 신체 샘플을 모두 표현하는 정확한 감지 볼륨은 아니다. 낮은 엄폐물에서는 그림만으로 안전 여부를 단정하지 않는다. `Show Vision`을 끄거나 Shipping/Test로 빌드하면 시야 그림은 표시하지 않지만, `!/?` 표시는 별도의 빌드 제외 조건이 없다.

## 수정할 때 참고할 사항

| 수정 목적 | 작업 위치와 확인 사항 |
|---|---|
| 순찰 경로 변경 | 경로 Points를 수정하고 NavMesh 안에 지점을 둔다. 시작 인덱스와 벽 양쪽 경로 연결 확인 |
| 경비 추가 | `BP_SPGuardCharacter` 배치 후 PatrolRoute, StartPointIndex, AlertGroup 설정. 같은 그룹이면 먼 경비도 보고를 받음 |
| 다른 차량으로 시험 이동 | 경비·경로·벽·숨김 바닥·NavMeshBounds·PlayerStart를 함께 이동. 차량 편성 변경 후도 동일 |
| 발각 난이도 변경 | Sight/Hearing/Movement/Search 디테일 조정. 앉기를 시야 면제로 처리하지 않는 현재 규칙 유지 여부 검토 |
| 키 변경 | 플레이어 CrouchAction을 지정하고 팀 IMC에 매핑. Triggered와 해제 이벤트가 모두 발생하는지 확인 |
| 앉기 속도·높이 변경 | Movement 기본값 및 배치/BP override 확인. 카메라, 캡슐 통과 높이, 가방 감속, 자세를 같이 확인 |
| 경비 외형·동작 변경 | 경비 BP의 Mesh/AnimClass와 `SPGuardAnimInstance`, 해당 블렌드 스페이스 확인 |
| 경보 해제·라운드 초기화 추가 | AlertSubsystem의 PlayerState 기억 정책부터 확장. TargetPlayer를 비우는 것만으로 신원이 해제되지는 않음 |

시험 액터는 생성되는 차량 ChildActor에 붙어 있지 않다. 편성 재생성으로 삭제되는 것을 피하지만 차량을 이동·재정렬해도 자동으로 따라가지 않는다. 현재 구성은 고정된 열차 지형과 움직이는 배경을 전제로 하며, 실제 이동 플랫폼이나 임의 회전된 차량까지 검증한 구성은 아니다.

모듈의 생성 바닥은 내비게이션 영향이 꺼져 있어 별도의 숨김 바닥을 두었다. 숨김 바닥을 지우면 경비가 멈출 수 있다. 현재 길찾기 검증은 첫 Big 칸 내부에 한정한다. 다른 차량까지 추격시키려면 NavMeshBounds만 키우지 말고 실제 내비게이션용 바닥과 통로 연결도 마련해야 한다.

대규모 경비 수에 대한 성능 검증은 없다. 현재는 경비별 매 Tick 시야 검사, 청각 샘플링, 보고 시 월드 경비 순회를 사용한다. 경비·플레이어 수를 늘릴 때는 트레이스 수와 보고 순회 비용을 측정한 뒤 갱신 주기나 대상 목록 관리를 개선한다.

### 에셋 재생성과 협업

일반 튜닝은 디테일이나 에셋 편집으로 진행한다. 처음부터 시험 구성을 재생성할 때만 다음 순서를 사용한다.

1. 팀 LFS 규칙에 따라 맵과 편집할 `.uasset`의 잠금을 확인한다. C++ 클래스 변경 후 프로젝트 파일을 재생성하고 Editor 타깃을 빌드한다.
2. `Lvl_SPTrainFreight`를 열고 에디터 Python과 생성 도구에 필요한 UE 5.8 EditorToolset 환경을 확인한다.
3. 에디터 하단 Cmd에서 `py "<체크아웃 경로>/Tools/Stealth/setup_freight.py"`를 실행한다.
4. 같은 방식으로 `build_crouch_pose.py`, `build_guard_animation.py`를 차례로 실행한다. 최초 구성에서는 setup이 임시로 플레이어 AnimClass를 사용할 수 있으므로 마지막 경비 애니메이션 연결까지 수행한다.
5. 내비게이션과 경비 AnimClass를 확인하고 레벨을 저장한다. 생성 스크립트의 에셋 저장과 레벨 저장은 구분한다.

`setup_freight.py` 재실행은 이름으로 찾은 시험 액터의 위치·경로 지점·일부 설정과 PlayerStart 배치를 덮어쓴다. 두 애니메이션 생성기는 해당 생성 AnimGraph를 다시 만들므로 수동으로 추가한 노드를 보존하지 않는다. 재실행 전에 수정 내용을 별도로 보존하고 변경 내역을 검토한다.

팀 공유 시 새 C++ 파일, `Content/SpacePirate/Stealth`, 변경 맵, `Tools/Stealth`, 이 문서를 함께 포함한다. 네이티브 클래스 이름을 임의로 바꾸거나 삭제하면 BP 부모 참조가 깨질 수 있다. `Saved`, `Intermediate`, `Binaries`와 개인 검사 로그는 공유 소스에 포함하지 않는다. `Saved/Stealth`의 임시 도구는 게임 실행 의존성이 아니다.

## 검증 방법과 확인된 범위

### 자동 검사

에디터 Session Frontend의 Automation에서 `SpacePirate.Stealth` 그룹을 실행한다. 테스트 구현은 `Source/SpacePirate/Tests/SPGuardTests.cpp`에 있다.

| 테스트 | 확인 목적 |
|---|---|
| `SightAndCover` | 정면·후방·시야각·거리·엄폐 차단과 제거 후 시야 복원 |
| `PerPlayerAlertAndMemory` | 플레이어별 보고·신원 기억·다른 플레이어 분리 |
| `FootstepsAndCrouch` | 일반 이동과 앉기·정지·공중 이동의 청각 조건, 방향 확인 |
| `OccludedFootsteps` | 벽 뒤 소리 반응과 시야 확정 분리 |

### 실제 2인 PIE 검사

저장된 시험 레벨에서 **한 프로세스의 2인 Listen Server PIE**를 시작하고 에디터 Cmd에서 아래 첫 스크립트를 실행한다. 경로는 본인 체크아웃으로 바꾼다.

```text
py "E:/GitHub/2ND_Run/Tools/Stealth/verify_pie.py"
```

검사 완료 후 PIE를 종료하고 **새 2인 세션**에서 두 번째 스크립트를 실행한다.

```text
py "E:/GitHub/2ND_Run/Tools/Stealth/verify_hearing_pie.py"
```

검사기는 PIE 캐릭터 위치·경비 설정을 변경하며 실제 원격 소유 클라이언트의 Enhanced Input을 주입한다. 동시 실행하지 않고, 각 검사 후 PIE 종료로 시험 상태를 버린다. 소스의 현재 배치 좌표를 전제로 하므로 레벨 배치를 바꾸면 검사 좌표도 수정해야 한다. 발소리 검사는 애니메이션 샘플링을 위해 백그라운드 CPU 절전을 일시 해제하고 종료 처리에서 복원한다. 중간에 강제 중단했다면 해당 에디터 설정을 확인한다.

로컬 결과 경로는 `Saved/Stealth/pie-verification.json`, `Saved/Stealth/hearing-pie-verification.json`이다. 자동 검사 내보내기 기록은 이번 작업 환경의 `Saved/Stealth/Automation/index.json`에 있다. 이 파일들은 팀원이 자신의 환경에서 다시 생성하는 검사 산출물이다.

### 2026-10-05 구현 검증 기록

- Development Editor 빌드 및 프로젝트 파일 재생성 성공.
- 자동 검사 4개 완료, 실패 0개. 요약의 `succeededWithWarnings`가 4이며 개별 테스트 상태는 `Success`다. 메시 없는 네이티브 테스트 캐릭터의 소켓 조회 경고가 포함됐다.
- 기존 2인 PIE 검사 16개와 발소리·애니메이션 검사 8개 통과. 시야 차단·확인 시간 초기화·특정 플레이어만 경보 전파·숨은 대상 위치 미추적·수색 복귀·구역 밖 알려진 대상 재인식 확인.
- 참가자 입력을 통해 앉기 속도 160cm/s, 캡슐 반높이 56cm, 원격 앉기·해제와 자세 복제 확인.
- 양쪽 월드 경비의 속도·블렌드 값·발 관절의 시간 변화로 걷기·달리기 확인. 발소리만으로 신원 전파가 일어나지 않음을 확인.
- 화면의 시야와 느낌표·앉은 자세 확인. 저장 맵 재로드 후 경비 AnimClass 연결 및 순찰 네 구간의 완전한 내비게이션 경로 확인.

위 기록은 기능 구현 시 수행한 검증이다. 이 문서를 갱신할 때 빌드나 PIE를 새로 실행했다는 의미는 아니다. 별도 PC 간 접속, 지연·패킷 손실 환경, 전용 서버, 분할 화면, 패키징 실행은 검증하지 않았다.

## 예외 처리 현황

이 절의 예외 처리는 비정상 입력·참조 누락·실패 상황에 대한 방어와 복구를 뜻한다. 런타임 C++의 주된 방식은 조건 검사 후 `return`, 값 제한, 상태 전환이다. **모든 오류를 잡아 자동 복구하는 구조는 구현되어 있지 않다.** 아래의 ‘처리됨’도 표에 적힌 조건에 한정하며, 실패 상황을 모두 실행 시험했다는 의미는 아니다. 이번 정리는 소스 확인 기준이고 실제 실행 검증 범위는 앞 절과 같다.

### 처리되어 있는 항목

| 예외 상황 | 현재 처리 및 남는 한계 | 근거 파일과 함수 |
|---|---|---|
| 감지 대상이 없거나 파괴 중인 객체, 자기 자신, 플레이어가 조종하지 않는 Pawn | 시야·청각 함수가 `false`를 반환해 감지에서 제외한다. 체력이나 사망 상태 검사는 별개다. | `SPGuardCharacter.cpp` / `CanSeePlayer`, `CanHearPlayer` |
| 남아 있는 추격 대상 참조가 유효하지 않거나 조종이 해제됨 | `UpdateBehavior`의 대상 분기에서 확인되면 `ReturnToPatrol`로 목표를 비우고 이동을 중단한다. 이 동작은 AI Controller가 있어야 실행된다. | `SPGuardCharacter.cpp` / `UpdateBehavior`, `ReturnToPatrol` |
| 시야 확인 중 대상이 사라지거나 가려짐 | 해당 Pawn의 확인 시간을 제거한다. 잠깐 본 시간이 다음 목격에 누적되지 않는다. | `SPGuardCharacter.cpp` / `UpdateSight` |
| 순찰 경로가 null이거나 지점 배열이 비어 있음 | 순찰 이동을 중단한다. 대체 경로 자동 생성이나 누락 경고는 없다. | `SPGuardCharacter.cpp` / `UpdateBehavior` |
| 시작 지점·현재 순찰 인덱스가 배열 범위를 벗어남 | 지점이 있을 때 인덱스를 유효 범위로 제한한다. 경로의 위치 조회 함수 자체도 잘못된 인덱스에는 경로 액터 위치를 반환한다. | `SPGuardCharacter.cpp` / `BeginPlay`, `UpdateBehavior`; `SPGuardPatrolRoute.cpp` / `GetPatrolLocation` |
| 클라이언트 또는 잘못된 대상의 경보 보고 | 보고 함수는 목격자 권한, 대상 유효성·플레이어 조종 여부·PlayerState·대상 월드를 검사해 거부한다. 수신 함수도 서버 권한과 대상 유효성·조종 여부를 검사한다. | `SPGuardAlertSubsystem.cpp` / `ReportSighting`; `SPGuardCharacter.cpp` / `ReceiveSighting` |
| 다른 플레이어의 경보 때문에 현재 눈앞의 목표가 바뀜 | 현재 목표가 유효하고 직접 보이는 동안 다른 대상의 보고를 무시한다. 기존 목표가 보이지 않을 때까지 목표 고정을 보장하지는 않는다. | `SPGuardCharacter.cpp` / `ReceiveSighting` |
| 시야를 잃은 목표 위치에 도달하지 못함 | 새 목격·보고가 없으면 마지막 목격 후 12초에 순찰로 복귀한다. 일반적인 길찾기 실패 복구 기능은 아니다. | `SPGuardCharacter.cpp` / `UpdateBehavior` |
| 기본 앉기 액션이 지정되지 않음 | 런타임 액션과 좌우 Ctrl 매핑을 생성한다. 액션을 직접 지정했는데 키 매핑이 없는 경우까지 보완하지는 않는다. | `SPPlayerCharacter.cpp` / `SetupPlayerInputComponent` |
| 입력 컴포넌트가 Enhanced Input이 아님 | 개발용 오류 로그를 남기고 입력 설정을 중단한다. 자동으로 컴포넌트를 교체하지 않는다. 로그는 Shipping/Test에서 제외된다. | `SPPlayerCharacter.cpp` / `SetupPlayerInputComponent`; `SPDebug.h` |
| 캐릭터 종료 후 임시 앉기 매핑이 남음 | 유효한 LocalPlayer·Subsystem과 생성한 Context가 있으면 종료 시 제거한다. | `SPPlayerCharacter.cpp` / `EndPlay` |
| 애니메이션 소유 Pawn이 기대한 클래스가 아님 | 경비 애님은 속도·방향을 0, 재생 배율을 1로 두고 종료한다. 앉기 애님은 Alpha를 0으로 보간한다. | `SPGuardAnimInstance.cpp`, `SPCrouchAnimInstance.cpp` / `NativeUpdateAnimation` |
| 일부 계산의 0 나눗셈·잘못된 범위 | 확인 진행률의 분모를 최소 0.001로 제한한다. 경비 애니메이션은 걷기 기준을 최소 1, 달리기 기준을 걷기+1 이상으로 계산하고 재생 배율을 제한한다. 수평 감지각도 1~179°로 제한한다. | `SPGuardCharacter.cpp` / `UpdateSight`, `CanSeePlayer`; `SPGuardAnimInstance.cpp` |

### 일부만 처리되었거나 처리되지 않은 항목

| 상태 | 예외 상황 | 현재 실제 동작과 부족한 부분 | 관련 위치 |
|---|---|---|---|
| 미처리 | NavMesh 누락, 막힌 순찰 지점, 이동 요청 실패 | `MoveToLocation`의 반환 결과와 이동 완료 실패를 별도로 처리하지 않는다. 요청 간격 제한은 있지만 실패 지점 건너뛰기, 막힘 감지, 별도 복구 경로·오류 로그는 없다. 순찰 지점에 도착하지 못하면 같은 지점으로 계속 이동을 시도할 수 있다. | `SPGuardCharacter.cpp` / `MoveToLocation`, `UpdateBehavior` |
| 미처리 | 보이지만 도달할 수 없는 대상 | 직접 보이는 대상은 매번 목격 시간이 갱신되고 시야 상실 타임아웃 분기에도 들어가지 않는다. 길이 막혀 있어도 자동으로 추격을 포기하는 처리는 없다. | `SPGuardCharacter.cpp` / `UpdateSight`, `UpdateBehavior` |
| 부분 처리 | AI Controller 없음 | 이동·행동 함수는 null을 확인하고 반환한다. Controller 자동 복구·재소유, 오류 안내는 없다. 시야 처리와 상태 변경은 계속될 수 있어 표시 상태와 이동이 어긋날 수 있다. | `SPGuardCharacter.cpp` / `MoveToLocation`, `UpdateBehavior` |
| 부분 처리 | AlertSubsystem 없음 | 시야 갱신을 건너뛰지만, 반환 전에 기존 시야·의심 상태를 초기화하지 않는다. 이전 상태 정리나 Subsystem 복구는 없다. 정상 Game/PIE 월드에서는 Subsystem 생성을 전제로 한다. | `SPGuardCharacter.cpp` / `UpdateSight` |
| 부분 처리 | 플레이어의 PlayerState가 없음 | Subsystem은 신원 등록·전파를 거부한다. 그러나 시야 확정 후 목표가 없는 경비는 별도 `ReceiveSighting` 경로로 그 Pawn을 로컬 목표로 삼을 수 있다. 신원·전파와 개별 목표의 일관성을 보장하는 처리는 없다. | `SPGuardAlertSubsystem.cpp` / `ReportSighting`; `SPGuardCharacter.cpp` / `UpdateSight` |
| 부분 처리 | 앉기 AnimBP가 비어 있거나 로드 실패 | 메시·클래스 지정·로드 성공을 확인해 후처리 연결을 건너뛴다. 앉기 이동·캡슐 처리는 남을 수 있으나 대체 자세, 재시도, 이 기능 전용 실패 로그는 없다. | `SPPlayerCharacter.cpp` / `BeginPlay` |
| 미처리 | 경비 AnimBP 오류, 다른 스켈레톤·본, 후처리 충돌 | 제작 시 일부 컴파일 검사는 있지만 런타임 호환성 검사나 대체 AnimBP 연결은 없다. 기존 Post Process override와 자동 합성하는 처리도 없다. | 경비 BP, 두 AnimBP, `SPPlayerCharacter.cpp` / `BeginPlay` |
| 부분 처리 | LocalPlayer 또는 Enhanced Input Subsystem 없음 | 존재 여부를 확인하고 임시 매핑 추가를 건너뛴다. 앉기 매핑 전용 재시도·오류 안내는 없다. 직접 지정한 CrouchAction의 키 매핑도 검증하지 않는다. | `SPPlayerCharacter.cpp` / `SetupPlayerInputComponent` |
| 부분 처리 | 런타임에 음수·비정상 수치가 설정됨 | 디테일의 `ClampMin/ClampMax`와 일부 계산 방어만 있다. 모든 값의 런타임 범위·NaN·무한대 검증, PatrolSpeed/ChaseSpeed 관계 보정은 없다. 애니메이션 계산의 보정이 실제 이동 설정까지 고치지는 않는다. | `SPGuardCharacter.h`, `SPGuardCharacter.cpp`, `SPGuardAnimInstance.cpp` |
| 미처리 | 사망했지만 여전히 조종 중인 플레이어 | 감지 함수에 체력·사망·관전자 상태의 별도 필터가 없다. 해당 상태를 도입할 때 대상 제외 정책을 추가해야 한다. | `SPGuardCharacter.cpp` / `CanSeePlayer`, `CanHearPlayer` |
| 미처리 | 재접속·리스폰·라운드 초기화 시 신원 정리 | 신원은 PlayerState 약한 참조로 저장하지만 명시적 초기화·영구 저장·재접속 매핑이나 만료 항목 정리 정책은 없다. 약한 참조가 객체 생존을 강제로 연장하지 않는 것과 게임 규칙에 맞게 기억을 정리하는 것은 다르다. | `SPGuardAlertSubsystem.h/.cpp` |
| 미처리 | 런타임 경로 액터 파괴·기본 컴포넌트 제거 | 경로 사용 시 주로 null 검사만 하며 모든 호출에 `IsValid` 검사가 있는 것은 아니다. Capsule·Movement·Mesh·RestrictedArea 같은 기본 컴포넌트가 존재한다고 가정한다. 동적 제거에 대한 일관된 방어·대체 구성은 없다. | `SPGuardCharacter.cpp`, `SPGuardPatrolRoute.cpp`, `SPPlayerCharacter.cpp` |
| 미처리 | 서버에서만 시야·청각·속도·경로 설정 변경 | 설정값 자동 복제가 없어 클라이언트의 그림·애니메이션 계산과 달라질 수 있다. 설정 불일치 감지·재동기화 처리는 없다. | `SPGuardCharacter.h`, `SPGuardPatrolRoute.h` |

### 제작 및 검사 스크립트의 실패 처리

| 구분 | 처리된 내용 | 처리되지 않은 내용 |
|---|---|---|
| `setup_freight.py` | 다른 맵을 연 상태, 중복 시험 액터 이름, 잘못된 NavMesh 브러시 크기에서 `RuntimeError`로 중단한다. 경비 BP 컴파일·저장 성공도 검사한다. | Big 차량이 없으면 `min()` 예외로 중단된다. 참조 에셋 로드·생성 실패를 모두 사전 검사하지 않는다. PlayerStart 수가 4개인지 검증하지 않아 부족하면 있는 것만 옮긴다. 실패 전 수정·저장한 내용의 자동 롤백은 없다. |
| 두 애니메이션 생성기 | 노드 검색과 일부 값, 컴파일·저장 결과를 `assert` 또는 예외로 검사한다. | 기존 그래프를 지운 뒤 중간에 실패해도 이전 그래프를 복원하지 않는다. 전체 작업의 원자적 저장·자동 백업·롤백은 없다. |
| 두 PIE 검사기 | 세션·액터 등 일부 전제조건을 검사하고, Tick 콜백 안의 예외를 결과에 기록한 뒤 종료 함수를 호출한다. 종료 함수는 결과 파일 저장 후 콜백을 해제하고, 발소리 검사는 성능 설정 복원을 시도한다. | 초기화 구간의 모든 예외와 에디터 강제 종료를 포괄하는 복구는 없다. 종료 함수에서 입력·액터 상태를 일괄 초기화하지 않는다. 결과 저장이 실패하면 뒤의 콜백 해제·설정 복원도 실행되지 않을 수 있으며 별도 재시도는 없다. 검사 후 PIE 종료로 시험 상태를 폐기한다. |

따라서 ‘실패 시 중단한다’와 ‘실패 전 상태로 복원한다’를 구분해야 한다. 현재 제작 스크립트는 주로 전자이며, 실행 전에 작업 내용을 저장·보존하고 실패 후에는 맵과 에셋 변경 내역을 확인해야 한다. 미처리 항목은 문서에 기록한 상태이며 이번 문서 수정으로 기능 코드에 예외 처리를 추가한 것은 아니다.

## 증상별 확인 순서

| 증상 | 먼저 확인할 내용 |
|---|---|
| 경비가 서 있기만 함 | PatrolRoute·Points, AI Controller 소유, 숨김 바닥·NavMesh 범위, 지점 사이 경로 연결 |
| 이동하지만 다리가 움직이지 않음 | 경비 Mesh의 AnimClass가 `ABP_SPGuardLocomotion_C`인지, `SPGuardAnimInstance`가 로드됐는지 확인 |
| Ctrl을 눌러도 앉지 않음 | CrouchAction을 직접 지정했다면 IMC 매핑 확인. CharacterMovement의 앉기 허용과 실제 BP 기본값 확인 |
| 앉아도 겉모습이 서 있음 | CrouchPoseClass 경로, Post Process AnimBP override, 스켈레톤·본 호환 확인 |
| 벽 뒤에서 발각됨 | Visibility 충돌과 실제 상체 노출 확인. 벽 뒤에서 `?`만 뜨는 것은 청각 반응일 수 있음 |
| 서서 걸어도 안 돌아봄 | 구역·650cm 거리·80cm/s 하한·지상 이동 확인. 기존 목표나 시야 확인 후보가 있으면 청각은 후순위 |
| 두 경비가 함께 출동하지 않음 | AlertGroup 일치, 1초 시야 확정, PlayerState 유효성, 다른 확정 대상을 눈앞에서 추격 중인지 확인 |
| 순찰 복귀 후 바로 재발각됨 | 같은 PlayerState의 신원을 기억하는 의도된 동작. 초기 상태 검사는 새 PIE 월드에서 수행 |
| 시야 그림과 발각이 다름 | 그림은 수평 투영 참고용. 수직각·두 신체 샘플·서버 설정을 확인 |
