# 기차 화물 레벨 — 팀 프로젝트 통합

작업일: 2026-09-30 · Unreal Engine 5.8.2 · 프로젝트: `SpacePirate.uproject`

## 실행과 편집

콘텐츠 브라우저에서 **SpacePirate → Maps → Lvl_SPTrainFreight**를 연다.
기본 시작 맵과 프로젝트 전체 GameMode는 변경하지 않았다.

| 대상 | 위치 / 역할 |
|---|---|
| 레벨 | `/Game/SpacePirate/Maps/Lvl_SPTrainFreight` |
| 기차 제작 에셋 | `/Game/SpacePirate/Train` |
| 차량 편성 | `Freight/Blueprints/BP_SPTrainConsist` |
| 차량 정의 | `Freight/Data/DA_SPCar_*` |
| 배경 선택 | `Systems/TrainEnvironment/Scenes/BP_SPTrainSceneController` |
| 화물 맵 배경 프리셋 | `Freight/Data/DA_SPTrainScene_FreightPlanet` |
| 레벨 전용 GameMode | `Environment/PlanetFlyby/Blueprints/BP_SPPlanetTrainGameMode` |
| 하늘 목록 | `Environment/PlanetFlyby/Data/DA_SPPlanetSkyCatalog` |

레벨의 `TrainConsist_EditCarSequence`에서 **Car Sequence / Max Cars**를 편집한다.
기존 12량 배열, 차량 배치, 메시, 재질과 연결 통로를 유지했다.
배경은 `TrainScene_SelectPreset`의 **Environment Preset**에서 선택한다.
전체 프리셋 교체는 Play 전에 한다. 실행 중 하늘 전환은 아래 키를 사용한다.

## 팀 규칙 적용

커밋 `0bf4935`의 `Content/SpacePirate`, `BP_SP*`, `IA_SP*` 구성과
`4c0eb5e`에서 사용한 `Lvl_SPTestMap` 명칭을 근거로 정리했다.
저장소에 `L_`와 `Lvl_`가 공존하므로, 이번 레벨에는 최근 사용된 `Lvl_`를 적용했다.

- `L_TrainFreight` → `Lvl_SPTrainFreight`
- 직접 제작한 기차 에셋 110개에 `SP` 적용: `BP_SP*`, `SM_SP*`, `M_SP*`, `MI_SP*`, `T_SP*`, `DA_SP*`, `ST_SP*`
- 직접 제작한 에셋은 `SpacePirate/Train`으로 이동하고 맵은 팀 `SpacePirate/Maps`에 둔다.
- 원본 외부 콘텐츠 `Train`, `Planet_Project`, `StarfieldFree`의 이름과 경로는 유지한다.
- 전체 이전/이후 경로는 [asset-paths.json](asset-paths.json)에 기록했다.

탐색기에서 바이너리 파일 이름을 바꾼 것이 아니라 Unreal AssetTools로 참조까지 갱신했다.
이전 맵 경로 `TrainGame/Maps/L_TrainFreight`에는 호환용 Redirector 하나가 남는다.
이것은 두 번째 플레이 레벨이 아니다. 새 작업과 맵 선택에는 항상 새 경로를 사용한다.
나머지 이전 기차 에셋 경로는 정리됐다.

## 협동 플레이 연결과 수정

이주 직후에는 기차 GameMode가 FirstPerson 템플릿 캐릭터/입력에 연결되어 있었다.
기차 쪽 Blueprint만 다음처럼 연결했다.

- `BP_SPTrainGameMode`의 부모: 팀 `BP_SPGameMode`
- `BP_SPTrainPlayerController`의 부모: 팀 `BP_SPPlayerController`
- 기차/행성 GameMode의 Pawn: 팀 `BP_SPPlayerCharacter`
- 기차/행성 PlayerController의 입력: 팀 `IMC_SPPlayer`
- 열차 GameState와 행성 하늘 GameState, 서버 권한 로직은 유지

팀의 공용 캐릭터, 컨트롤러, 입력 에셋, C++ 소스는 수정하지 않았다.
기차 고유 동작은 기차 자식 클래스에 추가하고 공용 플레이어 기능은 팀 에셋에서 계속 관리한다.

| 입력 | 동작 | 권한 |
|---|---|---|
| WASD / 마우스 / Space / Shift | 팀 이동·시점·점프·달리기 | 각 플레이어 |
| 숫자 1~3 / 휠 | 팀 인벤토리 슬롯 선택 | 각 플레이어 |
| F6 | 열차 감속 정지 | 호스트 |
| F7 | 열차 목표 속도 1500 cm/s | 호스트 |
| F8 | 열차 목표 속도 3000 cm/s | 호스트 |
| F9 | 열차 즉시 정지 | 호스트 |
| 숫자 0 / NumPad 0 | 다음 하늘 전환 요청 | 모든 플레이어, 서버 승인 |

기존 열차 속도 키 1~4는 인벤토리와 충돌하므로 F6~F9로 옮겼다.
열차는 고정된 플레이 지형이며 배경이 이동한다.

네 개의 PlayerStart는 첫 Small 차량의 실내로 옮겼다.
기존 높이 Z=595~635는 지붕 위 스폰을 만들었다. 새 캡슐 중심은 Z=320이며,
XY는 `(-650,-80)`, `(-650,80)`, `(-1000,-80)`, `(-1000,80)`이다.
차량 배열의 첫 번째 차량이나 높이를 바꾸면 스폰도 다시 확인한다.

## 검증과 협업

검사 스크립트는 `Tools/TrainFreight/validate_assets.py`이며 에셋을 저장하지 않는다.
에디터 하단 **Cmd** 입력에서 실행한다(경로는 본인 체크아웃에 맞게 변경).

```text
py "E:/GitHub/2ND_Run/Tools/TrainFreight/validate_assets.py"
```

결과는 `Saved/TrainFreightMigration/asset-validation.json`에 기록된다.
110개 로드, 기차 Blueprint 컴파일, 재귀 의존성, 이전 프로젝트/경로 참조,
레벨 GameMode와 팀 Pawn/Input 연결을 검사한다.

이번 작업의 [검증 요약](verification.json):

- 별도 UnrealEditor-Cmd 프로세스에서 110개 로드, Blueprint 37개 컴파일 성공(종료 코드 0).
- 게임 콘텐츠 의존 패키지 285개 검사: 누락, 이전 `TrainGame` 참조, 개인 `Test2`/제작 플러그인 참조 없음.
- Listen Server 1개 + Client 3개 모두 플레이어 4명과 로컬 플레이어 1명 확인.
- 네 월드에서 실내 스폰 후 캡슐 중심 Z≈308.15로 안정화, 분리된 화물칸 컴포넌트 없음.
- 서버 함수를 통해 속도 1500→0→3000과 하늘 전환을 실행하고 네 월드의 복제 상태 일치 확인.
- 실제 키 입력부터 RPC까지의 전 과정, 화물 집기/협동 운반, 인터넷 세션은 이번 자동 검사의 범위가 아니다.
- 테스트 후 PIE는 종료하고 개인 실행 설정은 기존 1인 Standalone으로 복원했다.

레벨 편집 전 `git lfs lock Content/SpacePirate/Maps/Lvl_SPTrainFreight.umap`을 사용한다.
이 맵은 OFPA/World Partition으로 새로 변환하지 않았다. 맵 동시 편집은 피하고,
차량 Blueprint·재질·데이터 에셋 단위로 작업을 나누되 해당 파일도 팀 LFS 규칙을 따른다.

마이그레이션은 C++ 소스, 프로젝트 설정과 제작 도구를 자동으로 이식하지 않는다.
이 레벨의 실행은 기존 팀 C++ 모듈과 생성된 Blueprint를 사용한다.
원본 `Test2`의 `TrainBlueprintBuilder` 제작 플러그인은 실행 의존성으로 추가하지 않았다.

**이주 파일은 아직 커밋되지 않은 신규 파일이다.** 맵만 커밋하면 팀원에게 메시·재질이 누락된다.
새 맵, `Content/SpacePirate/Train`, 원본 의존 콘텐츠 세 폴더,
호환 맵 Redirector, 이 문서 폴더와 검사 스크립트를 같은 변경으로 검토한다.
`Saved`, `Intermediate`, `Binaries`, 백업과 검사 로그는 커밋하지 않는다.
최초 이주에 섞인 `Content/FirstPerson/Blueprints/BP_FirstPersonCameraManager.uasset`는
별도로 보존했으며 이번 이름 변경 대상이 아니다.

4인 테스트에서는 Number of Players=4, Play As Listen Server로 확인한다.
로컬 네 화면 렌더링에서 GPU 메모리/텍스처 스트리밍 풀 부족 경고가 관찰됐다.
이는 별도 최적화 항목이며 프로젝트 공용 렌더링 설정은 변경하지 않았다.
일반 아트 작업은 1인으로, 네트워크 검사는 낮은 해상도 또는 여러 PC로 나누어 진행한다.
인터넷 세션, Steam/EOS, 패키지 배포 및 전용 서버 검증과 PIE 검증은 구분한다.

공식 동작 참고: [에셋 마이그레이션](https://dev.epicgames.com/documentation/unreal-engine/migrating-assets-in-unreal-engine),
[에셋 Redirector](https://dev.epicgames.com/documentation/unreal-engine/asset-redirectors-in-unreal-engine).
